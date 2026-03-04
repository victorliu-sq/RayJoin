#ifndef RAYJOIN_VK_CONTEXT_H
#define RAYJOIN_VK_CONTEXT_H

#include <glog/logging.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <stdexcept>
#include <vector>

#include "vk_compute_context.h"
#include "vk_helpers.h"
#include "vk_mem_alloc.h"

namespace rayjoin {
namespace vk {
class VkContext {
 public:
  explicit VkContext() {
    initVkInstance();

    initVkPhysicalDevice();

    initVkQueueAndVkCommandPool();
  }

 private:
  void initVkInstance() {
    VkApplicationInfo app{
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "rayjoin_vk",
        .apiVersion = VK_API_VERSION_1_3,
    };

    VkInstanceCreateInfo instance_create_info{
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app};

    VkInstance instance = VK_NULL_HANDLE;
    vkCheck(vkCreateInstance(&instance_create_info, nullptr, &instance_));
  }

  void initVkPhysicalDevice() {
    // Get the information of all physical device
    uint32_t devCount = 0;
    vkCheck(vkEnumeratePhysicalDevices(instance_, &devCount, nullptr));
    LOG(INFO) << "# of devices: " << devCount;
    std::vector<VkPhysicalDevice> devices(devCount);
    vkCheck(vkEnumeratePhysicalDevices(instance_, &devCount, devices.data()));

    // Select a discrete physical device
    VkPhysicalDevice selected = VK_NULL_HANDLE;
    size_t i = 0;
    while (i < devices.size() && selected == VK_NULL_HANDLE) {
      VkPhysicalDevice dev = devices[i];

      VkPhysicalDeviceProperties2 props{
          VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};

      vkGetPhysicalDeviceProperties2(dev, &props);

      if (props.properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
        LOG(INFO) << "Selected device: " << props.properties.deviceName;
        selected = dev;
      }

      ++i;
    }

    // if no physical device is found, throw an exception
    if (selected == VK_NULL_HANDLE) {
      if (devCount == 0) {
        throw std::runtime_error("No Vulkan physical devices found");
      } else {
        throw std::runtime_error("No discrete GPU found");
      }
    }
    phys_ = selected;
  }

  void initVkQueueAndVkCommandPool() {
    auto findComputeQueueFamily = [&]() {
      // Return QueueFamily Count
      uint32_t count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(phys_, &count, nullptr);
      // Return a list of QueueFamily Properties
      std::vector<VkQueueFamilyProperties> props(count);
      vkGetPhysicalDeviceQueueFamilyProperties(phys_, &count, props.data());

      // Prefer compute-capabile
      uint32_t index = count;
      uint32_t i = 0;

      while (i < count && index == count) {
        if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
          index = i;
        }
        ++i;
      }

      if (index == count) {
        throw std::runtime_error("No compute queue family found");
      }

      return index;
    };

    auto initQueueFromQueueFamily = [&](uint32_t queueFamilyIndex) {
      const float priority[] = {1.0f};
      VkDeviceQueueCreateInfo qci{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = queueFamilyIndex,
          .queueCount = 1,
          .pQueuePriorities = priority,
      };

      // Logical Device: Enable float64 + int64
      VkPhysicalDeviceFeatures features{.shaderFloat64 = VK_TRUE,
                                        .shaderInt64 = VK_TRUE};
      VkDeviceCreateInfo dci{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                             .queueCreateInfoCount = 1,
                             .pQueueCreateInfos = &qci,
                             .pEnabledFeatures = &features};
      vkCheck(vkCreateDevice(phys_, &dci, nullptr, &device_));
      vkGetDeviceQueue(device_, queueFamilyIndex, 0, &queue_);
    };

    auto initCommandPoolFromQueueFamily = [&](uint32_t queueFamilyIndex) {
      VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
      cpci.queueFamilyIndex = queueFamilyIndex;
      cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
      vkCheck(vkCreateCommandPool(device_, &cpci, nullptr, &cmdPool_));
    };

    uint32_t queueFamilyIndex = findComputeQueueFamily();
    initQueueFromQueueFamily(queueFamilyIndex);
    initCommandPoolFromQueueFamily(queueFamilyIndex);
  }

  VkPhysicalDevice phys_ = VK_NULL_HANDLE;

  VkInstance instance_ = VK_NULL_HANDLE;
  VkDevice device_ = VK_NULL_HANDLE;

  // Submit Command Buffer
  VkQueue queue_ = VK_NULL_HANDLE;
  // Create Command Buffer for that queue family
  // Command buffers from a pool are tied to that queue family
  VkCommandPool cmdPool_ = VK_NULL_HANDLE;
};
}  // namespace vk
}  // namespace rayjoin

#endif  // RAYJOIN_VK_CONTEXT_H
