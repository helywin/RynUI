include_guard(GLOBAL)

set(RYNUI_SDL3_VERSION "3.4.14")
set(RYNUI_SDL3_COMMIT "147a8ee32dbf9ac02f3794964490687b6bbda1bc")
set(RYNUI_SDL3_SOURCE_URL
    "https://github.com/libsdl-org/SDL/releases/download/release-3.4.14/SDL3-3.4.14.tar.gz")
set(RYNUI_SDL3_SOURCE_SHA256
    "30d4aa2b3037718142b32dffd4e72f917ebb6cc5227150e7bb9c45efb2153aeb")
set(RYNUI_SDL3_LICENSE "Zlib")

# SDL_shadercross had no release or tag when this snapshot was locked, so the
# archive is tied to an exact upstream commit rather than a moving branch.
set(RYNUI_SDL_SHADERCROSS_COMMIT "e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba")
set(RYNUI_SDL_SHADERCROSS_SOURCE_URL
    "https://github.com/libsdl-org/SDL_shadercross/archive/e55cf5e31ced6f3d1be5cc6d0c50e99384f9f4ba.tar.gz")
set(RYNUI_SDL_SHADERCROSS_SOURCE_SHA256
    "342bb6a8e734745eb5951f25c87fa7aad62f46b3736def8681d9fa7ad046887f")
set(RYNUI_SDL_SHADERCROSS_LICENSE "Zlib")
