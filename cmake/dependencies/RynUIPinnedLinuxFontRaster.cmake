include_guard(GLOBAL)

option(RYNUI_ENABLE_PINNED_LINUX_FONT_RASTER "Enable the locked Linux grayscale rasterizer for pinned Gallery runs" OFF)
set(RYNUI_SKIA_SOURCE_ARCHIVE "" CACHE FILEPATH "Offline archive matching the pinned Skia source SHA256")
set(RYNUI_GN_LINUX_ARCHIVE "" CACHE FILEPATH "Offline archive matching the pinned Linux GN SHA256")

function(rynui_resolve_pinned_linux_font_raster)
    if(NOT RYNUI_ENABLE_PINNED_LINUX_FONT_RASTER)
        return()
    endif()
    if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT RYNUI_HAS_SDL_GPU
            OR NOT CMAKE_HOST_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64)$"
            OR CMAKE_CROSSCOMPILING OR NOT RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
        message(FATAL_ERROR "The locked font rasterizer requires native Linux x64 with SDL GPU and BUNDLED dependencies.")
    endif()

    include(FetchContent)
    include(ExternalProject)
    find_program(rynui_cargo NAMES cargo REQUIRED)
    find_program(rynui_ninja NAMES ninja REQUIRED)
    set(skia_url "${RYNUI_SKIA_RASTER_SOURCE_URL}")
    set(gn_url "${RYNUI_SKIA_RASTER_GN_URL}")
    if(RYNUI_SKIA_SOURCE_ARCHIVE)
        set(skia_url "${RYNUI_SKIA_SOURCE_ARCHIVE}")
    endif()
    if(RYNUI_GN_LINUX_ARCHIVE)
        set(gn_url "${RYNUI_GN_LINUX_ARCHIVE}")
    endif()
    FetchContent_Declare(rynui_skia_raster
        URL "${skia_url}" URL_HASH "SHA256=${RYNUI_SKIA_RASTER_SOURCE_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR "__rynui_no_cmake_project__")
    FetchContent_Declare(rynui_skia_raster_gn
        URL "${gn_url}" URL_HASH "SHA256=${RYNUI_SKIA_RASTER_GN_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR "__rynui_no_cmake_project__")
    FetchContent_MakeAvailable(rynui_skia_raster rynui_skia_raster_gn)

    set(skia_out "${rynui_skia_raster_SOURCE_DIR}/out/rynui-raster")
    set(skia_library "${skia_out}/libskia.a")
    set(gn "${rynui_skia_raster_gn_SOURCE_DIR}/gn")
    file(CHMOD "${gn}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
    string(JOIN "\n" skia_args
        "is_official_build=true" "is_debug=false"
        "cc=\"${CMAKE_C_COMPILER}\"" "cxx=\"${CMAKE_CXX_COMPILER}\""
        "extra_cflags=[\"-DSK_DISABLE_LEGACY_PNG_WRITEBUFFER\"]"
        "skia_enable_ganesh=false" "skia_enable_graphite=false" "skia_enable_gpu_debug_layers=false"
        "skia_enable_pdf=false" "skia_enable_svg=false" "skia_enable_skottie=false" "skia_enable_tools=false"
        "skia_use_x11=false" "skia_use_gl=false" "skia_use_perfetto=false"
        "skia_use_fontconfig=false" "skia_use_freetype=false" "skia_use_fontations=false"
        "skia_use_harfbuzz=false" "skia_use_icu=false" "skia_use_expat=false"
        "skia_use_libjpeg_turbo_decode=false" "skia_use_libjpeg_turbo_encode=false"
        "skia_use_libpng_decode=false" "skia_use_libpng_encode=false"
        "skia_use_libwebp_decode=false" "skia_use_libwebp_encode=false"
        "skia_use_wuffs=false" "skia_use_zlib=false" "skia_use_piex=false")
    file(MAKE_DIRECTORY "${skia_out}")
    file(CONFIGURE OUTPUT "${skia_out}/args.gn" CONTENT "${skia_args}\n" @ONLY)
    ExternalProject_Add(rynui_skia_raster_build
        SOURCE_DIR "${rynui_skia_raster_SOURCE_DIR}"
        BINARY_DIR "${skia_out}"
        DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
        CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E chdir "${rynui_skia_raster_SOURCE_DIR}"
            "${gn}" gen "out/rynui-raster"
        BUILD_COMMAND "${rynui_ninja}" -j 2 -C "${skia_out}" skia
        BUILD_BYPRODUCTS "${skia_library}"
        INSTALL_COMMAND "")
    add_library(rynui_skia_raster_lib STATIC IMPORTED GLOBAL)
    set_target_properties(rynui_skia_raster_lib PROPERTIES IMPORTED_LOCATION "${skia_library}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${rynui_skia_raster_SOURCE_DIR}"
        INTERFACE_INCLUDE_DIRECTORIES "${rynui_skia_raster_SOURCE_DIR}")
    add_dependencies(rynui_skia_raster_lib rynui_skia_raster_build)

    set(fontations_sources "${PROJECT_SOURCE_DIR}/src/platform/fontations")
    set(fontations_build "${CMAKE_CURRENT_BINARY_DIR}/_deps/rynui-fontations-build")
    set(fontations_lib "${fontations_build}/release/librynui_fontations.a")
    set(cargo_mode --locked)
    if(FETCHCONTENT_FULLY_DISCONNECTED OR RYNUI_SKIA_SOURCE_ARCHIVE AND RYNUI_GN_LINUX_ARCHIVE)
        list(APPEND cargo_mode --offline)
    endif()
    add_custom_command(OUTPUT "${fontations_lib}"
        COMMAND "${CMAKE_COMMAND}" -E env "CARGO_TARGET_DIR=${fontations_build}"
            "${rynui_cargo}" build --release ${cargo_mode} --manifest-path "${fontations_sources}/Cargo.toml"
        DEPENDS "${fontations_sources}/Cargo.toml" "${fontations_sources}/Cargo.lock" "${fontations_sources}/src/lib.rs"
        VERBATIM)
    add_custom_target(rynui_fontations_raster_build DEPENDS "${fontations_lib}")
    add_library(rynui_fontations_raster_lib STATIC IMPORTED GLOBAL)
    set_target_properties(rynui_fontations_raster_lib PROPERTIES IMPORTED_LOCATION "${fontations_lib}")
    add_dependencies(rynui_fontations_raster_lib rynui_fontations_raster_build)

    find_package(Threads REQUIRED)
    add_library(rynui_platform_font_raster INTERFACE)
    target_link_libraries(rynui_platform_font_raster INTERFACE
        rynui_skia_raster_lib rynui_fontations_raster_lib Threads::Threads ${CMAKE_DL_LIBS})
endfunction()
