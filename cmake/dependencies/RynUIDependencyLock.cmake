include_guard(GLOBAL)

set(RYNUI_SHEENBIDI_VERSION "3.0.0")
set(RYNUI_SHEENBIDI_COMMIT "cfe430e7375a7845b679adae9d51dac6deaa8858")
set(RYNUI_SHEENBIDI_SOURCE_URL
    "https://codeload.github.com/Tehreer/SheenBidi/tar.gz/refs/tags/v3.0.0")
set(RYNUI_SHEENBIDI_SOURCE_SHA256
    "86c56014034739ba39a24c23eb00323b0bf6f737354f665786015fca842af786")
set(RYNUI_SHEENBIDI_LICENSE "Apache-2.0")
set(RYNUI_BIDI_TEST_LICENSE "Unicode-3.0")
set(RYNUI_BIDI_TEST_SOURCE_URL "https://www.unicode.org/Public/17.0.0/ucd/BidiTest.txt")
set(RYNUI_BIDI_TEST_SOURCE_SHA256 "888bdfc8090652272d1f859cdb00ae659e2dc6c26740be61ef1d03998a687620")
set(RYNUI_BIDI_CHARACTER_TEST_SOURCE_URL "https://www.unicode.org/Public/17.0.0/ucd/BidiCharacterTest.txt")
set(RYNUI_BIDI_CHARACTER_TEST_SOURCE_SHA256 "a3e6e905ab5afbe318a96df5401d0372a04cd73ef139ab5e3cf0ae241c255488")
set(RYNUI_NOTO_SANS_ARABIC_VERSION "2.009")
set(RYNUI_NOTO_SANS_HEBREW_VERSION "3.000")
foreach(script IN ITEMS ARABIC HEBREW)
    set(RYNUI_NOTO_SANS_${script}_COMMIT "ffebf8c1ee449e544955a7e813c54f9b73848eac")
    set(RYNUI_NOTO_SANS_${script}_LICENSE "OFL-1.1")
endforeach()
set(RYNUI_NOTO_SANS_ARABIC_SOURCE_URL "https://raw.githubusercontent.com/notofonts/noto-fonts/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSansArabic/NotoSansArabic-Regular.ttf")
set(RYNUI_NOTO_SANS_ARABIC_SOURCE_SHA256 "ceea25b464a656dc3b26849bab9356740401af62aedf1bfa8b7f0d9b75925b1b")
set(RYNUI_NOTO_SANS_HEBREW_SOURCE_URL "https://raw.githubusercontent.com/notofonts/noto-fonts/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSansHebrew/NotoSansHebrew-Regular.ttf")
set(RYNUI_NOTO_SANS_HEBREW_SOURCE_SHA256 "a7fa16fffb27bedb060a0866267c29e9859aeb9c21cc33f5b3aaf6eb062eca85")

# Checked-in icon artwork. No runtime download or system-font fallback.
set(RYNUI_ANT_ICONS_VERSION "4.6.0")
set(RYNUI_ANT_ICONS_COMMIT "7f2516ac91226d2b41f93b35cb5197c8d94f7189")
set(RYNUI_ANT_ICONS_SOURCE_URL
    "https://registry.npmjs.org/@ant-design/icons-svg/-/icons-svg-4.6.0.tgz")
set(RYNUI_ANT_ICONS_SOURCE_SHA256
    "7e07fdcf459f1f2ae6721ca1796811d9fb7737693af0724b55840f4edccde80b")
set(RYNUI_ANT_ICONS_LICENSE "MIT")
# Optional maintenance tool, not a configure/build/runtime dependency.
set(RYNUI_ICON_FONTTOOLS_VERSION "4.60.1")
set(RYNUI_ICON_FONTTOOLS_SOURCE_URL
    "https://files.pythonhosted.org/packages/4b/42/97a13e47a1e51a5a7142475bbcf5107fe3a68fc34aef331c897d5fb98ad0/fonttools-4.60.1.tar.gz")
set(RYNUI_ICON_FONTTOOLS_SOURCE_SHA256
    "ef00af0439ebfee806b25f24c8f92109157ff3fac5731dc7867957812e87b8d9")
set(RYNUI_ICON_FONTTOOLS_LICENSE "MIT")

set(RYNUI_UTF8PROC_VERSION "2.11.3")
set(RYNUI_UTF8PROC_SOURCE_URL
    "https://github.com/JuliaStrings/utf8proc/releases/download/v2.11.3/utf8proc-2.11.3.tar.gz")
