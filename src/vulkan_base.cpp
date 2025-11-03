#include "vulkan_base.h"
#include "hashmap.h"
#include "renderer.h"
#include "tuke_engine.h"
#include "utils.h"

// https://www.glfw.org/docs/latest/group__vulkan.html#ga9308f2acf6b5f6ff49cf0d4aa9ba1fab
#include "GLFW/glfw3.h"
#include "vulkan/vulkan_beta.h"
#include "vulkan/vulkan_core.h"
#include "window.h"
#include <cassert>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// If you're multithreading submissions, use a mutex or lockless ring buffer
// per queue.

void destroy_swapchain(VulkanContext *context) {
  SwapchainStorage *storage = &context->swapchain_storage;

  // framebuffers
  for (u32 i = 0; i < storage->image_count; ++i) {
    vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
    context->framebuffers[i] = VK_NULL_HANDLE;
  }

  // swapchain image views
  for (u32 i = 0; i < storage->image_count; i++) {
    if (storage->use_static) {
      vkDestroyImageView(context->device, storage->as.static_storage.image_views[i], NULL);
    } else {
      vkDestroyImageView(context->device, storage->as.dynamic_storage.image_views[i], NULL);
    }
  }

  if (!context->swapchain_storage.use_static) {
    free(storage->as.dynamic_storage.image_views);
    free(storage->as.dynamic_storage.images);
  }

  for (u32 i = 0; i < NUM_SWAPCHAIN_IMAGES; i++) {
    DepthBuffer *depth_buffer = &storage->depth_buffers[i];

    vkDestroyImage(context->device, depth_buffer->image, NULL);
    vkDestroyImageView(context->device, depth_buffer->image_view, NULL);
    vkFreeMemory(context->device, depth_buffer->device_memory, NULL);
  }

  if (context->swapchain) {
    vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
  } else {
    printf("Tried to destroy null swapchain\n");
  }

  memset(storage, 0, sizeof(SwapchainStorage));
}

void destroy_vulkan_context(VulkanContext *context) {
  vkDeviceWaitIdle(context->device);

  destroy_shader_cache(context->shader_cache);
  delete context->shader_cache;

  vkDestroyPipelineCache(context->device, context->pipeline_cache, NULL);
  vkDestroyCommandPool(context->device, context->transient_command_pool, NULL);
  vkDestroyCommandPool(context->device, context->graphics_command_pool, NULL);
  if (context->present_queue_index_is_different_than_graphics) {
    vkDestroyCommandPool(context->device, context->present_command_pool, NULL);
  }
  if (context->compute_queue_index_is_different_than_graphics) {
    vkDestroyCommandPool(context->device, context->compute_command_pool, NULL);
  }

  // sync primitives
  for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(context->device, context->frame_sync_objects[i].image_available_semaphore, NULL);
    vkDestroySemaphore(context->device, context->frame_sync_objects[i].render_finished_semaphore, NULL);
    vkDestroyFence(context->device, context->frame_sync_objects[i].in_flight_fence, NULL);
  }

  destroy_swapchain(context);

  vkDestroyRenderPass(context->device, context->render_pass, NULL);
  vkDestroyDevice(context->device, NULL);
  vkDestroySurfaceKHR(context->instance, context->surface, NULL);

#ifdef TUKE_DISABLE_VULKAN_VALIDATION
#else
  PFN_vkDestroyDebugUtilsMessengerEXT fpDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkDestroyDebugUtilsMessengerEXT");
  if (fpDestroyDebugUtilsMessengerEXT) {
    fpDestroyDebugUtilsMessengerEXT(context->instance, context->debug_messenger, NULL);
  }
#endif
  vkDestroyInstance(context->instance, NULL);
}

// TODO Enable debug messenger extension, add macros for disabling in release
// mode
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                                              VkDebugUtilsMessageTypeFlagsEXT message_type,
                                              const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
                                              void *user_data) {

  (void)message_type;
  (void)callback_data;
  (void)user_data;

  const char *severity = "UNKNOWN";
  if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    severity = "VERBOSE";
  else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    severity = "INFO";
  else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    severity = "WARNING";
  else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
    severity = "ERROR";

  fprintf(stderr, "[%s] %s\n", severity, callback_data->pMessage);
  return VK_FALSE;
}

void populate_debug_msngr_create_info(VkDebugUtilsMessengerCreateInfoEXT *debug_msngr_create_info) {
  debug_msngr_create_info->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_msngr_create_info->pNext = NULL;
  debug_msngr_create_info->flags = 0;
  debug_msngr_create_info->messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_msngr_create_info->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                         VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  debug_msngr_create_info->pfnUserCallback = debug_callback;
  debug_msngr_create_info->pUserData = NULL;
}

VkInstance create_instance(const char *name) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkApplicationInfo.html
  VkApplicationInfo application_info;
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pNext = NULL;
  application_info.pApplicationName = name;
  application_info.applicationVersion = 1;
  application_info.pEngineName = NULL;
  application_info.engineVersion = 0;
  application_info.apiVersion = VK_API_VERSION_1_3;

  // https://www.glfw.org/docs/latest/vulkan_guide.html#vulkan_ext
  u32 glfw_extension_count;
  const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);
  if (glfw_extensions == NULL) {
    fprintf(stderr, "create_instance: glfw_extensions is NULL\n");
    exit(1);
  }

  // TODO debug/release
#ifdef TUKE_DISABLE_VULKAN_VALIDATION
  const char *extra_extensions[] = {
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
  };
  const u32 extra_extension_count = 1;
  const char **validation_layers = {};
  const u32 layer_count = 0;
#else
  const char *extra_extensions[] = {
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
  };
  const u32 extra_extension_count = 2;
  const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
  const u32 layer_count = 1;
#endif

  const u32 MAX_EXTENSIONS = 16;
  const u32 total_extension_count = extra_extension_count + glfw_extension_count;
  if (total_extension_count > MAX_EXTENSIONS) {
    fprintf(stderr,
            "create_instance: Too many extensions enabled. MAX_EXTENSIONS is "
            "%d, total_extension_count is %d\n",
            MAX_EXTENSIONS, total_extension_count);
    exit(1);
  }

  const char *enabled_extensions[MAX_EXTENSIONS];
  memcpy(enabled_extensions, glfw_extensions, sizeof(char *) * glfw_extension_count);

  for (u32 i = 0; i < extra_extension_count; i++) {
    enabled_extensions[glfw_extension_count + i] = extra_extensions[i];
  }

  VkDebugUtilsMessengerCreateInfoEXT debug_msngr_create_info;
  populate_debug_msngr_create_info(&debug_msngr_create_info);

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkInstanceCreateInfo.html
  VkInstanceCreateInfo instance_create_info;
  instance_create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
#ifdef TUKE_DISABLE_VULKAN_VALIDATION
  instance_create_info.pNext = NULL;
#else
  instance_create_info.pNext = &debug_msngr_create_info;
#endif
  instance_create_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
  instance_create_info.pApplicationInfo = &application_info;
  instance_create_info.enabledLayerCount = layer_count;
  instance_create_info.ppEnabledLayerNames = validation_layers;
  instance_create_info.enabledExtensionCount = total_extension_count;
  instance_create_info.ppEnabledExtensionNames = enabled_extensions;

  VkInstance instance;
  VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &instance), "create_instance: Failed to createInstance");
  return instance;
}

VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance) {
  VkDebugUtilsMessengerCreateInfoEXT debug_msngr_create_info;
  populate_debug_msngr_create_info(&debug_msngr_create_info);

  PFN_vkCreateDebugUtilsMessengerEXT create_fn =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

  if (!create_fn) {
    fprintf(stderr, "Failed to load vkCreateDebugUtilsMessengerEXT\n");
    exit(1);
  }
  VkDebugUtilsMessengerEXT debug_messenger;
  VK_CHECK(create_fn(instance, &debug_msngr_create_info, NULL, &debug_messenger),
           "create_instance: Failed to create VkDebugUtilsMessengerEXT");
  return debug_messenger;
}

VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow *window) {
  // https://www.glfw.org/docs/latest/vulkan_guide.html#vulkan_surface
  VkSurfaceKHR surface;
  VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface),
           "create_surface: Failed to createWindowSurface\n");
  return surface;
}

u32 get_physical_devices(VkInstance instance, VkPhysicalDevice *physical_devices) {
  u32 device_count;
  VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, NULL),
           "get_physical_devices, failed to vkEnumeratePhysicalDevices");
  if (device_count == 0) {
    fprintf(stderr, "get_physical_devices: Failed to find a physical device. "
                    "device_count = 0\n");
    exit(1);
  }

  if (device_count > MAX_PHYSICAL_DEVICES) {
    printf("get_physical_devices: Found more physical devices (%u) than "
           "MAX_PHYSICAL_DEVICES (%u)\n",
           device_count, MAX_PHYSICAL_DEVICES);
    device_count = MAX_PHYSICAL_DEVICES;
  }

  VK_CHECK(vkEnumeratePhysicalDevices(instance, &device_count, physical_devices),
           "get_physical_devices, failed to vkEnumeratePhysicalDevices");

  return device_count;
}

QueueFamilyIndices queue_family_indices_from_physical_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
  u32 queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);

  const i32 MAX_QUEUE_FAMILIES = 16;
  VkQueueFamilyProperties queue_families[MAX_QUEUE_FAMILIES];
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

  i32 graphics_family = -1;
  i32 present_family = -1;
  i32 compute_family = -1;

  if (queue_family_count > MAX_QUEUE_FAMILIES) {
    printf("is_suitable_physical_device: MAX_QUEUE_FAMILIES is %d but "
           "queue_family_count is %d\n",
           MAX_QUEUE_FAMILIES, queue_family_count);
    queue_family_count = MAX_QUEUE_FAMILIES;
  }

  for (u32 i = 0; i < queue_family_count; i++) {
    if (queue_families[i].queueCount <= 0) {
      continue;
    }

    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_family = i;
    }

    // Check for compute support
    if (queue_families[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
      compute_family = i;
    }

    // Check for presentation support
    VkBool32 present_support = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface, &present_support);
    if (present_support == VK_TRUE) {
      present_family = i;
    }
  }

  QueueFamilyIndices res;
  res.graphics_family = graphics_family;
  res.present_family = present_family;
  res.compute_family = compute_family;
  return res;
}

bool is_valid_queue_family_indices(QueueFamilyIndices indices) {
  bool is_invalid = indices.compute_family == -1 || indices.present_family == -1 || indices.graphics_family == -1;
  return !is_invalid;
}

VkPhysicalDevice pick_physical_device(VkInstance instance, QueueFamilyIndices *queue_family_indices,
                                      VkSurfaceKHR surface) {

  VkPhysicalDevice physical_devices[MAX_PHYSICAL_DEVICES];
  u32 num_physical_devices = get_physical_devices(instance, physical_devices);

  VkPhysicalDevice res = NULL;
  for (u32 i = 0; i < num_physical_devices; i++) {
    VkPhysicalDevice physical_device = physical_devices[i];
    *queue_family_indices = queue_family_indices_from_physical_device(physical_device, surface);

    if (!is_valid_queue_family_indices(*queue_family_indices)) {
      continue;
    }

    res = physical_device;
    VkPhysicalDeviceProperties props;
    vkGetPhysicalDeviceProperties(physical_device, &props);
    if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
      break;
    }
  }

  if (res == NULL) {
    fprintf(stderr, "pick_physical_device: result is NULL "
                    "failed to find suitable physical device\n");
    exit(1);
  }

  return res;
}

