#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <chrono>

#include <vector>
#include <filesystem>
#include <fstream>
#include <string>
#include <initializer_list>
#include <stdexcept>
#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include <emscripten.h>
#include <emscripten/html5.h>

using glm::mat4x4;
using glm::vec4;
using glm::vec3;
using glm::vec2;

struct Timer
{
    std::string_view name;
    std::chrono::high_resolution_clock::time_point start;

    Timer(std::string_view name) : name(name), start(std::chrono::high_resolution_clock::now()) {}
    ~Timer()
    {
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        std::cout << "[Timer] " << name << ": " << duration << " ms" << std::endl; 
    }
};

struct TextureResource
{
    wgpu::Texture texture;
    wgpu::TextureView view;
    wgpu::Sampler sampler;
};

inline TextureResource createTextureFromBytes(const wgpu::Device& device, const uint8_t* data, int width, int height, wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm)
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


struct VertexAttributes
{
    vec3 position;
    vec3 normal;
    vec2 uv;
};

class DynamicVertexLayout 
{
    public:
        DynamicVertexLayout(std::initializer_list<uint32_t> sizes)
        {
            uint32_t currentOffset = 0;
            uint32_t location = 0;
            attributes.reserve(sizes.size());

            for (uint32_t count : sizes)
            {
                wgpu::VertexFormat format;
                switch (count)
                {
                    case 1: format = wgpu::VertexFormat::Float32; break;
                    case 2: format = wgpu::VertexFormat::Float32x2; break;
                    case 3: format = wgpu::VertexFormat::Float32x3; break;
                    case 4: format = wgpu::VertexFormat::Float32x4; break;
                    default:
                        throw std::invalid_argument("Invalid size of vertex attribute (only 1-4)!");
                }
                attributes.push_back(wgpu::VertexAttribute {
                    .format = format,
                    .offset = currentOffset,
                    .shaderLocation = location++
                });
                currentOffset += count * sizeof(float);
            }
            layout = wgpu::VertexBufferLayout {
                .stepMode = wgpu::VertexStepMode::Vertex,
                .arrayStride = currentOffset,
                .attributeCount = static_cast<uint32_t>(attributes.size()),
                .attributes = attributes.data()
            };
        }

        const wgpu::VertexBufferLayout* getLayout() const
        {
            return &layout;
        }
    private:
        std::vector<wgpu::VertexAttribute> attributes;
        wgpu::VertexBufferLayout layout;
};

class DepthManager
{
    public:
        DepthManager(wgpu::TextureFormat depthTextureFormat, wgpu::Device device, uint32_t kWidth, uint32_t kHeight)
        {
            wgpu::TextureDescriptor depthTextureDesc {
                .usage = wgpu::TextureUsage::RenderAttachment,
                .dimension = wgpu::TextureDimension::e2D,
                .size = {kWidth, kHeight, 1},
                .format = depthTextureFormat,
                .mipLevelCount = 1,
                .sampleCount = 1,
                .viewFormatCount = 1,
                .viewFormats = (wgpu::TextureFormat*)&depthTextureFormat,
            };

            depthTexture = device.CreateTexture(&depthTextureDesc);
            depthTextureView = depthTexture.CreateView();

            depthAttachment = {
                .view = depthTextureView,
                .depthLoadOp = wgpu::LoadOp::Clear,
                .depthStoreOp = wgpu::StoreOp::Store,
                .depthClearValue = 1.0f,
                .stencilLoadOp = wgpu::LoadOp::Undefined,
                .stencilStoreOp = wgpu::StoreOp::Undefined,
                .stencilClearValue = 0,
            };

            depthStencilState = {
                .format = depthTextureFormat,
                .depthWriteEnabled = true,
                .depthCompare = wgpu::CompareFunction::Less,
                .stencilReadMask = 0,
                .stencilWriteMask = 0,
            };
        }
        const wgpu::RenderPassDepthStencilAttachment* getDepthAttachment() const
        {
            return &depthAttachment;
        }
        const wgpu::DepthStencilState* getDepthStencilState() const
        {
            return &depthStencilState;
        }
    private:
        wgpu::RenderPassDepthStencilAttachment depthAttachment;
        wgpu::TextureView depthTextureView;
        wgpu::Texture depthTexture;
        wgpu::DepthStencilState depthStencilState;
};

inline wgpu::Buffer createBuffer(const wgpu::Device& device, std::string_view label, uint64_t size_in_bytes, wgpu::BufferUsage usage)
{
    wgpu::BufferDescriptor desc{
        .label = label,
        .usage = usage | wgpu::BufferUsage::CopyDst,
        .size = size_in_bytes,
    };
    return device.CreateBuffer(&desc);
}

inline wgpu::ShaderModule createShaderModule(const wgpu::Device& device, std::string_view label, std::string_view src)
{
    wgpu::ShaderSourceWGSL wgslDesc;
    wgslDesc.code = src;

    wgpu::ShaderModuleDescriptor desc{
        .nextInChain = &wgslDesc,
        .label = label,
    };

    return device.CreateShaderModule(&desc);
}

inline wgpu::ShaderModule loadShaderModule(const std::filesystem::path& path, wgpu::Device device)
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

inline void glfwError (int code, const char* message)
{
    std::cerr << "GLFW error: " << code << ":" << message;
    assert(false);
}

inline void adapterRequest(wgpu::RequestAdapterStatus status, wgpu::Adapter adapter, wgpu::StringView message, wgpu::Adapter* data)
{
    if (status != wgpu::RequestAdapterStatus::Success) {
        std::cout << "Adapter request failed: " << std::string_view(message);
        exit(1);
    }
    *data = adapter;
}

inline void deviceLost([[maybe_unused]] const wgpu::Device& device, wgpu::DeviceLostReason reason, struct wgpu::StringView message)
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

inline void uncapturedError ([[maybe_unused]] const wgpu::Device& device, wgpu::ErrorType type, struct wgpu::StringView message)
{
    std::cout << "uncaptured error: \n";
    if (message.length > 0)
        std::cerr << ": {}" << std::string_view(message);
    std::cout << std::endl;
    assert(false);
}