# `Cory` Engine
> Vulkan toy renderer/playground

## Goals
This project is intended to be a toy rendering engine to familiarize myself with vulkan.

I also use it to test more modern C++ features such as coroutines and the executors proposal.
As such, it uses features that are only supported in rather modern compilers. 

#### Done
 - Basic coroutine-based framegraph **concept/API draft** 
 - ImGui integration
 - Performance and Logging classes
 - On-the fly shader compilation using [google/shaderc](https://github.com/google/shaderc)
 - The triangle!
 - Basic window and renderer infrastructure

#### Short Term
 - push constants
 - Render Graphs/Frame Graphs
 - descriptor set/shader uniforms abstraction
 - Window and event system

#### Mid Term
 - C++ Modules (if possible)
 - Implement a simple Volume Raymarcher
 - Extend usage of c++20 coroutines where meaningful

#### Long Term
 - Implement Monte Carlo Volume Raycasting
 - Play around with ray tracing
 - Play around with mesh shaders

### Inspiration and Resources
 - Engine structure and basic from Hazel engine by TheCherno, greatly documented on his Youtube Channel https://www.youtube.com/user/TheChernoProject
 - Alexander Overvoorde's excellent Vulkan tutorial https://vulkan-tutorial.com/
 - Brendan Galea's Vulkan Tutorial https://www.youtube.com/watch?v=Y9U9IE0gVHA&list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR
 - Tutorial on how to actually integrate imgui into a vulkan renderer https://frguthmann.github.io/posts/vulkan_imgui/, based off of the vulkan-tutorial.com source code
 - Framegraphs in Frostbite: GDC presentation by Yuri O'Donnell https://www.gdcvault.com/play/1024045/FrameGraph-Extensible-Rendering-Architecture-in
 - TheMaister blog post on Framegraphs: http://themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/

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

See `conanfile.txt` for the additional libraries and their exact versions used. I update them to their latest versions sporadically, but there is of course always some flexibility in using different versions.


## Building
I'm currently building on MSVC 2022 and use several of the latest features, so you'll only be able to build this with a fairly recent compiler.
While I generally try to stick with platform-independent code, I did not (yet?) invest much time into actual testing
with different platforms (compilers and/or drivers) - likely I'm already hitting some MSVC and NVidia-specific quirks that
may make compiling/running the application difficult on other environments.
I'm always interested in feedback on compatibility issues, but I can of course not promise to be able to fix anything.

That being said, to build the app, you will need to roughly perform the following steps:

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
