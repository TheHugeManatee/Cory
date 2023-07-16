#version 450

layout (location = 0) out vec4 outColor;

layout (push_constant) uniform PushConstants {
    float dummy;
} push;


#define SET_Static  0
#define SET_Frame   1
#define SET_Pass    2
#define SET_User    3


layout (set = SET_Frame, binding = 0) uniform Uniforms {
    vec2 center;
    vec2 size;
    vec2 window;
} globals;

layout (set = SET_Frame, binding = 1) uniform sampler2DMS textures[8];

layout (location = 0) in vec2 inTex;

vec4 colorMap(float t) {
    return mix(vec4(0.0, 0.0, 0.0, 1.0), vec4(1.0, 1.0, 1.0, 1.0), t);
}

void main() {
    vec2 start = globals.center - globals.size / 2.0;
    vec2 end = globals.center + globals.size / 2.0;

    if (inTex.x < start.x || inTex.x > end.x || inTex.y < start.y || inTex.y > end.y) {
        discard;
    }

    vec4 texColor = texelFetch(textures[0], ivec2(gl_FragCoord.xy), gl_SampleID);
    float depthNorm = (texColor.r - globals.window.x) / (globals.window.y - globals.window.x);

    outColor = colorMap(depthNorm);
}