
struct InstanceData
{
    float4 positionAndSize;
    float4 color;
    float4 parameters;
};

struct PointSpriteGlobals
{
    float4x4 projection;
    float4x4 view;
    float4x4 viewProjection;
    float3 lightPosition;
    float padding0;
    float3 cameraRight;
    float padding1;
    float3 cameraUp;
    float padding2;
};

// Bindings
[[vk::binding(0, 0)]] ConstantBuffer<PointSpriteGlobals> gGlobals;
[[vk::binding(2, 0)]] StructuredBuffer<InstanceData> gInstances;
[[vk::binding(3, 0)]] RWStructuredBuffer<uint> gKeys;

struct PushConstants {
    uint numInstances;
    uint pad;
};
[[vk::push_constant]] PushConstants p;

[numthreads(256, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    uint i = dispatchThreadId.x;
    if (i >= p.numInstances) return;

    float3 pos = gInstances[i].positionAndSize.xyz;
    float4 viewPos = mul(gGlobals.view, float4(pos, 1.0));
    
    // Use depth for sorting. Smaller depth (closer to camera) should be handled.
    // Radix sort usually sorts in ascending order. 
    // For back-to-front transparency, we want larger distance first.
    // Or we sort ascending and traverse backwards.
    // Let's sort so that closer objects have larger keys (for back-to-front).
    // Actually, let's sort by distance and then use the sorted indices.
    
    // View space Z is negative in front of camera in many systems, 
    // but let's just use the distance squared or similar.
    float dist = -viewPos.z; // Distance from camera plane
    
    // Convert float distance to a sortable uint
    // For positive floats, we can just flip the bits if we want to reverse or just use as is.
    // To sort descending (back-to-front):
    // Higher distance -> smaller key? No, higher distance -> higher key, then render from end to start.
    // Or higher distance -> smaller key, then render from start to end.
    // Let's do: higher distance -> smaller key.
    
    // Mapping [near, far] to [MAX, 0]
    // A simple way: use a large value and subtract distance.
    // Or just bit-cast if distance is always positive and we want ascending.
    
    uint key = asuint(max(0.0, dist));
    // To flip for descending order:
    key = ~key; 

    gKeys[i] = key;
}
