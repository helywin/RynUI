include_guard(GLOBAL)

option(RYNUI_ENABLE_PINNED_LINUX_FONT_RASTER "Enable the locked Linux grayscale rasterizer for pinned Gallery runs" OFF)
option(RYNUI_ENABLE_PINNED_SVG_ICON_RASTER "Enable the locked SVG icon rasterizer for pinned Gallery runs" OFF)
set(RYNUI_SKIA_SOURCE_ARCHIVE "" CACHE FILEPATH "Offline archive matching the pinned Skia source SHA256")
set(RYNUI_GN_LINUX_ARCHIVE "" CACHE FILEPATH "Offline archive matching the pinned Linux GN SHA256")
set(RYNUI_GN_WINDOWS_ARCHIVE "" CACHE FILEPATH "Offline archive matching the pinned Windows GN SHA256")

function(rynui_configure_windows_skia_raster source gn ninja common_args)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    if(NOT CMAKE_C_COMPILER MATCHES "^(.*/VC)/Tools/MSVC/([^/]+)/bin/[^/]+/x64/cl[.]exe$")
        message(FATAL_ERROR "Pinned Windows SVG rasterization requires the native x64 MSVC preset.")
    endif()
    set(vc "${CMAKE_MATCH_1}")
    set(msvc_version "${CMAKE_MATCH_2}")
    file(TO_CMAKE_PATH "$ENV{WindowsSdkDir}" sdk)
    string(REGEX REPLACE "[/\\\\]+$" "" sdk_version "$ENV{WindowsSDKVersion}")
    if(NOT EXISTS "${sdk}/Include/${sdk_version}/um/Windows.h")
        message(FATAL_ERROR "The MSVC developer environment must select the Windows SDK for pinned SVG rasterization.")
    endif()
    string(APPEND common_args "\ntarget_os=\"win\"\ntarget_cpu=\"x64\"\nwin_vc=\"${vc}\"\n"
        "win_toolchain_version=\"${msvc_version}\"\nwin_sdk=\"${sdk}\"\nwin_sdk_version=\"${sdk_version}\"\n"
        "skia_enable_fontmgr_win=false\nskia_enable_fontmgr_win_gdi=false\n")
    foreach(config IN ITEMS Debug Release)
        if(config STREQUAL "Debug")
            set(crt /MDd)
        else()
            set(crt /MD)
        endif()
        set(output "${source}/out/rynui-raster-${config}")
        set(skia_${config} "${output}/skia.lib")
        set(skcms_${config} "${output}/skcms.lib")
        file(MAKE_DIRECTORY "${output}")
        string(CONCAT flags
            "${common_args}is_debug=false\nis_official_build=true\n"
            "extra_cflags=[\"${crt}\",\"/DSK_DISABLE_LEGACY_PNG_WRITEBUFFER\"]\n")
        file(CONFIGURE OUTPUT "${output}/args.gn" CONTENT "${flags}" @ONLY)
        add_custom_command(OUTPUT "${skia_${config}}" "${skcms_${config}}"
            COMMAND "${gn}" "--script-executable=${Python3_EXECUTABLE}" gen "${output}"
            COMMAND "${ninja}" -j 2 -C "${output}" skia
            DEPENDS "${output}/args.gn" "${gn}"
            WORKING_DIRECTORY "${source}"
            VERBATIM)
    endforeach()
    add_custom_target(rynui_skia_raster_build DEPENDS
        "$<IF:$<CONFIG:Debug>,${skia_Debug},${skia_Release}>"
        "$<IF:$<CONFIG:Debug>,${skcms_Debug},${skcms_Release}>")
    set_target_properties(rynui_skia_raster_lib PROPERTIES
        IMPORTED_CONFIGURATIONS "DEBUG;RELEASE"
        IMPORTED_LOCATION_DEBUG "${skia_Debug}"
        IMPORTED_LOCATION_RELEASE "${skia_Release}"
        MAP_IMPORTED_CONFIG_RELWITHDEBINFO RELEASE
        MAP_IMPORTED_CONFIG_MINSIZEREL RELEASE
        INTERFACE_LINK_LIBRARIES "$<IF:$<CONFIG:Debug>,${skcms_Debug},${skcms_Release}>;ole32;gdi32;user32;dwrite;windowscodecs")
endfunction()

