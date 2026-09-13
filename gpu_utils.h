#pragma once
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

    TextureResource createTextureFromBytes(
        const wgpu::Device& device,
        const uint8_t* data,
        int width,
        int height,
        wgpu::TextureFormat format = wgpu::TextureFormat::RGBA8Unorm
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

}
