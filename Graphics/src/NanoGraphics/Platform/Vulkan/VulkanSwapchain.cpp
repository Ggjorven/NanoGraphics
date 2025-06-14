#include "ngpch.h"
#include "VulkanImage.hpp"

#include "NanoGraphics/Core/Logging.hpp"
#include "NanoGraphics/Utils/Profiler.hpp"

#include "NanoGraphics/Core/Window.hpp"

#include "NanoGraphics/Renderer/Device.hpp"
#include "NanoGraphics/Renderer/Swapchain.hpp"

#include "NanoGraphics/Platform/Vulkan/VulkanDevice.hpp"

#if defined(NG_PLATFORM_DESKTOP)
    #define GLFW_INCLUDE_VULKAN
    #include <GLFW/glfw3.h>
#endif

namespace Nano::Graphics::Internal
{

    static_assert(std::is_same_v<Device::Type, VulkanDevice>, "Current Device::Type is not VulkanDevice and Vulkan source code is being compiled.");
    static_assert(std::is_same_v<Swapchain::Type, VulkanSwapchain>, "Swapchain Image::Type is not VulkanSwapchain and Vulkan source code is being compiled.");

    ////////////////////////////////////////////////////////////////////////////////////
    // Constructor & Destructor
    ////////////////////////////////////////////////////////////////////////////////////
    VulkanSwapchain::VulkanSwapchain(const Device& device, const SwapchainSpecification& specs)
        : m_Device(*reinterpret_cast<const VulkanDevice*>(&device)), m_Specification(specs)
    {
        #if defined(NG_PLATFORM_DESKTOP)
            VK_VERIFY(glfwCreateWindowSurface(m_Device.GetContext().GetVkInstance(), static_cast<GLFWwindow*>(m_Specification.WindowTarget->GetNativeWindow()), VulkanAllocator::GetCallbacks(), &m_Surface));
        #endif

        Resize(m_Specification.WindowTarget->GetSize().x, m_Specification.WindowTarget->GetSize().y, m_Specification.VSync, m_Specification.RequestedFormat, m_Specification.RequestedColourSpace);
    
        // Semaphores
        {
            VkSemaphoreCreateInfo semaphoreInfo = {};
            semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
            for (size_t i = 0; i < m_ImageAvailableSemaphores.size(); i++)
            {
                VK_VERIFY(vkCreateSemaphore(m_Device.GetContext().GetVulkanLogicalDevice().GetVkDevice(), &semaphoreInfo, VulkanAllocator::GetCallbacks(), &m_ImageAvailableSemaphores[i]));
                VK_VERIFY(vkCreateSemaphore(m_Device.GetContext().GetVulkanLogicalDevice().GetVkDevice(), &semaphoreInfo, VulkanAllocator::GetCallbacks(), &m_SwapchainPresentableSemaphores[i]));
            
                if constexpr (VulkanContext::Validation)
                {
                    if (!m_Specification.DebugName.empty())
                    {
                        m_Device.GetContext().SetDebugName(m_ImageAvailableSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, std::format("ImageAvailable Semaphore({0}) for: {1}", i, m_Specification.DebugName));
                        m_Device.GetContext().SetDebugName(m_SwapchainPresentableSemaphores[i], VK_OBJECT_TYPE_SEMAPHORE, std::format("Presentable Semaphore({0}) for: {1}", i, m_Specification.DebugName));
                    }
                }
            }

            VkSemaphoreTypeCreateInfo timelineInfo = {};
            timelineInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
            timelineInfo.semaphoreType = VK_SEMAPHORE_TYPE_TIMELINE;
            timelineInfo.initialValue = 0;

            semaphoreInfo.pNext = &timelineInfo;

            VK_VERIFY(vkCreateSemaphore(m_Device.GetContext().GetVulkanLogicalDevice().GetVkDevice(), &semaphoreInfo, VulkanAllocator::GetCallbacks(), &m_TimelineSemaphore));
        
            if constexpr (VulkanContext::Validation)
            {
                if (!m_Specification.DebugName.empty())
                    m_Device.GetContext().SetDebugName(m_TimelineSemaphore, VK_OBJECT_TYPE_SEMAPHORE, std::format("Timeline Semaphore for: {0}", m_Specification.DebugName));
            }
        }
    }

