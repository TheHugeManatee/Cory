#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec3 outWorldPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;

layout (push_constant) uniform PushConstants {
    mat4 modelToWorld;
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
    vec4 worldPos = globals.view * push.modelToWorld * vec4(inPosition, 1.0);
    vec4 projectedPos = globals.projection * worldPos;
    gl_Position = projectedPos / projectedPos.w;
    outWorldPosition = worldPos.xyz;
    // we currently compute this in the shader because we don't have enough space in the push constants
    mat4 normalMatrix = transpose(inverse(push.modelToWorld));
    vec4 normal = normalMatrix * vec4(inNormal, 0.0);
    outNormal = normal.xyz;
    outColor = inColor;
}