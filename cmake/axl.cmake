# Axl from Mega Man X8 16-bit. The tracked art in assets/axl builds one
# Saber-format sidecar (tools/axl/axl_manifest.json, made by
# tools/axl/make_axl_manifest.py) that is compiled into the game.
set(_axl_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/assets/axl/sprites")
set(_axl_manifest "${CMAKE_CURRENT_SOURCE_DIR}/tools/axl/axl_manifest.json")
set(_axl_cache "${CMAKE_CURRENT_BINARY_DIR}/cache/mmx-source/axl-v1.bin")
set(_axl_embedded_source "${CMAKE_CURRENT_BINARY_DIR}/generated/mmx_axl_embedded.c")
set(_axl_giga_start "${CMAKE_CURRENT_SOURCE_DIR}/assets/axl/sfx/giga_crash_start.ogg")
set(_axl_giga_shot "${CMAKE_CURRENT_SOURCE_DIR}/assets/axl/sfx/giga_crash_shot.ogg")
file(GLOB _axl_sprite_files CONFIGURE_DEPENDS "${_axl_source_dir}/*.png")
if(NOT (_axl_sprite_files AND EXISTS "${_axl_manifest}"))
    message(FATAL_ERROR "Axl sprites or manifest missing from ${_axl_source_dir}")
endif()
add_custom_command(
    OUTPUT "${_axl_cache}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/cache/mmx-source"
    COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_CURRENT_SOURCE_DIR}/tools/saber/convert_saber_zero.py"
        --manifest "${_axl_manifest}" --source-dir "${_axl_source_dir}" --out "${_axl_cache}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tools/saber/convert_saber_zero.py" "${_axl_manifest}" ${_axl_sprite_files}
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    VERBATIM)
add_custom_command(
    OUTPUT "${_axl_embedded_source}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
    COMMAND "${CMAKE_COMMAND}" "-DOUT=${_axl_embedded_source}" "-DSPRITES=${_axl_cache}"
        "-DGIGA_START=${_axl_giga_start}" "-DGIGA_SHOT=${_axl_giga_shot}"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_axl_sprites.cmake"
    DEPENDS "${_axl_cache}" "${_axl_giga_start}" "${_axl_giga_shot}"
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed_axl_sprites.cmake"
    VERBATIM)
add_custom_target(axl_embedded_source DEPENDS "${_axl_embedded_source}")
set(_axl_sources "${_axl_embedded_source}"
    "${CMAKE_CURRENT_SOURCE_DIR}/tools/saber/third_party/stb_vorbis.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mmx_axl.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/mmx_axl_plugin.c")
foreach(_target MegaManXSNESRecomp RockmanXSNESRecomp mmx_state_tests mmx_saber_rom_tests)
    if(TARGET ${_target})
        target_sources(${_target} PRIVATE ${_axl_sources})
        add_dependencies(${_target} axl_embedded_source)
    endif()
endforeach()
# The Japanese build has no Saber Zero, but Axl shares its asset parser.
if(TARGET RockmanXSNESRecomp)
    target_sources(RockmanXSNESRecomp PRIVATE "${_saber_assets_source}")
endif()
