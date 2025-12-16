#version 450

layout (location = 0) in vec3 inWorldPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec4 inInstanceColor;
layout (location = 4) in float inBlend;

layout (location = 0) out vec4 outColor;

layout (set = 0, binding = 0) uniform CubeUBO {
    mat4 projection;
    mat4 view;
    mat4 viewProjection;
    vec3 lightPosition;
} globals;

void main() {
    const float ambient = 0.1;
    vec3 posToLight = globals.lightPosition - inWorldPosition;
    vec3 lightVector = normalize(posToLight);
    float diffuse = abs(dot(inNormal, lightVector));
    const vec4 albedo = mix(inColor, inInstanceColor, inBlend);

    outColor = min(diffuse + ambient, 1.0) * albedo;
}