function(rynui_resolve_pinned_linux_font_raster)
    if(NOT RYNUI_ENABLE_PINNED_LINUX_FONT_RASTER AND NOT RYNUI_ENABLE_PINNED_SVG_ICON_RASTER)
        return()
    endif()
    string(TOLOWER "${CMAKE_HOST_SYSTEM_PROCESSOR}" host_arch)
    if(NOT CMAKE_SYSTEM_NAME MATCHES "^(Linux|Windows)$" OR NOT RYNUI_HAS_SDL_GPU
            OR NOT host_arch MATCHES "^(x86_64|amd64)$"
            OR CMAKE_CROSSCOMPILING OR NOT RYNUI_DEPENDENCY_MODE STREQUAL "BUNDLED")
        message(FATAL_ERROR "Pinned rasterizers require a native x64 SDL GPU build and BUNDLED dependencies.")
    endif()
    if(RYNUI_ENABLE_PINNED_LINUX_FONT_RASTER AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(FATAL_ERROR "The Fontations provider is available on Linux only.")
    endif()

    include(FetchContent)
    include(ExternalProject)
    find_program(rynui_ninja NAMES ninja REQUIRED)
    set(skia_url "${RYNUI_SKIA_RASTER_SOURCE_URL}")
    set(gn_url "${RYNUI_SKIA_RASTER_GN_URL}")
    set(gn_sha "${RYNUI_SKIA_RASTER_GN_SHA256}")
    if(RYNUI_SKIA_SOURCE_ARCHIVE)
        set(skia_url "${RYNUI_SKIA_SOURCE_ARCHIVE}")
    endif()
    if(RYNUI_GN_LINUX_ARCHIVE)
        set(gn_url "${RYNUI_GN_LINUX_ARCHIVE}")
    endif()
    if(WIN32)
        set(gn_url "${RYNUI_SKIA_RASTER_WINDOWS_GN_URL}")
        set(gn_sha "${RYNUI_SKIA_RASTER_WINDOWS_GN_SHA256}")
        if(RYNUI_GN_WINDOWS_ARCHIVE)
            set(gn_url "${RYNUI_GN_WINDOWS_ARCHIVE}")
        endif()
    endif()
    FetchContent_Declare(rynui_skia_raster
        URL "${skia_url}" URL_HASH "SHA256=${RYNUI_SKIA_RASTER_SOURCE_SHA256}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR "__rynui_no_cmake_project__")
    FetchContent_Declare(rynui_skia_raster_gn
        URL "${gn_url}" URL_HASH "SHA256=${gn_sha}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE SOURCE_SUBDIR "__rynui_no_cmake_project__")
    FetchContent_MakeAvailable(rynui_skia_raster rynui_skia_raster_gn)

    string(JOIN "\n" skia_args
        "skia_enable_ganesh=false" "skia_enable_graphite=false" "skia_enable_gpu_debug_layers=false"
        "skia_enable_pdf=false" "skia_enable_svg=false" "skia_enable_skottie=false" "skia_enable_tools=false"
        "skia_use_x11=false" "skia_use_gl=false" "skia_use_perfetto=false"
        "skia_use_fontconfig=false" "skia_use_freetype=false" "skia_use_fontations=false"
        "skia_use_harfbuzz=false" "skia_use_icu=false" "skia_use_expat=false"
        "skia_use_libjpeg_turbo_decode=false" "skia_use_libjpeg_turbo_encode=false"
        "skia_use_libpng_decode=false" "skia_use_libpng_encode=false"
        "skia_use_libwebp_decode=false" "skia_use_libwebp_encode=false"
        "skia_use_wuffs=false" "skia_use_zlib=false" "skia_use_piex=false")
    add_library(rynui_skia_raster_lib STATIC IMPORTED GLOBAL)
    set_target_properties(rynui_skia_raster_lib PROPERTIES
        INTERFACE_COMPILE_DEFINITIONS SK_RELEASE
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${rynui_skia_raster_SOURCE_DIR}"
        INTERFACE_INCLUDE_DIRECTORIES "${rynui_skia_raster_SOURCE_DIR}")
    if(WIN32)
        rynui_configure_windows_skia_raster("${rynui_skia_raster_SOURCE_DIR}"
            "${rynui_skia_raster_gn_SOURCE_DIR}/gn.exe" "${rynui_ninja}" "${skia_args}")
    else()
        set(skia_out "${rynui_skia_raster_SOURCE_DIR}/out/rynui-raster")
        set(skia_library "${skia_out}/libskia.a")
        set(gn "${rynui_skia_raster_gn_SOURCE_DIR}/gn")
        file(CHMOD "${gn}" PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE)
        string(APPEND skia_args "\nis_debug=false\nis_official_build=true\n"
            "cc=\"${CMAKE_C_COMPILER}\"\ncxx=\"${CMAKE_CXX_COMPILER}\"\n"
            "extra_cflags=[\"-DSK_DISABLE_LEGACY_PNG_WRITEBUFFER\"]\n")
        file(MAKE_DIRECTORY "${skia_out}")
        file(CONFIGURE OUTPUT "${skia_out}/args.gn" CONTENT "${skia_args}" @ONLY)
        ExternalProject_Add(rynui_skia_raster_build
            SOURCE_DIR "${rynui_skia_raster_SOURCE_DIR}"
            BINARY_DIR "${skia_out}"
            DOWNLOAD_COMMAND "" UPDATE_COMMAND ""
            CONFIGURE_COMMAND "${CMAKE_COMMAND}" -E chdir "${rynui_skia_raster_SOURCE_DIR}"
                "${gn}" gen "out/rynui-raster"
            BUILD_COMMAND "${rynui_ninja}" -j 2 -C "${skia_out}" skia
            BUILD_BYPRODUCTS "${skia_library}"
            INSTALL_COMMAND "")
        set_target_properties(rynui_skia_raster_lib PROPERTIES IMPORTED_LOCATION "${skia_library}")
    endif()
    add_dependencies(rynui_skia_raster_lib rynui_skia_raster_build)

    if(NOT RYNUI_ENABLE_PINNED_LINUX_FONT_RASTER)
        return()
    endif()
    find_program(rynui_cargo NAMES cargo REQUIRED)
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
