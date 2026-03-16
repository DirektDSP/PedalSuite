set(PEDAL_NAME "Electrica")
set(PRODUCT_NAME "Electrica")
set(COMPANY_NAME "DirektDSP")
set(BUNDLE_ID "com.direktdsp.electrica")
set(FORMATS AU VST3 AUv3 CLAP Standalone)

juce_add_plugin("${PEDAL_NAME}"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/../packaging/icon.png"
    COMPANY_NAME "${COMPANY_NAME}"
    BUNDLE_ID "${BUNDLE_ID}"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Manu
    PLUGIN_CODE Elec
    FORMATS "${FORMATS}"
    PRODUCT_NAME "${PRODUCT_NAME}"
    NEEDS_MIDI_OUTPUT TRUE)

add_library(${PEDAL_NAME}_SharedCode INTERFACE)

clap_juce_extensions_plugin(TARGET "${PEDAL_NAME}"
    CLAP_ID "${BUNDLE_ID}"
    CLAP_FEATURES audio-effect)

# Per-pedal moonbase config generation + linking
pedal_setup_moonbase(
    TARGET "${PEDAL_NAME}"
    CONFIG_JSON "${CMAKE_CURRENT_SOURCE_DIR}/moonbase_api_config.json")

if (MSVC)
    target_compile_options(${PEDAL_NAME}_SharedCode INTERFACE $<$<CONFIG:RELEASE>:/fp:fast>)
    target_compile_options(${PEDAL_NAME}_SharedCode INTERFACE $<$<CONFIG:RELEASE>:/Ox>)
    target_compile_options(${PEDAL_NAME}_SharedCode INTERFACE /Zc:__cplusplus)
else ()
    target_compile_options(${PEDAL_NAME}_SharedCode INTERFACE $<$<CONFIG:RELEASE>:-Ofast>)
    target_compile_options(${PEDAL_NAME}_SharedCode INTERFACE $<$<CONFIG:RelWithDebInfo>:-Ofast>)
endif ()
target_compile_features(${PEDAL_NAME}_SharedCode INTERFACE cxx_std_23)

file(GLOB_RECURSE SourceFiles CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_include_directories(${PEDAL_NAME}_SharedCode INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/source"
    "${CMAKE_SOURCE_DIR}/modules/cycfi_q/q_lib/include"
    "${CMAKE_SOURCE_DIR}/modules/cycfi_infra/include")
target_sources(${PEDAL_NAME}_SharedCode INTERFACE ${SourceFiles})

file(GLOB_RECURSE AssetFiles CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
list(FILTER AssetFiles EXCLUDE REGEX "/\\.DS_Store$")
if (AssetFiles)
    juce_add_binary_data(${PEDAL_NAME}_Assets SOURCES ${AssetFiles})
    set_target_properties(${PEDAL_NAME}_Assets PROPERTIES POSITION_INDEPENDENT_CODE TRUE)
    target_link_libraries(${PEDAL_NAME}_SharedCode INTERFACE ${PEDAL_NAME}_Assets)
endif()

target_compile_definitions(${PEDAL_NAME}_SharedCode
    INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    CMAKE_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
    VERSION="${CURRENT_VERSION}"
    PRODUCT_NAME_WITHOUT_VERSION="${PEDAL_NAME}")

target_link_libraries(${PEDAL_NAME}_SharedCode
    INTERFACE
    DirektDSP_GUI
    melatonin_inspector
    juce_audio_utils
    juce_audio_processors
    juce_dsp
    juce_gui_basics
    juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

target_link_libraries("${PEDAL_NAME}" PRIVATE ${PEDAL_NAME}_SharedCode)
