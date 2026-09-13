#include "cube_sphere.h"

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

            glm::vec3 pointOnSphere = cubeToSphere(pointOnCube, radius);
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

void CubeSphere::init(wgpu::Device& device, float radius, uint32_t resolution)
{
    std::vector<gpuUtils::VertexAttributes> vertices;
    std::vector<uint32_t> indices;

    generateFace(glm::vec3(0, 1, 0), radius, resolution, vertices, indices); // Top
    generateFace(glm::vec3(0, -1, 0), radius, resolution, vertices, indices); // Bottom
    generateFace(glm::vec3(1, 0, 0), radius, resolution, vertices, indices); // Right
    generateFace(glm::vec3(-1, 0, 0), radius, resolution, vertices, indices); // Left
    generateFace(glm::vec3(0, 0, 1), radius, resolution, vertices, indices); // Front
    generateFace(glm::vec3(0, 0, -1), radius, resolution, vertices, indices); // Back

    indexCount = static_cast<uint32_t>(indices.size());

    vertexBuffer = gpuUtils::createBuffer(device, "cube_sphere_vertex_buffer", vertices.size() * sizeof(gpuUtils::VertexAttributes), wgpu::BufferUsage::Vertex);
    device.GetQueue().WriteBuffer(vertexBuffer, 0, vertices.data(), vertices.size() * sizeof(gpuUtils::VertexAttributes));

    indexBuffer = gpuUtils::createBuffer(device, "cube_sphere_index_buffer", indices.size() * sizeof(uint32_t), wgpu::BufferUsage::Index);
    device.GetQueue().WriteBuffer(indexBuffer, 0, indices.data(), indices.size() * sizeof(uint32_t));
}

void CubeSphere::render(wgpu::RenderPassEncoder& pass)
{
    pass.SetVertexBuffer(0, vertexBuffer, 0, vertexBuffer.GetSize());
    pass.SetIndexBuffer(indexBuffer, wgpu::IndexFormat::Uint32, 0, indexBuffer.GetSize());
    pass.DrawIndexed(indexCount, 1, 0, 0, 0);
}