u32 get_unique_queue_infos(QueueFamilyIndices queue_family_indices, VkDeviceQueueCreateInfo *out_infos) {
  u32 num_unique_queue_families = 0;
  u32 indices[NUM_QUEUE_FAMILY_INDICES] = {
      (u32)queue_family_indices.compute_family,
      (u32)queue_family_indices.graphics_family,
      (u32)queue_family_indices.present_family,
  };

  for (u32 i = 0; i < NUM_QUEUE_FAMILY_INDICES; i++) {
    bool is_duplicate = false;
    for (u32 j = 0; j < i; j++) {
      if (indices[i] == indices[j]) {
        is_duplicate = true;
        break;
      }
    }

    if (is_duplicate) {
      continue;
    }

    f32 queue_priority = 1.0f;
    // https://registry.khronos.org/vulkan/specs/latest/man/html/VkDeviceQueueCreateInfo.html
    VkDeviceQueueCreateInfo device_queue_create_info;
    device_queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    device_queue_create_info.pNext = NULL;
    device_queue_create_info.flags = 0;
    device_queue_create_info.queueFamilyIndex = indices[i];
    device_queue_create_info.queueCount = 1;
    device_queue_create_info.pQueuePriorities = &queue_priority;
    out_infos[num_unique_queue_families++] = device_queue_create_info;
  }

  return num_unique_queue_families;
}

VkDevice create_device(QueueFamilyIndices queue_family_indices, VkPhysicalDevice physical_device) {

  VkDeviceQueueCreateInfo queue_create_infos[NUM_QUEUE_FAMILY_INDICES];
  u32 num_unique_queue_families = get_unique_queue_infos(queue_family_indices, queue_create_infos);

  const char *device_extensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME, // "VK_KHR_swapchain"
                                     VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME};
  VkPhysicalDeviceFeatures device_features = {};

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkDeviceCreateInfo.html
  VkDeviceCreateInfo device_create_info;
  device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  device_create_info.pNext = NULL;
  device_create_info.flags = 0;
  device_create_info.queueCreateInfoCount = num_unique_queue_families;
  device_create_info.pQueueCreateInfos = queue_create_infos;
  // enabledLayerCount is deprecated and should not be used
  device_create_info.enabledLayerCount = 0;
  // ppEnabledLayerNames is deprecated and should not be used
  device_create_info.ppEnabledLayerNames = NULL;
  device_create_info.enabledExtensionCount = 2;
  device_create_info.ppEnabledExtensionNames = device_extensions;
  device_create_info.pEnabledFeatures = &device_features;

  // https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateDevice.html
  VkDevice device;
  VK_CHECK(vkCreateDevice(physical_device, &device_create_info, NULL, &device),
           "create_device: Failed to createDevice");
  return device;
}

VkSurfaceFormatKHR get_surface_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {

  u32 format_count = 0;
  const u32 max_format_count = 128;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, NULL);

  if (format_count > max_format_count) {
    printf("get_surface_format: found more formats than expected. Max is %d, "
           "found %d. Clamping.\n",
           max_format_count, format_count);
    format_count = max_format_count;
  }
  VkSurfaceFormatKHR formats[max_format_count];
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, formats);

  VkSurfaceFormatKHR surface_format = formats[0];
  for (u32 i = 0; i < format_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = formats[i];
      break;
    }
  }

  return surface_format;
}

VkExtent2D get_swapchain_extent(GLFWwindow *window, VkSurfaceCapabilitiesKHR surface_capabilities) {
  VkExtent2D extent;
  i32 window_width, window_height;
  glfwGetFramebufferSize(window, &window_width, &window_height);
  if (surface_capabilities.currentExtent.width != UINT32_MAX) {
    extent = surface_capabilities.currentExtent;
  } else {
    extent.width =
        clamp_u32(window_width, surface_capabilities.minImageExtent.width, surface_capabilities.maxImageExtent.width);
    extent.height = clamp_u32(window_height, surface_capabilities.minImageExtent.height,
                              surface_capabilities.maxImageExtent.height);
  }
  return extent;
}

VkSurfaceCapabilitiesKHR get_surface_capabilities(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities),
           "create_swapchain: Failed to vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
  return surface_capabilities;
}

VkSwapchainKHR create_swapchain(VkDevice device, VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                                QueueFamilyIndices queue_family_indices, VkSurfaceFormatKHR surface_format,
                                VkSurfaceCapabilitiesKHR surface_capabilities, VkExtent2D swapchain_extent) {
  u32 image_count = surface_capabilities.minImageCount + 1;
  if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
    image_count = surface_capabilities.maxImageCount;
  }

  u32 present_mode_count = 0;
  const u32 max_present_mode_count = 16;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);
  VkPresentModeKHR present_modes[max_present_mode_count];
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes);
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

  if (present_mode_count > max_present_mode_count) {
    present_mode_count = max_present_mode_count;
  }
  for (u32 i = 0; i < present_mode_count; i++) {
    if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = present_modes[i];
      break;
    }
  }

  u32 queue_family_indices_array[] = {
      (u32)queue_family_indices.graphics_family,
      (u32)queue_family_indices.present_family,
  };

  VkSwapchainCreateInfoKHR swapchain_create_info;
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSwapchainCreateInfoKHR.html
  swapchain_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  swapchain_create_info.pNext = NULL;
  swapchain_create_info.flags = 0;
  swapchain_create_info.surface = surface;
  swapchain_create_info.minImageCount = image_count;
  swapchain_create_info.imageFormat = surface_format.format;
  swapchain_create_info.imageColorSpace = surface_format.colorSpace;
  swapchain_create_info.imageExtent = swapchain_extent;
  swapchain_create_info.imageArrayLayers = 1;
  swapchain_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (queue_family_indices.graphics_family != queue_family_indices.present_family) {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_create_info.queueFamilyIndexCount = 2;
    swapchain_create_info.pQueueFamilyIndices = queue_family_indices_array;
  } else {
    swapchain_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_create_info.queueFamilyIndexCount = 0;
    swapchain_create_info.pQueueFamilyIndices = NULL;
  }
  swapchain_create_info.preTransform = surface_capabilities.currentTransform;
  swapchain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchain_create_info.presentMode = present_mode;
  swapchain_create_info.clipped = VK_TRUE;
  swapchain_create_info.oldSwapchain = VK_NULL_HANDLE;

  // https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateSwapchainKHR.html
  VkSwapchainKHR swapchain;
  VK_CHECK(vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain),
           "create_swapchain: Failed to createSwapchain");
  return swapchain;
}

void transition_image_layout(VkCommandBuffer command_buffer, VkImage image, VkImageLayout old_layout,
                             VkImageLayout new_layout) {
  // printf("Transitioning image %p: %d -> %d\n", (void *)image, old_layout,
  // new_layout);

  VkImageMemoryBarrier image_memory_barrier;
  image_memory_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  image_memory_barrier.pNext = NULL;
  image_memory_barrier.srcAccessMask = 0;
  image_memory_barrier.dstAccessMask = 0;
  image_memory_barrier.oldLayout = old_layout;
  image_memory_barrier.newLayout = new_layout;
  image_memory_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_memory_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  image_memory_barrier.image = image;
  image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  image_memory_barrier.subresourceRange.baseMipLevel = 0;
  image_memory_barrier.subresourceRange.levelCount = 1;
  image_memory_barrier.subresourceRange.baseArrayLayer = 0;
  image_memory_barrier.subresourceRange.layerCount = 1;

  // TODO I don't yet understand these transition cases
  VkPipelineStageFlags src_stage, dst_stage;
  if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {

    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;

  } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {

    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

    image_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;

  } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

    image_memory_barrier.srcAccessMask = 0;                         // Nothing is waiting on the old content
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT; // Will be read by shaders

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;     // Nothing needs to happen before
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // Fragment shader will read it

  } else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED &&
             new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {

    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    image_memory_barrier.srcAccessMask = 0;
    image_memory_barrier.dstAccessMask =
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

  } else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {

    image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  } else if (old_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR) {

    image_memory_barrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    image_memory_barrier.dstAccessMask = 0; // Presentation engine just needs layout

    src_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dst_stage = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

  } else if (old_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
             new_layout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL) {

    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    image_memory_barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    image_memory_barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    src_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    dst_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  } else {
    assert(false && "transition_image_layout: Unsupported layout transition");
  }

  // TODO what are all these arguments?
  vkCmdPipelineBarrier(command_buffer, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
}

VkImageView create_default_image_view(const VulkanContext *context, VkImage image, VkFormat format,
                                      VkImageAspectFlags aspect_flags) {
  VkImageViewCreateInfo image_view_create_info;
  image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  image_view_create_info.pNext = NULL;
  image_view_create_info.flags = 0;
  image_view_create_info.image = image;
  image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
  image_view_create_info.format = format;
  image_view_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
  image_view_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
  // TODO allow for depth and stencil attachments
  image_view_create_info.subresourceRange.aspectMask = aspect_flags;
  image_view_create_info.subresourceRange.baseMipLevel = 0;
  image_view_create_info.subresourceRange.levelCount = 1;
  image_view_create_info.subresourceRange.baseArrayLayer = 0;
  image_view_create_info.subresourceRange.layerCount = 1;

  VkImageView image_view;
  VK_CHECK(vkCreateImageView(context->device, &image_view_create_info, NULL, &image_view),
           "create_default_image_view: Failed to vkCreateImageView");

  return image_view;
}

VkImage create_default_image(const VulkanContext *context, u32 width, u32 height, VkImageUsageFlags usage,
                             VkFormat format) {

  // TODO better understand all these fields
  // When do I need more samples? Need mipmap support
  // don't understand queue families
  // what is needed for a texture image vs other images?
  VkImageCreateInfo texture_image_info;
  texture_image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  texture_image_info.pNext = 0;
  texture_image_info.flags = 0;
  texture_image_info.imageType = VK_IMAGE_TYPE_2D;
  // TODO fallback for HW where formats are not supported?
  texture_image_info.format = format;
  texture_image_info.extent.width = width;
  texture_image_info.extent.height = height;
  texture_image_info.extent.depth = 1;
  texture_image_info.mipLevels = 1;
  texture_image_info.arrayLayers = 1;
  texture_image_info.samples = VK_SAMPLE_COUNT_1_BIT;
  texture_image_info.tiling = VK_IMAGE_TILING_OPTIMAL;
  texture_image_info.usage = usage;
  texture_image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  texture_image_info.queueFamilyIndexCount = 0;
  texture_image_info.pQueueFamilyIndices = NULL;
  texture_image_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

  VkImage image;
  VK_CHECK(vkCreateImage(context->device, &texture_image_info, NULL, &image),
           "create_default_image: failed to vkCreateImage");

  return image;
}

