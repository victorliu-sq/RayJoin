#ifndef RAYJOIN_VK_CONTEXT_INIT_H
#define RAYJOIN_VK_CONTEXT_INIT_H

#include "vk_context.h"
#include <vector>
#include <stdexcept>
#include <cstring>

#ifndef VK_CHECK
#define VK_CHECK(x) do { VkResult err = (x); if (err) throw std::runtime_error("Vulkan error"); } while(0)
#endif

inline uint32_t findComputeQueueFamily(VkPhysicalDevice phys)
{
  uint32_t count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, nullptr);
  std::vector<VkQueueFamilyProperties> props(count);
  vkGetPhysicalDeviceQueueFamilyProperties(phys, &count, props.data());

  // Prefer compute-only
  for (uint32_t i = 0; i < count; ++i) {
    if ((props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
        !(props[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      return i;
    }
  }
  // Otherwise any compute-capable
  for (uint32_t i = 0; i < count; ++i) {
    if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) return i;
  }
  throw std::runtime_error("No compute queue family found");
}

inline VkPhysicalDevice pickPhysicalDevice(VkInstance instance)
{
  uint32_t count = 0;
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, nullptr));
  if (count == 0) throw std::runtime_error("No Vulkan physical devices found");

  std::vector<VkPhysicalDevice> devs(count);
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &count, devs.data()));

  // Prefer discrete GPU
  for (auto d : devs) {
    VkPhysicalDeviceProperties p{};
    vkGetPhysicalDeviceProperties(d, &p);
    if (p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return d;
  }
  return devs[0];
}

inline VkInstance createInstanceMinimal()
{
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "rayjoin_vk";
  app.apiVersion = VK_API_VERSION_1_3;

  VkInstanceCreateInfo ici{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  ici.pApplicationInfo = &app;

  VkInstance instance = VK_NULL_HANDLE;
  VK_CHECK(vkCreateInstance(&ici, nullptr, &instance));
  return instance;
}

inline VkDescriptorPool createDescriptorPoolSimple(VkDevice device)
{
  VkDescriptorPoolSize sizes[] = {
    { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 16 },
    { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 8  },
  };

  VkDescriptorPoolCreateInfo ci{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
  ci.maxSets = 16;
  ci.poolSizeCount = (uint32_t)std::size(sizes);
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
inline VkInstance initVkComputeContext(VkComputeContext& ctx, VkInstance instanceOrNull)
{
  VkInstance instance = instanceOrNull ? instanceOrNull : createInstanceMinimal();

  ctx.phys = pickPhysicalDevice(instance);
  ctx.queueFamily = findComputeQueueFamily(ctx.phys);

  // Enable float64 + int64
  VkPhysicalDeviceFeatures features{};
  features.shaderFloat64 = VK_TRUE;
  features.shaderInt64   = VK_TRUE;

  float priority = 1.0f;
  VkDeviceQueueCreateInfo qci{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
  qci.queueFamilyIndex = ctx.queueFamily;
  qci.queueCount = 1;
  qci.pQueuePriorities = &priority;

  VkDeviceCreateInfo dci{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;
  dci.pEnabledFeatures = &features;

  VK_CHECK(vkCreateDevice(ctx.phys, &dci, nullptr, &ctx.device));
  vkGetDeviceQueue(ctx.device, ctx.queueFamily, 0, &ctx.queue);

  VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
  cpci.queueFamilyIndex = ctx.queueFamily;
  cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  VK_CHECK(vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.cmdPool));

  // VMA needs instance to load function pointers
  VmaAllocatorCreateInfo vaci{};
  vaci.instance = instance;
  vaci.physicalDevice = ctx.phys;
  vaci.device = ctx.device;
  vaci.vulkanApiVersion = VK_API_VERSION_1_3;
  VK_CHECK(vmaCreateAllocator(&vaci, &ctx.vma));

  ctx.descPool = createDescriptorPoolSimple(ctx.device);

  return instance; // return the instance used (so caller can destroy if they created it)
}

inline void destroyVkComputeContext(VkComputeContext& ctx)
{
  if (ctx.device) {
    vkDeviceWaitIdle(ctx.device);
    if (ctx.descPool) vkDestroyDescriptorPool(ctx.device, ctx.descPool, nullptr);
    if (ctx.vma) vmaDestroyAllocator(ctx.vma);
    if (ctx.cmdPool) vkDestroyCommandPool(ctx.device, ctx.cmdPool, nullptr);
    vkDestroyDevice(ctx.device, nullptr);
  }
  ctx = {};
}

#endif  // RAYJOIN_VK_CONTEXT_INIT_H
