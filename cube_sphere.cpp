#include "cube_sphere.h"
#include "glm/fwd.hpp"
#include "gpu_utils.h"
#include "webgpu/webgpu_cpp.h"
#include <cstdint>

glm::vec3 CubeSphere::cubeToSphere(const glm::vec3& p, float radius)
{
    float x2 = p.x * p.x;
    float y2 = p.y * p.y;
    float z2 = p.z * p.z;

    glm::vec3 s;
    s.x = p.x * std::sqrt(1.0f - (y2 * 0.5f) - (z2 * 0.5f) + ((y2 * z2) / 3.0f));
    s.y = p.y * std::sqrt(1.0f - (z2 * 0.5f) - (x2 * 0.5f) + ((z2 * x2) / 3.0f));
    s.z = p.z * std::sqrt(1.0f - (x2 * 0.5f) - (y2 * 0.5f) + ((x2 * y2) / 3.0f));

    return s * radius;
}

glm::vec2 CubeSphere::calculateUV(const glm::vec3& normal)
{
    float u = 0.5f + (std::atan2(normal.z, normal.x) / (2.0f * glm::pi<float>()));
    float v = 0.5f - (std::asin(normal.y) / glm::pi<float>());
    return { u, v };
}

void CubeSphere::generateFace(const glm::vec3& localUp, float radius, uint32_t resolution,
                              std::vector<gpuUtils::VertexAttributes>& vertices, std::vector<uint32_t>& indices)
{
    glm::vec3 axisA = glm::vec3(localUp.y, localUp.z, localUp.x);
    glm::vec3 axisB = glm::cross(localUp, axisA);

    uint32_t vertexOffset = static_cast<uint32_t>(vertices.size());

    for (uint32_t y = 0; y < resolution; ++y)
    {
        for (uint32_t x = 0; x < resolution; ++x)
        {
            glm::vec2 percent = glm::vec2(x, y) / static_cast<float>(resolution - 1);
            glm::vec3 pointOnCube = localUp + (percent.x - 0.5f) * 2.0f * axisA 
                                            + (percent.y - 0.5f) * 2.0f * axisB;

            glm::vec3 pointOnSphere = pointOnCube * radius;
            glm::vec3 normal = glm::normalize(pointOnSphere);
            glm::vec2 uv = calculateUV(normal);

            vertices.push_back({ pointOnSphere, normal, uv });
        }
    }

    for (uint32_t y = 0; y < resolution - 1; ++y)
    {
        for (uint32_t x = 0; x < resolution - 1; ++x)
        {
            uint32_t i0 = vertexOffset + x + y * resolution;
            uint32_t i1 = vertexOffset + (x + 1) + y * resolution;
            uint32_t i2 = vertexOffset + x + (y + 1) * resolution;
            uint32_t i3 = vertexOffset + (x + 1) + (y + 1) * resolution;

            float u0 = vertices[i0].uv.x;
            float u1 = vertices[i1].uv.x;
            float u2 = vertices[i2].uv.x;
            float u3 = vertices[i3].uv.x;

            if (std::abs(u0 - u1) > 0.5f || std::abs(u0 - u2) > 0.5f || std::abs(u0 - u3) > 0.5f)
            {
                if (vertices[i0].uv.x < 0.5f) vertices[i0].uv.x += 1.0f;
                if (vertices[i1].uv.x < 0.5f) vertices[i1].uv.x += 1.0f;
                if (vertices[i2].uv.x < 0.5f) vertices[i2].uv.x += 1.0f;
                if (vertices[i3].uv.x < 0.5f) vertices[i3].uv.x += 1.0f;
            }
            indices.push_back(i0);
            indices.push_back(i2);
            indices.push_back(i1);

            indices.push_back(i1);
            indices.push_back(i2);
            indices.push_back(i3);
        }
    }
} 

constexpr uint32_t RESOLUTION = 33;

void CubeSphere::init(wgpu::Device& device, float radius, uint32_t resolution)
{
    std::vector<glm::vec2> vertices;
    std::vector<uint32_t> indices;

    // This is a base grid template used by every tile
    for (uint32_t i=0; i<RESOLUTION; i++)
    {
        for (uint32_t j=0; j<RESOLUTION; j++)
        {
            // Each vertex is a relative position on a tile
            vertices.push_back({i/(RESOLUTION-1),j/(RESOLUTION-1)});
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
    
    



    indexCount = static_cast<uint32_t>(indices.size());

    vertexBuffer = gpuUtils::createBuffer(device, "cube_sphere_vertex_buffer", vertices.size() * sizeof(glm::vec2), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(glm::vec2));

    indexBuffer = gpuUtils::createBuffer(device, "cube_sphere_index_buffer", indices.size() * sizeof(uint32_t), wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(indexBuffer, 0, indices.data(), indices.size() * sizeof(uint32_t));
}

void CubeSphere::render(wgpu::RenderPassEncoder& pass)
{
    pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
    pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32, 0, indexBuffer.GetSize());
    pass.DrawIndexed(indexCount, 1, 0, 0, 0);
}