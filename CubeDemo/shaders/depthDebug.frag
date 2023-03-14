#version 450

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstants {
    float dummy;
} push;

layout (set = 0, binding = 0) uniform CubeUBO {
    float dummy;
} globals;


layout (set = 0, binding = 1) uniform sampler2DMS textures[8];


layout (location = 0) in vec2 inTex;

vec4 colorMap(float t) {
    return mix(vec4(0.0, 0.0, 0.0, 1.0), vec4(1.0, 1.0, 1.0, 1.0), t);
}

void main() {
    if (inTex.x > 0.5) {
        discard;
    }

    vec4 texColor = texelFetch(textures[0], ivec2(gl_FragCoord.xy), gl_SampleID);
    float depthNorm = (texColor.r - 0.9) / 0.1;

    outColor = colorMap(depthNorm);
}