VkDeviceMemory allocate_and_bind_image_memory(const VulkanContext *context, VkImage image) {
  // create memory
  VkMemoryRequirements memory_requirements;
  vkGetImageMemoryRequirements(context->device, image, &memory_requirements);

  VkMemoryAllocateInfo memory_allocate_info;
  memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_allocate_info.pNext = 0;
  memory_allocate_info.allocationSize = memory_requirements.size;
  memory_allocate_info.memoryTypeIndex = find_memory_type(context->physical_device, memory_requirements.memoryTypeBits,
                                                          VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  VkDeviceMemory device_memory;
  VK_CHECK(vkAllocateMemory(context->device, &memory_allocate_info, NULL, &device_memory),
           "allocate_and_bind_image_memory: failed to vkAllocateMemory");

  VK_CHECK(vkBindImageMemory(context->device, image, device_memory, 0),
           "allocate_and_bind_image_memory: failed to vkBindImageMemory");

  return device_memory;
}

DepthBuffer create_depth_buffer(VulkanContext *context) {
  DepthBuffer depth_buffer;

  // TODO depth query formats
  VkFormat depth_format = VK_FORMAT_D32_SFLOAT;
  depth_buffer.image = create_default_image(context, context->swapchain_extent.width, context->swapchain_extent.height,
                                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, depth_format);

  depth_buffer.device_memory = allocate_and_bind_image_memory(context, depth_buffer.image);

  depth_buffer.image_view =
      create_default_image_view(context, depth_buffer.image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

  VkCommandBuffer command_buffer = begin_single_use_command_buffer(context);
  transition_image_layout(command_buffer, depth_buffer.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
  end_single_use_command_buffer(context, command_buffer);

  return depth_buffer;
}

SwapchainStorage create_swapchain_storage(VulkanContext *context) {
  SwapchainStorage storage;
  storage.use_static = true;

  u32 swapchain_image_count;
  VkResult result = vkGetSwapchainImagesKHR(context->device, context->swapchain, &swapchain_image_count, NULL);

  if (swapchain_image_count > NUM_SWAPCHAIN_IMAGES) {
    storage.use_static = false;
    storage.as.dynamic_storage.images = (VkImage *)malloc(swapchain_image_count * sizeof(VkImage));
    storage.as.dynamic_storage.image_views = (VkImageView *)malloc(swapchain_image_count * sizeof(VkImageView));

    if (!storage.as.dynamic_storage.images || !storage.as.dynamic_storage.image_views) {
      fprintf(stderr, "Failed to allocate memory for swapchain storage\n");
      exit(1);
    }
    result = vkGetSwapchainImagesKHR(context->device, context->swapchain, &swapchain_image_count,
                                     storage.as.dynamic_storage.images);
  } else {
    result = vkGetSwapchainImagesKHR(context->device, context->swapchain, &swapchain_image_count,
                                     storage.as.static_storage.images);
  }

  VK_CHECK(result, "get_swapchain_images: Failed to vkGetSwapchainImagesKHR");

  for (u32 i = 0; i < swapchain_image_count; i++) {

    if (storage.use_static) {
      VkImage image = storage.as.static_storage.images[i];

      storage.as.static_storage.image_views[i] =
          create_default_image_view(context, image, context->surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT);
    } else {

      VkImage image = storage.as.dynamic_storage.images[i];

      storage.as.dynamic_storage.image_views[i] =
          create_default_image_view(context, image, context->surface_format.format, VK_IMAGE_ASPECT_COLOR_BIT);
    }
  }

  storage.image_count = swapchain_image_count;

  for (u32 i = 0; i < NUM_SWAPCHAIN_IMAGES; i++) {
    storage.depth_buffers[i] = create_depth_buffer(context);
  }

  return storage;
}

VkAttachmentDescription make_color_attachment(VkFormat format) {
  VkAttachmentDescription color_attachment;
  color_attachment.flags = 0;
  color_attachment.format = format;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  return color_attachment;
}

VkAttachmentDescription make_depth_attachment(VkFormat format) {
  VkAttachmentDescription depth_attachment;
  depth_attachment.flags = 0;
  depth_attachment.format = format;
  depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  return depth_attachment;
}

VkRenderPass create_render_pass(VkDevice device, u32 num_attachment_descriptions,
                                const VkAttachmentDescription *attachment_descriptions, u32 num_subpass_descriptions,
                                const VkSubpassDescription *subpass_descriptions, u32 num_dependencies,
                                const VkSubpassDependency *dependencies) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkRenderPassCreateInfo.html
  VkRenderPassCreateInfo render_pass_create_info;
  render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_create_info.pNext = NULL;
  render_pass_create_info.flags = 0;
  render_pass_create_info.attachmentCount = num_attachment_descriptions;
  render_pass_create_info.pAttachments = attachment_descriptions;
  render_pass_create_info.subpassCount = num_subpass_descriptions;
  render_pass_create_info.pSubpasses = subpass_descriptions;
  render_pass_create_info.dependencyCount = num_dependencies;
  render_pass_create_info.pDependencies = dependencies;

  VkRenderPass render_pass;
  VK_CHECK(vkCreateRenderPass(device, &render_pass_create_info, NULL, &render_pass),
           "create_render_pass: Failed to vkCreateRenderPass");
  return render_pass;
}

VkRenderPass create_color_depth_render_pass(VkDevice device, VkFormat format) {
  const u32 num_attachments = 2;
  VkAttachmentDescription color_attachment = make_color_attachment(format);
  VkAttachmentDescription depth_attachment = make_depth_attachment(VK_FORMAT_D32_SFLOAT);

  VkAttachmentDescription attachment_descriptions[num_attachments] = {color_attachment, depth_attachment};

  VkAttachmentReference color_attachment_ref;
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_attachment_ref;
  depth_attachment_ref.attachment = 1;
  depth_attachment_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSubpassDescription.html
  VkSubpassDescription subpass;
  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = NULL;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.colorAttachmentCount = 1;
  subpass.pResolveAttachments = NULL;
  subpass.pDepthStencilAttachment = &depth_attachment_ref;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

  VkSubpassDependency dependency;
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  return create_render_pass(device, num_attachments, attachment_descriptions, 1, &subpass, 1, &dependency);
}

VkRenderPass create_color_render_pass(VkDevice device, VkFormat format) {
  const u32 num_attachments = 1;
  VkAttachmentDescription color_attachment = make_color_attachment(format);

  VkAttachmentDescription attachment_descriptions[num_attachments] = {color_attachment};

  VkAttachmentReference color_attachment_ref;
  color_attachment_ref.attachment = 0;
  color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSubpassDescription.html
  VkSubpassDescription subpass;
  subpass.flags = 0;
  subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass.inputAttachmentCount = 0;
  subpass.pInputAttachments = NULL;
  subpass.pColorAttachments = &color_attachment_ref;
  subpass.colorAttachmentCount = 1;
  subpass.pResolveAttachments = NULL;
  subpass.pDepthStencilAttachment = NULL;
  subpass.preserveAttachmentCount = 0;
  subpass.pPreserveAttachments = NULL;

  VkSubpassDependency dependency;
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  dependency.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  dependency.dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

  return create_render_pass(device, num_attachments, attachment_descriptions, 1, &subpass, 1, &dependency);
}

VkFramebuffer create_framebuffer(VkDevice device, VkRenderPass render_pass, u32 num_attachments,
                                 VkImageView *image_view_attachments, VkExtent2D extent) {

  VkFramebufferCreateInfo framebuffer_create_info;
  framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_create_info.pNext = NULL;
  framebuffer_create_info.flags = 0;
  framebuffer_create_info.renderPass = render_pass;
  framebuffer_create_info.attachmentCount = num_attachments;
  framebuffer_create_info.pAttachments = image_view_attachments;
  framebuffer_create_info.width = extent.width;
  framebuffer_create_info.height = extent.height;
  framebuffer_create_info.layers = 1;

  VkFramebuffer framebuffer;
  VK_CHECK(vkCreateFramebuffer(device, &framebuffer_create_info, NULL, &framebuffer),
           "create_framebuffer: Failed to vkCreateFramebuffer");
  return framebuffer;
}

void init_framebuffers(VulkanContext *context) {
  for (u32 i = 0; i < context->swapchain_storage.image_count; i++) {

    VkImageView color_view = context->swapchain_storage.use_static
                                 ? context->swapchain_storage.as.static_storage.image_views[i]
                                 : context->swapchain_storage.as.dynamic_storage.image_views[i];

    VkImageView views[] = {color_view, context->swapchain_storage.depth_buffers[i].image_view};

    context->framebuffers[i] =
        create_framebuffer(context->device, context->render_pass, NUM_ATTACHMENTS, views, context->swapchain_extent);
  }
}

void recreate_swapchain(VulkanContext *context) {
  vkDeviceWaitIdle(context->device);
  destroy_swapchain(context);

  context->swapchain =
      create_swapchain(context->device, context->physical_device, context->surface, context->queue_family_indices,
                       context->surface_format, context->surface_capabilities, context->swapchain_extent);
  create_swapchain_storage(context);
  init_framebuffers(context);
}

void create_frame_sync_objects(VkDevice device, FrameSyncObjects *frame_sync_objects) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSemaphoreCreateInfo.html
  VkSemaphoreCreateInfo semaphore_create_info;
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = NULL;
  semaphore_create_info.flags = 0;

  VkFenceCreateInfo fence_create_info;
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.pNext = NULL;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK_VARIADIC(
        vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_sync_objects[i].image_available_semaphore),
        "create_frame_sync_objects: Failed to create image_available_semaphore "
        "%d",
        i);

    VK_CHECK_VARIADIC(
        vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_sync_objects[i].render_finished_semaphore),
        "create_frame_sync_objects: Failed to create render_finished_semaphore "
        "%d",
        i);

    VK_CHECK_VARIADIC(vkCreateFence(device, &fence_create_info, NULL, &frame_sync_objects[i].in_flight_fence),
                      "create_frame_sync_objects: Failed to create in_flight_fence %d", i);
  }
}

VkCommandPool create_command_pool(VkDevice device, u32 queue_family_index, bool transient) {
  VkCommandPoolCreateInfo command_pool_create_info;
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.pNext = NULL;
  command_pool_create_info.flags =
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | (transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0);
  command_pool_create_info.queueFamilyIndex = queue_family_index;

  VkCommandPool command_pool;
  VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, NULL, &command_pool),
           "create_command_pool: failed to vkCreateCommandPool");
  return command_pool;
}

void allocate_command_buffers(VkDevice device, VkCommandPool command_pool, u32 command_buffer_count,
                              VkCommandBuffer *command_buffers) {
  VkCommandBufferAllocateInfo allocate_info;
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.pNext = 0;
  allocate_info.commandPool = command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = command_buffer_count;

  VK_CHECK(vkAllocateCommandBuffers(device, &allocate_info, command_buffers),
           "allocate_command_buffer: Failed to vkAllocateCommandBuffers");
}

VkPipelineCache create_pipeline_cache(VkDevice device) {
  // TODO add serialization to disk
  VkPipelineCache pipeline_cache;
  VkPipelineCacheCreateInfo pipeline_cache_create_info;
  pipeline_cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  pipeline_cache_create_info.pNext = NULL;
  pipeline_cache_create_info.flags = 0;
  pipeline_cache_create_info.initialDataSize = 0;
  pipeline_cache_create_info.pInitialData = NULL;
  VK_CHECK(vkCreatePipelineCache(device, &pipeline_cache_create_info, NULL, &pipeline_cache),
           "create_pipeline_cache: Failed to vkCreatePipelineLayout");
  return pipeline_cache;
}

VkVertexInputBindingDescription create_instanced_vertex_binding_description(u32 binding, u32 stride) {
  VkVertexInputBindingDescription description;
  description.binding = binding;
  description.stride = stride;
  description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
  return description;
}

