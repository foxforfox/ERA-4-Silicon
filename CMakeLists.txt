cmake_minimum_required(VERSION 3.22)
project(ERA_PLUGINS VERSION 5.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

add_subdirectory(JUCE)

set(ERA_COMPANY_NAME      "accusonus")
set(ERA_MANUFACTURER_CODE "Accu")
set(ERA_PLUGIN_VERSION    "5.0.0")
set(ERA_BUNDLE_PREFIX     "com.accusonus")

set(SHARED_SOURCES
    shared/ERAProcessor.h
    shared/ERAProcessor.cpp
    shared/ERAEditor.h
    shared/ERAEditor.cpp
    shared/ERALookAndFeel.h
    shared/ERALookAndFeel.cpp
    shared/SVGAssets.h
    shared/ERAWidgets.h
    shared/ERAWidgets.cpp
    shared/DSP/FFTProcessor.h
    shared/DSP/FFTProcessor.cpp
    shared/DSP/NoiseReducer.h
    shared/DSP/NoiseReducer.cpp
    shared/DSP/DeEsserDSP.h
    shared/DSP/DeEsserDSP.cpp
    shared/DSP/DeEsserProDSP.h
    shared/DSP/DeEsserProDSP.cpp
    shared/DSP/DeClipperDSP.h
    shared/DSP/DeClipperDSP.cpp
    shared/DSP/ReverbReducer.h
    shared/DSP/ReverbReducer.cpp
    shared/DSP/VoiceLevelerDSP.h
    shared/DSP/VoiceLevelerDSP.cpp
    shared/DSP/PlosiveReducerDSP.h
    shared/DSP/PlosiveReducerDSP.cpp
)

function(era_add_plugin TARGET_NAME PLUGIN_NAME SUBTYPE SOURCES ASSET_DIR)
    juce_add_plugin(${TARGET_NAME}
        PRODUCT_NAME          "${PLUGIN_NAME}"
        COMPANY_NAME          "${ERA_COMPANY_NAME}"
        PLUGIN_MANUFACTURER_CODE "${ERA_MANUFACTURER_CODE}"
        PLUGIN_CODE           "${SUBTYPE}"
        FORMATS               AU VST3 Standalone
        IS_SYNTH              FALSE
        NEEDS_MIDI_INPUT      FALSE
        NEEDS_MIDI_OUTPUT     FALSE
        IS_MIDI_EFFECT        FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        BUNDLE_ID             "${ERA_BUNDLE_PREFIX}.${TARGET_NAME}"
        AU_MAIN_TYPE          "kAudioUnitType_Effect"
    )

    juce_generate_juce_header(${TARGET_NAME})

    # Collect all SVG files from the asset directory
    if(NOT "${ASSET_DIR}" STREQUAL "")
        file(GLOB ASSET_FILES "${CMAKE_CURRENT_SOURCE_DIR}/${ASSET_DIR}/*.svg")
        if(ASSET_FILES)
            # Muli Bold: the typeface ERA 4 draws its value read-outs with
            list(APPEND ASSET_FILES "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/Muli-Bold.ttf")
            juce_add_binary_data(${TARGET_NAME}_Assets
                SOURCES ${ASSET_FILES}
            )
        endif()
    endif()

    target_sources(${TARGET_NAME} PRIVATE
        ${SHARED_SOURCES}
        ${SOURCES}
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/shared
    )

    target_compile_definitions(${TARGET_NAME} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )

    # Link binary data if we created it
    if(NOT "${ASSET_DIR}" STREQUAL "" AND ASSET_FILES)
        target_link_libraries(${TARGET_NAME} PRIVATE ${TARGET_NAME}_Assets)
    endif()
endfunction()

era_add_plugin(ERA5_NoiseRemover
    "ERA 4.5 Noise Remover" "NrS5"
    "plugins/NoiseRemover/PluginProcessor.h;plugins/NoiseRemover/PluginProcessor.cpp;plugins/NoiseRemover/PluginEditor.h;plugins/NoiseRemover/PluginEditor.cpp"
    "assets/ERA4_NoiseRemover"
)

era_add_plugin(ERA5_DeEsser
    "ERA 4.5 De-Esser" "DsS5"
    "plugins/DeEsser/PluginProcessor.h;plugins/DeEsser/PluginProcessor.cpp;plugins/DeEsser/PluginEditor.h;plugins/DeEsser/PluginEditor.cpp"
    "assets/ERA4_DeEsser"
)

era_add_plugin(ERA5_DeEsserPro
    "ERA 4.5 De-Esser Pro" "DsP5"
    "plugins/DeEsserPro/PluginProcessor.h;plugins/DeEsserPro/PluginProcessor.cpp;plugins/DeEsserPro/PluginEditor.h;plugins/DeEsserPro/PluginEditor.cpp"
    "assets/ERA4_DeEsserPro"
)

era_add_plugin(ERA5_DeClipper
    "ERA 4.5 De-Clipper" "DcS5"
    "plugins/DeClipper/PluginProcessor.h;plugins/DeClipper/PluginProcessor.cpp;plugins/DeClipper/PluginEditor.h;plugins/DeClipper/PluginEditor.cpp"
    "assets/ERA4_DeClipper"
)

era_add_plugin(ERA5_ReverbRemover
    "ERA 4.5 Reverb Remover" "RrS5"
    "plugins/ReverbRemover/PluginProcessor.h;plugins/ReverbRemover/PluginProcessor.cpp;plugins/ReverbRemover/PluginEditor.h;plugins/ReverbRemover/PluginEditor.cpp"
    "assets/ERA4_ReverbRemover"
)

era_add_plugin(ERA5_VoiceLeveler
    "ERA 4.5 Voice Leveler" "VlS5"
    "plugins/VoiceLeveler/PluginProcessor.h;plugins/VoiceLeveler/PluginProcessor.cpp;plugins/VoiceLeveler/PluginEditor.h;plugins/VoiceLeveler/PluginEditor.cpp"
    "assets/ERA4_VoiceLeveler"
)

era_add_plugin(ERA5_PlosiveRemover
    "ERA 4.5 Plosive Remover" "PrS5"
    "plugins/PlosiveRemover/PluginProcessor.h;plugins/PlosiveRemover/PluginProcessor.cpp;plugins/PlosiveRemover/PluginEditor.h;plugins/PlosiveRemover/PluginEditor.cpp"
    "assets/ERA4_PlosiveRemover"
)

# ERA-D uses mostly PNG assets but has 2 SVGs (okButton);
# provide the asset dir so BinaryData.h is available for shared code.
era_add_plugin(ERA5_D
    "ERA-D" "ErD5"
    "plugins/ERA-D/PluginProcessor.h;plugins/ERA-D/PluginProcessor.cpp;plugins/ERA-D/PluginEditor.h;plugins/ERA-D/PluginEditor.cpp"
    "assets/ERA-D"
)

# ═══════════════════════════════════════════════════════════════════════
# ERA 4 Silicon plugins — clean C reference DSP, separate from legacy
# ═══════════════════════════════════════════════════════════════════════

set(ERA4S_COMMON_DIR "${CMAKE_CURRENT_SOURCE_DIR}/reverse/recompiled/common")

function(era4s_add_plugin TARGET_NAME PLUGIN_NAME PLUGIN_CODE DSP_DIR DSP_SOURCES PLUGIN_SOURCES)
    juce_add_plugin(${TARGET_NAME}
        PRODUCT_NAME          "${PLUGIN_NAME}"
        COMPANY_NAME          "${ERA_COMPANY_NAME}"
        PLUGIN_MANUFACTURER_CODE "${ERA_MANUFACTURER_CODE}"
        PLUGIN_CODE           "${PLUGIN_CODE}"
        FORMATS               AU VST3 Standalone
        IS_SYNTH              FALSE
        NEEDS_MIDI_INPUT      FALSE
        NEEDS_MIDI_OUTPUT     FALSE
        IS_MIDI_EFFECT        FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        BUNDLE_ID             "${ERA_BUNDLE_PREFIX}.${TARGET_NAME}"
        AU_MAIN_TYPE          "kAudioUnitType_Effect"
    )

    juce_generate_juce_header(${TARGET_NAME})

    target_sources(${TARGET_NAME} PRIVATE
        ${DSP_SOURCES}
        ${PLUGIN_SOURCES}
    )

    target_include_directories(${TARGET_NAME} PRIVATE
        ${ERA4S_COMMON_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/${DSP_DIR}
    )

    target_compile_definitions(${TARGET_NAME} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
    )

    target_link_libraries(${TARGET_NAME} PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_gui_extra
        "-framework Accelerate"
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
    )
endfunction()

# ── De-Esser ERA 4 Silicon ──────────────────────────────────────────
era4s_add_plugin(ERA4S_DeEsser
    "De-Esser ERA 4 Silicon" "4SDs"
    "reverse/recompiled/DeEsser_ERA_4_Silicon"
    "reverse/recompiled/DeEsser_ERA_4_Silicon/deesser_dsp.c"
    "plugins/Silicon/DeEsser/PluginProcessor.h;plugins/Silicon/DeEsser/PluginProcessor.cpp;plugins/Silicon/DeEsser/PluginEditor.h;plugins/Silicon/DeEsser/PluginEditor.cpp"
)

# ── De-Esser Pro ERA 4 Silicon ──────────────────────────────────────
era4s_add_plugin(ERA4S_DeEsserPro
    "De-Esser Pro ERA 4 Silicon" "4SDp"
    "reverse/recompiled/DeEsserPro_ERA_4_Silicon"
    "reverse/recompiled/DeEsserPro_ERA_4_Silicon/deesser_pro_dsp.c"
    "plugins/Silicon/DeEsserPro/PluginProcessor.h;plugins/Silicon/DeEsserPro/PluginProcessor.cpp;plugins/Silicon/DeEsserPro/PluginEditor.h;plugins/Silicon/DeEsserPro/PluginEditor.cpp"
)

# ── Plosive Remover ERA 4 Silicon ──────────────────────────────────
era4s_add_plugin(ERA4S_PlosiveRemover
    "Plosive Remover ERA 4 Silicon" "4SPr"
    "reverse/recompiled/PlosiveRemover_ERA_4_Silicon"
    "reverse/recompiled/PlosiveRemover_ERA_4_Silicon/plosive_dsp.c"
    "plugins/Silicon/PlosiveRemover/PluginProcessor.h;plugins/Silicon/PlosiveRemover/PluginProcessor.cpp;plugins/Silicon/PlosiveRemover/PluginEditor.h;plugins/Silicon/PlosiveRemover/PluginEditor.cpp"
)

# ── Noise Remover ERA 4 Silicon ────────────────────────────────────
era4s_add_plugin(ERA4S_NoiseRemover
    "Noise Remover ERA 4 Silicon" "4SNr"
    "reverse/recompiled/NoiseRemover_ERA_4_Silicon"
    "reverse/recompiled/NoiseRemover_ERA_4_Silicon/noise_remover_dsp.c"
    "plugins/Silicon/NoiseRemover/PluginProcessor.h;plugins/Silicon/NoiseRemover/PluginProcessor.cpp;plugins/Silicon/NoiseRemover/PluginEditor.h;plugins/Silicon/NoiseRemover/PluginEditor.cpp"
)

# ── De-Clipper ERA 4 Silicon ───────────────────────────────────────
era4s_add_plugin(ERA4S_DeClipper
    "De-Clipper ERA 4 Silicon" "4SDc"
    "reverse/recompiled/DeClipper_ERA_4_Silicon"
    "reverse/recompiled/DeClipper_ERA_4_Silicon/declipper_dsp.c"
    "plugins/Silicon/DeClipper/PluginProcessor.h;plugins/Silicon/DeClipper/PluginProcessor.cpp;plugins/Silicon/DeClipper/PluginEditor.h;plugins/Silicon/DeClipper/PluginEditor.cpp"
)

# ── Reverb Remover ERA 4 Silicon ──────────────────────────────────
era4s_add_plugin(ERA4S_ReverbRemover
    "Reverb Remover ERA 4 Silicon" "4SRr"
    "reverse/recompiled/ReverbRemover_ERA_4_Silicon"
    "reverse/recompiled/ReverbRemover_ERA_4_Silicon/reverb_remover_dsp.c"
    "plugins/Silicon/ReverbRemover/PluginProcessor.h;plugins/Silicon/ReverbRemover/PluginProcessor.cpp;plugins/Silicon/ReverbRemover/PluginEditor.h;plugins/Silicon/ReverbRemover/PluginEditor.cpp"
)

# ── Voice Leveler ERA 4 Silicon ───────────────────────────────────
era4s_add_plugin(ERA4S_VoiceLeveler
    "Voice Leveler ERA 4 Silicon" "4SVl"
    "reverse/recompiled/VoiceLeveler_ERA_4_Silicon"
    "reverse/recompiled/VoiceLeveler_ERA_4_Silicon/voice_leveler_dsp.c"
    "plugins/Silicon/VoiceLeveler/PluginProcessor.h;plugins/Silicon/VoiceLeveler/PluginProcessor.cpp;plugins/Silicon/VoiceLeveler/PluginEditor.h;plugins/Silicon/VoiceLeveler/PluginEditor.cpp"
)

# ── ERA-D ERA 4 Silicon ───────────────────────────────────────────
# ERA-D combines noise remover + reverb remover, so it needs both .c files
juce_add_plugin(ERA4S_D
    PRODUCT_NAME          "ERA-D ERA 4 Silicon"
    COMPANY_NAME          "${ERA_COMPANY_NAME}"
    PLUGIN_MANUFACTURER_CODE "${ERA_MANUFACTURER_CODE}"
    PLUGIN_CODE           "4SED"
    FORMATS               AU VST3 Standalone
    IS_SYNTH              FALSE
    NEEDS_MIDI_INPUT      FALSE
    NEEDS_MIDI_OUTPUT     FALSE
    IS_MIDI_EFFECT        FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    BUNDLE_ID             "${ERA_BUNDLE_PREFIX}.ERA4S_D"
    AU_MAIN_TYPE          "kAudioUnitType_Effect"
)

juce_generate_juce_header(ERA4S_D)

target_sources(ERA4S_D PRIVATE
    reverse/recompiled/ERA-D_ERA_4_Silicon/era_d_dsp.c
    reverse/recompiled/NoiseRemover_ERA_4_Silicon/noise_remover_dsp.c
    reverse/recompiled/ReverbRemover_ERA_4_Silicon/reverb_remover_dsp.c
    plugins/Silicon/ERA-D/PluginProcessor.h
    plugins/Silicon/ERA-D/PluginProcessor.cpp
    plugins/Silicon/ERA-D/PluginEditor.h
    plugins/Silicon/ERA-D/PluginEditor.cpp
)

target_include_directories(ERA4S_D PRIVATE
    ${ERA4S_COMMON_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/reverse/recompiled/ERA-D_ERA_4_Silicon
    ${CMAKE_CURRENT_SOURCE_DIR}/reverse/recompiled/NoiseRemover_ERA_4_Silicon
    ${CMAKE_CURRENT_SOURCE_DIR}/reverse/recompiled/ReverbRemover_ERA_4_Silicon
)

target_compile_definitions(ERA4S_D PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(ERA4S_D PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    "-framework Accelerate"
PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
