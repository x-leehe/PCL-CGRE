cmake_minimum_required(VERSION 3.16)

# ── pcl-cgre GUI — GTK4 + libadwaita 前端 ──────────────────────────────────

# Collect GUI sources
file(GLOB_RECURSE GUI_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

# Find dependencies
find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK4 REQUIRED gtk4>=4.0)
pkg_check_modules(LIBADWAITA REQUIRED libadwaita-1>=1.0)
pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
pkg_check_modules(LIBCURL REQUIRED libcurl)

# ── Blueprint UI compilation ──────────────────────────────────────────────
find_program(BLUEPRINT_COMPILER blueprint-compiler REQUIRED)

# On Windows/MSYS2, blueprint-compiler is a Python script without .exe,
# which can't be invoked directly by cmd.exe/nmake.  Prepend python3.
if(WIN32 AND NOT BLUEPRINT_COMPILER MATCHES "\\.exe$")
    find_program(PYTHON3 python3 REQUIRED)
    set(BLUEPRINT_COMPILER ${PYTHON3} ${BLUEPRINT_COMPILER})
endif()

file(GLOB BLUEPRINT_FILES ${CMAKE_SOURCE_DIR}/data/ui/*.blp)
set(UI_DEST_DIR ${CMAKE_BINARY_DIR}/data/ui)
file(MAKE_DIRECTORY ${UI_DEST_DIR})

foreach(blp ${BLUEPRINT_FILES})
    get_filename_component(basename ${blp} NAME_WE)
    set(ui_file ${UI_DEST_DIR}/${basename}.ui)
    add_custom_command(
        OUTPUT ${ui_file}
        COMMAND ${BLUEPRINT_COMPILER} compile ${blp} --output ${ui_file}
        DEPENDS ${blp}
        COMMENT "Blueprint → UI: ${basename}.blp"
    )
    list(APPEND UI_TARGETS ${ui_file})
endforeach()
add_custom_target(blueprint_ui ALL DEPENDS ${UI_TARGETS})

# ── GResource (embed resources into binary) ───────────────────────────────
find_program(GLIB_COMPILE_RESOURCES glib-compile-resources REQUIRED)

set(GRESOURCE_XML  ${CMAKE_BINARY_DIR}/pcl-cgre.gresource.xml)
set(GRESOURCE_FILE ${CMAKE_BINARY_DIR}/pcl-cgre.gresource)

add_custom_command(
    OUTPUT ${GRESOURCE_XML}
    COMMAND ${CMAKE_COMMAND} -E env python3
        ${CMAKE_SOURCE_DIR}/scripts/gen_gresource_xml.py
        ${CMAKE_SOURCE_DIR}
        ${UI_DEST_DIR}
        > ${GRESOURCE_XML}
    DEPENDS
        ${CMAKE_SOURCE_DIR}/scripts/gen_gresource_xml.py
        ${UI_TARGETS}
    COMMENT "Generating GResource XML"
)

add_custom_command(
    OUTPUT ${GRESOURCE_FILE}
    COMMAND ${GLIB_COMPILE_RESOURCES}
        --target=${GRESOURCE_FILE}
        --sourcedir=${CMAKE_SOURCE_DIR}
        --sourcedir=${CMAKE_BINARY_DIR}
        ${GRESOURCE_XML}
    DEPENDS ${GRESOURCE_XML}
    COMMENT "Compiling GResource binary (.gresource)"
)
add_custom_target(gresource ALL DEPENDS ${GRESOURCE_FILE})

# ── Executable ────────────────────────────────────────────────────────────
add_executable(pcl-cgre ${GUI_SOURCES})
add_dependencies(pcl-cgre blueprint_ui gresource)

# ── Icon resources ────────────────────────────────────────────────────────
set(ICON_SRC_DIR  ${CMAKE_SOURCE_DIR}/resources/icons/pcl-cgre)
set(ICON_DEST_DIR ${CMAKE_BINARY_DIR}/resources/icons/pcl-cgre)
file(COPY ${ICON_SRC_DIR}/ DESTINATION ${ICON_DEST_DIR})

# ── Block images ──────────────────────────────────────────────────────────
set(BLOCK_SRC_DIR  ${CMAKE_SOURCE_DIR}/resources/blocks)
set(BLOCK_DEST_DIR ${CMAKE_BINARY_DIR}/resources/blocks)
file(COPY ${BLOCK_SRC_DIR}/ DESTINATION ${BLOCK_DEST_DIR})

# ── Font resources ────────────────────────────────────────────────────────
set(FONT_SRC_DIR  ${CMAKE_SOURCE_DIR}/resources/fonts)
set(FONT_DEST_DIR ${CMAKE_BINARY_DIR}/resources/fonts)
file(COPY ${FONT_SRC_DIR}/ DESTINATION ${FONT_DEST_DIR})

# ── April Fool version list ──────────────────────────────────────────────
file(COPY ${CMAKE_SOURCE_DIR}/resources/lirpa_loof.json
     DESTINATION ${CMAKE_BINARY_DIR}/resources/)

# Compile-time fallback for the icon search path (system install)
target_compile_definitions(pcl-cgre PRIVATE
    PCL_ICON_THEME_DIR="${CMAKE_INSTALL_PREFIX}/share/icons"
)

target_include_directories(pcl-cgre PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libpclcore/include
    ${CMAKE_SOURCE_DIR}/src/cli
    ${GTK4_INCLUDE_DIRS}
    ${LIBADWAITA_INCLUDE_DIRS}
    ${FONTCONFIG_INCLUDE_DIRS}
    ${LIBCURL_INCLUDE_DIRS}
)

if(MSVC)
    target_compile_options(pcl-cgre PRIVATE /W4 ${GTK4_CFLAGS_OTHER} ${LIBADWAITA_CFLAGS_OTHER})
else()
    target_compile_options(pcl-cgre PRIVATE -Wall -Wextra -Wpedantic ${GTK4_CFLAGS_OTHER} ${LIBADWAITA_CFLAGS_OTHER})
endif()

target_link_libraries(pcl-cgre
    pclcore_objs
    cli_objs
    ${GTK4_LIBRARIES}
    ${LIBADWAITA_LIBRARIES}
    ${FONTCONFIG_LIBRARIES}
    ${LIBCURL_LIBRARIES}
)

target_link_options(pcl-cgre PRIVATE
    ${GTK4_LDFLAGS_OTHER}
    ${LIBADWAITA_LDFLAGS_OTHER}
)

# ── Output naming per build type ───────────────────────────────────────────
set_target_properties(pcl-cgre PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}
    OUTPUT_NAME_RELEASE "pcl-cgre_release"
    OUTPUT_NAME_DEBUG   "pcl-cgre_debug"
)
