struct Globals {
    float2 center;
    float2 size;
    float2 window;
    uint textureIndex;
};

[[vk::push_constant]] Globals* globals;

[[vk::binding(1, 1)]] Sampler2DMS<float4> textures[];

struct PSIn {
    float4 pos : SV_Position;
    [[vk::location(0)]]
    float2 inTex : TEXCOORD0;
    uint sampleID : SV_SampleIndex;
};

float4 colorMap(float t)
{
    return lerp(float4(0, 0, 0, 1), float4(1, 1, 1, 1), t);
}

float4 main(PSIn input)
    : SV_Target
{
    float2 start = globals->center - globals->size * 0.5;
    float2 end = globals->center + globals->size * 0.5;

    if (input.inTex.x < start.x || input.inTex.x > end.x || input.inTex.y < start.y ||
        input.inTex.y > end.y) {
        discard;
    }

    float4 texColor = textures[globals->textureIndex].Load(int2(input.pos.xy), input.sampleID);

    float depthNorm = (texColor.r - globals.window.x) / (globals.window.y - globals.window.x);

    return colorMap(depthNorm);
}
