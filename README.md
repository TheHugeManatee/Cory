# `Cory` Engine
> Vulkan toy renderer/playground

## Goals
This project is intended to be a toy rendering engine to familiarize myself with vulkan.

I also use it to test more modern C++ features such as coroutines and the executors proposal.
As such, it uses features that are only supported in rather modern compilers. 

#### Short Term
 - ImGui integration
 - Window and event system

#### Mid Term
 - Use of Modern C++20
 - C++ Modules (if possible)
 - Render Graphs/Frame Graphs
 - Implement a simple Volume Raymarcher
 - Find a legitimate use for C++ coroutines?

#### Long Term
 - Implement Monte Carlo Volume Raycasting
 - Play around with ray tracing

#### Done
 - Performance and Logging classes
 - Refactoring of vulkan classes towards a reusable code base

### Inspiration and Resources
 - Engine structure and basic from Hazel engine by TheCherno, greatly documented on his Youtube Channel https://www.youtube.com/user/TheChernoProject
 - Alexander Overvoorde's excellent Vulkan tutorial https://vulkan-tutorial.com/
 - Brendan Galea's Vulkan Tutorial https://www.youtube.com/watch?v=Y9U9IE0gVHA&list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR
 - Tutorial on how to actually integrate imgui into a vulkan renderer https://frguthmann.github.io/posts/vulkan_imgui/, based off of the vulkan-tutorial.com source code


### Included Third-Party sources
For the "bigger" libraries, this project uses conan to manage the dependencies. 
The integration is transparent, which means that if you have a suitable conan installation, the packages should be downloaded and built automatically when you configure the CMake project.

`Cory` also pulls in sources from some third-party libraries into this repository. They all live in the subdirectory `ThirdParty` and are available under their respective license, included in the respecive directory or at the top of the header files:
 - some cmake files in /cmake are from https://github.com/Lectem/cpp-boilerplate, MIT License
 - glm, https://github.com/g-truc/glm, MIT / Happy Bunny License
 - stb_image, https://github.com/nothings/stb, MIT / Public Domain
 - tinyobjloader, https://github.com/tinyobjloader/tinyobjloader, MIT License
 - Modified `CameraManipulator` Class, https://github.com/KhronosGroup/Vulkan-Hpp/tree/master/samples/RayTracing, Apache 2.0 License
 - Dear ImGui Vulkan backend, https://github.com/ocornut/imgui/tree/master/backends, MIT License
 - imGuIZMO.quat for rotation widgets https://github.com/BrutPitt/imGuIZMO.quat, BSD 2-clause License
 - KDBindings, https://github.com/KDAB/KDBindings, MIT License

### Other used third party libraries
See `conanfile.txt` for the other libraries and their exact versions used, but there is of course always some flexibility in using different versions.


## Building
Roughly:
```
git clone https://github.com/TheHugeManatee/Cory
cd Cory
cd Cory/conan-recipes
conan export corrade 2022.09.10_vk13@TheHugeManatee/custom
conan export magnum 2022.09.10_vk13@TheHugeManatee/custom
cd ..
mkdir build && cd build
conan install .. -s build_type=Debug
# -or- conan install .. -s build_type=Release
cmake ..
cmake --build .
```

### Cory?
According to [this very trustworthy-looking web source](http://www.talesbeyondbelief.com/roman-gods/vulcan.htm), *Corynetes* was the son of the roman god Vulcan.