VkVertexInputBindingDescription create_vertex_binding_description(u32 binding, u32 stride) {
  VkVertexInputBindingDescription description;
  description.binding = binding;
  description.stride = stride;
  description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return description;
}

VkVertexInputAttributeDescription create_vertex_attribute_description(u32 location, u32 binding, VkFormat format,
                                                                      u32 offset) {
  VkVertexInputAttributeDescription description;
  description.location = location;
  description.binding = binding;
  description.format = format;
  description.offset = offset;
  return description;
}

VkPipelineShaderStageCreateInfo create_shader_stage_info(VkShaderModule module, VkShaderStageFlagBits stage,
                                                         const char *entry_point) {
  VkPipelineShaderStageCreateInfo shader_stage_create_info;
  shader_stage_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_info.pNext = NULL;
  shader_stage_create_info.flags = 0;
  shader_stage_create_info.stage = stage;
  shader_stage_create_info.module = module;
  shader_stage_create_info.pName = entry_point;
  shader_stage_create_info.pSpecializationInfo = NULL;
  return shader_stage_create_info;
}

PipelineConfig create_default_graphics_pipeline_config(VkRenderPass render_pass, VkShaderModule vertex_shader,
                                                       VkShaderModule fragment_shader,
                                                       const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
                                                       VkPipelineLayout pipeline_layout) {

  VkPipelineShaderStageCreateInfo pipeline_shader_create_info_vertex =
      create_shader_stage_info(vertex_shader, VK_SHADER_STAGE_VERTEX_BIT, "main");
  VkPipelineShaderStageCreateInfo pipeline_shader_create_info_fragment =
      create_shader_stage_info(fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT, "main");

  PipelineConfig pipeline_config;
  pipeline_config.stages[0] = pipeline_shader_create_info_vertex;
  pipeline_config.stages[1] = pipeline_shader_create_info_fragment;
  pipeline_config.stage_count = 2;
  pipeline_config.vertex_input_state_create_info = vertex_input_state;
  pipeline_config.render_pass = render_pass;
  pipeline_config.pipeline_layout = pipeline_layout;
  pipeline_config.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipeline_config.primitive_restart_enabled = VK_FALSE;
  pipeline_config.polygon_mode = VK_POLYGON_MODE_FILL;
  pipeline_config.cull_mode = VK_CULL_MODE_NONE;
  pipeline_config.front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE;
  pipeline_config.sample_count_flag = VK_SAMPLE_COUNT_1_BIT;
  pipeline_config.blend_mode = BLEND_MODE_ALPHA;
  return pipeline_config;
}

VulkanContext create_vulkan_context(const char *title) {
  VulkanContext context;
  context.window = new_window(true /* is_vulkan*/, title);
  glfwGetFramebufferSize(context.window, &context.window_framebuffer_width, &context.window_framebuffer_height);

  context.instance = create_instance(title);
#ifdef TUKE_DISABLE_VULKAN_VALIDATION
  context.debug_messenger = VK_NULL_HANDLE;
#else
  context.debug_messenger = create_debug_messenger(context.instance);
#endif
  context.surface = create_surface(context.instance, context.window);
  context.physical_device = pick_physical_device(context.instance, &context.queue_family_indices, context.surface);
  vkGetPhysicalDeviceProperties(context.physical_device, &context.physical_device_properties);

  context.queue_family_indices = queue_family_indices_from_physical_device(context.physical_device, context.surface);
  context.present_queue_index_is_different_than_graphics =
      (context.queue_family_indices.graphics_family != context.queue_family_indices.present_family);
  context.compute_queue_index_is_different_than_graphics =
      (context.queue_family_indices.graphics_family != context.queue_family_indices.compute_family);

  context.device = create_device(context.queue_family_indices, context.physical_device);
  context.surface_format = get_surface_format(context.physical_device, context.surface);

  create_frame_sync_objects(context.device, context.frame_sync_objects);
  context.current_frame = 0;
  context.current_frame_index = 0;

  // boolean argument is is_transient command pool
  context.transient_command_pool =
      create_command_pool(context.device, context.queue_family_indices.graphics_family, true);
  context.graphics_command_pool =
      create_command_pool(context.device, context.queue_family_indices.graphics_family, false);

  if (context.compute_queue_index_is_different_than_graphics) {
    context.compute_command_pool =
        create_command_pool(context.device, context.queue_family_indices.compute_family, false);
  } else {
    context.compute_command_pool = context.graphics_command_pool;
  }

  if (context.present_queue_index_is_different_than_graphics) {
    context.present_command_pool =
        create_command_pool(context.device, context.queue_family_indices.present_family, false);
  } else {
    context.present_command_pool = context.graphics_command_pool;
  }

  // swapchain creation needs to happen after the transient command pool is
  // allocated because depth buffer creation requires an image transition
  context.surface_capabilities = get_surface_capabilities(context.physical_device, context.surface);
  context.swapchain_extent = get_swapchain_extent(context.window, context.surface_capabilities);
  context.swapchain =
      create_swapchain(context.device, context.physical_device, context.surface, context.queue_family_indices,
                       context.surface_format, context.surface_capabilities, context.swapchain_extent);

  vkGetDeviceQueue(context.device, context.queue_family_indices.graphics_family, 0, &context.graphics_queue);
  vkGetDeviceQueue(context.device, context.queue_family_indices.present_family, 0, &context.present_queue);
  vkGetDeviceQueue(context.device, context.queue_family_indices.compute_family, 0, &context.compute_queue);
  context.swapchain_storage = create_swapchain_storage(&context);

  // TODO sync this with init_framebuffers()
  // don't need a depth attachment here
  context.render_pass = create_color_depth_render_pass(context.device, context.surface_format.format);

  init_framebuffers(&context);

  // regular command buffers need to be created after the swapchain, which
  // depends on the transient command bufers
  allocate_command_buffers(context.device, context.graphics_command_pool, context.swapchain_storage.image_count,
                           context.graphics_command_buffers);

  allocate_command_buffers(context.device, context.compute_command_pool, context.swapchain_storage.image_count,
                           context.compute_command_buffers);

  context.pipeline_cache = create_pipeline_cache(context.device);
  context.shader_cache = create_shader_cache(context.device);

  VK_CHECK(vkWaitForFences(context.device, 1, &context.frame_sync_objects[context.current_frame_index].in_flight_fence,
                           VK_TRUE, UINT64_MAX),
           "new_vulkan_context: Failed to vkWaitForFences");

  return context;
}

u32 find_memory_type(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_properties;

  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
  for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) && (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
      return i;
    }
  }

  fprintf(stderr, "Failed to find suitable memory type.\n");
  exit(1);
}

VulkanBuffer create_buffer_explicit(const VulkanContext *context, VkBufferUsageFlags usage, VkDeviceSize size,
                                    VkMemoryPropertyFlags properties) {
  VulkanBuffer vulkan_buffer;

  VkBufferCreateInfo buffer_create_info;
  buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buffer_create_info.pNext = 0;
  buffer_create_info.flags = 0;
  buffer_create_info.size = size;
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkBufferUsageFlagBits.html
  buffer_create_info.usage = usage;
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSharingMode.html
  // TODO learn when I might not want exclusive sharing
  buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_create_info.queueFamilyIndexCount = 0;
  buffer_create_info.pQueueFamilyIndices = NULL;

  VkBuffer buffer;
  VK_CHECK(vkCreateBuffer(context->device, &buffer_create_info, NULL, &buffer), "Failed to create staging buffer");
  vulkan_buffer.buffer = buffer;

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(context->device, buffer, &memory_requirements);
  vulkan_buffer.memory_requirements = memory_requirements;

  VkMemoryAllocateInfo memory_allocate_info;
  memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_allocate_info.pNext = 0;
  memory_allocate_info.allocationSize = memory_requirements.size;
  memory_allocate_info.memoryTypeIndex =
      find_memory_type(context->physical_device, memory_requirements.memoryTypeBits, properties);

  VK_CHECK(vkAllocateMemory(context->device, &memory_allocate_info, NULL, &vulkan_buffer.memory),
           "create_buffer: failed to vkAllocateMemory");

  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(context->physical_device, &memory_properties);
  vulkan_buffer.memory_property_flags =
      memory_properties.memoryTypes[memory_allocate_info.memoryTypeIndex].propertyFlags;

  VK_CHECK(vkBindBufferMemory(context->device, buffer, vulkan_buffer.memory, 0),
           "create_buffer: Failed to vkBindBufferMemory");

  return vulkan_buffer;
}

VulkanBuffer create_buffer(const VulkanContext *context, BufferType buffer_type, VkDeviceSize size) {
  switch (buffer_type) {
  case BUFFER_TYPE_VERTEX: {
    VkBufferUsageFlags vertex_buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags vertex_buffer_memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer_explicit(context, vertex_buffer_usage, size, vertex_buffer_memory_properties);
  }

  case BUFFER_TYPE_INDEX: {
    VkBufferUsageFlags index_buffer_usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags index_buffer_memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer_explicit(context, index_buffer_usage, size, index_buffer_memory_properties);
  }

  case BUFFER_TYPE_STAGING: {
    VkBufferUsageFlags staging_buffer_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags staging_buffer_memory_properties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer_explicit(context, staging_buffer_usage, size, staging_buffer_memory_properties);
  }

    // TODO expand for dynamic uniform buffers, or add a second helper
  case BUFFER_TYPE_UNIFORM: {
    VkBufferUsageFlags uniform_buffer_usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkMemoryPropertyFlags uniform_buffer_memory_properties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer_explicit(context, uniform_buffer_usage, size, uniform_buffer_memory_properties);
  }

  case BUFFER_TYPE_COHERENT_STREAMING: {
    VkBufferUsageFlags streaming_buffer_usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    VkMemoryPropertyFlags streaming_buffer_memory_properties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer_explicit(context, streaming_buffer_usage, size, streaming_buffer_memory_properties);
  }

  case BUFFER_TYPE_READONLY_STORAGE: {
    VkBufferUsageFlags readonly_storage_buffer_usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags readonly_storage_buffer_memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer_explicit(context, readonly_storage_buffer_usage, size,
                                  readonly_storage_buffer_memory_properties);
  }

  case BUFFER_TYPE_READ_WRITE_STORAGE: {
    VkBufferUsageFlags read_write_storage_buffer_usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags read_write_storage_buffer_memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer_explicit(context, read_write_storage_buffer_usage, size,
                                  read_write_storage_buffer_memory_properties);
  }

  default:
    assert(false && "create_buffer: got an invalid buffer_type");
  }
}

