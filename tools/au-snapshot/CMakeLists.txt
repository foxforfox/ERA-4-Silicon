# au-snapshot: hosts one Audio Unit, opens its editor and saves a PNG of it.
# Used to compare the ERA 4.5 editors against the original ERA 4 ones.
# Build universal so it can run under Rosetta (the ERA 4 AUs are x86_64 only):
#   cmake -S . -B <build dir> -DCMAKE_BUILD_TYPE=Release && cmake --build <build dir> -j
#   arch -x86_64 <build dir>/au-snapshot_artefacts/Release/au-snapshot.app/Contents/MacOS/au-snapshot <plugin.component> out.png
cmake_minimum_required(VERSION 3.22)
project(AU_SNAPSHOT VERSION 1.0.0 LANGUAGES C CXX OBJCXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")

add_subdirectory(../../JUCE JUCE)

juce_add_gui_app(au-snapshot PRODUCT_NAME "au-snapshot")

target_sources(au-snapshot PRIVATE Main.cpp Capture.mm)

target_compile_definitions(au-snapshot PRIVATE
    JUCE_PLUGINHOST_AU=1
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(au-snapshot PRIVATE
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_gui_extra
    juce::juce_recommended_config_flags)
