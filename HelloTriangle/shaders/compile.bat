"C:\VulkanSDK\1.3.224.1\Bin\glslc.exe" simple_shader.vert -o simple_shader.vert.spv
@if not %ERRORLEVEL% == 0 goto :err

"C:\VulkanSDK\1.3.224.1\Bin\glslc.exe" simple_shader.frag -o simple_shader.frag.spv
@if not %ERRORLEVEL% == 0 goto :err

goto :end

:err
pause

:end