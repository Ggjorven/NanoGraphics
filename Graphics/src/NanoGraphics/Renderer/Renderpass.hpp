#pragma once

#include "NanoGraphics/Core/Information.hpp"

#include "NanoGraphics/Renderer/RenderpassSpec.hpp"

#include "NanoGraphics/Platform/Vulkan/VulkanRenderpass.hpp"

#include <Nano/Nano.hpp>

namespace Nano::Graphics
{

    class Device;

    ////////////////////////////////////////////////////////////////////////////////////
    // Renderpass
    ////////////////////////////////////////////////////////////////////////////////////
    class Renderpass
    {
    public:
        using Type = Types::SelectorType<Information::RenderingAPI,
            Types::EnumToType<Information::Structs::RenderingAPI::Vulkan, Internal::VulkanRenderpass>
        >;
    public:
        // Destructor
        ~Renderpass() = default;

        // Methods
        inline Framebuffer& CreateFramebuffer(const FramebufferSpecification& specs) { return m_Renderpass.CreateFramebuffer(specs); } // Note: Framebuffers are stored in the Renderpass and will be destroyed when the renderpass is.

        inline void ResizeFramebuffers() { return m_Renderpass.ResizeFramebuffers(); }

        // Getters
        inline const RenderpassSpecification& GetSpecification() const { return m_Renderpass.GetSpecification(); }

        inline Framebuffer& GetFramebuffer(uint8_t frame) { return m_Renderpass.GetFramebuffer(frame); }

    private:
        // Constructor
        Renderpass(const Device& device, const RenderpassSpecification& specs)
            : m_Renderpass(device, specs) {}

    private:
        Type m_Renderpass;

        friend class Device;
    };

}