include_guard(GLOBAL)

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

set(RYNUI_NOTO_SANS_CJK_SC_VERSION "2.004")
set(RYNUI_NOTO_SANS_CJK_SC_COMMIT "523d033d6cb47f4a80c58a35753646f5c3608a78")
set(RYNUI_NOTO_SANS_CJK_SC_SOURCE_URL
    "https://raw.githubusercontent.com/notofonts/noto-cjk/523d033d6cb47f4a80c58a35753646f5c3608a78/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Regular.otf")
set(RYNUI_NOTO_SANS_CJK_SC_SOURCE_SHA256
    "2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b")
set(RYNUI_NOTO_SANS_CJK_SC_LICENSE "OFL-1.1")