set(RYNUI_UTF8PROC_SOURCE_SHA256
    "415189fd2c85cd6ee5ff26af500fa387de9ada1e3e316e93f7338551481d557d")
set(RYNUI_UTF8PROC_LICENSE "MIT AND Unicode-DFS-2015")
set(RYNUI_GRAPHEME_TEST_LICENSE "Unicode-3.0")
set(RYNUI_UNICODE_VERSION "17.0.0")
set(RYNUI_GRAPHEME_TEST_SOURCE_URL
    "https://www.unicode.org/Public/17.0.0/ucd/auxiliary/GraphemeBreakTest.txt")
set(RYNUI_GRAPHEME_TEST_SOURCE_SHA256
    "e2d134d2c52919bace503ebb6a551c1855fe1a1faec18478c78fff254a1793ec")

set(RYNUI_SDL3_VERSION "3.4.14")
set(RYNUI_SDL3_COMMIT "147a8ee32dbf9ac02f3794964490687b6bbda1bc")
set(RYNUI_SDL3_SOURCE_URL
    "https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz")
set(RYNUI_SDL3_SOURCE_SHA256
    "30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb")
set(RYNUI_SDL3_LICENSE "Zlib")
set(RYNUI_SDL3_LIBDECOR_PATCH_SHA256
    "c3971c84d9056b53f0ccf388a0a22381963ebc9ab838a7fadfd85afeb559c04f")
set(RYNUI_SDL3_LIBDECOR_PACING_PATCH_SHA256
    "c76789442c18166ab3588a1d109caa6a9341766ca3144f77e3dca6c87f824741")
set(RYNUI_SDL3_SDLCHECKS_SHA256
    "369cd0763a7d4135727a909579064cf3975ec6cf2f02efbd9166993a7f8d76c1")
set(RYNUI_SDL3_WAYLAND_WINDOW_SOURCE_SHA256
    "32919f7c4d7461f98277b2769782d45707698d81d1ce7b0c2b82d5dd635f36cc")
set(RYNUI_SDL3_WAYLAND_WINDOW_HEADER_SHA256
    "b87f2db76ff058bc7b4970a138d48122c51e5d21ce14a3790701482734474fa1")

set(RYNUI_LIBDECOR_VERSION "0.2.5")
set(RYNUI_LIBDECOR_SOURCE_URL
    "https://gitlab.freedesktop.org/libdecor/libdecor/-/archive/0.2.5/libdecor-0.2.5.tar.gz")
set(RYNUI_LIBDECOR_SOURCE_SHA256
    "39c109a9a7eae943ba34d18a282c447d5729f9c486c8bc05ea305e4acd341522")
set(RYNUI_LIBDECOR_LICENSE "MIT")
set(RYNUI_LIBDECOR_RESIZING_COMMIT
    "8dc6b627ae1d5d4e286d01a6bed4c7b0e7af847d")
set(RYNUI_LIBDECOR_RESIZING_PATCH_SHA256
    "f4f1702b24ad3469ae934bc5e2233c21275d4882cf7374f6dd003f26001601f9")
set(RYNUI_LIBDECOR_CONFIGURATION_PATCH_SHA256
    "dffd9c9a4ec5542c9e3a99570613b56459094b18d11322ff31b3b2f388729bce")

# SDL_shadercross had no release or tag when this snapshot was locked, so the
# archive is tied to an exact upstream commit rather than a moving branch.
set(RYNUI_SDL_SHADERCROSS_COMMIT "e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba")
set(RYNUI_SDL_SHADERCROSS_SOURCE_URL
    "https://github.com/libsdl-org/SDL_shadercross/archive/e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba.tar.gz")
set(RYNUI_SDL_SHADERCROSS_SOURCE_SHA256
    "342bb6a8e734745eb5951f25c87fa7aad62f46b3736def8681d9fa7ad046887f")
set(RYNUI_SDL_SHADERCROSS_LICENSE "Zlib")

set(RYNUI_SPIRV_CROSS_COMMIT "1a6169566c73d3da552748fc372fe2bbb856e46e")
set(RYNUI_SPIRV_CROSS_SOURCE_URL
    "https://github.com/KhronosGroup/SPIRV-Cross/archive/1a6169566c73d3da552748fc372fe2bbb856e46e.tar.gz")
set(RYNUI_SPIRV_CROSS_SOURCE_SHA256
    "0f295b214b164e42a1d21537c8da7b44569806c16220dda9798558edfaacd11e")
set(RYNUI_SPIRV_CROSS_LICENSE "Apache-2.0 OR MIT")