void write_to_vulkan_buffer(const VulkanContext *context, const void *src_data, VkDeviceSize size, VkDeviceSize offset,
                            VulkanBuffer vulkan_buffer) {
  if (!(vulkan_buffer.memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
    fprintf(stderr, "write_to_vulkan_buffer: attempting to write to host "
                    "invisible memory\n");
    exit(1);
  }

  VkDeviceSize aligned_offset = offset;
  VkDeviceSize aligned_size = size;
  VkDeviceSize atom_size = context->physical_device_properties.limits.nonCoherentAtomSize;
  bool not_coherent = !(vulkan_buffer.memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (not_coherent) {
    aligned_offset = offset & ~(atom_size - 1);
    aligned_size = ((offset + size + atom_size - 1) & ~(atom_size - 1)) - aligned_offset;
  }

  void *dest_data;
  VK_CHECK(vkMapMemory(context->device, vulkan_buffer.memory, aligned_offset, aligned_size, 0, &dest_data),
           "write_to_vulkan_buffer: Failed to VkMapMemory");
  memcpy((char *)dest_data + (offset - aligned_offset), src_data, size);

  if (not_coherent) {
    VkMappedMemoryRange range;
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.pNext = NULL;
    range.memory = vulkan_buffer.memory;
    range.offset = aligned_offset;
    range.size = aligned_size;

    VK_CHECK(vkFlushMappedMemoryRanges(context->device, 1, &range),
             "write_to_vulkan_buffer: Failed to vkFlushMappedMemoryRanges");
  }

  vkUnmapMemory(context->device, vulkan_buffer.memory);
}

void destroy_vulkan_buffer(const VulkanContext *context, VulkanBuffer buffer) {
  vkDestroyBuffer(context->device, buffer.buffer, NULL);
  vkFreeMemory(context->device, buffer.memory, NULL);
}

VkCommandBuffer begin_single_use_command_buffer(const VulkanContext *context) {

  assert(context);
  assert(context->device != VK_NULL_HANDLE);
  assert(context->transient_command_pool != VK_NULL_HANDLE);

  VkCommandBufferAllocateInfo allocate_info;
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.pNext = 0;
  allocate_info.commandPool = context->transient_command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  VK_CHECK(vkAllocateCommandBuffers(context->device, &allocate_info, &command_buffer),
           "allocate_command_buffer: Failed to vkAllocateCommandBuffers");

  VkCommandBufferBeginInfo begin_info;
  begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begin_info.pNext = 0;
  begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  begin_info.pInheritanceInfo = NULL;
  VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info),
           "begin_single_use_command_buffer: Failed to vkBeginCommandBuffer");

  return command_buffer;
}

void end_single_use_command_buffer(const VulkanContext *context, VkCommandBuffer command_buffer) {
  VK_CHECK(vkEndCommandBuffer(command_buffer), "end_single_use_command_buffer: failed to vkEndCommandBuffer");

  VkSubmitInfo submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = 0;
  submit_info.waitSemaphoreCount = 0;
  submit_info.pWaitSemaphores = 0;
  submit_info.pWaitDstStageMask = 0;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 0;
  submit_info.pSignalSemaphores = 0;

  VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
           "end_single_use_command_buffer: failed to vkQueueSubmit");
  VK_CHECK(vkQueueWaitIdle(context->graphics_queue), "end_single_use_command_buffer: failed to vkQueueWaitIdle");

  vkFreeCommandBuffers(context->device, context->transient_command_pool, 1, &command_buffer);
}

VkShaderModule create_shader_module(VkDevice device, const u32 *code, u32 code_size) {
  VkShaderModuleCreateInfo shader_module_create_info;
  shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_create_info.pNext = NULL;
  shader_module_create_info.flags = 0;
  shader_module_create_info.codeSize = code_size;
  shader_module_create_info.pCode = code;

  VkShaderModule module;
  VK_CHECK(vkCreateShaderModule(device, &shader_module_create_info, NULL, &module),
           "create_shader_module: failed to vkCreateShaderModule");
  return module;
}

ViewportState create_viewport_state_offset(VkExtent2D swapchain_extent, VkOffset2D offset) {
  ViewportState viewport_state;

  viewport_state.viewport.x = offset.x;
  viewport_state.viewport.y = offset.y;
  viewport_state.viewport.width = (f32)swapchain_extent.width;
  viewport_state.viewport.height = (f32)swapchain_extent.height;
  viewport_state.viewport.minDepth = 0.0f;
  viewport_state.viewport.maxDepth = 1.0f;

  viewport_state.scissor.extent = swapchain_extent;
  viewport_state.scissor.offset.x = offset.x;
  viewport_state.scissor.offset.y = offset.y;

  return viewport_state;
}

ViewportState create_viewport_state_xy(VkExtent2D swapchain_extent, u32 x, u32 y) {
  VkOffset2D offset;
  offset.x = x;
  offset.y = y;
  return create_viewport_state_offset(swapchain_extent, offset);
}

VkPipelineInputAssemblyStateCreateInfo create_pipeline_input_assembly_state(VkPrimitiveTopology topology,
                                                                            VkBool32 primitive_restart_enabled) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkPipelineInputAssemblyStateCreateInfo.html
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info;
  input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state_create_info.pNext = NULL;
  input_assembly_state_create_info.flags = 0;
  input_assembly_state_create_info.topology = topology;
  input_assembly_state_create_info.primitiveRestartEnable = primitive_restart_enabled;
  return input_assembly_state_create_info;
}

VkPipelineRasterizationStateCreateInfo create_rasterization_state(VkPolygonMode polygon_mode, VkCullModeFlags cull_mode,
                                                                  VkFrontFace front_face) {
  VkPipelineRasterizationStateCreateInfo rasterization_state_create_info;
  rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterization_state_create_info.pNext = NULL;
  rasterization_state_create_info.flags = 0;
  // TODO Set depthClampEnable = VK_TRUE for shadow support
  rasterization_state_create_info.depthClampEnable = VK_FALSE;
  rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
  rasterization_state_create_info.polygonMode = polygon_mode;
  rasterization_state_create_info.cullMode = cull_mode;
  rasterization_state_create_info.frontFace = front_face;
  // TODO understand how this is used for shadow mapping
  rasterization_state_create_info.depthBiasEnable = VK_FALSE;
  rasterization_state_create_info.depthBiasConstantFactor = 0.0f;
  rasterization_state_create_info.depthBiasClamp = 0.0f;
  rasterization_state_create_info.depthBiasSlopeFactor = 0.0f;
  rasterization_state_create_info.lineWidth = 1.0f;

  return rasterization_state_create_info;
}

VkPipelineMultisampleStateCreateInfo create_multisample_state(VkSampleCountFlagBits sample_count_flag) {
  VkPipelineMultisampleStateCreateInfo multisample_state_create_info;
  multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisample_state_create_info.pNext = NULL;
  multisample_state_create_info.flags = 0;
  multisample_state_create_info.rasterizationSamples = sample_count_flag;
  // TODO if sample count flag is not VK_SAMPLE_COUNT_1_BIT, make this true?
  multisample_state_create_info.sampleShadingEnable = VK_FALSE;
  multisample_state_create_info.minSampleShading = 1.0f;
  multisample_state_create_info.pSampleMask = NULL;
  // TODO understand alpha to coverage
  multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
  multisample_state_create_info.alphaToOneEnable = VK_FALSE;
  return multisample_state_create_info;
}

VkPipelineDepthStencilStateCreateInfo create_depth_stencil_state() {

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info;
  depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depth_stencil_state_create_info.pNext = NULL;
  depth_stencil_state_create_info.flags = 0;
  depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
  depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;
  depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_LESS;
  depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
  depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;

  VkStencilOpState empty_stencil_op_state;
  memset(&empty_stencil_op_state, 0, sizeof(empty_stencil_op_state));
  depth_stencil_state_create_info.front = empty_stencil_op_state;
  depth_stencil_state_create_info.back = empty_stencil_op_state;
  depth_stencil_state_create_info.minDepthBounds = 0.0f;
  depth_stencil_state_create_info.maxDepthBounds = 1.0f;

  return depth_stencil_state_create_info;
}

VkPipelineColorBlendAttachmentState create_color_blend_attachment_state(BlendMode blend_mode) {
  VkPipelineColorBlendAttachmentState color_blend_attachment_state;
  VkBool32 blending_enabled = (blend_mode == BLEND_MODE_ALPHA) ? VK_TRUE : VK_FALSE;
  color_blend_attachment_state.blendEnable = blending_enabled;
  color_blend_attachment_state.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  switch (blend_mode) {

  case BLEND_MODE_ALPHA:
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment_state;

  case BLEND_MODE_OPAQUE:
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;
    return color_blend_attachment_state;
  default:
    assert(false && "create_blend_mode_state got invalid blend_mode");
  }
}

VkPipelineColorBlendStateCreateInfo
create_color_blend_state(u32 num_color_blend_attachments,
                         const VkPipelineColorBlendAttachmentState *color_blend_attachment_states) {
  VkPipelineColorBlendStateCreateInfo color_blend_state_create_info;
  color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_state_create_info.pNext = NULL;
  color_blend_state_create_info.flags = 0;
  color_blend_state_create_info.logicOpEnable = VK_FALSE;
  color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
  color_blend_state_create_info.attachmentCount = num_color_blend_attachments;
  color_blend_state_create_info.pAttachments = color_blend_attachment_states;
  f32 blend_constants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  memcpy(color_blend_state_create_info.blendConstants, blend_constants, sizeof(blend_constants));
  return color_blend_state_create_info;
}

// TODO Validate shader stages if you want strict pipeline guarantees
// TODO Print/log pipeline cache usage for debugging
VkPipeline create_graphics_pipeline(VkDevice device, const PipelineConfig *config, VkPipelineCache pipeline_cache) {

  assert(config->stage_count <= MAX_SHADER_STAGE_COUNT);
  VkGraphicsPipelineCreateInfo graphics_pipeline_create_info;
  graphics_pipeline_create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphics_pipeline_create_info.pNext = 0;
  graphics_pipeline_create_info.flags = 0;
  graphics_pipeline_create_info.stageCount = config->stage_count;
  graphics_pipeline_create_info.pStages = config->stages;

  assert(config->vertex_input_state_create_info && "create_graphics_pipeline: vertex_input_state_create_info is NULL. "
                                                   "Did you mean to fill out this structure manually?");
  graphics_pipeline_create_info.pVertexInputState = config->vertex_input_state_create_info;

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
      create_pipeline_input_assembly_state(config->topology, config->primitive_restart_enabled);
  graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state;

  // TOOD?
  graphics_pipeline_create_info.pTessellationState = NULL;

  VkOffset2D offset;
  offset.x = 0.0f;
  offset.y = 0.0f;
  ViewportState viewport_state = create_viewport_state_offset(config->swapchain_extent, offset);

  VkPipelineViewportStateCreateInfo viewport_state_create_info;
  viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state_create_info.pNext = NULL;
  viewport_state_create_info.flags = 0;
  viewport_state_create_info.viewportCount = 1;
  viewport_state_create_info.pViewports = &viewport_state.viewport;
  viewport_state_create_info.scissorCount = 1;
  viewport_state_create_info.pScissors = &viewport_state.scissor;
  graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;

  VkPipelineRasterizationStateCreateInfo rasterization_state =
      create_rasterization_state(config->polygon_mode, config->cull_mode, config->front_face);
  graphics_pipeline_create_info.pRasterizationState = &rasterization_state;

  VkPipelineMultisampleStateCreateInfo multisample_state_create_info =
      create_multisample_state(config->sample_count_flag);
  graphics_pipeline_create_info.pMultisampleState = &multisample_state_create_info;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = create_depth_stencil_state();
  graphics_pipeline_create_info.pDepthStencilState = &depth_stencil_state_create_info;

  const u32 num_color_blend_attachments = 1;
  VkPipelineColorBlendAttachmentState color_blend_state = create_color_blend_attachment_state(config->blend_mode);
  VkPipelineColorBlendStateCreateInfo color_blend_state_create_info =
      create_color_blend_state(num_color_blend_attachments, &color_blend_state);
  graphics_pipeline_create_info.pColorBlendState = &color_blend_state_create_info;

  // anticipating use of vkCmdSetViewport(), vkCmdSetScissor()
  const u32 num_dynamic_states = 2;
  VkDynamicState dynamic_states[num_dynamic_states] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state_create_info;
  dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_create_info.pNext = NULL;
  dynamic_state_create_info.flags = 0;
  dynamic_state_create_info.dynamicStateCount = num_dynamic_states;
  dynamic_state_create_info.pDynamicStates = dynamic_states;
  graphics_pipeline_create_info.pDynamicState = &dynamic_state_create_info;

  graphics_pipeline_create_info.layout = config->pipeline_layout;
  graphics_pipeline_create_info.renderPass = config->render_pass;
  graphics_pipeline_create_info.subpass = 0;
  graphics_pipeline_create_info.basePipelineHandle = VK_NULL_HANDLE;
  graphics_pipeline_create_info.basePipelineIndex = -1;

  VkPipeline graphics_pipeline;
  VK_CHECK(
      vkCreateGraphicsPipelines(device, pipeline_cache, 1, &graphics_pipeline_create_info, NULL, &graphics_pipeline),
      "create_graphics_pipeline: Failed to vkCreateGraphicsPipelines");
  return graphics_pipeline;
}

