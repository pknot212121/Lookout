#pragma once
#include "bindgroup_manager.h"
#include "vertex_layout.hpp"
#include "webgpu/webgpu_cpp.h"
#include "mbtiles_reader.h"
#include <cstdint>
#include "lookout_define.h"
#include "lookout_struct.h"
#include "tile_cache.h"

class CubeSphere
{
public:
    CubeSphere() = default;
    void init(wgpu::Device& device, float radius, uint32_t resolution, wgpu::Buffer& uniformBuffer);
    void render(wgpu::RenderPassEncoder& pass, wgpu::Device& device, glm::vec3 camPos);
    wgpu::BindGroupLayout getBindGroupLayout(){ return earthBindGroup.getLayout(); }
    const wgpu::VertexBufferLayout* getVertexLayout(){ return vertexLayout.getLayout();}

    uint32_t getIndexCount() const { return indexCount; }
private:
    wgpu::Buffer vertexBuffer;
    wgpu::Buffer indexBuffer;
    wgpu::Buffer instanceBuffer;
    BindGroupManager earthBindGroup;
    TextureResource textureArray;
    DynamicVertexLayout vertexLayout;
    MBTilesReader tileReader;
    TileCache cache;

    uint32_t indexCount = 0;
    uint32_t instanceCount = 0;
};
