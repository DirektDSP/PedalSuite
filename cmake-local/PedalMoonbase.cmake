# PedalMoonbase.cmake
# Sets up a per-pedal copy of moonbase_JUCEClient with its own config.
# Each pedal gets its own copy so that per-pedal generated headers
# (ConfigDetails, IntegrityCheck) don't conflict across plugins.
#
# Set PEDALSUITE_DISABLE_MOONBASE=ON to build without licensing (dev builds).
#
# Usage:
#   pedal_setup_moonbase(
#       TARGET      <pedal_target_name>
#       CONFIG_JSON <path_to_moonbase_api_config.json>
#   )

option(PEDALSUITE_DISABLE_MOONBASE "Disable Moonbase licensing for dev builds" OFF)

function(pedal_setup_moonbase)
    cmake_parse_arguments(MB "" "TARGET;CONFIG_JSON" "" ${ARGN})

    if(NOT MB_TARGET OR NOT MB_CONFIG_JSON)
        message(FATAL_ERROR "pedal_setup_moonbase requires TARGET and CONFIG_JSON")
    endif()

    if(PEDALSUITE_DISABLE_MOONBASE)
        message(STATUS "Moonbase DISABLED for ${MB_TARGET} (dev build)")
        target_compile_definitions(${MB_TARGET} PRIVATE
            PEDALSUITE_NO_MOONBASE=1)
        # Add stubs header to include path
        target_include_directories(${MB_TARGET} PRIVATE
            "${CMAKE_SOURCE_DIR}/cmake-local")
        return()
    endif()

    set(MB_SOURCE_DIR "${CMAKE_SOURCE_DIR}/modules/moonbase_JUCEClient")
    set(MB_DEST_DIR "${CMAKE_CURRENT_BINARY_DIR}/moonbase_JUCEClient")

    # Copy moonbase module to per-pedal build directory
    file(COPY "${MB_SOURCE_DIR}/"
        DESTINATION "${MB_DEST_DIR}"
        PATTERN ".git" EXCLUDE)

    # Run PreBuild.sh with this pedal's config
    execute_process(
        COMMAND bash "${MB_DEST_DIR}/PreBuild.sh" "${MB_CONFIG_JSON}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        RESULT_VARIABLE MB_RESULT)

    if(NOT MB_RESULT EQUAL 0)
        message(FATAL_ERROR "moonbase PreBuild.sh failed for ${MB_TARGET}")
    endif()

    # Add moonbase sources directly to the pedal target
    target_sources(${MB_TARGET} PRIVATE "${MB_DEST_DIR}/moonbase_JUCEClient.cpp")

    # IMPORTANT: Add the per-pedal build dir BEFORE any other include paths
    # so that headers from the per-pedal copy are found first (not the source module).
    # This prevents #pragma once conflicts between source and build-dir copies.
    target_include_directories(${MB_TARGET} BEFORE PRIVATE "${MB_DEST_DIR}")

    target_compile_definitions(${MB_TARGET} PRIVATE
        JUCE_MODULE_AVAILABLE_moonbase_JUCEClient=1
        INCLUDE_MOONBASE_UI=0)

    # Moonbase module dependencies
    target_link_libraries(${MB_TARGET} PRIVATE
        juce_product_unlocking)

endfunction()