VkPipeline create_default_graphics_pipeline(const VulkanContext *context, VkRenderPass render_pass,
                                            VkShaderModule vertex_shader, VkShaderModule fragment_shader,
                                            const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
                                            VkPipelineLayout pipeline_layout) {

  PipelineConfig config = create_default_graphics_pipeline_config(render_pass, vertex_shader, fragment_shader,
                                                                  vertex_input_state, pipeline_layout);

  return create_graphics_pipeline(context->device, &config, context->pipeline_cache);
}

bool begin_frame(VulkanContext *context) {
  VK_CHECK(vkWaitForFences(context->device, 1,
                           &context->frame_sync_objects[context->current_frame_index].in_flight_fence, VK_TRUE,
                           UINT64_MAX),
           "begin_frame: failed to vkWaitForFences");

  vkResetFences(context->device, 1, &context->frame_sync_objects[context->current_frame_index].in_flight_fence);

  VkResult result =
      vkAcquireNextImageKHR(context->device, context->swapchain, UINT64_MAX,
                            context->frame_sync_objects[context->current_frame_index].image_available_semaphore,
                            VK_NULL_HANDLE, &context->image_index);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreate_swapchain(context);
    return false;
  }

  if (result == VK_ERROR_DEVICE_LOST) {
    fprintf(stderr, "begin_frame: result = VK_ERROR_DEVICE_LOST");
    exit(1);
  }

  return true;
}

VkCommandBuffer begin_command_buffer(const VulkanContext *context) {
  VkCommandBuffer command_buffer = context->graphics_command_buffers[context->image_index];
  assert(command_buffer != VK_NULL_HANDLE);

  VkCommandBufferBeginInfo command_buffer_begin_info;
  command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_begin_info.pNext = NULL;
  command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  command_buffer_begin_info.pInheritanceInfo = NULL;

  VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info),
           "begin_command_buffer: failed to vkBeginCommandBuffer");

  return command_buffer;
}

void begin_render_pass(const VulkanContext *context, VkCommandBuffer command_buffer, VkRenderPass render_pass,
                       VkFramebuffer framebuffer, const VkClearValue *clear_value, u32 clear_value_count,
                       VkOffset2D offset) {

  VkRenderPassBeginInfo render_pass_begin_info;
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.pNext = NULL;
  render_pass_begin_info.renderPass = render_pass;
  render_pass_begin_info.framebuffer = framebuffer;
  // context->framebuffers[context->image_index];
  render_pass_begin_info.renderArea.offset = offset;
  render_pass_begin_info.renderArea.extent = context->swapchain_extent;
  render_pass_begin_info.clearValueCount = clear_value_count;
  render_pass_begin_info.pClearValues = clear_value;

  vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
}

void submit_and_present(const VulkanContext *context, VkCommandBuffer command_buffer) {

  VkSubmitInfo submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = &context->frame_sync_objects[context->current_frame_index].image_available_semaphore;
  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = &context->frame_sync_objects[context->current_frame_index].render_finished_semaphore;

  VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &submit_info,
                         context->frame_sync_objects[context->current_frame_index].in_flight_fence),
           "sumbit_and_present: Failed to vkQueueSubmit");

  VkPresentInfoKHR present_info;
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = NULL;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = &context->frame_sync_objects[context->current_frame_index].render_finished_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &context->swapchain;
  present_info.pImageIndices = &context->image_index;
  present_info.pResults = NULL;

  // TODO handle swapchain recreation here if need be
  VK_CHECK(vkQueuePresentKHR(context->graphics_queue, &present_info),
           "sumbit_and_present: Failed to vkQueuePresentKHR");
}

VkPipelineVertexInputStateCreateInfo
create_vertex_input_state(u32 binding_description_count, const VkVertexInputBindingDescription *binding_descriptions,
                          u32 attribute_description_count,
                          const VkVertexInputAttributeDescription *attribute_descriptions) {

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info;
  vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertex_input_state_create_info.pNext = NULL;
  vertex_input_state_create_info.flags = 0;

  vertex_input_state_create_info.vertexBindingDescriptionCount = binding_description_count;
  vertex_input_state_create_info.pVertexBindingDescriptions = binding_descriptions;

  vertex_input_state_create_info.vertexAttributeDescriptionCount = attribute_description_count;
  vertex_input_state_create_info.pVertexAttributeDescriptions = attribute_descriptions;

  return vertex_input_state_create_info;
}

VkPipelineLayout create_pipeline_layout(VkDevice device, const VkDescriptorSetLayout *descriptor_set_layouts,
                                        u32 set_layout_count) {

  VkPipelineLayoutCreateInfo pipeline_layout_create_info;
  pipeline_layout_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.pNext = NULL;
  pipeline_layout_create_info.flags = 0;
  pipeline_layout_create_info.setLayoutCount = set_layout_count;
  pipeline_layout_create_info.pSetLayouts = descriptor_set_layouts;
  // TODO push constants
  pipeline_layout_create_info.pushConstantRangeCount = 0;
  pipeline_layout_create_info.pPushConstantRanges = NULL;

  VkPipelineLayout pipeline_layout;
  VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, NULL, &pipeline_layout),
           "create_graphics_pipeline: Failed to vkCreatePipelineLayout");
  return pipeline_layout;
}

// TODO VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
VkDescriptorPool create_descriptor_pool(VkDevice device, const VkDescriptorPoolSize *pool_sizes, u32 pool_size_count,
                                        u32 max_sets) {
  VkDescriptorPoolCreateInfo descriptor_pool_create_info;
  descriptor_pool_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_create_info.pNext = NULL;
  descriptor_pool_create_info.flags = 0;
  descriptor_pool_create_info.maxSets = max_sets;
  descriptor_pool_create_info.poolSizeCount = pool_size_count;
  descriptor_pool_create_info.pPoolSizes = pool_sizes;

  VkDescriptorPool descriptor_pool;
  VK_CHECK(vkCreateDescriptorPool(device, &descriptor_pool_create_info, NULL, &descriptor_pool),
           "create_descriptor_pool: Failed to vkCreateDescriptorPool");
  return descriptor_pool;
}

VkDescriptorSet create_descriptor_set(VkDevice device, VkDescriptorPool descriptor_pool,
                                      VkDescriptorSetLayout *descriptor_set_layout) {

  VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
  descriptor_set_allocate_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptor_set_allocate_info.pNext = NULL;
  descriptor_set_allocate_info.descriptorPool = descriptor_pool;
  descriptor_set_allocate_info.descriptorSetCount = 1;
  descriptor_set_allocate_info.pSetLayouts = descriptor_set_layout;

  VkDescriptorSet descriptor_set;
  VK_CHECK(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, &descriptor_set),
           "create_descriptor_set: failed to vkAllocateDescriptorSets");
  return descriptor_set;
}

// TODO VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
VkDescriptorSetLayout create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutBinding *bindings,
                                                   u32 binding_count) {

  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
  descriptor_set_layout_create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_layout_create_info.pNext = NULL;
  descriptor_set_layout_create_info.flags = 0;
  descriptor_set_layout_create_info.bindingCount = binding_count;
  descriptor_set_layout_create_info.pBindings = bindings;

  VkDescriptorSetLayout descriptor_set_layout;
  VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info, NULL, &descriptor_set_layout),
           "create_descriptor_set_layout: failed to vkCreateDescriptorSetLayout");

  return descriptor_set_layout;
}

StagingArena create_staging_arena(const VulkanContext *context, u32 total_size) {
  StagingArena staging_arena;
  VulkanBuffer staging_buffer = create_buffer(context, BUFFER_TYPE_STAGING, total_size);
  staging_arena.buffer = staging_buffer;
  staging_arena.total_size = total_size;
  staging_arena.offset = 0;
  staging_arena.num_copy_regions = 0;
  memset(staging_arena.copy_regions, 0, sizeof(staging_arena.copy_regions));
  return staging_arena;
}

// written_data_offset is the offset of the staged data in *data, i.e. the
// src_offset when the staging buffer is the src
u32 stage_data_explicit(const VulkanContext *context, StagingArena *arena, const void *data, u32 size,
                        VkBuffer destination, u32 dst_offset) {

  assert(arena->offset + size <= arena->total_size);
  assert(arena->num_copy_regions < MAX_COPY_REGIONS);

  u32 written_data_offset = arena->offset;

  VkBufferCopy copy_region;
  copy_region.srcOffset = arena->offset;
  copy_region.dstOffset = dst_offset;
  copy_region.size = size;

  arena->copy_regions[arena->num_copy_regions] = copy_region;
  arena->destination_buffers[arena->num_copy_regions] = destination;
  arena->num_copy_regions++;

  write_to_vulkan_buffer(context, data, size, arena->offset, arena->buffer);

  arena->offset += size;
  return written_data_offset;
}

u32 stage_data_auto(const VulkanContext *context, StagingArena *arena, const void *data, u32 size,
                    VkBuffer destination) {
  return stage_data_explicit(context, arena, data, size, destination, arena->offset);
}

void flush_staging_arena(const VulkanContext *context, StagingArena *arena) {

  VkCommandBuffer command_buffer = begin_single_use_command_buffer(context);

  if (arena->num_copy_regions == 0) {
    return;
  }

  VkBuffer current_destination = arena->destination_buffers[0];
  u32 current_batch_start = 0;

  for (u32 i = 1; i < arena->num_copy_regions; i++) {
    if (arena->destination_buffers[i] == current_destination) {
      continue;
    }

    vkCmdCopyBuffer(command_buffer, arena->buffer.buffer, current_destination, i - current_batch_start,
                    arena->copy_regions + current_batch_start);

    current_destination = arena->destination_buffers[i];
    current_batch_start = i;
  }

  vkCmdCopyBuffer(command_buffer, arena->buffer.buffer, current_destination,
                  arena->num_copy_regions - current_batch_start, arena->copy_regions + current_batch_start);

  end_single_use_command_buffer(context, command_buffer);
}

