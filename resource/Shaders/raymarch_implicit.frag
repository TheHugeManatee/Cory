#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

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

void main() {
    outColor = vec4(fragColor, 1.0);
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    //outColor = texture(texSampler, fragTexCoord);
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
}