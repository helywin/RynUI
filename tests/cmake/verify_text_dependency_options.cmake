cmake_minimum_required(VERSION 3.25)

if(NOT DEFINED TEST_CACHE_FILE OR NOT EXISTS "${TEST_CACHE_FILE}")
    message(FATAL_ERROR "TEST_CACHE_FILE must name the configured CMake cache.")
endif()

file(STRINGS "${TEST_CACHE_FILE}" cache_lines)
foreach(expected_entry IN ITEMS
        "BUILD_SHARED_LIBS:BOOL=OFF"
        "SKIP_INSTALL_ALL:BOOL=ON"
        "FT_DISABLE_ZLIB:BOOL=ON"
        "FT_DISABLE_BZIP2:BOOL=ON"
        "FT_DISABLE_PNG:BOOL=ON"
        "FT_DISABLE_HARFBUZZ:BOOL=ON"
        "FT_DISABLE_BROTLI:BOOL=ON"
        "HB_HAVE_FREETYPE:BOOL=ON"
        "HB_HAVE_CAIRO:BOOL=OFF"
        "HB_HAVE_GLIB:BOOL=OFF"
        "HB_HAVE_ICU:BOOL=OFF"
        "HB_BUILD_UTILS:BOOL=OFF"
        "HB_BUILD_SUBSET:BOOL=OFF"
        "HB_BUILD_RASTER:BOOL=OFF"
        "HB_BUILD_VECTOR:BOOL=OFF"
        "HB_BUILD_GPU:BOOL=OFF")
    if(NOT expected_entry IN_LIST cache_lines)
        message(FATAL_ERROR
            "Bundled text dependency option is missing: ${expected_entry}")
    endif()
endforeach()