set(RYNUI_DXC_VERSION "1.8.2502")
set(RYNUI_DXC_WINDOWS_X64_URL
    "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/dxc_2025_02_20.zip")
set(RYNUI_DXC_WINDOWS_X64_SHA256
    "70b1913a1bfce4a3e1a5311d16246f4ecdf3a3e613abec8aa529e57668426f85")
set(RYNUI_DXC_LINUX_X64_URL
    "https://github.com/microsoft/DirectXShaderCompiler/releases/download/v1.8.2502/linux_dxc_2025_02_20.x86_64.tar.gz")
set(RYNUI_DXC_LINUX_X64_SHA256
    "e0580d90dbf6053a783ddd8d5153285f0606e5deaad17a7a6452f03acdf88c71")
set(RYNUI_DXC_LICENSE "NCSA + MIT + Microsoft Software License Terms")

set(RYNUI_FREETYPE_VERSION "2.14.3")
set(RYNUI_FREETYPE_COMMIT "0a0221a1347e2f1e07c395263540026e9a0aa7c7")
set(RYNUI_FREETYPE_SOURCE_URL
    "https://download.savannah.gnu.org/releases/freetype/freetype-2.14.3.tar.xz")
set(RYNUI_FREETYPE_SOURCE_SHA256
    "36bc4f1cc413335368ee656c42afca65c5a3987e8768cc28cf11ba775e785a5f")
set(RYNUI_FREETYPE_LICENSE "FTL OR GPL-2.0-only")

set(RYNUI_HARFBUZZ_VERSION "14.3.1")
set(RYNUI_HARFBUZZ_COMMIT "ab5ecbb83985034a76214ac0b2b833dcd590d774")
set(RYNUI_HARFBUZZ_SOURCE_URL
    "https://github.com/harfbuzz/harfbuzz/releases/download/14.3.1/harfbuzz-14.3.1.tar.xz")
set(RYNUI_HARFBUZZ_SOURCE_SHA256
    "9dae9538aae2ffdf70cec31f2c27bf68e2aaeeae3112688467697d5faf6194f7")
set(RYNUI_HARFBUZZ_LICENSE "MIT")

# Validation fonts are build-time fixtures. Their binaries are downloaded to
# the build tree and are never committed to this repository.
set(RYNUI_NOTO_SANS_VERSION "2.008")
set(RYNUI_NOTO_SANS_COMMIT "ffebf8c1ee449e544955a7e813c54f9b73848eac")
set(RYNUI_NOTO_SANS_SOURCE_URL
    "https://raw.githubusercontent.com/notofonts/noto-fonts/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSans/NotoSans-Regular.ttf")
set(RYNUI_NOTO_SANS_SOURCE_SHA256
    "b85c38ecea8a7cfb39c24e395a4007474fa5a4fc864f6ee33309eb4948d232d5")
set(RYNUI_NOTO_SANS_LICENSE "OFL-1.1")
set(RYNUI_NOTO_SANS_FACE_INDEX "0")
set(RYNUI_NOTO_SANS_WEIGHT "400")
set(RYNUI_NOTO_SANS_SLANT "upright")
set(RYNUI_NOTO_SANS_POSTSCRIPT_NAME "NotoSans-Regular")
set(RYNUI_NOTO_SANS_LICENSE_URL "https://raw.githubusercontent.com/notofonts/noto-fonts/ffebf8c1ee449e544955a7e813c54f9b73848eac/LICENSE")
set(RYNUI_NOTO_SANS_LICENSE_SHA256 "0dab92d0544f7b233403f14b84a663bdbfa746982eda629e7f4f9ffe1b036feb")

# Differential acceptance only: these are not ordinary product build downloads.
set(RYNUI_NOTO_SANS_SEMIBOLD_VERSION "2.008")
set(RYNUI_NOTO_SANS_SEMIBOLD_COMMIT "ffebf8c1ee449e544955a7e813c54f9b73848eac")
set(RYNUI_NOTO_SANS_SEMIBOLD_SOURCE_URL "https://raw.githubusercontent.com/notofonts/noto-fonts/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSans/NotoSans-SemiBold.ttf")
set(RYNUI_NOTO_SANS_SEMIBOLD_SOURCE_SHA256 "87a8b90ece1e89746b544e4e086f85a3710e41485a8078f9be874837dfad45d5")
set(RYNUI_NOTO_SANS_SEMIBOLD_LICENSE "OFL-1.1")
set(RYNUI_NOTO_SANS_SEMIBOLD_FACE_INDEX "0")
set(RYNUI_NOTO_SANS_SEMIBOLD_WEIGHT "600")
set(RYNUI_NOTO_SANS_SEMIBOLD_SLANT "upright")
set(RYNUI_NOTO_SANS_SEMIBOLD_POSTSCRIPT_NAME "NotoSans-SemiBold")

