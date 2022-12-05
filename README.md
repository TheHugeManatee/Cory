# `Cory` Engine

> Vulkan toy renderer/playground

## Goals

This project is intended to be a toy rendering engine to familiarize myself with vulkan.

I also use it to test more modern C++ features such as coroutines and the executors proposal.
As such, it uses features that are only supported in rather modern compilers.

#### Done

- [x] Basic coroutine-based framegraph **concept/API draft**
- [x] ImGui integration
- [x] Performance and Logging classes
- [x] On-the fly shader compilation using [google/shaderc](https://github.com/google/shaderc)
- [x] The triangle!
- [x] Basic window and renderer infrastructure
- [x] push constants
- [x] dynamic rendering

#### Short Term

- descriptor set/shader uniforms abstraction
    - [x] basics demo done
    - [ ] manage descriptors and sets via ResourceManager
    - [ ] define a global descriptor set 0 that is managed by cory itself
- Render Graphs/Frame Graphs
    - [x] basic coroutine-based API and render pass resolution via graph search
    - [x] automatically create render passes and layouts
    - [x] perform automatic resource transitions
    - [x] actually execute render pass code that renders stuff
    - [ ] proper allocation of transient textures from arena
    - [ ] create AccessInfo templates for most common usages
    - [ ] automatically figure out required image usage for a transient image
    - [ ] extend transient resource system to buffers
    - [ ] split barriers
- Window and event system
    - [x] basic mouse and kb event forwarding
    - [ ] better abstraction/encapsulation
    - [ ] design simple coroutine-based event system for "game" logic

#### Mid Term

- GameObject system (Entities + Components, not ECS)
- Implement a simple Volume Raymarcher
- Extend usage of c++20 coroutines where meaningful
- Multithreading?!
    - Multithreaded framegraph recording?
    - Offload resource creation (shaders/pipelines) to another thread (pool)
    - explicit sync with queues where necessary
- C++ Modules (if possible)

#### Long Term

- Implement Monte Carlo Volume Raycasting
- Play around with ray tracing
- Play around with mesh shaders

### Inspiration and Resources

- Engine structure and basic from Hazel engine by TheCherno, greatly documented on his Youtube
  Channel https://www.youtube.com/user/TheChernoProject
- Alexander Overvoorde's excellent Vulkan tutorial https://vulkan-tutorial.com/
- Brendan Galea's Vulkan Tutorial https://www.youtube.com/watch?v=Y9U9IE0gVHA&list=PL8327DO66nu9qYVKLDmdLW_84-yE4auCR
- Tutorial on how to actually integrate imgui into a vulkan renderer https://frguthmann.github.io/posts/vulkan_imgui/,
  based off of the vulkan-tutorial.com source code
- Framegraphs/Render graphs:
    - Framegraphs in Frostbite: GDC presentation by Yuri
      O'Donnell https://www.gdcvault.com/play/1024045/FrameGraph-Extensible-Rendering-Architecture-in
    - TheMaister blog post on Framegraphs: http://themaister.net/blog/2017/08/15/render-graphs-and-vulkan-a-deep-dive/
    - Graham Wihlidal (SEED) talk on
      Halcyon: https://www.khronos.org/assets/uploads/developers/library/2019-reboot-develop-blue/SEED-EA_Rapid-Innovation-Using-Modern-Graphics_Apr19.pdf
- Dynamic rendering:
    - Lesley Lai's tutorial on dynamic rendering: https://lesleylai.info/en/vk-khr-dynamic-rendering/
    - [Sascha Willem's Dynamic Rendering example](https://github.com/SaschaWillems/Vulkan/blob/313ac10de4a765997ddf5202c599e4a0ca32c8ca/examples/dynamicrendering/dynamicrendering.cpp)
    - `VK_KHR_dynamic_rendering`
      proposal: https://github.com/KhronosGroup/Vulkan-Docs/blob/main/proposals/VK_KHR_dynamic_rendering.adoc

### Included Third-Party sources

For the "bigger" libraries, this project uses conan to manage the dependencies.
The integration is transparent, which means that if you have a suitable conan installation, the packages should be
downloaded and built automatically when you configure the CMake project. (after you have registered the corrade and
magnum recipes, see below).

`Cory` also pulls in sources from some third-party libraries into this repository. They all live in the
subdirectory `ThirdParty` and are available under their respective license, included in the respecive directory or at
the top of the header files:

- some cmake files in /cmake are from https://github.com/Lectem/cpp-boilerplate, MIT License
- glm, https://github.com/g-truc/glm, MIT / Happy Bunny License
- stb_image, https://github.com/nothings/stb, MIT / Public Domain
- tinyobjloader, https://github.com/tinyobjloader/tinyobjloader, MIT License
- Modified `CameraManipulator` Class, https://github.com/KhronosGroup/Vulkan-Hpp/tree/master/samples/RayTracing, Apache
  2.0 License
- Dear ImGui Vulkan backend, https://github.com/ocornut/imgui/tree/master/backends, MIT License
- imGuIZMO.quat for rotation widgets https://github.com/BrutPitt/imGuIZMO.quat, BSD 2-clause License
- KDBindings, https://github.com/KDAB/KDBindings, MIT License
- simpler_vulkan_synchronization by Tobias Hector, https://github.com/Tobski/simple_vulkan_synchronization/, MIT License -- this has been adapted to this project's conventions and interfaces, see Renderer/SimplerVulkanSynchronization

See `conanfile.txt` for the additional libraries and their exact versions used. I update them to their latest versions
sporadically, but there is of course always some flexibility in using different versions.

## Building

I'm currently building on MSVC 2022 and use several of the latest features, so you'll only be able to build this with a
fairly recent compiler.
While I generally try to stick with platform-independent code, I did not (yet?) invest much time into actual testing
with different platforms (compilers and/or drivers) - likely I'm already hitting some MSVC and NVidia-specific quirks
that
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

According to [this very trustworthy-looking web source](http://www.talesbeyondbelief.com/roman-gods/vulcan.htm),
*Corynetes* was the son of the roman god Vulcan.
Also, something something coroutines.

### Extensions Used or planned to use

- [x] KHR_dynamic_rendering: used for implementing the framegraph
- [x] [EXT_debug_utils](https://registry.khronos.org/vulkan/specs/1.2-extensions/man/html/VK_EXT_debug_utils.html):
  validation stuff!
- [x] [KHR_synchronization2](https://registry.khronos.org/vulkan/specs/1.2-extensions/man/html/VK_KHR_synchronization2.html):
  better barriers
- [ ] [EXT_device_memory_report](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_device_memory_report.html):
  would be nice to get some insights on memory usage
- [ ] [KHR_timeline_semaphore](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_timeline_semaphore.html):
  semaphores with monotonically increasing counters, useful for e.g. parallel subsystems like gpu-physics running
  asynchronously to rendering
- [ ] [EXT_mesh_shader](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_mesh_shader.html): mesh
  shaders are the new hot thing, I should try at some point
- [ ] [EXT_full_screen_exclusive](https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_EXT_full_screen_exclusive.html):
  full screen stuff. maybe better handled through GLFW?