ShaderModule create_shader_stage(VkShaderModule module, VkShaderStageFlagBits stage, const char *entry_point) {
  ShaderModule shader_stage;
  shader_stage.module = module;
  shader_stage.entry_point = entry_point;
  shader_stage.stage = stage;
  return shader_stage;
}

// binding: the binding stipulated in the shader
// stage_flags: the flags indicating what shader stages use this layout binding
// descriptor_type: flags for uniform, sampler, storage, etc
// descriptor_count: if the shader allocates an array, the array size
// TODO immutable samplers?
VkDescriptorSetLayoutBinding create_descriptor_set_layout_binding(u32 binding, VkShaderStageFlags stage_flags,
                                                                  VkDescriptorType descriptor_type,
                                                                  u32 descriptor_count) {

  VkDescriptorSetLayoutBinding layout_binding;
  layout_binding.binding = binding;
  layout_binding.descriptorType = descriptor_type;
  layout_binding.descriptorCount = descriptor_count;
  layout_binding.stageFlags = stage_flags;
  layout_binding.pImmutableSamplers = NULL;
  return layout_binding;
}

UniformBuffer create_uniform_buffer(const VulkanContext *context, u32 buffer_size) {

  UniformBuffer uniform_buffer;

  uniform_buffer.vulkan_buffer = create_buffer(context, BUFFER_TYPE_UNIFORM, buffer_size);
  uniform_buffer.size = buffer_size;

  VK_CHECK(vkMapMemory(context->device, uniform_buffer.vulkan_buffer.memory, 0, buffer_size, 0,
                       (void **)&uniform_buffer.mapped),
           "create_uniform_buffer: failed to vkMapMemory");

  return uniform_buffer;
}

void destroy_uniform_buffer(const VulkanContext *context, UniformBuffer *uniform_buffer) {
  destroy_vulkan_buffer(context, uniform_buffer->vulkan_buffer);
}

void write_to_uniform_buffer(UniformBuffer *uniform_buffer, const void *data, UniformWrite uniform_write) {
  memcpy(uniform_buffer->mapped + uniform_write.offset, data, uniform_write.size);
}

ReadOnlyStorageBuffer create_readonly_storage_buffer(const VulkanContext *context, u32 buffer_size) {

  ReadOnlyStorageBuffer readonly_storage_buffer;

  readonly_storage_buffer.vulkan_buffer = create_buffer(context, BUFFER_TYPE_READONLY_STORAGE, buffer_size);
  readonly_storage_buffer.size = buffer_size;

  VK_CHECK(vkMapMemory(context->device, readonly_storage_buffer.vulkan_buffer.memory, 0, buffer_size, 0,
                       (void **)&readonly_storage_buffer.mapped),
           "create_readonly_storage_buffer: failed to vkMapMemory");

  return readonly_storage_buffer;
}

// TODO how to handle reusing staging buffers?
// Need to see usage when multiple textures are present
// current idea: anticipate loading several textures at once, read from files,
// pick one with max size, and allocate staging buffer with that size, and
// stream the textures in one by one after that
VulkanTexture create_vulkan_texture_from_file(VulkanContext *context, const char *path, VulkanBuffer staging_buffer,
                                              void *ptr_to_mapped_memory) {

  VulkanTexture vulkan_texture;
  STBHandle stb_handle = load_texture(path);

  vulkan_texture.image =
      create_default_image(context, stb_handle.width, stb_handle.height,
                           VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_FORMAT_R8G8B8A8_SRGB);

  vulkan_texture.device_memory = allocate_and_bind_image_memory(context, vulkan_texture.image);

  // map memory
  u32 texture_size = 4 * stb_handle.width * stb_handle.height;
  assert(stb_handle.data);
  assert(ptr_to_mapped_memory);
  memcpy(ptr_to_mapped_memory, stb_handle.data, texture_size);

  // first transition
  VkCommandBuffer command_buffer = begin_single_use_command_buffer(context);
  transition_image_layout(command_buffer, vulkan_texture.image, VK_IMAGE_LAYOUT_UNDEFINED,
                          VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  end_single_use_command_buffer(context, command_buffer);

  // copy buffer to image
  VkBufferImageCopy buffer_image_copy;
  buffer_image_copy.bufferOffset = 0;
  // 0 indicates tightly packed - could we also used handle width/height?
  buffer_image_copy.bufferRowLength = 0;
  buffer_image_copy.bufferImageHeight = 0;

  buffer_image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  buffer_image_copy.imageSubresource.mipLevel = 0;
  buffer_image_copy.imageSubresource.baseArrayLayer = 0;
  buffer_image_copy.imageSubresource.layerCount = 1;

  buffer_image_copy.imageOffset.x = 0;
  buffer_image_copy.imageOffset.y = 0;
  buffer_image_copy.imageOffset.z = 0;
  buffer_image_copy.imageExtent.width = stb_handle.width;
  buffer_image_copy.imageExtent.height = stb_handle.height;
  buffer_image_copy.imageExtent.depth = 1;

  VkCommandBuffer image_copy_command_buffer = begin_single_use_command_buffer(context);
  vkCmdCopyBufferToImage(image_copy_command_buffer, staging_buffer.buffer, vulkan_texture.image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buffer_image_copy);
  end_single_use_command_buffer(context, image_copy_command_buffer);

  // second transition
  command_buffer = begin_single_use_command_buffer(context);
  transition_image_layout(command_buffer, vulkan_texture.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                          VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  end_single_use_command_buffer(context, command_buffer);

  vulkan_texture.width = stb_handle.width;
  vulkan_texture.height = stb_handle.height;
  vulkan_texture.image_view =
      create_default_image_view(context, vulkan_texture.image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT);
  free_stb_handle(&stb_handle);

  return vulkan_texture;
}

// TODO return type - would I prefer to return an array of textures, or do I
// want to be able to refer to textures by name?
// I could pass an array of structs containing a VulkanTexture and a path
// Also could do hashing
void load_vulkan_textures(VulkanContext *context, const char **paths, u32 num_paths, VulkanTexture *out_textures) {

  u32 max_size = 0;

  for (u32 i = 0; i < num_paths; i++) {
    STBHandle metadata = load_texture_metadata(paths[i]);

    // TODO need to fix this 4 issue
    u32 texture_size = metadata.width * metadata.height * 4;
    max_size = (texture_size > max_size) ? texture_size : max_size;
  }

  // TODO find a way to reuse another staging buffer, already allocated
  VulkanBuffer staging_buffer = create_buffer(context, BUFFER_TYPE_STAGING, max_size);
  void *texture_data;
  VK_CHECK(
      vkMapMemory(context->device, staging_buffer.memory, 0, staging_buffer.memory_requirements.size, 0, &texture_data),
      "load_vulkan_textures: failed to VkMapMemory");

  for (u32 i = 0; i < num_paths; i++) {
    out_textures[i] = create_vulkan_texture_from_file(context, paths[i], staging_buffer, texture_data);
  }

  vkUnmapMemory(context->device, staging_buffer.memory);
  destroy_vulkan_buffer(context, staging_buffer);
}

void destroy_vulkan_texture(VkDevice device, VulkanTexture *vulkan_texture) {
  vkDestroyImage(device, vulkan_texture->image, NULL);
  vkDestroyImageView(device, vulkan_texture->image_view, NULL);
  vkFreeMemory(device, vulkan_texture->device_memory, NULL);
}

// TODO give this any modularity at all
VkSampler create_sampler(VkDevice device) {

  VkSamplerCreateInfo sampler_create_info;
  sampler_create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  sampler_create_info.pNext = NULL;
  sampler_create_info.flags = 0;
  sampler_create_info.magFilter = VK_FILTER_LINEAR;
  sampler_create_info.minFilter = VK_FILTER_LINEAR;
  sampler_create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  sampler_create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
  sampler_create_info.mipLodBias = 0.0f;
  sampler_create_info.anisotropyEnable = VK_FALSE;
  sampler_create_info.maxAnisotropy = 0.0f;
  sampler_create_info.compareEnable = VK_FALSE;
  sampler_create_info.compareOp = VK_COMPARE_OP_LESS;
  sampler_create_info.minLod = 1.0f;
  sampler_create_info.maxLod = 1.0f;
  sampler_create_info.borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;
  sampler_create_info.unnormalizedCoordinates = VK_FALSE;

  VkSampler sampler;
  VK_CHECK(vkCreateSampler(device, &sampler_create_info, NULL, &sampler), "create_sampler: failed to vkCreateSampler");
  return sampler;
}

DescriptorSetBuilder create_descriptor_set_builder(VulkanContext *context) {

  DescriptorSetBuilder builder;
  builder.device = context->device;

  builder.binding_count = 0;
  memset(builder.layout_bindings, 0, sizeof(builder.layout_bindings));

  builder.write_descriptor_count = 0;
  memset(builder.descriptor_writes, 0, sizeof(builder.descriptor_writes));
  builder.copy_descriptor_count = 0;
  memset(builder.descriptor_copies, 0, sizeof(builder.descriptor_copies));

  builder.buffer_info_count = 0;
  memset(builder.descriptor_buffer_infos, 0, sizeof(builder.descriptor_buffer_infos));
  builder.image_info_count = 0;
  memset(builder.descriptor_image_infos, 0, sizeof(builder.descriptor_image_infos));

  return builder;
};

void merge_descriptor_set_layouts() {
  // asdf
  // asdf
}

// this binding handle would describe which binding, array of uniform
// size, and what shader stage the uniform is used in
void add_uniform_buffer_descriptor_set(DescriptorSetBuilder *builder, const UniformBuffer *uniform_buffer, u32 offset,
                                       u32 range, u32 binding, u32 descriptor_count, VkShaderStageFlags stage_flags,
                                       bool dynamic) {
  assert(builder->write_descriptor_count < MAX_DESCRIPTOR_WRITES);
  assert(builder->buffer_info_count < MAX_DESCRIPTOR_BUFFER_INFOS);
  assert(builder->binding_count < MAX_LAYOUT_BINDINGS);

  assert(dynamic == false && "Dynamic uniform buffers not yet implemented");
  VkDescriptorType descriptor_type =
      dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;

  builder->layout_bindings[builder->binding_count] =
      create_descriptor_set_layout_binding(binding, stage_flags, descriptor_type, descriptor_count);
  builder->binding_count++;

  VkDescriptorBufferInfo *descriptor_buffer_info = &builder->descriptor_buffer_infos[builder->buffer_info_count];
  descriptor_buffer_info->buffer = uniform_buffer->vulkan_buffer.buffer;
  descriptor_buffer_info->offset = offset;
  descriptor_buffer_info->range = range;
  builder->buffer_info_count++;

  VkWriteDescriptorSet *descriptor_write_info = &builder->descriptor_writes[builder->write_descriptor_count];
  descriptor_write_info->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write_info->pNext = NULL;
  descriptor_write_info->dstSet = VK_NULL_HANDLE;
  descriptor_write_info->dstBinding = binding;
  descriptor_write_info->dstArrayElement = 0;
  descriptor_write_info->descriptorCount = 1;
  descriptor_write_info->descriptorType = descriptor_type;
  descriptor_write_info->pImageInfo = NULL;
  descriptor_write_info->pBufferInfo = descriptor_buffer_info;
  descriptor_write_info->pTexelBufferView = NULL;
  builder->write_descriptor_count++;
}

// TODO check for proper transitions - still don't understand transitions
// TODO check how to pass immutable samplers through here
void add_image_descriptor_set(DescriptorSetBuilder *builder, VkImageView image_view, VkSampler sampler, u32 binding,
                              u32 descriptor_count, VkShaderStageFlags stage_flags) {
  assert(builder->write_descriptor_count < MAX_DESCRIPTOR_WRITES);
  assert(builder->image_info_count < MAX_DESCRIPTOR_IMAGE_INFOS);
  assert(builder->binding_count < MAX_LAYOUT_BINDINGS);

  // TODO handle separate textures and samplers
  VkDescriptorType descriptor_type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  builder->layout_bindings[builder->binding_count] =
      create_descriptor_set_layout_binding(binding, stage_flags, descriptor_type, descriptor_count);
  builder->binding_count++;

  VkDescriptorImageInfo *descriptor_image_info = &builder->descriptor_image_infos[builder->image_info_count];
  descriptor_image_info->sampler = sampler;
  descriptor_image_info->imageView = image_view;
  descriptor_image_info->imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  builder->image_info_count++;

  VkWriteDescriptorSet *descriptor_write_info = &builder->descriptor_writes[builder->write_descriptor_count];
  descriptor_write_info->sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  descriptor_write_info->pNext = NULL;
  descriptor_write_info->dstSet = VK_NULL_HANDLE;
  descriptor_write_info->dstBinding = binding;
  descriptor_write_info->dstArrayElement = 0;
  descriptor_write_info->descriptorCount = descriptor_count;
  descriptor_write_info->descriptorType = descriptor_type;
  descriptor_write_info->pImageInfo = descriptor_image_info;
  descriptor_write_info->pBufferInfo = NULL;
  descriptor_write_info->pTexelBufferView = NULL;
  builder->write_descriptor_count++;
}

// TODO decouple layouts from descriptor sets
// I make a layout for every descriptor set, but I should be able to
// create sets from mixtures of layouts
DescriptorSetHandle build_descriptor_set(DescriptorSetBuilder *builder, VkDescriptorPool descriptor_pool) {
  DescriptorSetHandle handle;

  handle.descriptor_set_layout =
      create_descriptor_set_layout(builder->device, builder->layout_bindings, builder->binding_count);

  handle.descriptor_set = create_descriptor_set(builder->device, descriptor_pool, &handle.descriptor_set_layout);

  for (u32 i = 0; i < builder->write_descriptor_count; i++) {
    builder->descriptor_writes[i].dstSet = handle.descriptor_set;
  }

  vkUpdateDescriptorSets(builder->device, builder->write_descriptor_count, builder->descriptor_writes,
                         builder->copy_descriptor_count, builder->descriptor_copies);

  return handle;
}

void destroy_descriptor_set_handle(VkDevice device, DescriptorSetHandle *handle) {
  vkDestroyDescriptorSetLayout(device, handle->descriptor_set_layout, NULL);
}

VulkanShaderCache *create_shader_cache(VkDevice device) {
  VulkanShaderCache *cache = new VulkanShaderCache;
  cache->device = device;
  return cache;
}

void destroy_shader_cache(VulkanShaderCache *cache) {
  for (u32 i = 0; i < cache->hash_map.capacity; i++) {
    if (cache->hash_map.key_values[i].occupancy == HASHMAP_OCCUPIED) {
      vkDestroyShaderModule(cache->device, cache->hash_map.key_values[i].value.module, NULL);
    }
  }
}

void render_mesh(VkCommandBuffer command_buffer, RenderCall *render_call) {

  vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_call->graphics_pipeline);

  // TODO engine: try to move branching to render call init
  u32 first_binding = 0;
  if (render_call->num_vertex_buffers > 0) {
    vkCmdBindVertexBuffers(command_buffer, first_binding, render_call->num_vertex_buffers, render_call->vertex_buffers,
                           render_call->vertex_buffer_offsets);
  }

  u32 first_set = 0;
  u32 dynamic_offset_count = 0;
  vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, render_call->pipeline_layout, first_set,
                          render_call->num_descriptor_sets, render_call->descriptor_sets, dynamic_offset_count, NULL);

  u32 first_instance = 0;
  i32 vertex_offset = 0; // TODO understand when this would be non zero
  if (render_call->is_indexed) {

    vkCmdBindIndexBuffer(command_buffer, render_call->index_buffer, render_call->index_buffer_offset,
                         VK_INDEX_TYPE_UINT16);

    u32 first_index = 0;
    vkCmdDrawIndexed(command_buffer, render_call->num_indices, render_call->instance_count, first_index, vertex_offset,
                     first_instance);
  } else {

    u32 first_vertex = 0;
    vkCmdDraw(command_buffer, render_call->num_vertices, render_call->instance_count, first_vertex, first_instance);
  }
}

