module CoryBindings;

// Keep these values in sync with Cory::DescriptorSetType and Cory::ImageBindPoint.
public static const int DESCRIPTOR_SET_GLOBAL_DATA = 0;
public static const int DESCRIPTOR_SET_BINDLESS_TEXTURES = 1;

public static const int BINDLESS_TEXTURE2D = 0;
public static const int BINDLESS_TEXTURE3D = 1;
public static const int BINDLESS_STORAGE_IMAGE2D = 2;
public static const int BINDLESS_STORAGE_IMAGE3D = 3;
public static const int BINDLESS_SAMPLERS = 4;
public static const int BINDLESS_STORAGE_IMAGE2DMS = 5;

// Canonical bindless resource declarations for set=1 managed by ShaderBindingContext.
public [[vk::binding(0, 1)]] Sampler2DMS<float4> gBindlessTexture2DMS[];
public [[vk::binding(1, 1)]] Sampler3D<float> gBindlessTexture3D[];
public [[vk::binding(2, 1)]] RWTexture2D<float4> gBindlessStorageImage2D;
public [[vk::binding(3, 1)]] RWTexture3D<float> gBindlessStorageImage3D;
public [[vk::binding(4, 1)]] SamplerState gBindlessSamplers[];
public [[vk::binding(5, 1)]] RWTexture2DMS<float4> gBindlessStorageImage2DMS;
