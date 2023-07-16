#version 450

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    vec4 color;
    mat2 transform;
    vec2 offset;
} pushConstants;

void main() {
    outColor = pushConstants.color;
}