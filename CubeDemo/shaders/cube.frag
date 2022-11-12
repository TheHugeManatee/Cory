#version 450

layout(location = 0) in vec4 inColor;

layout(location = 0) out vec4 outColor;

layout(push_constant) uniform PushConstants {
    mat4 modelTransform;
    vec4 color;
    float blend;
} push;

layout (set = 0, binding = 0) uniform CubeUBO {
    mat4 projection;
    mat4 view;
    mat4 viewProjection;
    vec3 lightPosition;
} globals;

void main() {
    outColor = mix(inColor, push.color, push.blend);
}