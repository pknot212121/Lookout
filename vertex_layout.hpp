#pragma once
#include <initializer_list>
#include <stdexcept>
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

class DynamicVertexLayout 
{
    public:
        DynamicVertexLayout(std::initializer_list<uint32_t> sizes)
        {
            uint32_t currentOffset = 0;
            uint32_t location = 0;
            attributes.reserve(sizes.size());

            for (uint32_t count : sizes)
            {
                wgpu::VertexFormat format;
                switch (count)
                {
                    case 1: format = wgpu::VertexFormat::Float32; break;
                    case 2: format = wgpu::VertexFormat::Float32x2; break;
                    case 3: format = wgpu::VertexFormat::Float32x3; break;
                    case 4: format = wgpu::VertexFormat::Float32x4; break;
                    default:
                        throw std::invalid_argument("Invalid size of vertex attribute (only 1-4)!");
                }
                attributes.push_back(wgpu::VertexAttribute {
                    .format = format,
                    .offset = currentOffset,
                    .shaderLocation = location++
                });
                currentOffset += count * sizeof(float);
            }
            layout = wgpu::VertexBufferLayout {
                .stepMode = wgpu::VertexStepMode::Vertex,
                .arrayStride = currentOffset,
                .attributeCount = static_cast<uint32_t>(attributes.size()),
                .attributes = attributes.data()
            };
        }

        const wgpu::VertexBufferLayout* getLayout() const
        {
            return &layout;
        }
    private:
        std::vector<wgpu::VertexAttribute> attributes;
        wgpu::VertexBufferLayout layout;
};