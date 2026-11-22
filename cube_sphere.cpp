#include "cube_sphere.h"
#include "glm/fwd.hpp"
#include "gpu_utils.h"
#include "vertex_layout.hpp"
#include "webgpu/webgpu_cpp.h"
#include <cstdint>
#include <vector>
#include <stb_image.h>

constexpr uint32_t RESOLUTION = 33;
constexpr uint32_t MAX_TILES = 256;
constexpr uint32_t TEX_SIZE = 256;
constexpr uint32_t TEST_ZOOM = 3;
constexpr uint32_t TILES_PER_SIDE = 1u << TEST_ZOOM;

void CubeSphere::init(wgpu::Device& device, float radius, uint32_t resolution, wgpu::Buffer& uniformBuffer)
{
    std::vector<glm::vec2> vertices;
    std::vector<uint32_t> indices;

    // This is a base grid template used by every tile
    for (uint32_t i=0; i<RESOLUTION; i++)
    {
        for (uint32_t j=0; j<RESOLUTION; j++)
        {
            // Each vertex is a relative position on a tile
            vertices.push_back({i/float(RESOLUTION-1),j/float(RESOLUTION-1)});
        }
    }

    // Index buffer for simple quads between the vertices
    for (uint32_t i=0; i<32; i++)
    {
        for (uint32_t j=0; j<32; j++)
        {
            uint32_t i0 = j + i * RESOLUTION;
            uint32_t i1 = j + 1 + i * RESOLUTION;
            uint32_t i2 = j + (i+1) * RESOLUTION;
            uint32_t i3 = j + 1 + (i+1) * RESOLUTION;
            indices.push_back(i0);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
    
    // Instance buffer is needed for tile data parameters in a final texture
    instanceBuffer = gpuUtils::createBuffer(
        device, 
        "earth_instance_buffer", 
        MAX_TILES * sizeof(gpuUtils::TileInstanceAttributes), 
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst
    );
    textureArray = gpuUtils::createTextureArray(device, TEX_SIZE, TEX_SIZE, MAX_TILES);

    vertexLayout = DynamicVertexLayout({2});

    earthBindGroup.addBuffer(0, uniformBuffer, sizeof(gpuUtils::MyUniforms), wgpu::BufferBindingType::Uniform);
    earthBindGroup.addBuffer(1, instanceBuffer, MAX_TILES * sizeof(gpuUtils::TileInstanceAttributes), wgpu::BufferBindingType::ReadOnlyStorage, wgpu::ShaderStage::Vertex);
    earthBindGroup.addTexture(2, textureArray.view, wgpu::TextureSampleType::Float, wgpu::TextureViewDimension::e2DArray);
    earthBindGroup.addSampler(3, textureArray.sampler);
    earthBindGroup.build(device);

    std::vector<gpuUtils::TileInstanceAttributes> tileData;

    for (uint32_t y = 0; y < TILES_PER_SIDE; y++)
    {
        for (uint32_t x = 0; x < TILES_PER_SIDE; x++)
        {
            tileData.push_back({TEST_ZOOM, x, y, y * TILES_PER_SIDE + x});
            tileReader.requestTile(TEST_ZOOM, x, y, 
                [device, this](uint32_t tz, uint32_t tx, uint32_t ty, std::vector<uint8_t> bytes)
                {
                    int w, h, ch;
                    stbi_uc* px = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &ch, 4);
                    if (!px)
                    {
                        std::cerr << "decode failed" << tz << "/" << tx << "/" << ty << "\n";
                        return;
                    }
                    if (w != TEX_SIZE || h != TEX_SIZE)
                    {
                        std::cerr << "unexpected tile size" << w << "x" << h << "\n";
                        stbi_image_free(px);
                        return;
                    }

                    uint32_t layer = ty * TILES_PER_SIDE + tx;
                    gpuUtils::writeTextureLayer(device, textureArray.texture, layer, px, w, h);
                    stbi_image_free(px);
                }
            );
        }
    }

    vertexBuffer = gpuUtils::createBuffer(device, "cube_sphere_vertex_buffer", vertices.size() * sizeof(glm::vec2), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(glm::vec2));

    indexBuffer = gpuUtils::createBuffer(device, "cube_sphere_index_buffer", indices.size() * sizeof(uint32_t), wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(indexBuffer, 0, indices.data(), indices.size() * sizeof(uint32_t));
    indexCount = static_cast<uint32_t>(indices.size());

    device.GetQueue().WriteBuffer(instanceBuffer, 0, tileData.data(), sizeof(gpuUtils::TileInstanceAttributes) * tileData.size());
    instanceCount = tileData.size();
}

void CubeSphere::render(wgpu::RenderPassEncoder& pass)
{
    earthBindGroup.bind(pass, 0);
    pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
    pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32, 0, indexBuffer.GetSize());
    pass.DrawIndexed(indexCount, instanceCount, 0, 0, 0);
}