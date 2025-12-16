#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;

layout (location = 0) out vec3 outWorldPosition;
layout (location = 1) out vec3 outNormal;
layout (location = 2) out vec4 outColor;
layout (location = 3) out vec4 outInstanceColor;
layout (location = 4) out float outBlend;

layout (set = 0, binding = 0) uniform CubeUBO {
    mat4 projection;
    mat4 view;
    mat4 viewProjection;
    vec3 lightPosition;
} globals;

struct InstanceData {
    mat4 modelToWorld;
    vec4 color;
    vec4 parameters;
};

layout(std430, set = 0, binding = 2) readonly buffer InstanceBuffer {
    InstanceData instances[];
};

void main() {
    InstanceData instance = instances[gl_InstanceIndex];
    mat4 model = instance.modelToWorld;

    vec4 worldPos = model * vec4(inPosition, 1.0);

    gl_Position = globals.viewProjection * worldPos;

    outWorldPosition = worldPos.xyz;
    mat4 normalMatrix = transpose(inverse(model));
    vec4 normal = normalMatrix * vec4(inNormal, 0.0);
    outNormal = normal.xyz;
    outColor = inColor;
    outInstanceColor = instance.color;
    outBlend = instance.parameters.x;
}