BufferUploadQueue new_buffer_upload_queue() {
  BufferUploadQueue queue;

  queue.vertex_buffer_offset = 0;
  queue.index_buffer_offset = 0;
  memset(queue.slices, 0, sizeof(queue.slices));
  queue.num_slices = 0;

  return queue;
}

const BufferHandle *upload_data(BufferUploadQueue *queue, BufferType buffer_type, void *data, u64 size) {

  assert(queue->num_slices < MAX_BUFFER_UPLOADS);

  BufferHandle *handle = &queue->slices[queue->num_slices];

  u64 offset = -1;
  switch (buffer_type) {
  case BUFFER_TYPE_INDEX:
    offset = queue->index_buffer_offset;
    queue->index_buffer_offset += size;
    break;
  case BUFFER_TYPE_VERTEX:
    offset = queue->vertex_buffer_offset;
    queue->vertex_buffer_offset += size;
    break;
  default:
    assert(false);
  }

  handle->offset = offset;
  handle->size = size;
  handle->buffer_type = buffer_type;
  handle->data = data;
  ++queue->num_slices;

  return handle;
}

BufferManager flush_buffers(VulkanContext *context, BufferUploadQueue *queue) {

  BufferManager manager;
  manager.context = context;
  manager.vertex_buffer = create_buffer(context, BUFFER_TYPE_VERTEX, queue->vertex_buffer_offset);

  manager.index_buffer = create_buffer(context, BUFFER_TYPE_INDEX, queue->index_buffer_offset);

  u64 total_size = queue->vertex_buffer_offset + queue->index_buffer_offset;
  manager.staging_arena = create_staging_arena(context, total_size);

  for (u32 i = 0; i < queue->num_slices; i++) {

    const BufferHandle handle = queue->slices[i];
    VkBuffer destination;
    switch (handle.buffer_type) {
    case BUFFER_TYPE_INDEX:
      destination = manager.index_buffer.buffer;
      break;
    case BUFFER_TYPE_VERTEX:
      destination = manager.vertex_buffer.buffer;
      break;
    default:
      assert(false);
    }

    stage_data_explicit(context, &manager.staging_arena, handle.data, handle.size, destination, handle.offset);
  }

  flush_staging_arena(context, &manager.staging_arena);

  return manager;
}

void destroy_buffer_manager(BufferManager *buffer_manager) {
  VulkanContext *ctx = buffer_manager->context;
  destroy_vulkan_buffer(ctx, buffer_manager->vertex_buffer);
  destroy_vulkan_buffer(ctx, buffer_manager->index_buffer);
  destroy_vulkan_buffer(ctx, buffer_manager->staging_arena.buffer);
}

UniformBufferManager new_uniform_buffer_manager() {
  UniformBufferManager uniform_buffer_manager;
  uniform_buffer_manager.current_offset = 0;
  return uniform_buffer_manager;
}

UniformWrite push_uniform(UniformBufferManager *uniform_buffer_manager, u32 size) {
  UniformWrite uniform_write;
  uniform_write.size = size;
  uniform_write.offset = uniform_buffer_manager->current_offset;
  uniform_buffer_manager->current_offset += size;
  return uniform_write;
}

CoherentStreamingBuffer create_coherent_streaming_buffer(const VulkanContext *ctx, u32 size) {
  CoherentStreamingBuffer coherent_streaming_buffer;

  coherent_streaming_buffer.vulkan_buffer = create_buffer(ctx, BUFFER_TYPE_COHERENT_STREAMING, size);

  vkMapMemory(ctx->device, coherent_streaming_buffer.vulkan_buffer.memory, 0, size, 0,
              (void **)&coherent_streaming_buffer.data);

  coherent_streaming_buffer.size = size;
  coherent_streaming_buffer.head = 0;

  return coherent_streaming_buffer;
}

void write_to_streaming_buffer(CoherentStreamingBuffer *coherent_streaming_buffer, void *data, u32 size) {

  assert(size <= coherent_streaming_buffer->size);

  u8 *dest = coherent_streaming_buffer->data;
  u32 offset = coherent_streaming_buffer->head;
  bool overflowed = (offset + size > coherent_streaming_buffer->size);
  if (overflowed) {
    offset = 0;
  }

  memcpy(dest + offset, data, size);

  coherent_streaming_buffer->head = overflowed ? size : offset + size;
}

ColorDepthFramebuffer create_color_depth_framebuffer(const VulkanContext *context, VkExtent2D extent,
                                                     VkFormat color_format, VkFormat depth_format) {
  ColorDepthFramebuffer color_depth_framebuffer;

  color_depth_framebuffer.render_pass = create_color_depth_render_pass(context->device, color_format);

  color_depth_framebuffer.color_image =
      create_default_image(context, extent.width, extent.height,
                           VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, color_format);
  color_depth_framebuffer.color_image_device_memory =
      allocate_and_bind_image_memory(context, color_depth_framebuffer.color_image);
  color_depth_framebuffer.color_image_view =
      create_default_image_view(context, color_depth_framebuffer.color_image, color_format, VK_IMAGE_ASPECT_COLOR_BIT);

  color_depth_framebuffer.depth_image =
      create_default_image(context, extent.width, extent.height,
                           VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, depth_format);
  color_depth_framebuffer.depth_image_device_memory =
      allocate_and_bind_image_memory(context, color_depth_framebuffer.depth_image);
  color_depth_framebuffer.depth_image_view =
      create_default_image_view(context, color_depth_framebuffer.depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

  VkImageView offscreen_image_views[] = {color_depth_framebuffer.color_image_view,
                                         color_depth_framebuffer.depth_image_view};

  color_depth_framebuffer.framebuffer =
      create_framebuffer(context->device, color_depth_framebuffer.render_pass, 2, offscreen_image_views, extent);

  return color_depth_framebuffer;
}

void destroy_color_depth_framebuffer(const VulkanContext *context, ColorDepthFramebuffer *color_depth_framebuffer) {
  vkDestroyFramebuffer(context->device, color_depth_framebuffer->framebuffer, NULL);
  vkDestroyImageView(context->device, color_depth_framebuffer->color_image_view, NULL);
  vkDestroyImage(context->device, color_depth_framebuffer->color_image, NULL);
  vkFreeMemory(context->device, color_depth_framebuffer->color_image_device_memory, NULL);
  vkDestroyImageView(context->device, color_depth_framebuffer->depth_image_view, NULL);
  vkDestroyImage(context->device, color_depth_framebuffer->depth_image, NULL);
  vkFreeMemory(context->device, color_depth_framebuffer->depth_image_device_memory, NULL);
  vkDestroyRenderPass(context->device, color_depth_framebuffer->render_pass, NULL);
}
