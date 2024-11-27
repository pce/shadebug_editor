

## Build

    git submodule update --init --recursive
 
    mkdir build
    cd build
    cmake ..
    make



# Setup from Scratch

Already added Submodules

    git submodule add https://github.com/ocornut/imgui.git external/imgui
    git submodule add https://github.com/sammycage/lunasvg.git external/lunasvg
    git submodule add https://github.com/glfw/glfw.git external/glfw


Update Submodules

    git submodule update --init --recursive


## macOS 

Install CMake and ddd CMake to the PATH

    PATH="/Applications/CMake.app/Contents/bin":"$PATH"


