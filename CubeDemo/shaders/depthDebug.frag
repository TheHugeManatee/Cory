#version 450

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstants {
    float dummy;
} push;

layout (set = 0, binding = 0) uniform CubeUBO {
    float dummy;
} globals;

void main() {
    outColor = vec4(gl_FragCoord.xyz, 0.0);
}