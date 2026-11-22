#pragma once
#include "gpu_utils.h"
#include "bindgroup_manager.h"
#include "vertex_layout.hpp"
#include "webgpu/webgpu_cpp.h"
#include "mbtiles_reader.h"
#include <cstdint>

class CubeSphere
{
public:
    CubeSphere() = default;
    void init(wgpu::Device& device, float radius, uint32_t resolution, wgpu::Buffer& uniformBuffer);
    void render(wgpu::RenderPassEncoder& pass);
    wgpu::BindGroupLayout getBindGroupLayout(){ return earthBindGroup.getLayout(); }
    const wgpu::VertexBufferLayout* getVertexLayout(){ return vertexLayout.getLayout();}

    uint32_t getIndexCount() const { return indexCount; }
private:
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    wgpu::Buffer instanceBuffer;
    BindGroupManager earthBindGroup;
    gpuUtils::TextureResource textureArray;
    DynamicVertexLayout vertexLayout;
    MBTilesReader tileReader;

    uint32_t indexCount = 0;
    uint32_t instanceCount = 0;

    void generateFace(const glm::vec3& localUp, float radius, uint32_t resolution, std::vector<gpuUtils::VertexAttributes>& vertices, std::vector<uint32_t>& indices);
    static glm::vec3 cubeToSphere(const glm::vec3& p, float radius);
    static glm::vec2 calculateUV(const glm::vec3& normal);
};
