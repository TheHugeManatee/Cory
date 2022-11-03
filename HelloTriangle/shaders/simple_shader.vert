// A simple vertex shader with a hardcoded triangle
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec4 color;
    mat2 transform;
    vec2 offset;
} pushConstants;

void main() {
    vec2 pos = inPosition.xy * pushConstants.transform + pushConstants.offset;
    gl_Position = vec4(pos, 0.0, 1.0);
    outColor = inColor;
}