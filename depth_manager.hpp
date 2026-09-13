#pragma once
#include <webgpu/webgpu.h>
#include <webgpu/webgpu_cpp.h>

class DepthManager
{
    public:
        DepthManager(wgpu::TextureFormat depthTextureFormat, wgpu::Device device, uint32_t kWidth, uint32_t kHeight)
        {
            wgpu::TextureDescriptor depthTextureDesc {
                .usage = wgpu::TextureUsage::RenderAttachment,
                .dimension = wgpu::TextureDimension::e2D,
                .size = {kWidth, kHeight, 1},
                .format = depthTextureFormat,
                .mipLevelCount = 1,
                .sampleCount = 1,
                .viewFormatCount = 1,
                .viewFormats = (wgpu::TextureFormat*)&depthTextureFormat,
            };

            depthTexture = device.CreateTexture(&depthTextureDesc);
            depthTextureView = depthTexture.CreateView();

            depthAttachment = {
                .view = depthTextureView,
                .depthLoadOp = wgpu::LoadOp::Clear,
                .depthStoreOp = wgpu::StoreOp::Store,
                .depthClearValue = 1.0f,
                .stencilLoadOp = wgpu::LoadOp::Undefined,
                .stencilStoreOp = wgpu::StoreOp::Undefined,
                .stencilClearValue = 0,
            };

            depthStencilState = {
                .format = depthTextureFormat,
                .depthWriteEnabled = true,
                .depthCompare = wgpu::CompareFunction::Less,
                .stencilReadMask = 0,
                .stencilWriteMask = 0,
            };
        }
        const wgpu::RenderPassDepthStencilAttachment* getDepthAttachment() const
        {
            return &depthAttachment;
        }
        const wgpu::DepthStencilState* getDepthStencilState() const
        {
            return &depthStencilState;
        }
    private:
        wgpu::RenderPassDepthStencilAttachment depthAttachment;
        wgpu::TextureView depthTextureView;
        wgpu::Texture depthTexture;
        wgpu::DepthStencilState depthStencilState;
};