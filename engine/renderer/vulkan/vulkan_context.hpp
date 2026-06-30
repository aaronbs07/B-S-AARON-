#pragma once
#include <volk.h>
#include <vector>
#include <string_view>

namespace KumariEngine::Window { class Window; }

namespace KumariEngine::Renderer {

struct QueueFamilyIndices {
    int graphicsFamily = -1;
    int presentFamily = -1;

    bool isComplete() const {
        return graphicsFamily >= 0 && presentFamily >= 0;
    }
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanContext {
public:
    VulkanContext();
    ~VulkanContext();

    bool Initialize(Window::Window* window);
    void Shutdown();

    void RecreateSwapChain(Window::Window* window);

    VkInstance GetInstance() const { return m_instance; }
    VkDevice GetDevice() const { return m_device; }
    VkPhysicalDevice GetPhysicalDevice() const { return m_physicalDevice; }
    VkSurfaceKHR GetSurface() const { return m_surface; }
    VkSwapchainKHR GetSwapChain() const { return m_swapChain; }
    VkFormat GetSwapChainImageFormat() const { return m_swapChainImageFormat; }
    VkExtent2D GetSwapChainExtent() const { return m_swapChainExtent; }
    const std::vector<VkImageView>& GetSwapChainImageViews() const { return m_swapChainImageViews; }
    VkQueue GetGraphicsQueue() const { return m_graphicsQueue; }
    VkQueue GetPresentQueue() const { return m_presentQueue; }
    QueueFamilyIndices GetQueueFamilyIndices() const { return m_queueFamilyIndices; }

private:
    bool CreateInstance();
    bool SetupDebugMessenger();
    bool CreateSurface(Window::Window* window);
    bool PickPhysicalDevice();
    bool CreateLogicalDevice();
    bool CreateSwapChain(Window::Window* window);
    bool CreateImageViews();
    void CleanupSwapChain();

    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device);
    bool CheckDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, Window::Window* window);

    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debugMessenger = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;

    VkQueue m_graphicsQueue = VK_NULL_HANDLE;
    VkQueue m_presentQueue = VK_NULL_HANDLE;

    VkSwapchainKHR m_swapChain = VK_NULL_HANDLE;
    std::vector<VkImage> m_swapChainImages;
    VkFormat m_swapChainImageFormat;
    VkExtent2D m_swapChainExtent;
    std::vector<VkImageView> m_swapChainImageViews;

    QueueFamilyIndices m_queueFamilyIndices;
    bool m_validationLayersEnabled = false;
};

} // namespace KumariEngine::Renderer
