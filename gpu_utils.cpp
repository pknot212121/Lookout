#include "gpu_utils.h"
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <stb_image.h>

namespace gpuUtils
{
    TextureResource createTextureFromBytes(const wgpu::Device& device, const uint8_t* data, uint32_t width, uint32_t height, wgpu::TextureFormat format)
    {
        if (!data)
        {
            std::cerr << "[Texture creator] Texture data is invalid" << std::endl;
            return {};
        }

        wgpu::TextureDescriptor textureDesc {
            .label = "Model Texture",
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .dimension = wgpu::TextureDimension::e2D,
            .size = { width, height, 1 },
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
            .bytesPerRow = 4 * width,
            .rowsPerImage = height,
        };
        wgpu::Extent3D writeSize {
            .width = width,
            .height = height,
            .depthOrArrayLayers = 1,
        };
        device.GetQueue().WriteTexture(
            &destination, 
            data, 
            static_cast<size_t>(width * height * 4), 
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

    TextureResource createTextureArray(const wgpu::Device& device, uint32_t width, uint32_t height, uint32_t layerCount, wgpu::TextureFormat format)
    {
        wgpu::TextureDescriptor textureDesc {
            .label = "Model Texture",
            .usage = wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst,
            .dimension = wgpu::TextureDimension::e2D,
            .size = { width, height, layerCount},
            .format = format,
            .mipLevelCount = 1,
            .sampleCount = 1,
        };
        wgpu::Texture texture = device.CreateTexture(&textureDesc);
        wgpu::TextureViewDescriptor viewDesc {
            .label = "Model Texture View",
            .format = format,
            .dimension = wgpu::TextureViewDimension::e2DArray,
            .baseMipLevel = 0,
            .mipLevelCount = 1,
            .baseArrayLayer = 0,
            .arrayLayerCount = layerCount,
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

    void writeTextureLayer(const wgpu::Device& device, const wgpu::Texture& textureArray, uint32_t layer, const uint8_t* data, uint32_t width, uint32_t height)
    {
        wgpu::TexelCopyTextureInfo destination {
            .texture = textureArray,
            .mipLevel = 0,
            .origin = { 0, 0, layer },
            .aspect = wgpu::TextureAspect::All,
        };
        wgpu::TexelCopyBufferLayout source {
            .offset = 0,
            .bytesPerRow = 4 * width,
            .rowsPerImage = height,
        };
        wgpu::Extent3D writeSize {
            .width = width,
            .height = height,
            .depthOrArrayLayers = 1,
        };
        device.GetQueue().WriteTexture(
            &destination, 
            data, 
            static_cast<size_t>(width * height * 4), 
            &source, 
            &writeSize
        );
    }

    void clearAllLayers(const wgpu::Device& device, const wgpu::Texture& texture,
                    uint32_t layerCount, wgpu::TextureFormat format)
    {
        auto encoder = device.CreateCommandEncoder();

        for (uint32_t layer = 0; layer < layerCount; ++layer)
        {
            wgpu::TextureViewDescriptor vd {
                .format = format,
                .dimension = wgpu::TextureViewDimension::e2D,   // jedna warstwa jako zwykły 2D
                .baseMipLevel = 0,
                .mipLevelCount = 1,
                .baseArrayLayer = layer,
                .arrayLayerCount = 1,
            };
            wgpu::RenderPassColorAttachment att {
                .view = texture.CreateView(&vd),
                .loadOp = wgpu::LoadOp::Clear,
                .storeOp = wgpu::StoreOp::Store,
                .clearValue = { 0.0, 0.0, 0.0, 0.0 },
            };
            wgpu::RenderPassDescriptor rp { .colorAttachmentCount = 1, .colorAttachments = &att };
            encoder.BeginRenderPass(&rp).End();   // pusty pass wystarczy
        }

        auto cmds = encoder.Finish();
        device.GetQueue().Submit(1, &cmds);
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

    void glfwError (int code, const char* message)
    {
        std::cerr << "GLFW error: " << code << ":" << message;
        assert(false);
    }

    void adapterRequest(wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message, wgpu::Adapter* data)
    {
        if (status != wgpu::RequestAdapterStatus::Success) {
            std::cout << "Adapter request failed: " << std::string_view(message);
            exit(1);
        }
        *data = adapter;
    }

    void deviceLost([[maybe_unused]] const wgpu::Device& device, wgpu::DeviceLostReason reason, struct wgpu::StringView message)
    {
        if (message == std::string_view("A valid external Instance reference no longer exists.")) {
            return;
        }
        std::cerr << "device lost: \n";
        if (message.length > 0) {
            std::cout << ": " << std::string_view(message);
        }
        std::cout << std::endl;
    }

    void uncapturedError ([[maybe_unused]] const wgpu::Device& device, wgpu::ErrorType type, struct wgpu::StringView message)
    {
        std::cout << "uncaptured error: \n";
        if (message.length > 0)
            std::cerr << ": {}" << std::string_view(message);
        std::cout << std::endl;
        assert(false);
    }
}