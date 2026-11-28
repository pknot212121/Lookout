#include "cube_sphere.h"
#include "glm/fwd.hpp"
#include "glm/geometric.hpp"
#include "glm/trigonometric.hpp"
#include "gpu_utils.h"
#include "lookout_define.h"
#include "tile_cache.h"
#include "vertex_layout.hpp"
#include "webgpu/webgpu_cpp.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <stb_image.h>

static glm::dvec3 mercatorToDir(float u, float v)
{
    double lon = (u - 0.5) * 2.0 * glm::pi<double>();
    double lat = glm::atan(glm::sinh(glm::pi<double>() * (1 - 2*v)));

    glm::dvec3 p = {
        glm::cos(lat)*glm::cos(lon),
        glm::sin(lat),
        -glm::cos(lat)*glm::sin(lon)
    };
    return p;
}

static TileBounds computeBounds(uint32_t z, uint32_t x, uint32_t y)
{
    double n = (1u << z);
    auto dir = [&](double fx, double fy) {
        return mercatorToDir((x+fx)/n, (y+fy)/n);
    };

    TileBounds b;
    b.centerDir = glm::normalize(dir(0.5,0.5));
    
    const double s[] = {0.0, 0.5, 1.0};
    double minCos = 1.0;
    for (double fy : s)
    {
        for (double fx : s)
        {
            // we check minimal arc because we do not know the rotation of the tile an we have to be sure
            // that it does not have to be rendered
            minCos = std::min(minCos, glm::dot(b.centerDir, dir(fx,fy)));
        }
    }

    b.angularRadius = std::acos(std::clamp(minCos, -1.0, 1.0));
    return b;
}

static bool isBelowHorizon(const TileBounds& b, glm::dvec3 camPos, double R)
{
    // Distance of the camera from center of the earth
    double d = glm::length(camPos);
    if (d <= R) return false;
    double angle = std::acos(std::clamp(glm::dot(b.centerDir, camPos / d), -1.0, 1.0));
    return angle > std::acos(R / d) + b.angularRadius;
}

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
        MAX_TILES * sizeof(TileInstanceAttributes), 
        wgpu::BufferUsage::Storage | wgpu::BufferUsage::CopyDst
    );
    textureArray = gpuUtils::createTextureArray(device, TEX_SIZE, TEX_SIZE, MAX_TILES);

    vertexLayout = DynamicVertexLayout({2});

    earthBindGroup.addBuffer(0, uniformBuffer, sizeof(MyUniforms), wgpu::BufferBindingType::Uniform);
    earthBindGroup.addBuffer(1, instanceBuffer, MAX_TILES * sizeof(TileInstanceAttributes), wgpu::BufferBindingType::ReadOnlyStorage, wgpu::ShaderStage::Vertex);
    earthBindGroup.addTexture(2, textureArray.view, wgpu::TextureSampleType::Float, wgpu::TextureViewDimension::e2DArray);
    earthBindGroup.addSampler(3, textureArray.sampler);
    earthBindGroup.build(device);

    vertexBuffer = gpuUtils::createBuffer(device, "cube_sphere_vertex_buffer", vertices.size() * sizeof(glm::vec2), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(glm::vec2));

    indexBuffer = gpuUtils::createBuffer(device, "cube_sphere_index_buffer", indices.size() * sizeof(uint32_t), wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(indexBuffer, 0, indices.data(), indices.size() * sizeof(uint32_t));
    indexCount = static_cast<uint32_t>(indices.size());

    cache = TileCache(
        device,
        textureArray.texture,
        MAX_TILES,
        TEX_SIZE,
        [this](const TileKey& k, TileCache::FetchResult r)
        {
            tileReader.requestTile(k.z, k.x, k.y, std::move(r));
        } 
    );
}

void CubeSphere::render(wgpu::RenderPassEncoder& pass, wgpu::Device& device, glm::vec3 cameraPos)
{
    earthBindGroup.bind(pass, 0);
    pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
    pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32, 0, indexBuffer.GetSize());

    cache.beginFrame();
    for (uint32_t y = 0; y < TILES_PER_SIDE; y++)
    {
        for (uint32_t x = 0; x < TILES_PER_SIDE; x++)
        {
            TileBounds b = computeBounds(TEST_ZOOM, x, y);
            if (isBelowHorizon(b, {cameraPos.x, cameraPos.y, -cameraPos.z}, PLANET_RADIUS))
            {
                continue;
            }
            
            cache.request(TileKey(TEST_ZOOM, x, y), TEST_ZOOM);
        }
    }

    cache.update();
    std::vector<TileInstanceAttributes> tileData = cache.getInstances(MAX_TILES);
    std::cout << "TILEDATA SIZE: " << tileData.size() << std::endl;
    device.GetQueue().WriteBuffer(instanceBuffer, 0, tileData.data(), sizeof(TileInstanceAttributes) * tileData.size());
    instanceCount = tileData.size();

    pass.DrawIndexed(indexCount, instanceCount, 0, 0, 0);
}