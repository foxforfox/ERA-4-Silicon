cmake_minimum_required(VERSION 3.21)
project(ERA4_Silicon_Reference
    VERSION     1.0.0
    DESCRIPTION "Cleaned-up reference implementations from Ghidra decompilation of ERA 4 plugins"
    LANGUAGES   C
)

# ── Build settings ────────────────────────────────────────────────────
set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# ── Apple Accelerate framework (for vDSP on macOS) ───────────────────
if(APPLE)
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    if(NOT ACCELERATE_FRAMEWORK)
        message(WARNING "Accelerate framework not found; vDSP calls will use portable fallback")
    endif()
endif()

# ── DeEsser ERA 4 Silicon ────────────────────────────────────────────
add_library(DeEsser_ERA_4_Silicon STATIC
    DeEsser_ERA_4_Silicon/deesser_dsp.c
)
target_include_directories(DeEsser_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/DeEsser_ERA_4_Silicon
)
if(APPLE AND ACCELERATE_FRAMEWORK)
    target_link_libraries(DeEsser_ERA_4_Silicon PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
target_link_libraries(DeEsser_ERA_4_Silicon PRIVATE m)
set_target_properties(DeEsser_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "DeEsser_ERA_4_Silicon"
)

# ── DeEsserPro ERA 4 Silicon ─────────────────────────────────────────
add_library(DeEsserPro_ERA_4_Silicon STATIC
    DeEsserPro_ERA_4_Silicon/deesser_pro_dsp.c
)
target_include_directories(DeEsserPro_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/DeEsserPro_ERA_4_Silicon
)
if(APPLE AND ACCELERATE_FRAMEWORK)
    target_link_libraries(DeEsserPro_ERA_4_Silicon PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
target_link_libraries(DeEsserPro_ERA_4_Silicon PRIVATE m)
set_target_properties(DeEsserPro_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "DeEsserPro_ERA_4_Silicon"
)

# ── PlosiveRemover ERA 4 Silicon ─────────────────────────────────────
add_library(PlosiveRemover_ERA_4_Silicon STATIC
    PlosiveRemover_ERA_4_Silicon/plosive_dsp.c
)
target_include_directories(PlosiveRemover_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/PlosiveRemover_ERA_4_Silicon
)
if(APPLE AND ACCELERATE_FRAMEWORK)
    target_link_libraries(PlosiveRemover_ERA_4_Silicon PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
target_link_libraries(PlosiveRemover_ERA_4_Silicon PRIVATE m)
set_target_properties(PlosiveRemover_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "PlosiveRemover_ERA_4_Silicon"
)

# ── NoiseRemover ERA 4 Silicon ───────────────────────────────────────
add_library(NoiseRemover_ERA_4_Silicon STATIC
    NoiseRemover_ERA_4_Silicon/noise_remover_dsp.c
)
target_include_directories(NoiseRemover_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/NoiseRemover_ERA_4_Silicon
)
if(APPLE AND ACCELERATE_FRAMEWORK)
    target_link_libraries(NoiseRemover_ERA_4_Silicon PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
target_link_libraries(NoiseRemover_ERA_4_Silicon PRIVATE m)
set_target_properties(NoiseRemover_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "NoiseRemover_ERA_4_Silicon"
)

# ── DeClipper ERA 4 Silicon ──────────────────────────────────────────
add_library(DeClipper_ERA_4_Silicon STATIC
    DeClipper_ERA_4_Silicon/declipper_dsp.c
)
target_include_directories(DeClipper_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/DeClipper_ERA_4_Silicon
)
if(APPLE AND ACCELERATE_FRAMEWORK)
    target_link_libraries(DeClipper_ERA_4_Silicon PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
target_link_libraries(DeClipper_ERA_4_Silicon PRIVATE m)
set_target_properties(DeClipper_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "DeClipper_ERA_4_Silicon"
)

# ── ReverbRemover ERA 4 Silicon ──────────────────────────────────────
add_library(ReverbRemover_ERA_4_Silicon STATIC
    ReverbRemover_ERA_4_Silicon/reverb_remover_dsp.c
)
target_include_directories(ReverbRemover_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/ReverbRemover_ERA_4_Silicon
)
if(APPLE AND ACCELERATE_FRAMEWORK)
    target_link_libraries(ReverbRemover_ERA_4_Silicon PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
target_link_libraries(ReverbRemover_ERA_4_Silicon PRIVATE m)
set_target_properties(ReverbRemover_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "ReverbRemover_ERA_4_Silicon"
)

# ── VoiceLeveler ERA 4 Silicon ───────────────────────────────────────
add_library(VoiceLeveler_ERA_4_Silicon STATIC
    VoiceLeveler_ERA_4_Silicon/voice_leveler_dsp.c
)
target_include_directories(VoiceLeveler_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/VoiceLeveler_ERA_4_Silicon
)
target_link_libraries(VoiceLeveler_ERA_4_Silicon PRIVATE m)
set_target_properties(VoiceLeveler_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "VoiceLeveler_ERA_4_Silicon"
)

# ── ERA-D ERA 4 Silicon ──────────────────────────────────────────────
# Dual-engine: depends on NoiseRemover + ReverbRemover
add_library(ERA_D_ERA_4_Silicon STATIC
    ERA-D_ERA_4_Silicon/era_d_dsp.c
)
target_include_directories(ERA_D_ERA_4_Silicon PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/ERA-D_ERA_4_Silicon
    ${CMAKE_CURRENT_SOURCE_DIR}/NoiseRemover_ERA_4_Silicon
    ${CMAKE_CURRENT_SOURCE_DIR}/ReverbRemover_ERA_4_Silicon
)
if(APPLE AND ACCELERATE_FRAMEWORK)
    target_link_libraries(ERA_D_ERA_4_Silicon PRIVATE ${ACCELERATE_FRAMEWORK})
endif()
target_link_libraries(ERA_D_ERA_4_Silicon PRIVATE m NoiseRemover_ERA_4_Silicon ReverbRemover_ERA_4_Silicon)
set_target_properties(ERA_D_ERA_4_Silicon PROPERTIES
    OUTPUT_NAME "ERA-D_ERA_4_Silicon"
)
