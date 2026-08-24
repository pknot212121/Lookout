#include <webgpu/webgpu_cpp.h>
#include <cstdint>


class BindGroupManager
{
    public:
        struct BufferEntry
        {
            uint32_t binding;
            wgpu::Buffer buffer;
            uint64_t size;
            wgpu::BufferBindingType type;
            wgpu::ShaderStage visibility;
        };

        struct TextureEntry
        {
            uint32_t binding;
            wgpu::TextureView view;
            wgpu::TextureSampleType sampleType;
            wgpu::TextureViewDimension viewDimension;
            wgpu::ShaderStage visibility;
        };

        struct SamplerEntry
        {
            uint32_t binding;
            wgpu::Sampler sampler;
            wgpu::SamplerBindingType type;
            wgpu::ShaderStage visibility;
        };

        BindGroupManager& addBuffer(
            uint32_t binding,
            wgpu::Buffer buffer,
            uint64_t size,
            wgpu::BufferBindingType type,
            wgpu::ShaderStage visibility = wgpu::ShaderStage::Vertex | wgpu::ShaderStage::Fragment)
        {
            bufferEntries.push_back({binding, buffer, size, type, visibility});
            return *this;
        }

        BindGroupManager& addTexture(
            uint32_t binding,
            wgpu::TextureView view,
            wgpu::TextureSampleType sampleType = wgpu::TextureSampleType::Float,
            wgpu::TextureViewDimension viewDimension = wgpu::TextureViewDimension::e2D,
            wgpu::ShaderStage visibility = wgpu::ShaderStage::Fragment)
        {
            textureEntries.push_back({binding, view, sampleType, viewDimension, visibility});
            return *this;
        }

        BindGroupManager& addSampler(
            uint32_t binding,
            wgpu::Sampler sampler,
            wgpu::SamplerBindingType type = wgpu::SamplerBindingType::Filtering,
            wgpu::ShaderStage visibility = wgpu::ShaderStage::Fragment)
        {
            samplerEntries.push_back({binding, sampler, type, visibility});
            return *this;
        }

        void build(wgpu::Device device)
        {
            size_t totalCount = bufferEntries.size() + textureEntries.size() + samplerEntries.size();
            
            std::vector<wgpu::BindGroupLayoutEntry> layoutEntries;
            std::vector<wgpu::BindGroupEntry> groupEntries;
            layoutEntries.reserve(totalCount);
            groupEntries.reserve(totalCount);

            for (const auto& entry : bufferEntries)
            {
                layoutEntries.push_back(wgpu::BindGroupLayoutEntry {
                    .binding = entry.binding,
                    .visibility = entry.visibility,
                    .buffer = {
                        .type = entry.type,
                        .minBindingSize = 0,
                    }
                });
                groupEntries.push_back(wgpu::BindGroupEntry {
                    .binding = entry.binding,
                    .buffer = entry.buffer,
                    .offset = 0,
                    .size = entry.size,
                });
            }

            for (const auto& entry : textureEntries)
            {
                layoutEntries.push_back(wgpu::BindGroupLayoutEntry {
                    .binding = entry.binding,
                    .visibility = entry.visibility,
                    .texture = {
                        .sampleType = entry.sampleType,
                        .viewDimension = entry.viewDimension,
                    }
                });
                groupEntries.push_back(wgpu::BindGroupEntry {
                    .binding = entry.binding,
                    .textureView = entry.view,
                });
            }

            for (const auto& entry : samplerEntries)
            {
                layoutEntries.push_back(wgpu::BindGroupLayoutEntry {
                    .binding = entry.binding,
                    .visibility = entry.visibility,
                    .sampler = {
                        .type = entry.type,
                    }
                });
                groupEntries.push_back(wgpu::BindGroupEntry {
                    .binding = entry.binding,
                    .sampler = entry.sampler,
                });
            }

            wgpu::BindGroupLayoutDescriptor layoutDesc {
                .entryCount = static_cast<uint32_t>(layoutEntries.size()),
                .entries = layoutEntries.data(),
            };
            layout = device.CreateBindGroupLayout(&layoutDesc);
            
            wgpu::BindGroupDescriptor groupDesc {
                .layout = layout,
                .entryCount = static_cast<uint32_t>(groupEntries.size()),
                .entries = groupEntries.data(),
            };
            bindGroup = device.CreateBindGroup(&groupDesc);
        }

        void bind(wgpu::RenderPassEncoder pass, uint32_t groupIndex) const 
        {
            pass.SetBindGroup(groupIndex, bindGroup);
        }

        wgpu::BindGroupLayout getLayout() const {return layout;}
        wgpu::BindGroup getBindGroup() const {return bindGroup;}
        
    private:
        std::vector<BufferEntry> bufferEntries;
        std::vector<TextureEntry> textureEntries;
        std::vector<SamplerEntry> samplerEntries;
        wgpu::BindGroupLayout layout;
        wgpu::BindGroup bindGroup;
};