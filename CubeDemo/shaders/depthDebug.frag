#version 450

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstants {
    float dummy;
} push;

layout (set = 0, binding = 0) uniform CubeUBO {
    float dummy;
} globals;

layout (location = 0) in vec2 inTex;

void main() {
    outColor = vec4(inTex.xy, 0.0, 1.0);
}