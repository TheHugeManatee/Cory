#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragTexCoord;

layout(location = 0) out vec4 outColor;

//layout(binding = 1) uniform sampler2D texSampler;

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

vec2 rayBoxIntersection(vec3 rayOrigin, vec3 rayDirection, 
                        vec3 boxLlf, vec3 boxUrb) 
{
    float t1 = (boxLlf[0] - rayOrigin[0])/rayDirection[0];
    float t2 = (boxUrb[0] - rayOrigin[0])/rayDirection[0];

    float tmin = min(t1, t2);
    float tmax = max(t1, t2);

    for (int i = 1; i < 3; ++i) {
        t1 = (boxLlf[i] - rayOrigin[i])/rayDirection[i];
        t2 = (boxUrb[i] - rayOrigin[i])/rayDirection[i];

        tmin = max(tmin, min(min(t1, t2), tmax));
        tmax = min(tmax, max(max(t1, t2), tmin));
    }

    return vec2(tmin, tmax);
}

void main() {

    vec3 rayOrigin = ubo.camPos;
    vec3 rayDirection = normalize(fragWorldPos - ubo.camPos);

    vec2 t = rayBoxIntersection(rayOrigin, rayDirection, vec3(-0.5), vec3(0.5));

    vec3 rayStart = rayOrigin + t[0]*rayDirection;
    vec3 rayEnd = rayOrigin + t[1]*rayDirection;

    outColor = vec4(rayStart + 0.5, 1.0);
    outColor = vec4(rayEnd + 0.5, 1.0);
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = texture(texSampler, fragTexCoord);
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
}