set(RYNUI_NOTO_SANS_MEDIUM_VERSION "2.008")
set(RYNUI_NOTO_SANS_MEDIUM_COMMIT "ffebf8c1ee449e544955a7e813c54f9b73848eac")
set(RYNUI_NOTO_SANS_MEDIUM_SOURCE_URL "https://raw.githubusercontent.com/notofonts/noto-fonts/ffebf8c1ee449e544955a7e813c54f9b73848eac/hinted/ttf/NotoSans/NotoSans-Medium.ttf")
set(RYNUI_NOTO_SANS_MEDIUM_SOURCE_SHA256 "7bbe267354704c6ad18bde24b1dbc756c8e4380ca1c3f3c25c45ec5c4471510b")
set(RYNUI_NOTO_SANS_MEDIUM_LICENSE "OFL-1.1")
set(RYNUI_NOTO_SANS_MEDIUM_FACE_INDEX "0")
set(RYNUI_NOTO_SANS_MEDIUM_WEIGHT "500")
set(RYNUI_NOTO_SANS_MEDIUM_SLANT "upright")
set(RYNUI_NOTO_SANS_MEDIUM_POSTSCRIPT_NAME "NotoSans-Medium")

set(RYNUI_NOTO_SANS_CJK_SC_VERSION "2.004")
set(RYNUI_NOTO_SANS_CJK_SC_COMMIT "523d033d6cb47f4a80c58a35753646f5c3608a78")
set(RYNUI_NOTO_SANS_CJK_SC_SOURCE_URL
    "https://raw.githubusercontent.com/notofonts/noto-cjk/523d033d6cb47f4a80c58a35753646f5c3608a78/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf")
set(RYNUI_NOTO_SANS_CJK_SC_SOURCE_SHA256
    "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b")
set(RYNUI_NOTO_SANS_CJK_SC_LICENSE "OFL-1.1")
set(RYNUI_NOTO_SANS_CJK_SC_FACE_INDEX "0")
set(RYNUI_NOTO_SANS_CJK_SC_WEIGHT "400")
set(RYNUI_NOTO_SANS_CJK_SC_SLANT "upright")
set(RYNUI_NOTO_SANS_CJK_SC_POSTSCRIPT_NAME "NotoSansCJKsc-Regular")
set(RYNUI_NOTO_SANS_CJK_SC_LICENSE_URL "https://raw.githubusercontent.com/notofonts/noto-cjk/523d033d6cb47f4a80c58a35753646f5c3608a78/LICENSE")
set(RYNUI_NOTO_SANS_CJK_SC_LICENSE_SHA256 "6a73f9541c2de74158c0e7cf6b0a58ef774f5a780bf191f2d7ec9cc53efe2bf2")

# Official SC static faces have no 600. Pin the real wght=600 outline instance.
# Optional test preparation reuses the exact ICON_FONTTOOLS source lock above;
# it never installs a product dependency or changes the default system chain.
set(RYNUI_NOTO_SANS_CJK_SC_VARIABLE_VERSION "2.004")
set(RYNUI_NOTO_SANS_CJK_SC_VARIABLE_COMMIT "523d033d6cb47f4a80c58a35753646f5c3608a78")
set(RYNUI_NOTO_SANS_CJK_SC_VARIABLE_SOURCE_URL "https://raw.githubusercontent.com/notofonts/noto-cjk/523d033d6cb47f4a80c58a35753646f5c3608a78/Sans/Variable/TTF/NotoSansCJKsc-VF.ttf")
set(RYNUI_NOTO_SANS_CJK_SC_VARIABLE_SOURCE_SHA256 "990c807e79c25662a5a9ecf7f971baeb2bf2eab9a559e5ecf15cdfdb8561d21f")
set(RYNUI_NOTO_SANS_CJK_SC_VARIABLE_LICENSE "OFL-1.1")
set(RYNUI_NOTO_SANS_CJK_SC_SEMIBOLD_SHA256 "4137d864b16d2670304ca58d1f884e20a2656d19230b8f497e0ce53584fa3d2a")
set(RYNUI_NOTO_SANS_CJK_SC_SEMIBOLD_FACE_INDEX "0")
set(RYNUI_NOTO_SANS_CJK_SC_SEMIBOLD_WEIGHT "600")
set(RYNUI_NOTO_SANS_CJK_SC_SEMIBOLD_SLANT "upright")
set(RYNUI_NOTO_SANS_CJK_SC_SEMIBOLD_POSTSCRIPT_NAME "NotoSansCJKsc-SemiBold")

