"""Isolated, pure Python, locked fontTools instance recipe (acceptance only)."""
from pathlib import Path
import sys


def main():
    library, source, target, identity, weight = sys.argv[1:]
    weight = int(weight)
    if (identity, weight) not in (('noto-sans-cjk-sc', 500), ('noto-sans-cjk-sc', 600), ('noto-emoji', 400)):
        raise ValueError('only locked CJK 500/600 and emoji 400 instances are supported')
    sys.path.insert(0, library)
    import fontTools
    if fontTools.__version__ != '4.60.1' or not Path(fontTools.__file__).resolve().is_relative_to(Path(library).resolve()):
        raise ValueError('locked fontTools 4.60.1 source required')
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont
    with TTFont(source, recalcTimestamp=False) as font:
        if [axis.axisTag for axis in font['fvar'].axes] != ['wght'] or 'glyf' not in font:
            raise ValueError('expected the locked monochrome TrueType weight variable font')
        cjk = identity == 'noto-sans-cjk-sc'
        if not cjk and identity != 'noto-emoji':
            raise ValueError('unknown locked instance')
        instance = instantiateVariableFont(font, {'wght': weight}, inplace=True, updateFontNames=not cjk)
        if cjk:
            # Keep the existing 600 naming recipe byte stable. Both names follow
            # actual gvar/HVAR interpolation, never relabeling regular outlines.
            style = 'Medium' if weight == 500 else 'SemiBold'
            for name_id, value in {1: 'Noto Sans CJK SC ' + style, 2: 'Regular',
                                   3: f'RynUI:NotoSansCJKsc-{style}:2.004:wght{weight}',
                                   4: 'Noto Sans CJK SC ' + style, 6: 'NotoSansCJKsc-' + style,
                                   16: 'Noto Sans CJK SC', 17: style}.items():
                instance['name'].removeNames(nameID=name_id)
                instance['name'].setName(value, name_id, 3, 1, 0x409)
        if 'fvar' in instance or 'gvar' in instance or instance['OS/2'].usWeightClass != weight:
            raise ValueError('instance still variable or has wrong actual weight')
        instance.save(target, reorderTables=True)


if __name__ == '__main__':
    main()
