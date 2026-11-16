# au-render: runs a WAV file through one Audio Unit offline and writes the result.
# Used to measure what an ERA 4 original removes, so ERA 4.5 can be tuned to match.
# Universal, so it can run under Rosetta for the x86_64-only ERA 4 AUs:
#   cmake -S . -B <build dir> -DCMAKE_BUILD_TYPE=Release && cmake --build <build dir> -j
#   arch -x86_64 <build dir>/au-render_artefacts/Release/au-render <plugin.component> in.wav out.wav
cmake_minimum_required(VERSION 3.22)
project(AU_RENDER VERSION 1.0.0 LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

add_subdirectory(../../JUCE JUCE)

juce_add_console_app(au-render PRODUCT_NAME "au-render")
target_sources(au-render PRIVATE Main.cpp)

target_compile_definitions(au-render PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(au-render PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_recommended_config_flags
    "-framework CoreAudioKit")
