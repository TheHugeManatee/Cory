// A simple vertex shader with a hardcoded triangle
#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inTexCoord;
layout(location = 2) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 transform;
    vec4 color;
    float blend;
} pushConstants;

void main() {
    vec4 pos = pushConstants.transform * vec4(inPosition, 1.0);
    gl_Position = pos / pos.w;
    outColor = inColor;
}