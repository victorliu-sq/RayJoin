#ifndef RAYJOIN_VK_CONTEXT_H
#define RAYJOIN_VK_CONTEXT_H

#include <glog/logging.h>
#include <vulkan/vulkan.h>

#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

#include "vk_compute_context.h"
#include "vk_helpers.h"
#include "vk_mem_alloc.h"

struct VkComputeContext {
  // Private
  VkPhysicalDevice phys = VK_NULL_HANDLE;
  uint32_t queueFamilyIndex = 0;

  // Public
  VkInstance instance = VK_NULL_HANDLE;
  VkDevice device = VK_NULL_HANDLE;
  // Create Command Buffer
  VkCommandPool cmdPool = VK_NULL_HANDLE;
  // Submit Command Buffer
  VkQueue queue = VK_NULL_HANDLE;

  // Create Staging or Device Buffer
  VmaAllocator vma = VK_NULL_HANDLE;
};

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
  uint32_t devCount = 0;
  vkCheck(vkEnumeratePhysicalDevices(instance, &devCount, nullptr));

  LOG(INFO) << "# of devices: " << devCount;

  std::vector<VkPhysicalDevice> devices(devCount);
  vkCheck(vkEnumeratePhysicalDevices(instance, &devCount, devices.data()));

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

  if (selected == VK_NULL_HANDLE) {
    if (devCount == 0) {
      throw std::runtime_error("No Vulkan physical devices found");
    } else {
      throw std::runtime_error("No discrete GPU found");
    }
  }
  return selected;
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
// inline VkInstance initVkComputeContext(VkComputeContext& ctx) {
//   ctx.instance = createInstanceMinimal();
//
//   ctx.phys = findDiscretePhysicalDevice(ctx.instance);
//
//   ctx.queueFamilyIndex = findComputeQueueFamily(ctx.phys);
//
//   const float priority[] = {1.0f};
//   VkDeviceQueueCreateInfo qci{
//       .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
//       .queueFamilyIndex = ctx.queueFamilyIndex,
//       .queueCount = 1,
//       .pQueuePriorities = priority,
//   };
//
//   // Logical Device
//   // Enable float64 + int64
//   VkPhysicalDeviceFeatures features{.shaderFloat64 = VK_TRUE,
//                                     .shaderInt64 = VK_TRUE};
//   VkDeviceCreateInfo dci{.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
//                          .queueCreateInfoCount = 1,
//                          .pQueueCreateInfos = &qci,
//                          .pEnabledFeatures = &features};
//   VK_CHECK(vkCreateDevice(ctx.phys, &dci, nullptr, &ctx.device));
//   vkGetDeviceQueue(ctx.device, ctx.queueFamilyIndex, 0, &ctx.queue);
//
//   // Command Pool
//   VkCommandPoolCreateInfo cpci{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
//   cpci.queueFamilyIndex = ctx.queueFamilyIndex;
//   cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
//   VK_CHECK(vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.cmdPool));
//
//   // VMA needs instance to load function pointers
//   VmaAllocatorCreateInfo vaci{};
//   vaci.instance = ctx.instance;
//   vaci.physicalDevice = ctx.phys;
//   vaci.device = ctx.device;
//   vaci.vulkanApiVersion = VK_API_VERSION_1_3;
//   // for VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
//   vaci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
//   VK_CHECK(vmaCreateAllocator(&vaci, &ctx.vma));
//   return ctx.instance;  // return the instance used (so caller can destroy if
//                         // they created it)
// }

inline VkInstance initVkComputeContext(VkComputeContext& ctx) {

  ctx.instance = createInstanceMinimal();

  ctx.phys = findDiscretePhysicalDevice(ctx.instance);

  ctx.queueFamilyIndex = findComputeQueueFamily(ctx.phys);

  const float priority[] = {1.0f};

  VkDeviceQueueCreateInfo qci{};
  qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  qci.queueFamilyIndex = ctx.queueFamilyIndex;
  qci.queueCount = 1;
  qci.pQueuePriorities = priority;

  //////////////////////////////////////////////////////
  // Query supported device extensions
  //////////////////////////////////////////////////////

  uint32_t extCount = 0;
  vkEnumerateDeviceExtensionProperties(ctx.phys, nullptr, &extCount, nullptr);

  std::vector<VkExtensionProperties> extProps(extCount);
  vkEnumerateDeviceExtensionProperties(
      ctx.phys, nullptr, &extCount, extProps.data());

  std::set<std::string> supported;
  for (auto& e : extProps)
    supported.insert(e.extensionName);

  size_t num_supported = supported.size();
  //////////////////////////////////////////////////////
  // Enable extensions only if supported
  //////////////////////////////////////////////////////
  std::vector<const char*> extensions;

  if (supported.count(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME))
    extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);

  if (supported.count(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME))
    extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

  if (supported.count(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME))
    extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);

  if (supported.count(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME))
    extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);


  size_t num_extensions = extensions.size();
  //////////////////////////////////////////////////////
  // Query device features
  //////////////////////////////////////////////////////

  VkPhysicalDeviceBufferDeviceAddressFeatures bufferAddress{};
  bufferAddress.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;

  VkPhysicalDeviceAccelerationStructureFeaturesKHR accelFeature{};
  accelFeature.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

  VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayFeature{};
  rayFeature.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;

  rayFeature.pNext = &accelFeature;
  accelFeature.pNext = &bufferAddress;

  VkPhysicalDeviceFeatures2 features2{};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &rayFeature;

  vkGetPhysicalDeviceFeatures2(ctx.phys, &features2);

  //////////////////////////////////////////////////////
  // Enable base shader features
  //////////////////////////////////////////////////////

  features2.features.shaderFloat64 = VK_TRUE;
  features2.features.shaderInt64 = VK_TRUE;

  //////////////////////////////////////////////////////
  // Create logical device
  //////////////////////////////////////////////////////

  VkDeviceCreateInfo dci{};
  dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  dci.queueCreateInfoCount = 1;
  dci.pQueueCreateInfos = &qci;

  dci.enabledExtensionCount =
      static_cast<uint32_t>(extensions.size());
  dci.ppEnabledExtensionNames = extensions.data();

  dci.pNext = &features2;
  // dci.pNext = &rayFeature;

  VK_CHECK(vkCreateDevice(ctx.phys, &dci, nullptr, &ctx.device));

  vkGetDeviceQueue(ctx.device, ctx.queueFamilyIndex, 0, &ctx.queue);

  //////////////////////////////////////////////////////
  // Command pool
  //////////////////////////////////////////////////////

  VkCommandPoolCreateInfo cpci{};
  cpci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cpci.queueFamilyIndex = ctx.queueFamilyIndex;
  cpci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  VK_CHECK(vkCreateCommandPool(ctx.device, &cpci, nullptr, &ctx.cmdPool));

  //////////////////////////////////////////////////////
  // VMA allocator
  //////////////////////////////////////////////////////
  VmaAllocatorCreateInfo vaci{};
  vaci.instance = ctx.instance;
  vaci.physicalDevice = ctx.phys;
  vaci.device = ctx.device;
  vaci.vulkanApiVersion = VK_API_VERSION_1_3;

  // Needed for VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT
  vaci.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VK_CHECK(vmaCreateAllocator(&vaci, &ctx.vma));

  return ctx.instance;
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

#endif  // RAYJOIN_VK_CONTEXT_H
