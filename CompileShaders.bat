glslc.exe shaders/triangleimplicit.vert -o build/triangle_vert.spv
glslc.exe shaders/passthrough.vert -o build/passthrough.spv
glslc.exe shaders/triangle.frag -o build/frag.spv
glslc.exe shaders/default.vert -o build/default-frag.spv


robocopy "build" "out/build/x64-Debug/bin/" *.spv
robocopy "build" "out/build/x64-Release/bin/" *.spv
robocopy "build" "build19" *.spv
pause