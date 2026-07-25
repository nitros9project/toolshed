cmake_minimum_required(VERSION 3.20)

project(DiskShed VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

get_filename_component(TOOLSHED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)

set(TOOLSHED_CORE_SOURCES
    ${TOOLSHED_ROOT}/libcoco/libcocodelete.c
    ${TOOLSHED_ROOT}/libcoco/libcocogs.c
    ${TOOLSHED_ROOT}/libcoco/libcocomakdir.c
    ${TOOLSHED_ROOT}/libcoco/libcocoopen.c
    ${TOOLSHED_ROOT}/libcoco/libcocoread.c
    ${TOOLSHED_ROOT}/libcoco/libcocoreadln.c
    ${TOOLSHED_ROOT}/libcoco/libcocorename.c
    ${TOOLSHED_ROOT}/libcoco/libcocoseek.c
    ${TOOLSHED_ROOT}/libcoco/libcocoss.c
    ${TOOLSHED_ROOT}/libcoco/libcocowrite.c
    ${TOOLSHED_ROOT}/libnative/libnativedelete.c
    ${TOOLSHED_ROOT}/libnative/libnativegs.c
    ${TOOLSHED_ROOT}/libnative/libnativemakdir.c
    ${TOOLSHED_ROOT}/libnative/libnativeopen.c
    ${TOOLSHED_ROOT}/libnative/libnativeread.c
    ${TOOLSHED_ROOT}/libnative/libnativereadln.c
    ${TOOLSHED_ROOT}/libnative/libnativerename.c
    ${TOOLSHED_ROOT}/libnative/libnativeseek.c
    ${TOOLSHED_ROOT}/libnative/libnativess.c
    ${TOOLSHED_ROOT}/libnative/libnativewrite.c
    ${TOOLSHED_ROOT}/libcecb/libcecbbulkerase.c
    ${TOOLSHED_ROOT}/libcecb/libcecbcas.c
    ${TOOLSHED_ROOT}/libcecb/libcecbgs.c
    ${TOOLSHED_ROOT}/libcecb/libcecbopen.c
    ${TOOLSHED_ROOT}/libcecb/libcecbread.c
    ${TOOLSHED_ROOT}/libcecb/libcecbseek.c
    ${TOOLSHED_ROOT}/libcecb/libcecbtokenize.c
    ${TOOLSHED_ROOT}/libcecb/libcecbwav.c
    ${TOOLSHED_ROOT}/libcecb/libcecbwrite.c
    ${TOOLSHED_ROOT}/librbf/librbfbitmap.c
    ${TOOLSHED_ROOT}/librbf/librbfdelete.c
    ${TOOLSHED_ROOT}/librbf/librbfformat.c
    ${TOOLSHED_ROOT}/librbf/librbfgs.c
    ${TOOLSHED_ROOT}/librbf/librbfmakdir.c
    ${TOOLSHED_ROOT}/librbf/librbfopen.c
    ${TOOLSHED_ROOT}/librbf/librbfread.c
    ${TOOLSHED_ROOT}/librbf/librbfreadln.c
    ${TOOLSHED_ROOT}/librbf/librbfrename.c
    ${TOOLSHED_ROOT}/librbf/librbfseek.c
    ${TOOLSHED_ROOT}/librbf/librbfss.c
    ${TOOLSHED_ROOT}/librbf/librbfwrite.c
    ${TOOLSHED_ROOT}/libdecb/libdecbbinconcat.c
    ${TOOLSHED_ROOT}/libdecb/libdecbdskini.c
    ${TOOLSHED_ROOT}/libdecb/libdecbgs.c
    ${TOOLSHED_ROOT}/libdecb/libdecbkill.c
    ${TOOLSHED_ROOT}/libdecb/libdecbopen.c
    ${TOOLSHED_ROOT}/libdecb/libdecbread.c
    ${TOOLSHED_ROOT}/libdecb/libdecbrename.c
    ${TOOLSHED_ROOT}/libdecb/libdecbsrec.c
    ${TOOLSHED_ROOT}/libdecb/libdecbseek.c
    ${TOOLSHED_ROOT}/libdecb/libdecbss.c
    ${TOOLSHED_ROOT}/libdecb/libdecbtokenize.c
    ${TOOLSHED_ROOT}/libdecb/libdecbwrite.c
    ${TOOLSHED_ROOT}/libmisc/libmisccococonv.c
    ${TOOLSHED_ROOT}/libmisc/libmiscendian.c
    ${TOOLSHED_ROOT}/libmisc/libmiscqueue.c
    ${TOOLSHED_ROOT}/libmisc/libmiscutil.c
    ${TOOLSHED_ROOT}/libsys/crc.c
    ${TOOLSHED_ROOT}/libsys/prsnam.c
)

add_library(toolshed_filesystems STATIC ${TOOLSHED_CORE_SOURCES})
target_include_directories(toolshed_filesystems PUBLIC ${TOOLSHED_ROOT}/include)
target_compile_definitions(toolshed_filesystems PRIVATE
    $<$<PLATFORM_ID:Windows>:WIN32>
    $<$<NOT:$<PLATFORM_ID:Windows>>:unix;UNIX;_FILE_OFFSET_BITS=64>
)
if(NOT WIN32)
    target_link_libraries(toolshed_filesystems PUBLIC m)
endif()

add_library(toolshed_image STATIC src/disk_image.cpp)
target_include_directories(toolshed_image PUBLIC src)
target_link_libraries(toolshed_image PUBLIC toolshed_filesystems)

add_executable(toolshed-image-smoke src/smoke.cpp)
target_link_libraries(toolshed-image-smoke PRIVATE toolshed_image)

add_executable(toolshed-image-edit-smoke src/edit_smoke.cpp)
target_link_libraries(toolshed-image-edit-smoke PRIVATE toolshed_image)

option(DISKSHED_BUILD_GUI "Build the DiskShed desktop application" ON)
option(DISKSHED_REQUIRE_GUI "Fail configuration when wxWidgets is unavailable" OFF)
if(DISKSHED_BUILD_GUI)
    # vcpkg and modern wxWidgets installations export wx:: targets. Distribution
    # packages and Homebrew are also supported through CMake's FindwxWidgets
    # module, which exposes the traditional variables.
    find_package(wxWidgets CONFIG QUIET)
    if(TARGET wx::core AND TARGET wx::base)
        set(DISKSHED_WX_LIBRARIES wx::core wx::base)
    else()
        find_package(wxWidgets QUIET COMPONENTS core base)
        if(wxWidgets_FOUND)
            include(${wxWidgets_USE_FILE})
            set(DISKSHED_WX_LIBRARIES ${wxWidgets_LIBRARIES})
        endif()
    endif()

    if(DISKSHED_WX_LIBRARIES)
        add_executable(diskshed MACOSX_BUNDLE WIN32 src/main.cpp)
        target_link_libraries(diskshed PRIVATE toolshed_image ${DISKSHED_WX_LIBRARIES})
        if(APPLE)
            set(DISKSHED_MAC_ICON
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/org.nitros9.diskshed.icns")
            set_source_files_properties(${DISKSHED_MAC_ICON} PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources")
            target_sources(diskshed PRIVATE ${DISKSHED_MAC_ICON})
        elseif(WIN32)
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
            set(DISKSHED_WINDOWS_ICON
                "${CMAKE_CURRENT_SOURCE_DIR}/packaging/org.nitros9.diskshed.ico")
            configure_file(packaging/diskshed.rc.in diskshed.rc @ONLY)
            target_sources(diskshed PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/diskshed.rc")
        endif()
        if(MINGW)
            target_link_options(diskshed PRIVATE -static-libgcc -static-libstdc++)
        endif()
        set_target_properties(diskshed PROPERTIES
            OUTPUT_NAME "DiskShed"
            MACOSX_BUNDLE_BUNDLE_NAME "DiskShed"
            MACOSX_BUNDLE_GUI_IDENTIFIER "org.nitros9.diskshed"
            MACOSX_BUNDLE_ICON_FILE "org.nitros9.diskshed.icns"
            MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
            MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        )

        if(APPLE)
            install(TARGETS diskshed BUNDLE DESTINATION .)
        elseif(WIN32)
            install(TARGETS diskshed RUNTIME DESTINATION .)
        else()
            include(GNUInstallDirs)
            install(TARGETS diskshed RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
            install(FILES packaging/org.nitros9.diskshed.desktop
                DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
            install(FILES packaging/org.nitros9.diskshed.svg
                DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
        endif()
    else()
        if(DISKSHED_REQUIRE_GUI)
            message(FATAL_ERROR "wxWidgets was not found but DISKSHED_REQUIRE_GUI is enabled")
        else()
            message(WARNING "wxWidgets was not found; building the adapter smoke test only")
        endif()
    endif()
endif()