    VulkanSwapchain::~VulkanSwapchain()
    {
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // Destruction methods
    ////////////////////////////////////////////////////////////////////////////////////
    void VulkanSwapchain::FreePool(CommandListPool& pool) const
    {
        VkDevice device = m_Device.GetContext().GetVulkanLogicalDevice().GetVkDevice();
        VkCommandPool commandPool = (*reinterpret_cast<VulkanCommandListPool*>(&pool)).GetVkCommandPool();
        m_Device.GetContext().Destroy([device, commandPool]() mutable
        {
            vkDestroyCommandPool(device, commandPool, VulkanAllocator::GetCallbacks());
        });
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // Methods
    ////////////////////////////////////////////////////////////////////////////////////
    void VulkanSwapchain::Resize(uint32_t width, uint32_t height)
    {
        Resize(width, height, m_Specification.VSync, m_Specification.RequestedFormat, m_Specification.RequestedColourSpace);
    }

    void VulkanSwapchain::Resize(uint32_t width, uint32_t height, bool vsync, Format colourFormat, ColourSpace colourSpace)
    {
        NG_PROFILE("VkSwapchain::Resize()")
        if (width == 0 || height == 0) [[unlikely]]
            return;

        SwapchainSupportDetails details = SwapchainSupportDetails::Query(m_Surface, m_Device.GetContext().GetVulkanPhysicalDevice().GetVkPhysicalDevice());
        
        if ((colourFormat != m_Specification.RequestedFormat) || (colourSpace != m_Specification.RequestedColourSpace)) [[unlikely]]
            ResolveFormatAndColourSpace(details, colourFormat, colourSpace);

        VkExtent2D swapchainExtent = {};
        if (details.Capabilities.currentExtent.width == 0xFFFFFFFF) // When it's 0xFFFFFFFF we can decide ourselves.
        {
            swapchainExtent.width = width;
            swapchainExtent.height = height;
        }
        else
        {
            swapchainExtent = details.Capabilities.currentExtent;
        }

        VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;
        if (!vsync)
        {
            for (size_t i = 0; i < details.PresentModes.size(); i++)
            {
                if (details.PresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    swapchainPresentMode = VK_PRESENT_MODE_MAILBOX_KHR;
                    break;
                }
                if ((swapchainPresentMode != VK_PRESENT_MODE_MAILBOX_KHR) && (details.PresentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR))
                {
                    swapchainPresentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
                }
            }
        }

        NG_ASSERT((details.Capabilities.minImageCount <= Information::BackBufferCount), "[VkSwapchain] BackBufferCount is less than the minimum amount of Swapchain images.");
        NG_ASSERT((Information::BackBufferCount <= details.Capabilities.maxImageCount), "[VkSwapchain] BackBufferCount is more than the maximum amount of Swapchain images.");

        // Get current transform?
        VkSurfaceTransformFlagsKHR preTransform;
        if (details.Capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        else
            preTransform = details.Capabilities.currentTransform;

        // Note: Not all platforms support alpha opaque, so check for possible values
        VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        std::vector<VkCompositeAlphaFlagBitsKHR> possibleAlphaFlags = { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
        for (const auto& flag : possibleAlphaFlags)
        {
            if (details.Capabilities.supportedCompositeAlpha & flag)
            {
                compositeAlpha = flag;
                break;
            }
        }

        VkSwapchainCreateInfoKHR swapchainCI = {};
        swapchainCI.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCI.pNext = nullptr;
        swapchainCI.surface = m_Surface;
        swapchainCI.minImageCount = static_cast<uint32_t>(Information::BackBufferCount);
        swapchainCI.imageFormat = FormatToVkFormat(m_Specification.RequestedFormat);
        swapchainCI.imageColorSpace = ColourSpaceToVkColorSpaceKHR(m_Specification.RequestedColourSpace);
        swapchainCI.imageExtent = { swapchainExtent.width, swapchainExtent.height };
        swapchainCI.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        swapchainCI.preTransform = static_cast<VkSurfaceTransformFlagBitsKHR>(preTransform);
        swapchainCI.imageArrayLayers = 1;
        swapchainCI.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCI.queueFamilyIndexCount = 0;
        swapchainCI.pQueueFamilyIndices = nullptr;
        swapchainCI.presentMode = swapchainPresentMode;
        swapchainCI.oldSwapchain = m_Swapchain;
        swapchainCI.clipped = VK_TRUE; // Note: Setting clipped to VK_TRUE allows the implementation to discard rendering outside of the surface area
        swapchainCI.compositeAlpha = compositeAlpha; 

        VkDevice device = m_Device.GetContext().GetVulkanLogicalDevice().GetVkDevice();
        VkSwapchainKHR oldSwapchain = m_Swapchain;
        VK_VERIFY(vkCreateSwapchainKHR(device, &swapchainCI, VulkanAllocator::GetCallbacks(), &m_Swapchain));

        if (oldSwapchain)
            vkDestroySwapchainKHR(device, oldSwapchain, VulkanAllocator::GetCallbacks()); // Destroys old swapchain images

        if constexpr (VulkanContext::Validation)
        {
            if (!m_Specification.DebugName.empty())
                m_Device.GetContext().SetDebugName(m_Swapchain, VK_OBJECT_TYPE_SWAPCHAIN_KHR, std::string(m_Specification.DebugName));
        }

        uint32_t imageCount = 0;
        std::array<VkImage, Information::BackBufferCount> swapchainImages = { };

        VK_VERIFY(vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, nullptr));
        NG_ASSERT((imageCount == Information::BackBufferCount), "[VkSwapchain] Swapchain image count doesn't match requested BackBufferCount");
        VK_VERIFY(vkGetSwapchainImagesKHR(device, m_Swapchain, &imageCount, swapchainImages.data()));

        for (uint32_t i = 0; i < imageCount; i++)
        {
            ImageSpecification imageSpec = ImageSpecification()
                .SetImageDimension(ImageDimension::Image2D)
                .SetImageFormat(colourFormat)
                .SetWidthAndHeight(width, height)
                .SetIsRenderTarget(true)
                .SetResourceState(ResourceState::Present)
                .SetKeepResourceState(true);

            if (m_Images[i].IsConstructed())
            {
                m_Device.DestroySubresourceViews(*reinterpret_cast<Image*>(&m_Images[i].Get()));
                m_Images[i]->SetInternalData(imageSpec, swapchainImages[i]);
            }
            else
            {
                m_Images[i].Construct(m_Device, imageSpec, swapchainImages[i]);
            }


            ImageSubresourceSpecification imageViewSpec = ImageSubresourceSpecification(0, ImageSubresourceSpecification::AllMipLevels, 0, ImageSubresourceSpecification::AllArraySlices);
            (void)m_Images[i]->GetSubresourceView(imageViewSpec, ImageDimension::Image2D, colourFormat, 0, ImageSubresourceViewType::AllAspects); // Note: Makes sure to already lazy initialize the image view
        
            if constexpr (VulkanContext::Validation)
            {
                if (!m_Specification.DebugName.empty())
                    m_Device.GetContext().SetDebugName(swapchainImages[i], VK_OBJECT_TYPE_IMAGE, std::format("Image({0}) for: {1}", i, m_Specification.DebugName));
            }
        }

        // Temporary transition // TODO: Replace
        {
            // Create command pool
            VkCommandPool commandPool;
            VkCommandPoolCreateInfo poolInfo{ VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO };
            poolInfo.queueFamilyIndex = m_Device.GetContext().GetVulkanPhysicalDevice().GetQueueFamilyIndices().QueueFamily;
            vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);

            // Allocate command buffer
            VkCommandBuffer commandBuffer;
            VkCommandBufferAllocateInfo allocInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO };
            allocInfo.commandPool = commandPool;
            allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandBufferCount = 1;
            vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

            // Begin recording
            VkCommandBufferBeginInfo beginInfo{ VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO };
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            vkBeginCommandBuffer(commandBuffer, &beginInfo);

            // Transition layout
            VkImageMemoryBarrier barrier{ VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER };
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = 1;
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = 0;

            barrier.image = m_Images[0]->GetVkImage();
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            barrier.image = m_Images[1]->GetVkImage();
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
            barrier.image = m_Images[2]->GetVkImage();
            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

            // End recording
            vkEndCommandBuffer(commandBuffer);

            // Submit and wait
            VkSubmitInfo submitInfo{ VK_STRUCTURE_TYPE_SUBMIT_INFO };
            submitInfo.commandBufferCount = 1;
            submitInfo.pCommandBuffers = &commandBuffer;
            vkQueueSubmit(m_Device.GetContext().GetVulkanLogicalDevice().GetVkQueue(CommandQueue::Graphics), 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(m_Device.GetContext().GetVulkanLogicalDevice().GetVkQueue(CommandQueue::Graphics));

            // Cleanup
            vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
            vkDestroyCommandPool(device, commandPool, nullptr);
        }
    }

    void VulkanSwapchain::AcquireNextImage()
    {
        NG_PROFILE("VkSwapchain::AcquireImage()");

        // Wait for this frame's previous last value
        {
            VkSemaphoreWaitInfo waitInfo = {};
            waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
            waitInfo.semaphoreCount = 1;
            waitInfo.pSemaphores = &m_TimelineSemaphore;
            waitInfo.pValues = &m_WaitTimelineValues[m_CurrentFrame];

            vkWaitSemaphores(m_Device.GetContext().GetVulkanLogicalDevice().GetVkDevice(), &waitInfo, std::numeric_limits<uint64_t>::max());
        }

        // Acquire image
        VkResult result = vkAcquireNextImageKHR(m_Device.GetContext().GetVulkanLogicalDevice().GetVkDevice(), m_Swapchain, std::numeric_limits<uint64_t>::max(), m_ImageAvailableSemaphores[m_CurrentFrame], VK_NULL_HANDLE, &m_AcquiredImage);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            Resize(m_Specification.WindowTarget->GetSize().x, m_Specification.WindowTarget->GetSize().y);
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            m_Device.GetContext().Error("[VkSwapchain] Failed to acquire Swapchain image!");
        }
    }

    void VulkanSwapchain::Present()
    {
        NG_PROFILE("VkSwapchain::Present()");

        VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &m_SwapchainPresentableSemaphores[m_CurrentFrame];
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &m_Swapchain;
        presentInfo.pImageIndices = &m_AcquiredImage;
        presentInfo.pResults = nullptr; // Optional

        VkResult result = VK_SUCCESS;
        {
            NG_PROFILE("VkSwapchain::Present::QueuePresent");
            result = vkQueuePresentKHR(m_Device.GetContext().GetVulkanLogicalDevice().GetVkQueue(CommandQueue::Present), &presentInfo);
        }

        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR)
        {
            Resize(m_Specification.WindowTarget->GetSize().x, m_Specification.WindowTarget->GetSize().y);
        }
        else if (result != VK_SUCCESS)
        {
            m_Device.GetContext().Error("[VkSwapchain] Failed to present Swapchain image.");
        }

        m_WaitTimelineValues[m_CurrentFrame] = m_CurrentTimelineValue;
        m_CurrentFrame = (m_CurrentFrame + 1) % Information::BackBufferCount;
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // Internal methods
    ////////////////////////////////////////////////////////////////////////////////////
    uint64_t VulkanSwapchain::GetPreviousCommandListWaitValue(const VulkanCommandList& commandList) const
    {
        NG_ASSERT(m_CommandListSemaphoreValues.contains(&commandList), "[VkSwapchain] Commandlist is not known in current Swapchain.");
        return m_CommandListSemaphoreValues.at(&commandList);
    }

    uint64_t VulkanSwapchain::RetrieveCommandListWaitValue(const VulkanCommandList& commandList)
    {
        uint64_t value = ++m_CurrentTimelineValue;
        m_CommandListSemaphoreValues[&commandList] = value;
        return value;
    }

    ////////////////////////////////////////////////////////////////////////////////////
    // Private methods
    ////////////////////////////////////////////////////////////////////////////////////
    void VulkanSwapchain::ResolveFormatAndColourSpace(const SwapchainSupportDetails& details, Format format, ColourSpace space)
    {
        const std::vector<VkSurfaceFormatKHR>& formats = details.Formats;
        
        // If the surface format list only includes one entry with VK_FORMAT_UNDEFINED anything is fine, so the requested can stay
        if ((formats.size() == 1ull) && (formats[0].format == VK_FORMAT_UNDEFINED))
        {
            m_Specification.RequestedFormat = format;
            m_Specification.RequestedColourSpace = space;
        }
        else
        {
            bool foundDesiredCombination = false;
            for (const auto& [form, colspace] : formats)
            {
                if ((form == FormatToVkFormat(format)) && (colspace == ColourSpaceToVkColorSpaceKHR(space)))
                {
                    m_Specification.RequestedFormat = format;
                    m_Specification.RequestedColourSpace = space;

                    foundDesiredCombination = true;
                    break;
                }
            }

            // If not available, try again
            if (!foundDesiredCombination)
                ResolveFormatAndColourSpace(details, format, ColourSpace::SRGB);
        }
    }

}