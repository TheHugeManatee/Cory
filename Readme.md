# `Cory` Engine
> Vulkan toy renderer/playground

## Goals
This project is inteded to be a toy rendering engine to familiarize myself with vulkan.
I also use it to test more modern C++ features.


#### Short Term
 - establish basic structures for a reusable rendering lib

#### Mid Term
 - Modern C++ (17/20)
 - C++ Modules (if possible)
 - Render Graphs/Frame Graphs
 - Implement a simple Volume Raymarcher
 - Find a legitimate use for C++ coroutines?
 - Type Reflection

#### Long Term
 - Implement Monte Carlo Volume Raycasting
 - Play around with ray tracing

#### Done
 - ImGui integration
 - Performance and Logging classes
 - Refactoring of vulkan classes towards a reusable code base
 - conversion towards using vulkan.hpp C++ interface
 - Vulkan-Tutorial.com

### Inspiration and Resources
 - Engine structure and basic facilities are inspired heavily by the Hazel engine by TheCherno, greatly documented on his Youtube Channel https://www.youtube.com/user/TheChernoProject
 - Alexander Overvoorde's excellent Vulkan tutorial https://vulkan-tutorial.com/
 - Tutorial on how to actually integrate imgui into a vulkan renderer https://frguthmann.github.io/posts/vulkan_imgui/, based off of the vulkan-tutorial.com source code


### Included Third-Party libraries
`Cory` pulls some third-party libraries into this repository. They all live in the subdirectory `ThirdParty` and are available under their respective license, included in the respecive directory or at the top of the header files:
 - glm, https://github.com/g-truc/glm, MIT / Happy Bunny License
 - stb_image, https://github.com/nothings/stb, MIT / Public Domain
 - tinyobjloader, https://github.com/tinyobjloader/tinyobjloader, MIT License
 - Vulkan Memory Allocator, https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator, MIT
 - refl-cpp, https://github.com/veselink1/refl-cpp, MIT license
 - Modified `CameraManipulator` Class, https://github.com/KhronosGroup/Vulkan-Hpp/tree/master/samples/RayTracing, Apache 2.0
 - Dear ImGui Vulkan backend, https://github.com/ocornut/imgui/tree/master/backends, MIT
 - imGuIZMO.quat for totation widgets https://github.com/BrutPitt/imGuIZMO.quat, BSD 2-clause

### Other referenced third party libraries
See `conanfile.txt` for the exact versions used, but there is of course always some flexibility.
 - glfw
 - glew
 - spdlog
 - libfmt


## Cory?
According to [this very trustworthy-looking web source](http://www.talesbeyondbelief.com/roman-gods/vulcan.htm), *Corynetes* was the son of the roman god Vulcan.
