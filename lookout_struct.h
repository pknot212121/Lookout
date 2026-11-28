#pragma once
#include "webgpu/webgpu_cpp.h"
#include <glm/glm.hpp>

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

struct TileBounds {
    glm::dvec3 centerDir;
    double angularRadius;
};