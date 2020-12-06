#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) in vec3 fragColor;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

layout(binding = 1) uniform sampler2D texSamplers[2];
#define texSampler texSamplers[0]
#define overlaySampler texSamplers[1]

const vec2 overlayTL = vec2(54, 14) / 256;
const vec2 overlayBR = vec2(54+86, 14+86)/256;

void main() {

    //outColor = vec4(fragColor, 1.0);
    //outColor = vec4(fragTexCoord, 0.0, 1.0);
    vec2 overlayTexCoord = (fragTexCoord - overlayTL) / (overlayBR - overlayTL);

    vec4 tex1 = texture(texSampler, fragTexCoord);
    vec4 tex2 = texture(overlaySampler, overlayTexCoord);

    outColor = vec4(mix(tex1.rgb, tex2.rgb, tex2.a), tex2.a + (1 - tex2.a)*tex1.a);
}