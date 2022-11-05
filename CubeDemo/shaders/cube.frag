#version 450

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 transform;
    vec4 color;
    float blend;
} pushConstants;

void main() {
    outColor = mix(inColor, pushConstants.color, pushConstants.blend);
}