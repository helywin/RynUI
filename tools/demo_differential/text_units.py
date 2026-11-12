"""Lossless scalar-boundary unit conversion; Core owns grapheme eligibility."""
from .protocol import ProtocolError


def boundaries(text):
    if not isinstance(text,str):raise ProtocolError('selection text must be Unicode')
    positions=[(0,0)];byte=0;utf16=0
    try:
        for scalar in text:
            byte+=len(scalar.encode('utf-8'));utf16+=len(scalar.encode('utf-16-le'))//2
            positions.append((byte,utf16))
    except UnicodeError as error:
        raise ProtocolError('selection text contains an unpaired surrogate') from error
    return positions


def convert(text,offset,source,target):
    units={'UTF-8-bytes':0,'UTF-16':1}
    if source not in units or target not in units or type(offset) is not int or offset<0:
        raise ProtocolError('unknown/non-integral selection unit or offset')
    for position in boundaries(text):
        if position[units[source]]==offset:return position[units[target]]
    raise ProtocolError('selection endpoint splits a scalar or exceeds text')


def selection(text,anchor,caret,source,target='UTF-16'):
    # Direction and duplicates are preserved, including a reversed selection.
    # A valid scalar boundary does not authorize InputRef.select inside a grapheme.
    return [convert(text,anchor,source,target),convert(text,caret,source,target)]
