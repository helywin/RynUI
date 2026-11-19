use skrifa::{
    outline::{
        DrawSettings, Engine, HintingInstance, HintingOptions, OutlineGlyphFormat, OutlinePen,
        SmoothMode, Target,
    },
    prelude::{LocationRef, Size},
    FontRef, GlyphId, MetadataProvider,
};
use std::{ffi::c_void, slice};

type Emit = extern "C" fn(*mut c_void, u8, *const f32);

struct Path<'a> {
    context: *mut c_void,
    emit: &'a Emit,
}

impl Path<'_> {
    fn append(&self, verb: u8, coordinates: &[f32]) {
        (self.emit)(self.context, verb, coordinates.as_ptr());
    }
}

impl OutlinePen for Path<'_> {
    fn move_to(&mut self, x: f32, y: f32) {
        self.append(b'M', &[x, -y]);
    }
    fn line_to(&mut self, x: f32, y: f32) {
        self.append(b'L', &[x, -y]);
    }
    fn quad_to(&mut self, x1: f32, y1: f32, x: f32, y: f32) {
        self.append(b'Q', &[x1, -y1, x, -y]);
    }
    fn curve_to(&mut self, x1: f32, y1: f32, x2: f32, y2: f32, x: f32, y: f32) {
        self.append(b'C', &[x1, -y1, x2, -y2, x, -y]);
    }
    fn close(&mut self) {
        self.append(b'Z', &[]);
    }
}

fn read_font(data: &[u8], index: u32) -> Option<FontRef<'_>> {
    FontRef::from_index(data, index).ok()
}

/// Bytes and callback arguments remain borrowed for this call only.
#[no_mangle]
pub unsafe extern "C" fn rynui_fontations_draw_outline(
    data: *const u8,
    length: usize,
    face: u32,
    glyph: u32,
    em: f32,
    hint_style: u8,
    context: *mut c_void,
    emit: Emit,
    advance: *mut f32,
) -> bool {
    if data.is_null()
        || length == 0
        || length > isize::MAX as usize
        || advance.is_null()
        || !em.is_finite()
        || em <= 0.0
        || hint_style > 4
    {
        return false;
    }
    let data = unsafe { slice::from_raw_parts(data, length) };
    let Some(font) = read_font(data, face) else {
        return false;
    };
    let index = GlyphId::new(glyph);
    let size = Size::new(em);
    let coordinates = LocationRef::default();
    let outlines = font.outline_glyphs();
    let Some(outline) = outlines.get(index) else {
        return false;
    };
    let Some(width) = font.glyph_metrics(size, coordinates).advance_width(index) else {
        return false;
    };
    let mut path = Path {
        context,
        emit: &emit,
    };
    let result = if hint_style == 1 && !outlines.require_interpreter() {
        outline.draw(DrawSettings::unhinted(size, coordinates), &mut path)
    } else {
        let hinting = if outlines.require_interpreter() {
            HintingInstance::new(
                &outlines,
                size,
                coordinates,
                skrifa::outline::HintingMode::Strong,
            )
        } else {
            let light = hint_style == 2;
            let engine = if light && outlines.format() == Some(OutlineGlyphFormat::Glyf) {
                Engine::Auto(None)
            } else {
                Engine::AutoFallback
            };
            let target = Target::Smooth {
                mode: if light {
                    SmoothMode::Light
                } else {
                    SmoothMode::Normal
                },
                symmetric_rendering: true,
                preserve_linear_metrics: false,
            };
            HintingInstance::new(
                &outlines,
                size,
                coordinates,
                HintingOptions { engine, target },
            )
        };
        let Ok(hinting) = hinting else { return false };
        outline.draw(DrawSettings::hinted(&hinting, false), &mut path)
    };
    if result.is_err() {
        return false;
    }
    unsafe { advance.write(width) };
    true
}
