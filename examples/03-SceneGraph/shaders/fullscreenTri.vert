// Vulkan GLSL vertex shader << vkCmdDraw(_,_,3,1,0,0);
#version 450

const vec3[3] full_screen_triangle = vec3[3]
(
vec3(-1.0, 1.0, 0.0),
vec3(3.0, 1.0, 0.0),
vec3(-1.0, -3.0, 0.0)
);

layout (location = 0) out vec2 outTex;

void main()
{
    gl_Position = vec4(full_screen_triangle[gl_VertexIndex], 1.0);
    outTex = (full_screen_triangle[gl_VertexIndex].xy + 1.0) / 2.0;
}