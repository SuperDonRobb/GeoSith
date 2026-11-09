cmake_minimum_required(VERSION 3.20)
project(GeoSithGeometry C CXX)
enable_testing()
add_executable(resources_tests resources_test.cpp)
target_compile_features(resources_tests PRIVATE cxx_std_20)
add_test(NAME custom_resources COMMAND resources_tests)
find_package(PNG QUIET)
if(PNG_FOUND)
  add_executable(png_material_tests png_material_test.cpp)
  target_compile_features(png_material_tests PRIVATE cxx_std_20)
  target_link_libraries(png_material_tests PRIVATE PNG::PNG)
  add_test(NAME png_materials COMMAND png_material_tests)
endif()
add_executable(editor_tests geometry_test.c ../src/Main/edGeometry.c ../src/Main/edLevelIO.c)
target_include_directories(editor_tests PRIVATE ../src/Main ../src)
if(MSVC)
  target_link_options(editor_tests PRIVATE /STACK:8388608)
endif()
if(NOT MSVC)
  target_link_libraries(editor_tests PRIVATE m)
endif()
add_test(NAME geometry COMMAND editor_tests)

add_executable(network_unit_tests network_test.cpp)
target_compile_features(network_unit_tests PRIVATE cxx_std_17)
add_test(NAME network_unit COMMAND network_unit_tests)

set_source_files_properties(geometry_test.c ../src/Main/edGeometry.c ../src/Main/edLevelIO.c PROPERTIES LANGUAGE CXX)
target_compile_features(editor_tests PRIVATE cxx_std_20)

option(GEOSITH_ASAN "Instrument editor memory access" OFF)
if(MSVC AND GEOSITH_ASAN)
  target_compile_options(editor_tests PRIVATE /fsanitize=address /Zi)
  target_link_options(editor_tests PRIVATE /DEBUG /INCREMENTAL:NO)
endif()

add_test(NAME growing_storage COMMAND editor_tests --storage-only)

# Compile the exact production lock and preview bodies with lightweight backend
# stubs. Keep the anchors explicit so source drift fails configuration instead
# of silently testing a separate implementation.
function(extract_display_test_code source_file start_anchor end_anchor output_file)
  file(READ "${source_file}" source_text)
  string(FIND "${source_text}" "${start_anchor}" start_at)
  string(FIND "${source_text}" "${end_anchor}" end_at)
  if(start_at LESS 0 OR end_at LESS 0 OR end_at LESS_EQUAL start_at)
    message(FATAL_ERROR "Display test extraction anchors missing or reordered: ${source_file}")
  endif()
  string(LENGTH "${start_anchor}" anchor_length)
  math(EXPR after_start "${start_at} + ${anchor_length}")
  string(SUBSTRING "${source_text}" ${after_start} -1 remaining_text)
  string(FIND "${remaining_text}" "${start_anchor}" duplicate_start)
  string(LENGTH "${end_anchor}" anchor_length)
  math(EXPR after_end "${end_at} + ${anchor_length}")
  string(SUBSTRING "${source_text}" ${after_end} -1 remaining_text)
  string(FIND "${remaining_text}" "${end_anchor}" duplicate_end)
  if(NOT duplicate_start EQUAL -1 OR NOT duplicate_end EQUAL -1)
    message(FATAL_ERROR "Display test extraction anchors are not unique: ${source_file}")
  endif()
  math(EXPR code_length "${end_at} - ${start_at}")
  string(SUBSTRING "${source_text}" ${start_at} ${code_length} extracted_code)
  file(WRITE "${output_file}" "// Extracted from ${source_file}; do not edit.\n${extracted_code}")
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${source_file}")
endfunction()

extract_display_test_code("${CMAKE_CURRENT_SOURCE_DIR}/../src/Win95/stdDisplay.c"
  "int stdDisplay_VBufferLock(tVBuffer *buf)"
  "int stdDisplay_VBufferCopy(tVBuffer *vbuf,"
  "${CMAKE_CURRENT_BINARY_DIR}/display_lock_production.inc")
extract_display_test_code("${CMAKE_CURRENT_SOURCE_DIR}/../src/Main/jkEditor.c"
  "static void drawMaterialPreview(tVBuffer *buffer,int material,int x,int y,int width,int height) {"
  "static void materialTile(jkGuiElement *element,"
  "${CMAKE_CURRENT_BINARY_DIR}/display_preview_production.inc")
add_executable(display_lock_tests display_lock_test.cpp)
target_include_directories(display_lock_tests PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
target_compile_features(display_lock_tests PRIVATE cxx_std_17)
foreach(display_case null_buffer missing_surface backend_failure valid_pixels null_pixels
    preview_source_failure preview_destination_failure preview_source_null_pixels
    preview_destination_null_pixels preview_valid_pixels)
  add_test(NAME display_${display_case} COMMAND display_lock_tests ${display_case})
endforeach()

add_executable(relay_channel_tests relay_channel_test.cpp)
target_compile_features(relay_channel_tests PRIVATE cxx_std_17)
add_test(NAME relay_channel COMMAND relay_channel_tests)

add_executable(jkrelay_server ../tools/jkrelay/jkrelay_server.c)
if(WIN32)
  target_link_libraries(jkrelay_server PRIVATE ws2_32)
endif()

add_executable(sound_identity_tests sound_identity_test.cpp)
add_test(NAME sound_identity COMMAND sound_identity_tests)
