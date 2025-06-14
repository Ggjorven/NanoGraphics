#pragma once

#include "NanoGraphics/Core/Information.hpp"

#include "NanoGraphics/Renderer/DeviceSpec.hpp"

#include "NanoGraphics/Platform/Vulkan/Vulkan.hpp"
#include "NanoGraphics/Platform/Vulkan/VulkanContext.hpp"

#include <Nano/Nano.hpp>

namespace Nano::Graphics
{
    class Swapchain;
    class Image;
    class StagingImage;
    class Sampler;
    class InputLayout;
    class BindingLayout;
    class BindingSetPool;
    class Buffer;
    class Framebuffer;
    class Renderpass;
    class Shader;
    class Pipeline;
}

namespace Nano::Graphics::Internal
{

    ////////////////////////////////////////////////////////////////////////////////////
    // VulkanDevice
    ////////////////////////////////////////////////////////////////////////////////////
    class VulkanDevice : public Traits::NoMove
    {
    public:
        // Constructors & Destructor
        VulkanDevice(const DeviceSpecification& specs);
        ~VulkanDevice();

        // Methods
        void Wait() const;

        // Destruction methods
        void DestroySwapchain(Swapchain& swapchain) const;

        void DestroyImage(Image& image) const;
        void DestroySubresourceViews(Image& image) const;
        void DestroyStagingImage(StagingImage& stagingImage) const;
        void DestroySampler(Sampler& sampler) const;

        void DestroyBuffer(Buffer& buffer) const;

        void DestroyFramebuffer(Framebuffer& framebuffer) const;
        void DestroyRenderpass(Renderpass& renderpass) const;

        void DestroyShader(Shader& shader) const;

        void DestroyInputLayout(InputLayout& layout) const;
        void DestroyBindingLayout(BindingLayout& layout) const;

        void FreeBindingSetPool(BindingSetPool& pool) const;

        void DestroyGraphicsPipeline(GraphicsPipeline& pipeline) const;

        // Internal Getters
        inline const VulkanContext& GetContext() const { return m_Context; }
        inline const VulkanAllocator& GetAllocator() const { return m_Allocator; }

    private:
        VulkanContext m_Context;
        VulkanAllocator m_Allocator;
    };

}