


> The trick is: push almost everything to the GPU, and keep the CPU side as a thin command buffer.


Building the app:

        mkdir build && cd build && cmake .. && make -j

or
        cmake -B build -DCMAKE_BUILD_TYPE=Release

or with presets:
        cmake --preset default

Then:
        cmake --build build -j


Modular Shaders

- Shakura-Sunyaev temperature gradient
- ...
