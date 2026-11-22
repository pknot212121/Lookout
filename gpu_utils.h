#pragma once
#include <iostream>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <filesystem>
#include <tiny_obj_loader.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

#include <emscripten.h>
#include <emscripten/html5.h>

namespace gpuUtils
{
    struct TextureResource
    {
        wgpu::Texture texture;
        wgpu::TextureView view;
        wgpu::Sampler sampler;
    };

    struct VertexAttributes
    {
        glm::vec3 position;
        glm::vec3 normal;
        glm::vec2 uv;
    };

    struct TileInstanceAttributes {
        uint32_t z, x, y, layer;
    };

    struct MyUniforms
    {
        glm::mat4x4 projectionMatrix;
        glm::mat4x4 viewMatrix;
    };
    static_assert(sizeof(MyUniforms) % 16 == 0);

    TextureResource createTextureFromBytes(
        const wgpu::Device& device,
        const uint8_t* data,
        uint32_t width,
        uint32_t height,
        wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm
    );

    TextureResource createTextureArray(
        const wgpu::Device& device,
        uint32_t width,
        uint32_t height,
        uint32_t layerCount,
        wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm
    );

    void writeTextureLayer(
        const wgpu::Device& device,
        const wgpu::Texture& textureArray,
        uint32_t layer,
        const uint8_t* data,
        uint32_t width,
        uint32_t height
    );

    TextureResource loadTextureFromFile(
        wgpu::Device& device,
        const std::string& filepath
    );

    wgpu::ShaderModule createShaderModule(
        const wgpu::Device& device,
        std::string_view label,
        std::string_view src
    );

    wgpu::ShaderModule loadShaderModule(
        const std::filesystem::path& path,
        wgpu::Device device
    );

    wgpu::Buffer createBuffer(
        const wgpu::Device& device,
        std::string_view label,
        uint64_t size_in_bytes,
        wgpu::BufferUsage usage
    );

    void glfwError (int code, const char* message);

    void adapterRequest(
        wgpu::RequestAdapterStatus status,
        wgpu::Adapter adapter,
        wgpu::StringView message,
        wgpu::Adapter* data
    );

    void deviceLost(
        [[maybe_unused]] const wgpu::Device& device,
        wgpu::DeviceLostReason reason,
        struct wgpu::StringView message
    );

    void uncapturedError ([[maybe_unused]] const wgpu::Device& device, wgpu::ErrorType type, struct wgpu::StringView message);

}
