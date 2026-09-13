#pragma once
#include "gpu_utils.h"

class CubeSphere
{
public:
    CubeSphere() = default;
    void init(wgpu::Device& device, float radius, uint32_t resolution);
    void render(wgpu::RenderPassEncoder& pass);
    uint32_t getIndexCount() const { return indexCount; }
private:
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    uint32_t indexCount = 0;

    void generateFace(const glm::vec3& localUp, float radius, uint32_t resolution, std::vector<gpuUtils::VertexAttributes>& vertices, std::vector<uint32_t>& indices);
    static glm::vec3 cubeToSphere(const glm::vec3& p, float radius);
    static glm::vec2 calculateUV(const glm::vec3& normal);
};
