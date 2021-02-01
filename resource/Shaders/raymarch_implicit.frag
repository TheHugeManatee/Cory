#version 450
#extension GL_ARB_separate_shader_objects : enable
#extension GL_GOOGLE_include_directive : enable

#include "raymarch_utils.glsl"

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler3D texSampler;

layout(binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 proj;

    mat4 modelInv;
    mat4 viewInv;
    mat4 projInv;

    vec3 camPos;
    vec3 camFocus;
} ubo;

// because 200 is the best number.
const float SAMPLING_BASE_INTERVAL_RCP = 200.0f;

float GetIntensity(vec3 pos) {
    return texture(texSampler, pos).r;
}

float GetNormalizedIntensity(vec3 pos) {
    float I = GetIntensity(pos);
    return I * 10;
}

float GetOpacity(vec3 pos) {
    return GetNormalizedIntensity(pos);
}

vec4 GetColor(vec3 pos) {
    float alpha = GetOpacity(pos);
    return vec4(pos*alpha, alpha);
}

void IntegrateStep(inout vec4 rayColor, vec3 pos, float dT) {
    // sample color
    vec4 sampleColor = GetColor(pos);
    // account for varying stepsize
    sampleColor.a = 1.0 - pow(1.0 - sampleColor.a, dT * SAMPLING_BASE_INTERVAL_RCP);

    rayColor.rgb = rayColor.rgb + (1 - rayColor.a) * sampleColor.rgb * sampleColor.a;
    rayColor.a = rayColor.a + (1 - rayColor.a) * sampleColor.a;
}

vec4 RayMarch(vec3 startPos, vec3 rayDir, float tMax, float dT) {
    vec4 rayColor = vec4(0.0);

    float t = 0;
    while(t < tMax) {
        vec3 pos = startPos + t*rayDir;
        IntegrateStep(rayColor, pos, dT);
        t += dT;
    }
    IntegrateStep(rayColor, startPos + t*rayDir, tMax - t);

    return rayColor;
}


void main() {

    vec3 rayOrigin = ubo.camPos;
    vec3 rayDirection = normalize(fragWorldPos - ubo.camPos);

    vec2 t = rayBoxIntersection(rayOrigin, rayDirection, vec3(-0.5), vec3(0.5));

    vec3 rayStart = rayOrigin + t[0]*rayDirection + 0.5;
    vec3 rayEnd = rayOrigin + t[1]*rayDirection + 0.5;

    vec3 rayDir = rayEnd - rayStart;
    float stepSize = 0.005;

    outColor = RayMarch(rayStart, normalize(rayDir), length(rayDir), stepSize);
    //outColor = vec4(rayStart, 1.0);
    //outColor = vec4(rayEnd, 1.0);
    //outColor = vec4(vec3(intensity), 1.0);
    
    //outColor = vec4((rayStart+rayEnd)/2, 1.0);
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = texture(texSampler, fragTexCoord);
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
}