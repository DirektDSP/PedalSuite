cmake_minimum_required(VERSION 3.25)

# This tells cmake we have goodies in the /cmake folder (submodule) and /cmake-local (our own)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake-local")
include(PamplejuceVersion)

# Modern concise way to add dependencies to your project
include(CPM)

# Configures universal binaries and decides which version of macOS to support
include(PamplejuceMacOS)

# Couple tweaks that IMO should be JUCE defaults
include(JUCEDefaults)

# Top-level project for PedalSuite
project(PedalSuite VERSION ${CURRENT_VERSION})

# JUCE is setup as a submodule in the /JUCE folder
add_subdirectory(JUCE)

# Shared modules (moonbase is handled per-pedal via PedalMoonbase)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)
add_subdirectory(modules/melatonin_inspector)

# Per-pedal moonbase setup helper
include(PedalMoonbase)

# Add each pedal subdirectory
add_subdirectory(Pneumatica)
add_subdirectory(Hydraulica)
add_subdirectory(Electrica)
add_subdirectory(Mechanica)
