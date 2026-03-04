#ifndef RAYJOIN_VK_CONTEXT_INIT_H
#define RAYJOIN_VK_CONTEXT_INIT_H

#include <cstring>
#include <stdexcept>
#include <vector>

#include "vk_compute_context.h"

#ifndef VK_CHECK
#define VK_CHECK(x)                             \
  do {                                          \
    VkResult err = (x);                         \
    if (err)                                    \
      throw std::runtime_error("Vulkan error"); \
  } while (0)
#endif

struct VkComputeContext {
  // Private
  VkPhysicalDevice phys = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;

  // Public
  VkDevice device = VK_NULL_HANDLE;
  // Create Command Buffer
  VkCommandPool cmdPool = VK_NULL_HANDLE;
  // Submit Command Buffer
  VkQueue queue = VK_NULL_HANDLE;
  // Create Staging or Device Buffer
  VmaAllocator vma = VK_NULL_HANDLE;

  // TODO: Remove this
  // You can pass an existing pool, or we can create one
  // VkDescriptorPool descPool = VK_NULL_HANDLE;
};

static inline void vkCheck(VkResult result) {
  if (result != VK_SUCCESS) {
    std::cerr << "Vulkan call returned an error (" << result << ")\n";
    exit(result);
  }
}

inline uint32_t findComputeQueueFamily(VkPhysicalDevice phys) {
  // Return QueueFamily Count
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
  // Return a list of QueueFamily Properties
  std::vector<VkQueueFamilyProperties> props(count);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, props.data());

  // Prefer compute-capabile
  for (uint32_t i = 0; i < count; ++i) {
    if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
      return i;
  }
  throw std::runtime_error("No compute queue family found");
}

inline VkPhysicalDevice findDiscretePhysicalDevice(VkInstance instance) {
  // Return the device count via instance
  uint32_t dev_count = 0;
  vkCheck(vkEnumeratePhysicalDevices(instance, &dev_count, nullptr));
  if (dev_count == 0) {
    throw std::runtime_error("No Vulkan physical devices found");
  }
  LOG(INFO) << "# of devices: " << dev_count;

  // Return a list of device handlers via instance
  std::vector<VkPhysicalDevice> devs(dev_count);
  vkCheck(vkEnumeratePhysicalDevices(instance, &dev_count, devs.data()));

  // Find a device is typically a separate processor connected to the host
  // via an interlink.
  uint32_t dev_index = dev_count;

  uint32_t i = 0;
  while (i < dev_count && dev_index == dev_count) {
    // Get Device Properties
    VkPhysicalDeviceProperties2 dev_prop{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
    vkGetPhysicalDeviceProperties2(devs[i], &dev_prop);
    auto& prop = dev_prop.properties;

    // Print out information of devices
    LOG(INFO) << "Selected device: " << prop.deviceName;
    if (prop.deviceType ==
        VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {  // the separate device
      dev_index = i;
    }
    i++;
  }

  if (dev_index == dev_count) {
    throw std::runtime_error("No Discrete GPU");
  }

  return devs[dev_index];
}

inline VkInstance createInstanceMinimal() {
  VkApplicationInfo app{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "rayjoin_vk",
      .apiVersion = VK_API_VERSION_1_3,
  };

  VkInstanceCreateInfo instance_create_info{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app};

  VkInstance instance = VK_NULL_HANDLE;
  vkCheck(vkCreateInstance(&instance_create_info, nullptr, &instance));
  return instance;
}

inline VkDescriptorPool createDescriptorPoolSimple(VkDevice device) {
  VkDescriptorPoolSize sizes[] = {
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8},
  };

  VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  ci.maxSets = 16;
  ci.poolSizeCount = (uint32_t) std::size(sizes);
  ci.pPoolSizes = sizes;

  VkDescriptorPool pool = VK_NULL_HANDLE;
  VK_CHECK(vkCreateDescriptorPool(device, &ci, nullptr, &pool));
  return pool;
}

/**
 * Initializes ctx using either:
 * - an externally provided VkInstance (recommended), OR
 * - creates a minimal one if you pass VK_NULL_HANDLE and want quick start.
 *
 * IMPORTANT: if this function creates the instance internally, you must
 * call destroyVkComputeContext(ctx, createdInstance) with that instance.
 */
inline VkInstance initVkComputeContext(VkComputeContext& ctx) {
  VkInstance instance = createInstanceMinimal();

  ctx.phys = findDiscretePhysicalDevice(instance);

  ctx.queueFamilyIndex = findComputeQueueFamily(ctx.phys);

  const float priority[] = {1.0f};
  VkDeviceQueueCreateInfo qci{
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .queueFamilyIndex = ctx.queueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = priority,
  };

  // Logical Device
  // Enable float64 + int64
  VkPhysicalDeviceFeatures features{.shaderFloat64 = VK_TRUE,
                                    .shaderInt64 = VK_TRUE};
  VkDeviceCreateInfo dci{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                         .queueCreateInfoCount = 1,
                         .pQueueCreateInfos = &qci,
                         .pEnabledFeatures = &features};
  VK_CHECK(vkCreateDevice(ctx.phys, &dci, nullptr, &ctx.device));
  vkGetDeviceQueue(ctx.device, ctx.queueFamilyIndex, 0, &ctx.queue);

  // Command Pool
  VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cpci.queueFamilyIndex = ctx.queueFamilyIndex;
  cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK(vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.cmdPool));

  // VMA needs instance to load function pointers
  VmaAllocatorCreateInfo vaci{};
  vaci.instance = instance;
  vaci.physicalDevice = ctx.phys;
  vaci.device = ctx.device;
  vaci.vulkanApiVersion = VK_API_VERSION_1_3;
  VK_CHECK(vmaCreateAllocator(&vaci, &ctx.vma));
  return instance;  // return the instance used (so caller can destroy if they
                    // created it)
}

inline void destroyVkComputeContext(VkComputeContext& ctx) {
  if (ctx.device) {
    vkDeviceWaitIdle(ctx.device);
    if (ctx.vma)
      vmaDestroyAllocator(ctx.vma);
    if (ctx.cmdPool)
      vkDestroyCommandPool(ctx.device, ctx.cmdPool, nullptr);
    vkDestroyDevice(ctx.device, nullptr);
  }
  ctx = {};
}

#endif  // RAYJOIN_VK_CONTEXT_INIT_H
