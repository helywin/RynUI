#include <cstdlib>
#include <iostream>
#include <string_view>

#include <ft2build.h>
#include FT_FREETYPE_H
#include <hb-ft.h>
#include <hb.h>

namespace {

[[noreturn]] void fail(std::string_view message) {
    std::cerr << message << '\n';
    std::exit(EXIT_FAILURE);
}

void require(bool condition, std::string_view message) {
    if (!condition) {
        fail(message);
    }
}

} // namespace

int main() {
    FT_Library library = nullptr;
    require(FT_Init_FreeType(&library) == 0, "FreeType initialization failed");

    FT_Int free_type_major = 0;
    FT_Int free_type_minor = 0;
    FT_Int free_type_patch = 0;
    FT_Library_Version(
        library, &free_type_major, &free_type_minor, &free_type_patch);
    require(
        free_type_major == 2 && free_type_minor == 14 && free_type_patch == 3,
        "FreeType runtime version does not match the dependency lock");

    unsigned int harfbuzz_major = 0;
    unsigned int harfbuzz_minor = 0;
    unsigned int harfbuzz_patch = 0;
    hb_version(&harfbuzz_major, &harfbuzz_minor, &harfbuzz_patch);
    require(
        harfbuzz_major == 14 && harfbuzz_minor == 3 && harfbuzz_patch == 1,
        "HarfBuzz runtime version does not match the dependency lock");

    FT_Face latin_face = nullptr;
    require(
        FT_New_Face(library, RYNUI_VALIDATION_LATIN_FONT, 0, &latin_face) == 0,
        "Unable to load the locked Latin validation font");
    require(
        FT_Select_Charmap(latin_face, FT_ENCODING_UNICODE) == 0,
        "Latin validation font has no Unicode charmap");

    FT_Face cjk_face = nullptr;
    require(
        FT_New_Face(library, RYNUI_VALIDATION_CJK_FONT, 0, &cjk_face) == 0,
        "Unable to load the locked CJK validation font");
    require(
        FT_Select_Charmap(cjk_face, FT_ENCODING_UNICODE) == 0,
        "CJK validation font has no Unicode charmap");

    constexpr FT_ULong latin_a = 0x0041;
    constexpr FT_ULong cjk_middle = 0x4E2D;
    require(
        FT_Get_Char_Index(latin_face, latin_a) != 0,
        "Latin validation font must cover U+0041");
    require(
        FT_Get_Char_Index(latin_face, cjk_middle) == 0,
        "Latin validation font unexpectedly covers U+4E2D");
    require(
        FT_Get_Char_Index(cjk_face, cjk_middle) != 0,
        "CJK fallback validation font must cover U+4E2D");

    hb_font_t* harfbuzz_font = hb_ft_font_create_referenced(cjk_face);
    require(harfbuzz_font != nullptr, "HarfBuzz FreeType bridge creation failed");
    hb_font_destroy(harfbuzz_font);

    FT_Done_Face(cjk_face);
    FT_Done_Face(latin_face);
    FT_Done_FreeType(library);
    return EXIT_SUCCESS;
}
