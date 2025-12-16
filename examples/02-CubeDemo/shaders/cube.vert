// A simple vertex shader with a hardcoded triangle
#version 450

layout (location = 0) in vec3 inPosition;
layout (location = 1) in vec3 inNormal;
layout (location = 2) in vec4 inColor;
layout (location = 3) in vec4 inInstanceTransform0;
layout (location = 4) in vec4 inInstanceTransform1;
layout (location = 5) in vec4 inInstanceTransform2;
layout (location = 6) in vec4 inInstanceTransform3;
layout (location = 7) in vec4 inInstanceColor;
layout (location = 8) in float inInstanceBlend;

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

void main() {
    mat4 model = mat4(
        inInstanceTransform0,
        inInstanceTransform1,
        inInstanceTransform2,
        inInstanceTransform3
    );

    vec4 worldPos = globals.view * model * vec4(inPosition, 1.0);

    gl_Position = globals.projection * worldPos;
    
    outWorldPosition = worldPos.xyz;
    // derive normal matrix in the shader from the per-instance transform
    mat4 normalMatrix = transpose(inverse(model));
    vec4 normal = normalMatrix * vec4(inNormal, 0.0);
    outNormal = normal.xyz;
    outColor = inColor;
    outInstanceColor = inInstanceColor;
    outBlend = inInstanceBlend;
}