# Same locked variable source and deterministic recipe, pinned to real wght=500.
set(RYNUI_NOTO_SANS_CJK_SC_MEDIUM_SHA256 "feef2a772298a640a5d9a68185b12c068b8ce824b6edaa96d0979a290fe5a251")
set(RYNUI_NOTO_SANS_CJK_SC_MEDIUM_FACE_INDEX "0")
set(RYNUI_NOTO_SANS_CJK_SC_MEDIUM_WEIGHT "500")
set(RYNUI_NOTO_SANS_CJK_SC_MEDIUM_SLANT "upright")
set(RYNUI_NOTO_SANS_CJK_SC_MEDIUM_POSTSCRIPT_NAME "NotoSansCJKsc-Medium")

# Monochrome glyf outlines for the unchanged Input abc + U+1F642 fixture.
set(RYNUI_NOTO_EMOJI_VARIABLE_VERSION "3.002")
set(RYNUI_NOTO_EMOJI_VARIABLE_COMMIT "8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5")
set(RYNUI_NOTO_EMOJI_VARIABLE_SOURCE_URL "https://raw.githubusercontent.com/google/fonts/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/notoemoji/NotoEmoji%5Bwght%5D.ttf")
set(RYNUI_NOTO_EMOJI_VARIABLE_SOURCE_SHA256 "de6c18832938afc99caf132b39d6a30a19bac7f2e812e28db2535b4608d27551")
set(RYNUI_NOTO_EMOJI_VARIABLE_LICENSE "OFL-1.1")
set(RYNUI_NOTO_EMOJI_VARIABLE_LICENSE_URL "https://raw.githubusercontent.com/google/fonts/8b0a1d0f5983c89bc2b93f1b5fb55f9e252744b5/ofl/notoemoji/OFL.txt")
set(RYNUI_NOTO_EMOJI_VARIABLE_LICENSE_SHA256 "500bb1ccf43df7bbb522112f9133a52b16e1c35e809632f5d8609b179152de5b")
set(RYNUI_NOTO_EMOJI_REGULAR_SHA256 "1ada997ccb67bf2404828e25528e5257d6db10c80287e77684abf6737a5d198f")
set(RYNUI_NOTO_EMOJI_REGULAR_FACE_INDEX "0")
set(RYNUI_NOTO_EMOJI_REGULAR_WEIGHT "400")
set(RYNUI_NOTO_EMOJI_REGULAR_SLANT "upright")
set(RYNUI_NOTO_EMOJI_REGULAR_POSTSCRIPT_NAME "NotoEmoji-Regular")

set(RYNUI_STB_IMAGE_VERSION "2.30")
set(RYNUI_STB_COMMIT "d14d46f6b44456eb88beac58cba833a4135d4c89")
set(RYNUI_STB_SOURCE_URL "https://codeload.github.com/nothings/stb/tar.gz/${RYNUI_STB_COMMIT}")
set(RYNUI_STB_SOURCE_SHA256 "1b59a668a789a28a8fd4e01ff1413eec7eaeb28688bafdc66ec7068f4e7095e1")
set(RYNUI_STB_IMAGE_HEADER_SHA256 "594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3")
set(RYNUI_STB_LICENSE "MIT OR Unlicense")

# QR Model 2 encoder. Sources are fetched to ignored build output, never vendored.
set(RYNUI_QRCODEGEN_VERSION "1.8.0")
set(RYNUI_QRCODEGEN_SOURCE_URL "https://codeload.github.com/nayuki/QR-Code-generator/tar.gz/refs/tags/v1.8.0")
set(RYNUI_QRCODEGEN_SOURCE_SHA256 "2ec0a4d33d6f521c942eeaf473d42d5fe139abcfa57d2beffe10c5cf7d34ae60")
set(RYNUI_QRCODEGEN_HEADER_SHA256 "b779c3b156cf7a57ce789d6fee4fc991ccc2913774d26c909d22bb8f26b2a793")
set(RYNUI_QRCODEGEN_CPP_SHA256 "1f3b3fcdac6954c32cf583ccd02ec9b5901f756a38c461acedc70be4a77d3757")
set(RYNUI_QRCODEGEN_LICENSE "MIT")
