#include "gpu_utils.h"
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <stb_image.h>

namespace gpuUtils
{
    TextureResource createTextureFromBytes(const wgpu::Device& device, const uint8_t* data, int width, int height, wgpu::TextureFormat format)
    {
        if (!data || width <= 0 || height <= 0)
        {
            std::cerr << "[Texture creator] Texture data is invalid" << std::endl;
            return {};
        }

        uint32_t uWidth = static_cast<uint32_t>(width);
        uint32_t uHeight = static_cast<uint32_t>(height);
        wgpu::TextureDescriptor textureDesc {
            .label = "Model Texture",
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .dimension = wgpu::TextureDimension::e2D,
            .size = { uWidth, uHeight, 1 },
            .format = format,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };
        wgpu::Texture texture = device.CreateTexture(&textureDesc);
        wgpu::TexelCopyTextureInfo destination {
            .texture = texture,
            .mipLevel = 0,
            .origin = { 0, 0, 0 },
            .aspect = wgpu::TextureAspect::All,
        };
        wgpu::TexelCopyBufferLayout source {
            .offset = 0,
            .bytesPerRow = 4 * uWidth,
            .rowsPerImage = uHeight,
        };
        wgpu::Extent3D writeSize {
            .width = uWidth,
            .height = uHeight,
            .depthOrArrayLayers = 1,
        };
        device.GetQueue().WriteTexture(
            &destination, 
            data, 
            static_cast<size_t>(uWidth * uHeight * 4), 
            &source, 
            &writeSize
        );
        wgpu::TextureViewDescriptor viewDesc {
            .label = "Model Texture View",
            .format = format,
            .dimension = wgpu::TextureViewDimension::e2D,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = 1,
            .aspect = wgpu::TextureAspect::All,
        };
        wgpu::TextureView view = texture.CreateView(&viewDesc);
        wgpu::SamplerDescriptor samplerDesc {
            .label = "Model Texture Sampler",
            .addressModeU = wgpu::AddressMode::Repeat,
            .addressModeV = wgpu::AddressMode::Repeat,
            .addressModeW = wgpu::AddressMode::Repeat,
            .magFilter = wgpu::FilterMode::Linear,
            .minFilter = wgpu::FilterMode::Linear,
            .mipmapFilter = wgpu::MipmapFilterMode::Linear,
        };
        wgpu::Sampler sampler = device.CreateSampler(&samplerDesc);
        return { texture, view, sampler };
    }

    TextureResource loadTextureFromFile(wgpu::Device& device, const std::string& filepath)
    {
        int width = 0, height = 0, channels = 0;

        unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 4);

        if (!data)
        {
            std::cerr << "[Texture Error] Failed to load image: " << filepath << std::endl;
            std::cerr << "STB Reason: " << stbi_failure_reason() << std::endl;
            return decltype(createTextureFromBytes(device, nullptr, 0, 0)){};
        }

        auto texture = createTextureFromBytes(device, data, width, height);
        stbi_image_free(data);

        return texture;
    }

    wgpu::ShaderModule createShaderModule(const wgpu::Device& device, std::string_view label, std::string_view src)
    {
        wgpu::ShaderSourceWGSL wgslDesc;
        wgslDesc.code = src;

        wgpu::ShaderModuleDescriptor desc{
            .nextInChain = &wgslDesc,
            .label = label,
        };

        return device.CreateShaderModule(&desc);
    }

    wgpu::ShaderModule loadShaderModule(const std::filesystem::path& path, wgpu::Device device)
    {
        std::ifstream file(path);
        if (!file.is_open()) {
            return nullptr;
        }
        file.seekg(0, std::ios::end);
        size_t size = file.tellg();
        std::string shaderSource(size, ' ');
        file.seekg(0);
        file.read(shaderSource.data(), size);
        return createShaderModule(device, "shader from file", shaderSource);
    }

    wgpu::Buffer createBuffer(const wgpu::Device& device, std::string_view label, uint64_t size_in_bytes, wgpu::BufferUsage usage)
    {
        wgpu::BufferDescriptor desc{
            .label = label,
            .usage = usage | wgpu::BufferUsage::CopyDst,
            .size = size_in_bytes,
        };
        return device.CreateBuffer(&desc);
    }
}