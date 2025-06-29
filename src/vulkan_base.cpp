#include "vulkan_base.h"
#include "renderer.h"
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
  // framebuffers
  for (uint32_t i = 0; i < context->swapchain_storage.image_count; ++i) {
    vkDestroyFramebuffer(context->device, context->framebuffers[i], NULL);
    context->framebuffers[i] = VK_NULL_HANDLE;
  }

  // swapchain image views
  for (uint32_t i = 0; i < context->swapchain_storage.image_count; i++) {
    if (context->swapchain_storage.use_static) {
      vkDestroyImageView(
          context->device,
          context->swapchain_storage.as.static_storage.image_views[i], NULL);
    } else {
      vkDestroyImageView(
          context->device,
          context->swapchain_storage.as.dynamic_storage.image_views[i], NULL);
    }
  }

  if (!context->swapchain_storage.use_static) {
    free(context->swapchain_storage.as.dynamic_storage.image_views);
    free(context->swapchain_storage.as.dynamic_storage.images);
  }

  if (context->swapchain) {
    vkDestroySwapchainKHR(context->device, context->swapchain, NULL);
  } else {
    printf("Tried to destroy null swapchain\n");
  }

  memset(&context->swapchain_storage, 0, sizeof(context->swapchain_storage));
}

void destroy_vulkan_context(VulkanContext *context) {
  vkDeviceWaitIdle(context->device);
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
  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(context->device,
                       context->frame_sync_objects[i].image_available_semaphore,
                       NULL);
    vkDestroySemaphore(context->device,
                       context->frame_sync_objects[i].render_finished_semaphore,
                       NULL);
    vkDestroyFence(context->device,
                   context->frame_sync_objects[i].in_flight_fence, NULL);
  }

  destroy_swapchain(context);

  vkDestroyRenderPass(context->device, context->render_pass, NULL);
  vkDestroyDevice(context->device, NULL);
  vkDestroySurfaceKHR(context->instance, context->surface, NULL);

#ifdef TUKE_DISABLE_VULKAN_VALIDATION
#else
  PFN_vkDestroyDebugUtilsMessengerEXT fpDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          context->instance, "vkDestroyDebugUtilsMessengerEXT");
  if (fpDestroyDebugUtilsMessengerEXT) {
    fpDestroyDebugUtilsMessengerEXT(context->instance, context->debug_messenger,
                                    NULL);
  }
#endif
  vkDestroyInstance(context->instance, NULL);
}

// TODO Enable debug messenger extension, add macros for disabling in release
// mode
VKAPI_ATTR VkBool32 VKAPI_CALL
debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
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

void populate_debug_msngr_create_info(
    VkDebugUtilsMessengerCreateInfoEXT *debug_msngr_create_info) {
  debug_msngr_create_info->sType =
      VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  debug_msngr_create_info->pNext = NULL;
  debug_msngr_create_info->flags = 0;
  debug_msngr_create_info->messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  debug_msngr_create_info->messageType =
      VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
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
  uint32_t glfw_extension_count;
  const char **glfw_extensions =
      glfwGetRequiredInstanceExtensions(&glfw_extension_count);
  if (glfw_extensions == NULL) {
    fprintf(stderr, "create_instance: glfw_extensions is NULL\n");
    exit(1);
  }

  // TODO debug/release
#ifdef TUKE_DISABLE_VULKAN_VALIDATION
  const char *extra_extensions[] = {
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
  };
  const uint32_t extra_extension_count = 1;
  const char **validation_layers = {};
  const uint32_t layer_count = 0;
#else
  const char *extra_extensions[] = {
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME,
  };
  const uint32_t extra_extension_count = 2;
  const char *validation_layers[] = {"VK_LAYER_KHRONOS_validation"};
  const uint32_t layer_count = 1;
#endif

  const uint32_t MAX_EXTENSIONS = 16;
  const uint32_t total_extension_count =
      extra_extension_count + glfw_extension_count;
  if (total_extension_count > MAX_EXTENSIONS) {
    fprintf(stderr,
            "create_instance: Too many extensions enabled. MAX_EXTENSIONS is "
            "%d, total_extension_count is %d\n",
            MAX_EXTENSIONS, total_extension_count);
    exit(1);
  }

  const char *enabled_extensions[MAX_EXTENSIONS];
  memcpy(enabled_extensions, glfw_extensions,
         sizeof(char *) * glfw_extension_count);

  for (uint32_t i = 0; i < extra_extension_count; i++) {
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
  VK_CHECK(vkCreateInstance(&instance_create_info, NULL, &instance),
           "create_instance: Failed to createInstance");
  return instance;
}

VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance) {
  VkDebugUtilsMessengerCreateInfoEXT debug_msngr_create_info;
  populate_debug_msngr_create_info(&debug_msngr_create_info);

  PFN_vkCreateDebugUtilsMessengerEXT create_fn =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
          instance, "vkCreateDebugUtilsMessengerEXT");

  if (!create_fn) {
    fprintf(stderr, "Failed to load vkCreateDebugUtilsMessengerEXT\n");
    exit(1);
  }
  VkDebugUtilsMessengerEXT debug_messenger;
  VK_CHECK(
      create_fn(instance, &debug_msngr_create_info, NULL, &debug_messenger),
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

uint32_t get_physical_devices(VkInstance instance,
                              VkPhysicalDevice *physical_devices) {
  uint32_t device_count;
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

  VK_CHECK(
      vkEnumeratePhysicalDevices(instance, &device_count, physical_devices),
      "get_physical_devices, failed to vkEnumeratePhysicalDevices");

  return device_count;
}

QueueFamilyIndices
queue_family_indices_from_physical_device(VkPhysicalDevice physical_device,
                                          VkSurfaceKHR surface) {
  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                           NULL);

  const int MAX_QUEUE_FAMILIES = 16;
  VkQueueFamilyProperties queue_families[MAX_QUEUE_FAMILIES];
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count,
                                           queue_families);

  int graphics_family = -1;
  int present_family = -1;
  int compute_family = -1;

  if (queue_family_count > MAX_QUEUE_FAMILIES) {
    printf("is_suitable_physical_device: MAX_QUEUE_FAMILIES is %d but "
           "queue_family_count is %d\n",
           MAX_QUEUE_FAMILIES, queue_family_count);
    queue_family_count = MAX_QUEUE_FAMILIES;
  }

  for (uint32_t i = 0; i < queue_family_count; i++) {
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
    vkGetPhysicalDeviceSurfaceSupportKHR(physical_device, i, surface,
                                         &present_support);
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
  bool is_invalid = indices.compute_family == -1 ||
                    indices.present_family == -1 ||
                    indices.graphics_family == -1;
  return !is_invalid;
}

VkPhysicalDevice pick_physical_device(VkInstance instance,
                                      QueueFamilyIndices *queue_family_indices,
                                      VkSurfaceKHR surface) {

  VkPhysicalDevice physical_devices[MAX_PHYSICAL_DEVICES];
  uint32_t num_physical_devices =
      get_physical_devices(instance, physical_devices);

  VkPhysicalDevice res = NULL;
  for (uint32_t i = 0; i < num_physical_devices; i++) {
    VkPhysicalDevice physical_device = physical_devices[i];
    *queue_family_indices =
        queue_family_indices_from_physical_device(physical_device, surface);

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

uint32_t get_unique_queue_infos(QueueFamilyIndices queue_family_indices,
                                VkDeviceQueueCreateInfo *out_infos) {
  uint32_t num_unique_queue_families = 0;
  uint32_t indices[NUM_QUEUE_FAMILY_INDICES] = {
      (uint32_t)queue_family_indices.compute_family,
      (uint32_t)queue_family_indices.graphics_family,
      (uint32_t)queue_family_indices.present_family,
  };

  for (uint32_t i = 0; i < NUM_QUEUE_FAMILY_INDICES; i++) {
    bool is_duplicate = false;
    for (uint32_t j = 0; j < i; j++) {
      if (indices[i] == indices[j]) {
        is_duplicate = true;
        break;
      }
    }

    if (is_duplicate) {
      continue;
    }

    float queue_priority = 1.0f;
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

VkDevice create_device(QueueFamilyIndices queue_family_indices,
                       VkPhysicalDevice physical_device) {

  VkDeviceQueueCreateInfo queue_create_infos[NUM_QUEUE_FAMILY_INDICES];
  uint32_t num_unique_queue_families =
      get_unique_queue_infos(queue_family_indices, queue_create_infos);

  const char *device_extensions[] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME, // "VK_KHR_swapchain"
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

uint32_t clamp(uint32_t x, uint32_t min, uint32_t max) {
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }
  return x;
}

VkSurfaceFormatKHR get_surface_format(VkPhysicalDevice physical_device,
                                      VkSurfaceKHR surface) {

  uint32_t format_count = 0;
  const uint32_t max_format_count = 128;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                       NULL);

  if (format_count > max_format_count) {
    printf("get_surface_format: found more formats than expected. Max is %d, "
           "found %d. Clamping.\n",
           max_format_count, format_count);
    format_count = max_format_count;
  }
  VkSurfaceFormatKHR formats[max_format_count];
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count,
                                       formats);

  VkSurfaceFormatKHR surface_format = formats[0];
  for (uint32_t i = 0; i < format_count; i++) {
    if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM &&
        formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      surface_format = formats[i];
      break;
    }
  }

  return surface_format;
}

VkExtent2D get_swapchain_extent(GLFWwindow *window,
                                VkSurfaceCapabilitiesKHR surface_capabilities) {
  VkExtent2D extent;
  int window_width, window_height;
  glfwGetFramebufferSize(window, &window_width, &window_height);
  if (surface_capabilities.currentExtent.width != UINT32_MAX) {
    extent = surface_capabilities.currentExtent;
  } else {
    extent.width =
        clamp(window_width, surface_capabilities.minImageExtent.width,
              surface_capabilities.maxImageExtent.width);
    extent.height =
        clamp(window_height, surface_capabilities.minImageExtent.height,
              surface_capabilities.maxImageExtent.height);
  }
  return extent;
}

VkSurfaceCapabilitiesKHR
get_surface_capabilities(VkPhysicalDevice physical_device,
                         VkSurfaceKHR surface) {
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VK_CHECK(
      vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                                &surface_capabilities),
      "create_swapchain: Failed to vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
  return surface_capabilities;
}

VkSwapchainKHR create_swapchain(VkDevice device,
                                VkPhysicalDevice physical_device,
                                VkSurfaceKHR surface,
                                QueueFamilyIndices queue_family_indices,
                                VkSurfaceFormatKHR surface_format,
                                VkSurfaceCapabilitiesKHR surface_capabilities,
                                VkExtent2D swapchain_extent) {
  uint32_t image_count = surface_capabilities.minImageCount + 1;
  if (surface_capabilities.maxImageCount > 0 &&
      image_count > surface_capabilities.maxImageCount) {
    image_count = surface_capabilities.maxImageCount;
  }

  uint32_t present_mode_count = 0;
  const uint32_t max_present_mode_count = 16;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_mode_count, NULL);
  VkPresentModeKHR present_modes[max_present_mode_count];
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface,
                                            &present_mode_count, present_modes);
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;

  if (present_mode_count > max_present_mode_count) {
    present_mode_count = max_present_mode_count;
  }
  for (uint32_t i = 0; i < present_mode_count; i++) {
    if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = present_modes[i];
      break;
    }
  }

  uint32_t queue_family_indices_array[] = {
      (uint32_t)queue_family_indices.graphics_family,
      (uint32_t)queue_family_indices.present_family,
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
  swapchain_create_info.imageUsage =
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

  if (queue_family_indices.graphics_family !=
      queue_family_indices.present_family) {
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
  VK_CHECK(
      vkCreateSwapchainKHR(device, &swapchain_create_info, NULL, &swapchain),
      "create_swapchain: Failed to createSwapchain");
  return swapchain;
}

SwapchainStorage create_swapchain_storage(VkDevice device,
                                          VkSurfaceFormatKHR surface_format,
                                          VkSwapchainKHR swapchain) {
  SwapchainStorage storage;
  storage.use_static = true;

  uint32_t swapchain_image_count;
  VkResult result =
      vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count, NULL);

  if (swapchain_image_count > NUM_SWAPCHAIN_IMAGES) {
    storage.use_static = false;
    storage.as.dynamic_storage.images =
        (VkImage *)malloc(swapchain_image_count * sizeof(VkImage));
    storage.as.dynamic_storage.image_views =
        (VkImageView *)malloc(swapchain_image_count * sizeof(VkImageView));

    if (!storage.as.dynamic_storage.images ||
        !storage.as.dynamic_storage.image_views) {
      fprintf(stderr, "Failed to allocate memory for swapchain storage\n");
      exit(1);
    }
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count,
                                     storage.as.dynamic_storage.images);
  } else {
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchain_image_count,
                                     storage.as.static_storage.images);
  }

  VK_CHECK(result, "get_swapchain_images: Failed to vkGetSwapchainImagesKHR");

  for (uint32_t i = 0; i < swapchain_image_count; i++) {
    VkImageViewCreateInfo image_view_create_info;
    image_view_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    image_view_create_info.pNext = NULL;
    image_view_create_info.flags = 0;
    if (storage.use_static) {
      image_view_create_info.image = storage.as.static_storage.images[i];
    } else {
      image_view_create_info.image = storage.as.dynamic_storage.images[i];
    }
    image_view_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    image_view_create_info.format = surface_format.format;
    image_view_create_info.components = {
        .r = VK_COMPONENT_SWIZZLE_IDENTITY,
        .g = VK_COMPONENT_SWIZZLE_IDENTITY,
        .b = VK_COMPONENT_SWIZZLE_IDENTITY,
        .a = VK_COMPONENT_SWIZZLE_IDENTITY,
    };
    image_view_create_info.subresourceRange = {
        .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1,
    };

    if (storage.use_static) {
      result = vkCreateImageView(device, &image_view_create_info, NULL,
                                 &storage.as.static_storage.image_views[i]);
    } else {
      result = vkCreateImageView(device, &image_view_create_info, NULL,
                                 &storage.as.dynamic_storage.image_views[i]);
    }
    VK_CHECK_VARIADIC(result, "Failed to create image view %u\n", i);
  }

  storage.image_count = swapchain_image_count;
  return storage;
}

VkAttachmentDescription make_color_attachment(VkFormat format) {
  VkAttachmentDescription color_attachment;
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
  (void)format;
  (void)depth_attachment.format;
  (void)depth_attachment.samples;
  (void)depth_attachment.loadOp;
  (void)depth_attachment.storeOp;
  (void)depth_attachment.stencilLoadOp;
  (void)depth_attachment.stencilStoreOp;
  (void)depth_attachment.initialLayout;
  (void)depth_attachment.finalLayout;
  return depth_attachment;
}

VkRenderPass create_render_pass(VkDevice device,
                                VkSurfaceFormatKHR surface_format) {
  VkAttachmentDescription color_attachment =
      make_color_attachment(surface_format.format);

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
  dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  dependency.srcAccessMask = 0;
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkAttachmentDescription attachments[NUM_ATTACHMENTS] = {color_attachment};

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkRenderPassCreateInfo.html
  VkRenderPassCreateInfo render_pass_create_info;
  render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  render_pass_create_info.pNext = NULL;
  render_pass_create_info.flags = 0;
  render_pass_create_info.attachmentCount = NUM_ATTACHMENTS;
  render_pass_create_info.pAttachments = attachments;
  render_pass_create_info.subpassCount = 1;
  render_pass_create_info.pSubpasses = &subpass;
  render_pass_create_info.dependencyCount = 1;
  render_pass_create_info.pDependencies = &dependency;

  VkRenderPass render_pass;
  VK_CHECK(
      vkCreateRenderPass(device, &render_pass_create_info, NULL, &render_pass),
      "create_render_pass: Failed to vkCreateRenderPass");
  return render_pass;
}

VkFramebuffer create_framebuffer(VkDevice device, VkRenderPass render_pass,
                                 VkImageView *image_view_attachments,
                                 VkExtent2D extent) {
  VkFramebufferCreateInfo framebuffer_create_info;
  framebuffer_create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
  framebuffer_create_info.pNext = NULL;
  framebuffer_create_info.flags = 0;
  framebuffer_create_info.renderPass = render_pass;
  framebuffer_create_info.attachmentCount = NUM_ATTACHMENTS;
  framebuffer_create_info.pAttachments = image_view_attachments;
  framebuffer_create_info.width = extent.width;
  framebuffer_create_info.height = extent.height;
  framebuffer_create_info.layers = 1;

  VkFramebuffer framebuffer;
  VK_CHECK(
      vkCreateFramebuffer(device, &framebuffer_create_info, NULL, &framebuffer),
      "create_framebuffer: Failed to vkCreateFramebuffer");
  return framebuffer;
}

void init_framebuffers(VulkanContext *context) {
  for (uint32_t i = 0; i < context->swapchain_storage.image_count; i++) {
    VkImageView view =
        context->swapchain_storage.use_static
            ? context->swapchain_storage.as.static_storage.image_views[i]
            : context->swapchain_storage.as.dynamic_storage.image_views[i];
    context->framebuffers[i] =
        create_framebuffer(context->device, context->render_pass, &view,
                           context->swapchain_extent);
  }
}

void recreate_swapchain(VulkanContext *context) {
  vkDeviceWaitIdle(context->device);
  destroy_swapchain(context);

  context->swapchain = create_swapchain(
      context->device, context->physical_device, context->surface,
      context->queue_family_indices, context->surface_format,
      context->surface_capabilities, context->swapchain_extent);
  create_swapchain_storage(context->device, context->surface_format,
                           context->swapchain);
  init_framebuffers(context);
}

void create_frame_sync_objects(VkDevice device,
                               FrameSyncObjects *frame_sync_objects) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSemaphoreCreateInfo.html
  VkSemaphoreCreateInfo semaphore_create_info;
  semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
  semaphore_create_info.pNext = NULL;
  semaphore_create_info.flags = 0;

  VkFenceCreateInfo fence_create_info;
  fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_create_info.pNext = NULL;
  fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    VK_CHECK_VARIADIC(
        vkCreateSemaphore(device, &semaphore_create_info, NULL,
                          &frame_sync_objects[i].image_available_semaphore),
        "create_frame_sync_objects: Failed to create image_available_semaphore "
        "%d",
        i);

    VK_CHECK_VARIADIC(
        vkCreateSemaphore(device, &semaphore_create_info, NULL,
                          &frame_sync_objects[i].render_finished_semaphore),
        "create_frame_sync_objects: Failed to create render_finished_semaphore "
        "%d",
        i);

    VK_CHECK_VARIADIC(
        vkCreateFence(device, &fence_create_info, NULL,
                      &frame_sync_objects[i].in_flight_fence),
        "create_frame_sync_objects: Failed to create in_flight_fence %d", i);
  }
}

VkCommandPool create_command_pool(VkDevice device, uint32_t queue_family_index,
                                  bool transient) {
  VkCommandPoolCreateInfo command_pool_create_info;
  command_pool_create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  command_pool_create_info.pNext = NULL;
  command_pool_create_info.flags =
      VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT |
      (transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0);
  command_pool_create_info.queueFamilyIndex = queue_family_index;

  VkCommandPool command_pool;
  VK_CHECK(vkCreateCommandPool(device, &command_pool_create_info, NULL,
                               &command_pool),
           "create_command_pool: failed to vkCreateCommandPool");
  return command_pool;
}

void allocate_command_buffers(VkDevice device, VkCommandPool command_pool,
                              uint32_t command_buffer_count,
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
  pipeline_cache_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
  pipeline_cache_create_info.pNext = NULL;
  pipeline_cache_create_info.flags = 0;
  pipeline_cache_create_info.initialDataSize = 0;
  pipeline_cache_create_info.pInitialData = NULL;
  VK_CHECK(vkCreatePipelineCache(device, &pipeline_cache_create_info, NULL,
                                 &pipeline_cache),
           "create_pipeline_cache: Failed to vkCreatePipelineLayout");
  return pipeline_cache;
}

VkVertexInputBindingDescription
create_instanced_vertex_binding_description(uint32_t binding, uint32_t stride) {
  VkVertexInputBindingDescription description;
  description.binding = binding;
  description.stride = stride;
  description.inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
  return description;
}

VkVertexInputBindingDescription
create_vertex_binding_description(uint32_t binding, uint32_t stride) {
  VkVertexInputBindingDescription description;
  description.binding = binding;
  description.stride = stride;
  description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
  return description;
}

VkVertexInputAttributeDescription
create_vertex_attribute_description(uint32_t location, uint32_t binding,
                                    VkFormat format, uint32_t offset) {
  VkVertexInputAttributeDescription description;
  description.location = location;
  description.binding = binding;
  description.format = format;
  description.offset = offset;
  return description;
}

const VulkanVertexLayout vulkan_vertex_layouts[VERTEX_LAYOUT_COUNT] = {
    [VERTEX_LAYOUT_POSITION] = {.attribute_description_count = 1,
                                .attribute_descriptions[0] =
                                    create_vertex_attribute_description(
                                        0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),

                                .binding_description_count = 1,
                                .binding_descriptions[0] =
                                    create_vertex_binding_description(
                                        0, 3 * sizeof(float))},

    [VERTEX_LAYOUT_POSITION_NORMAL] =
        {
            .attribute_description_count = 2,
            .attribute_descriptions[0] = create_vertex_attribute_description(
                0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
            .attribute_descriptions[1] = create_vertex_attribute_description(
                1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)),

            .binding_description_count = 1,
            .binding_descriptions[0] =
                create_vertex_binding_description(0, 6 * sizeof(float)),
        },

    [VERTEX_LAYOUT_POSITION_NORMAL_UV] =
        {.attribute_description_count = 3,
         .attribute_descriptions[0] = create_vertex_attribute_description(
             0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0),
         .attribute_descriptions[1] = create_vertex_attribute_description(
             1, 0, VK_FORMAT_R32G32B32_SFLOAT, 3 * sizeof(float)),
         .attribute_descriptions[2] = create_vertex_attribute_description(
             2, 0, VK_FORMAT_R32G32_SFLOAT, 6 * sizeof(float)),

         .binding_description_count = 1,
         .binding_descriptions[0] = {.binding = 0,
                                     .stride = 8 * sizeof(float),
                                     .inputRate = VK_VERTEX_INPUT_RATE_VERTEX}},
};

VkPipelineVertexInputStateCreateInfo
get_common_vertex_input_state(VulkanContext *context,
                              VertexLayoutID layout_id) {
  assert(layout_id >= 0 && layout_id <= VERTEX_LAYOUT_MANUAL);
  return context->vertex_layouts[layout_id];
}

PipelineConfig create_default_graphics_pipeline_config(
    const VulkanContext *context, const GraphicsPipelineStages shader_stages,
    const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
    VkPipelineLayout pipeline_layout) {
  PipelineConfig pipeline_config;
  pipeline_config.stages[0] = shader_stages.vertex_shader;
  pipeline_config.stages[1] = shader_stages.fragment_shader;
  pipeline_config.stage_count = 2;
  pipeline_config.vertex_input_state_create_info = vertex_input_state;
  pipeline_config.render_pass = context->render_pass;
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
  context.instance = create_instance(title);
#ifdef TUKE_DISABLE_VULKAN_VALIDATION
  context.debug_messenger = VK_NULL_HANDLE;
#else
  context.debug_messenger = create_debug_messenger(context.instance);
#endif
  context.surface = create_surface(context.instance, context.window);
  context.physical_device = pick_physical_device(
      context.instance, &context.queue_family_indices, context.surface);
  vkGetPhysicalDeviceProperties(context.physical_device,
                                &context.physical_device_properties);

  context.queue_family_indices = queue_family_indices_from_physical_device(
      context.physical_device, context.surface);
  context.present_queue_index_is_different_than_graphics =
      (context.queue_family_indices.graphics_family !=
       context.queue_family_indices.present_family);
  context.compute_queue_index_is_different_than_graphics =
      (context.queue_family_indices.graphics_family !=
       context.queue_family_indices.compute_family);

  context.device =
      create_device(context.queue_family_indices, context.physical_device);
  context.surface_format =
      get_surface_format(context.physical_device, context.surface);

  context.surface_capabilities =
      get_surface_capabilities(context.physical_device, context.surface);
  context.swapchain_extent =
      get_swapchain_extent(context.window, context.surface_capabilities);
  context.swapchain =
      create_swapchain(context.device, context.physical_device, context.surface,
                       context.queue_family_indices, context.surface_format,
                       context.surface_capabilities, context.swapchain_extent);

  vkGetDeviceQueue(context.device, context.queue_family_indices.graphics_family,
                   0, &context.graphics_queue);
  vkGetDeviceQueue(context.device, context.queue_family_indices.present_family,
                   0, &context.present_queue);
  vkGetDeviceQueue(context.device, context.queue_family_indices.compute_family,
                   0, &context.compute_queue);
  context.swapchain_storage = create_swapchain_storage(
      context.device, context.surface_format, context.swapchain);

  context.render_pass =
      create_render_pass(context.device, context.surface_format);

  init_framebuffers(&context);

  create_frame_sync_objects(context.device, context.frame_sync_objects);
  context.current_frame = 0;

  // boolean argument is is_transient command pool
  context.transient_command_pool = create_command_pool(
      context.device, context.queue_family_indices.graphics_family, true);
  context.graphics_command_pool = create_command_pool(
      context.device, context.queue_family_indices.graphics_family, false);

  if (context.compute_queue_index_is_different_than_graphics) {
    context.compute_command_pool = create_command_pool(
        context.device, context.queue_family_indices.compute_family, false);
  } else {
    context.compute_command_pool = context.graphics_command_pool;
  }

  if (context.present_queue_index_is_different_than_graphics) {
    context.present_command_pool = create_command_pool(
        context.device, context.queue_family_indices.present_family, false);
  } else {
    context.present_command_pool = context.graphics_command_pool;
  }

  allocate_command_buffers(context.device, context.graphics_command_pool,
                           context.swapchain_storage.image_count,
                           context.graphics_command_buffers);

  allocate_command_buffers(context.device, context.compute_command_pool,
                           context.swapchain_storage.image_count,
                           context.compute_command_buffers);

  context.pipeline_cache = create_pipeline_cache(context.device);

  for (int i = 0; i < VERTEX_LAYOUT_COUNT; i++) {
    context.vertex_layouts[i] = create_vertex_input_state(
        vulkan_vertex_layouts[i].binding_description_count,
        vulkan_vertex_layouts[i].binding_descriptions,
        vulkan_vertex_layouts[i].attribute_description_count,
        vulkan_vertex_layouts[i].attribute_descriptions);
  }

  return context;
}

uint32_t find_memory_type(VkPhysicalDevice physical_device,
                          uint32_t type_filter,
                          VkMemoryPropertyFlags properties) {
  VkPhysicalDeviceMemoryProperties mem_properties;
  vkGetPhysicalDeviceMemoryProperties(physical_device, &mem_properties);
  for (uint32_t i = 0; i < mem_properties.memoryTypeCount; i++) {
    if ((type_filter & (1 << i)) &&
        (mem_properties.memoryTypes[i].propertyFlags & properties) ==
            properties) {
      return i;
    }
  }
  fprintf(stderr, "Failed to find suitable memory type.\n");
  exit(1);
}

VulkanBuffer create_buffer_explicit(const VulkanContext *context,
                                    VkBufferUsageFlags usage, VkDeviceSize size,
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
  buffer_create_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  buffer_create_info.queueFamilyIndexCount = 0;
  buffer_create_info.pQueueFamilyIndices = NULL;

  VkBuffer buffer;
  VK_CHECK(vkCreateBuffer(context->device, &buffer_create_info, NULL, &buffer),
           "Failed to create staging buffer");
  vulkan_buffer.buffer = buffer;

  VkMemoryRequirements memory_requirements;
  vkGetBufferMemoryRequirements(context->device, buffer, &memory_requirements);
  vulkan_buffer.memory_requirements = memory_requirements;

  VkMemoryAllocateInfo memory_allocate_info;
  memory_allocate_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memory_allocate_info.pNext = 0;
  memory_allocate_info.allocationSize = memory_requirements.size;
  memory_allocate_info.memoryTypeIndex = find_memory_type(
      context->physical_device, memory_requirements.memoryTypeBits, properties);
  VK_CHECK(vkAllocateMemory(context->device, &memory_allocate_info, NULL,
                            &vulkan_buffer.memory),
           "create_buffer: failed to vkAllocateMemory");

  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(context->physical_device,
                                      &memory_properties);
  vulkan_buffer.memory_property_flags =
      memory_properties.memoryTypes[memory_allocate_info.memoryTypeIndex]
          .propertyFlags;

  VK_CHECK(vkBindBufferMemory(context->device, buffer, vulkan_buffer.memory, 0),
           "create_buffer: Failed to vkBindBufferMemory");

  return vulkan_buffer;
}

VulkanBuffer create_buffer(const VulkanContext *context, BufferType buffer_type,
                           VkDeviceSize size) {
  switch (buffer_type) {
  case BUFFER_TYPE_VERTEX: {
    VkBufferUsageFlags vertex_buffer_usage =
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags vertex_buffer_memory_properties =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer_explicit(context, vertex_buffer_usage, size,
                                  vertex_buffer_memory_properties);
  }

  case BUFFER_TYPE_INDEX: {
    VkBufferUsageFlags index_buffer_usage =
        VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    VkMemoryPropertyFlags index_buffer_memory_properties =
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer_explicit(context, index_buffer_usage, size,
                                  index_buffer_memory_properties);
  }

  case BUFFER_TYPE_STAGING: {
    VkBufferUsageFlags staging_buffer_usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags staging_buffer_memory_properties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer_explicit(context, staging_buffer_usage, size,
                                  staging_buffer_memory_properties);
  }

    // TODO expand for dynamic uniform buffers, or add a second helper
  case BUFFER_TYPE_UNIFORM: {
    VkBufferUsageFlags uniform_buffer_usage =
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    VkMemoryPropertyFlags uniform_buffer_memory_properties =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    return create_buffer_explicit(context, uniform_buffer_usage, size,
                                  uniform_buffer_memory_properties);
  }

  default:
    assert(false && "create_buffer: got an invalid buffer_type");
  }
}

void write_to_vulkan_buffer(const VulkanContext *context, void *src_data,
                            VkDeviceSize size, VkDeviceSize offset,
                            VulkanBuffer vulkan_buffer) {
  if (!(vulkan_buffer.memory_property_flags &
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
    fprintf(stderr, "write_to_vulkan_buffer: attempting to write to host "
                    "invisible memory\n");
    exit(1);
  }

  VkDeviceSize aligned_offset = offset;
  VkDeviceSize aligned_size = size;
  VkDeviceSize atom_size =
      context->physical_device_properties.limits.nonCoherentAtomSize;
  bool not_coherent = !(vulkan_buffer.memory_property_flags &
                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (not_coherent) {
    aligned_offset = offset & ~(atom_size - 1);
    aligned_size =
        ((offset + size + atom_size - 1) & ~(atom_size - 1)) - aligned_offset;
  }

  void *dest_data;
  VK_CHECK(vkMapMemory(context->device, vulkan_buffer.memory, aligned_offset,
                       aligned_size, 0, &dest_data),
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
  VkCommandBufferAllocateInfo allocate_info;
  allocate_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocate_info.pNext = 0;
  allocate_info.commandPool = context->transient_command_pool;
  allocate_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocate_info.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  VK_CHECK(vkAllocateCommandBuffers(context->device, &allocate_info,
                                    &command_buffer),
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

void end_single_use_command_buffer(const VulkanContext *context,
                                   VkCommandBuffer command_buffer) {
  VK_CHECK(vkEndCommandBuffer(command_buffer),
           "end_single_use_command_buffer: failed to vkEndCommandBuffer");

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

  VK_CHECK(
      vkQueueSubmit(context->graphics_queue, 1, &submit_info, VK_NULL_HANDLE),
      "end_single_use_command_buffer: failed to vkQueueSubmit");
  VK_CHECK(vkQueueWaitIdle(context->graphics_queue),
           "end_single_use_command_buffer: failed to vkQueueWaitIdle");
  vkFreeCommandBuffers(context->device, context->transient_command_pool, 1,
                       &command_buffer);
}

VkShaderModule create_shader_module(VkDevice device, const uint32_t *code,
                                    size_t code_size) {
  VkShaderModuleCreateInfo shader_module_create_info;
  shader_module_create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_module_create_info.pNext = NULL;
  shader_module_create_info.flags = 0;
  shader_module_create_info.codeSize = code_size;
  shader_module_create_info.pCode = code;

  VkShaderModule module;
  VK_CHECK(
      vkCreateShaderModule(device, &shader_module_create_info, NULL, &module),
      "create_shader_module: failed to vkCreateShaderModule");
  return module;
}

ViewportState create_viewport_state(VkExtent2D swapchain_extent,
                                    VkOffset2D offset) {
  ViewportState viewport_state;

  viewport_state.viewport.x = offset.x;
  viewport_state.viewport.y = offset.y;
  viewport_state.viewport.width = (float)swapchain_extent.width;
  viewport_state.viewport.height = (float)swapchain_extent.height;
  viewport_state.viewport.minDepth = 0.0f;
  viewport_state.viewport.maxDepth = 1.0f;

  viewport_state.scissor.extent = swapchain_extent;
  viewport_state.scissor.offset.x = offset.x;
  viewport_state.scissor.offset.y = offset.y;

  return viewport_state;
}

VkPipelineInputAssemblyStateCreateInfo
create_pipeline_input_assembly_state(VkPrimitiveTopology topology,
                                     VkBool32 primitive_restart_enabled) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkPipelineInputAssemblyStateCreateInfo.html
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info;
  input_assembly_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_state_create_info.pNext = NULL;
  input_assembly_state_create_info.flags = 0;
  input_assembly_state_create_info.topology = topology;
  input_assembly_state_create_info.primitiveRestartEnable =
      primitive_restart_enabled;
  return input_assembly_state_create_info;
}

VkPipelineRasterizationStateCreateInfo
create_rasterization_state(VkPolygonMode polygon_mode,
                           VkCullModeFlags cull_mode, VkFrontFace front_face) {
  VkPipelineRasterizationStateCreateInfo rasterization_state_create_info;
  rasterization_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
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

VkPipelineMultisampleStateCreateInfo
create_multisample_state(VkSampleCountFlagBits sample_count_flag) {
  VkPipelineMultisampleStateCreateInfo multisample_state_create_info;
  multisample_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
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
  depth_stencil_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
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

VkPipelineColorBlendAttachmentState
create_color_blend_attachment_state(BlendMode blend_mode) {
  VkPipelineColorBlendAttachmentState color_blend_attachment_state;
  VkBool32 blending_enabled =
      (blend_mode == BLEND_MODE_ALPHA) ? VK_TRUE : VK_FALSE;
  color_blend_attachment_state.blendEnable = blending_enabled;
  color_blend_attachment_state.colorWriteMask =
      VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

  switch (blend_mode) {

  case BLEND_MODE_ALPHA:
    color_blend_attachment_state.srcColorBlendFactor =
        VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor =
        VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
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

VkPipelineColorBlendStateCreateInfo create_color_blend_state(
    uint32_t num_color_blend_attachments,
    const VkPipelineColorBlendAttachmentState *color_blend_attachment_states) {
  VkPipelineColorBlendStateCreateInfo color_blend_state_create_info;
  color_blend_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  color_blend_state_create_info.pNext = NULL;
  color_blend_state_create_info.flags = 0;
  color_blend_state_create_info.logicOpEnable = VK_FALSE;
  color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
  color_blend_state_create_info.attachmentCount = num_color_blend_attachments;
  color_blend_state_create_info.pAttachments = color_blend_attachment_states;
  float blend_constants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  memcpy(color_blend_state_create_info.blendConstants, blend_constants,
         sizeof(blend_constants));
  return color_blend_state_create_info;
}

VkPipelineShaderStageCreateInfo
create_shader_stage_info(VkShaderModule module, VkShaderStageFlagBits stage,
                         const char *entry_point) {
  VkPipelineShaderStageCreateInfo shader_stage_create_info;
  shader_stage_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
  shader_stage_create_info.pNext = NULL;
  shader_stage_create_info.flags = 0;
  shader_stage_create_info.stage = stage;
  shader_stage_create_info.module = module;
  shader_stage_create_info.pName = entry_point;
  shader_stage_create_info.pSpecializationInfo = NULL;
  return shader_stage_create_info;
}

// TODO Validate shader stages if you want strict pipeline guarantees
// TODO Print/log pipeline cache usage for debugging
VkPipeline create_graphics_pipeline(VkDevice device, PipelineConfig *config,
                                    VkPipelineCache pipeline_cache) {

  assert(config->stage_count <= MAX_SHADER_STAGE_COUNT);
  VkPipelineShaderStageCreateInfo stages[MAX_SHADER_STAGE_COUNT];
  for (uint32_t i = 0; i < config->stage_count; i++) {
    stages[i] = create_shader_stage_info(config->stages[i].module,
                                         config->stages[i].stage,
                                         config->stages[i].entry_point);
  }

  VkGraphicsPipelineCreateInfo graphics_pipeline_create_info;
  graphics_pipeline_create_info.sType =
      VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  graphics_pipeline_create_info.pNext = 0;
  graphics_pipeline_create_info.flags = 0;
  graphics_pipeline_create_info.stageCount = config->stage_count;
  graphics_pipeline_create_info.pStages = stages;

  assert(config->vertex_input_state_create_info &&
         "create_graphics_pipeline: vertex_input_state_create_info is NULL. "
         "Did you mean to fill out this structure manually?");
  graphics_pipeline_create_info.pVertexInputState =
      config->vertex_input_state_create_info;

  VkPipelineInputAssemblyStateCreateInfo input_assembly_state =
      create_pipeline_input_assembly_state(config->topology,
                                           config->primitive_restart_enabled);
  graphics_pipeline_create_info.pInputAssemblyState = &input_assembly_state;

  // TOOD?
  graphics_pipeline_create_info.pTessellationState = NULL;

  VkOffset2D offset;
  offset.x = 0.0f;
  offset.y = 0.0f;
  ViewportState viewport_state =
      create_viewport_state(config->swapchain_extent, offset);

  VkPipelineViewportStateCreateInfo viewport_state_create_info;
  viewport_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state_create_info.pNext = NULL;
  viewport_state_create_info.flags = 0;
  viewport_state_create_info.viewportCount = 1;
  viewport_state_create_info.pViewports = &viewport_state.viewport;
  viewport_state_create_info.scissorCount = 1;
  viewport_state_create_info.pScissors = &viewport_state.scissor;
  graphics_pipeline_create_info.pViewportState = &viewport_state_create_info;

  VkPipelineRasterizationStateCreateInfo rasterization_state =
      create_rasterization_state(config->polygon_mode, config->cull_mode,
                                 config->front_face);
  graphics_pipeline_create_info.pRasterizationState = &rasterization_state;

  VkPipelineMultisampleStateCreateInfo multisample_state_create_info =
      create_multisample_state(config->sample_count_flag);
  graphics_pipeline_create_info.pMultisampleState =
      &multisample_state_create_info;

  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info =
      create_depth_stencil_state();
  graphics_pipeline_create_info.pDepthStencilState =
      &depth_stencil_state_create_info;

  const uint32_t num_color_blend_attachments = 1;
  VkPipelineColorBlendAttachmentState color_blend_state =
      create_color_blend_attachment_state(config->blend_mode);
  VkPipelineColorBlendStateCreateInfo color_blend_state_create_info =
      create_color_blend_state(num_color_blend_attachments, &color_blend_state);
  graphics_pipeline_create_info.pColorBlendState =
      &color_blend_state_create_info;

  // anticipating use of vkCmdSetViewport(), vkCmdSetScissor()
  const uint32_t num_dynamic_states = 2;
  VkDynamicState dynamic_states[num_dynamic_states] = {
      VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state_create_info;
  dynamic_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
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
  VK_CHECK(vkCreateGraphicsPipelines(device, pipeline_cache, 1,
                                     &graphics_pipeline_create_info, NULL,
                                     &graphics_pipeline),
           "create_graphics_pipeline: Failed to vkCreateGraphicsPipelines");
  return graphics_pipeline;
}

bool begin_frame(VulkanContext *context) {
  vkWaitForFences(context->device, 1,
                  &context->frame_sync_objects[context->current_frame_index]
                       .in_flight_fence,
                  VK_TRUE, UINT64_MAX);

  vkResetFences(context->device, 1,
                &context->frame_sync_objects[context->current_frame_index]
                     .in_flight_fence);

  VkResult result = vkAcquireNextImageKHR(
      context->device, context->swapchain, UINT64_MAX,
      context->frame_sync_objects[context->current_frame_index]
          .image_available_semaphore,
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
  VkCommandBuffer command_buffer =
      context->graphics_command_buffers[context->image_index];

  VkCommandBufferBeginInfo command_buffer_begin_info;
  command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  command_buffer_begin_info.pNext = NULL;
  command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  command_buffer_begin_info.pInheritanceInfo = NULL;

  VK_CHECK(vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info),
           "begin_command_buffer: failed to vkBeginCommandBuffer");

  return command_buffer;
}

// potentially rename to begin_default_render_pass
// leaves open possibility of rendering to other framebuffers
void begin_render_pass(const VulkanContext *context,
                       VkCommandBuffer command_buffer, VkClearValue clear_value,
                       VkOffset2D offset) {
  VkRenderPassBeginInfo render_pass_begin_info;
  render_pass_begin_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  render_pass_begin_info.pNext = NULL;
  render_pass_begin_info.renderPass = context->render_pass;
  render_pass_begin_info.framebuffer =
      context->framebuffers[context->image_index];
  render_pass_begin_info.renderArea.offset = offset;
  render_pass_begin_info.renderArea.extent = context->swapchain_extent;
  render_pass_begin_info.clearValueCount = 1;
  render_pass_begin_info.pClearValues = &clear_value;
  vkCmdBeginRenderPass(command_buffer, &render_pass_begin_info,
                       VK_SUBPASS_CONTENTS_INLINE);
}

void submit_and_present(const VulkanContext *context,
                        VkCommandBuffer command_buffer) {

  VkSubmitInfo submit_info;
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  submit_info.pNext = NULL;
  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores =
      &context->frame_sync_objects[context->current_frame_index]
           .image_available_semaphore;
  VkPipelineStageFlags wait_stage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  submit_info.pWaitDstStageMask = &wait_stage;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffer;
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores =
      &context->frame_sync_objects[context->current_frame_index]
           .render_finished_semaphore;

  VK_CHECK(
      vkQueueSubmit(context->graphics_queue, 1, &submit_info,
                    context->frame_sync_objects[context->current_frame_index]
                        .in_flight_fence),
      "sumbit_and_present: Failed to vkQueueSubmit");

  VkPresentInfoKHR present_info;
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.pNext = NULL;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores =
      &context->frame_sync_objects[context->current_frame_index]
           .render_finished_semaphore;
  present_info.swapchainCount = 1;
  present_info.pSwapchains = &context->swapchain;
  present_info.pImageIndices = &context->image_index;
  present_info.pResults = NULL;

  // TODO handle swapchain recreation here if need be
  VK_CHECK(vkQueuePresentKHR(context->graphics_queue, &present_info),
           "sumbit_and_present: Failed to vkQueuePresentKHR");
}

VkPipelineVertexInputStateCreateInfo create_vertex_input_state(
    uint32_t binding_description_count,
    const VkVertexInputBindingDescription *binding_descriptions,
    uint32_t attribute_description_count,
    const VkVertexInputAttributeDescription *attribute_descriptions) {

  VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info;
  vertex_input_state_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

  vertex_input_state_create_info.pNext = NULL;
  vertex_input_state_create_info.flags = 0;

  vertex_input_state_create_info.vertexBindingDescriptionCount =
      binding_description_count;
  vertex_input_state_create_info.pVertexBindingDescriptions =
      binding_descriptions;

  vertex_input_state_create_info.vertexAttributeDescriptionCount =
      attribute_description_count;
  vertex_input_state_create_info.pVertexAttributeDescriptions =
      attribute_descriptions;

  return vertex_input_state_create_info;
}

VkPipelineLayout
create_pipeline_layout(VkDevice device,
                       const VkDescriptorSetLayout *descriptor_set_layout,
                       uint32_t set_layout_count) {

  VkPipelineLayoutCreateInfo pipeline_layout_create_info;
  pipeline_layout_create_info.sType =
      VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_create_info.pNext = NULL;
  pipeline_layout_create_info.flags = 0;
  pipeline_layout_create_info.setLayoutCount = set_layout_count;
  pipeline_layout_create_info.pSetLayouts = descriptor_set_layout;
  pipeline_layout_create_info.pushConstantRangeCount = 0;
  pipeline_layout_create_info.pPushConstantRanges = NULL;

  VkPipelineLayout pipeline_layout;
  VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_create_info, NULL,
                                  &pipeline_layout),
           "create_graphics_pipeline: Failed to vkCreatePipelineLayout");
  return pipeline_layout;
}

// TODO VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT
VkDescriptorPool create_descriptor_pool(VkDevice device,
                                        const VkDescriptorPoolSize *pool_sizes,
                                        uint32_t pool_size_count,
                                        uint32_t max_sets) {
  VkDescriptorPoolCreateInfo descriptor_pool_create_info;
  descriptor_pool_create_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  descriptor_pool_create_info.pNext = NULL;
  descriptor_pool_create_info.flags = 0;
  descriptor_pool_create_info.maxSets = max_sets;
  descriptor_pool_create_info.poolSizeCount = pool_size_count;
  descriptor_pool_create_info.pPoolSizes = pool_sizes;

  VkDescriptorPool descriptor_pool;
  VK_CHECK(vkCreateDescriptorPool(device, &descriptor_pool_create_info, NULL,
                                  &descriptor_pool),
           "create_descriptor_pool: Failed to vkCreateDescriptorPool");
  return descriptor_pool;
}

VkDescriptorSet
create_descriptor_set(VkDevice device, VkDescriptorPool descriptor_pool,
                      VkDescriptorSetLayout descriptor_set_layout) {
  VkDescriptorSetAllocateInfo descriptor_set_allocate_info;
  descriptor_set_allocate_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  descriptor_set_allocate_info.pNext = NULL;
  descriptor_set_allocate_info.descriptorPool = descriptor_pool;
  descriptor_set_allocate_info.descriptorSetCount = 1;
  descriptor_set_allocate_info.pSetLayouts = &descriptor_set_layout;

  VkDescriptorSet descriptor_set;
  VK_CHECK(vkAllocateDescriptorSets(device, &descriptor_set_allocate_info,
                                    &descriptor_set),
           "create_descriptor_set: failed to vkAllocateDescriptorSets");
  return descriptor_set;
}

// TODO VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR
VkDescriptorSetLayout
create_descriptor_set_layout(VkDevice device,
                             const VkDescriptorSetLayoutBinding *bindings,
                             uint32_t binding_count) {
  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_create_info;
  descriptor_set_layout_create_info.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  descriptor_set_layout_create_info.pNext = NULL;
  descriptor_set_layout_create_info.flags = 0;
  descriptor_set_layout_create_info.bindingCount = binding_count;
  descriptor_set_layout_create_info.pBindings = bindings;

  VkDescriptorSetLayout descriptor_set_layout;
  VK_CHECK(
      vkCreateDescriptorSetLayout(device, &descriptor_set_layout_create_info,
                                  NULL, &descriptor_set_layout),
      "create_descriptor_set_layout: failed to vkCreateDescriptorSetLayout");
  return descriptor_set_layout;
}

void update_uniform_descriptor_sets(VkDevice device, VkBuffer buffer,
                                    VkDeviceSize offset, VkDeviceSize range,
                                    VkDescriptorSet descriptor_set,
                                    uint32_t binding) {
  VkDescriptorBufferInfo descriptor_buffer_info;
  descriptor_buffer_info.buffer = buffer;
  descriptor_buffer_info.offset = offset;
  descriptor_buffer_info.range = range;

  VkWriteDescriptorSet write_descriptor_set;
  write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write_descriptor_set.pNext = NULL;
  write_descriptor_set.dstSet = descriptor_set;
  write_descriptor_set.dstBinding = binding;
  write_descriptor_set.dstArrayElement = 0;
  write_descriptor_set.descriptorCount = 1;
  write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write_descriptor_set.pImageInfo = NULL;
  write_descriptor_set.pBufferInfo = &descriptor_buffer_info;
  write_descriptor_set.pTexelBufferView = NULL;
  vkUpdateDescriptorSets(device, 1, &write_descriptor_set, 0, NULL);
}

StagingArena create_staging_arena(const VulkanContext *context,
                                  uint32_t total_size) {
  StagingArena staging_arena;
  VulkanBuffer staging_buffer =
      create_buffer(context, BUFFER_TYPE_STAGING, total_size);
  staging_arena.buffer = staging_buffer;
  staging_arena.total_size = total_size;
  staging_arena.offset = 0;
  staging_arena.num_copy_regions = 0;
  memset(staging_arena.copy_regions, 0, sizeof(staging_arena.copy_regions));
  return staging_arena;
}

// TODO alignment padding?
// add handle with size, usage, offset, instead of just returning offset?
uint32_t stage_data_explicit(const VulkanContext *context, StagingArena *arena,
                             void *data, uint32_t size, VkBuffer destination,
                             uint32_t dst_offset) {
  assert(arena->offset + size <= arena->total_size);
  assert(arena->num_copy_regions < MAX_COPY_REGIONS);
  uint32_t written_data_offset = arena->offset;

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

uint32_t stage_data_auto(const VulkanContext *context, StagingArena *arena,
                         void *data, uint32_t size, VkBuffer destination) {
  return stage_data_explicit(context, arena, data, size, destination,
                             arena->offset);
}

void flush_staging_arena(const VulkanContext *context, StagingArena *arena) {

  VkCommandBuffer command_buffer = begin_single_use_command_buffer(context);

  if (arena->num_copy_regions == 0) {
    return;
  }

  VkBuffer current_destination = arena->destination_buffers[0];
  uint32_t current_batch_start = 0;

  for (uint32_t i = 1; i < arena->num_copy_regions; i++) {
    if (arena->destination_buffers[i] == current_destination) {
      continue;
    }

    vkCmdCopyBuffer(command_buffer, arena->buffer.buffer, current_destination,
                    i - current_batch_start,
                    arena->copy_regions + current_batch_start);

    current_destination = arena->destination_buffers[i];
    current_batch_start = i;
  }

  vkCmdCopyBuffer(command_buffer, arena->buffer.buffer, current_destination,
                  arena->num_copy_regions - current_batch_start,
                  arena->copy_regions + current_batch_start);

  end_single_use_command_buffer(context, command_buffer);
}

ShaderStage create_shader_stage(VkShaderModule module,
                                VkShaderStageFlagBits stage,
                                const char *entry_point) {
  ShaderStage shader_stage;
  shader_stage.module = module;
  shader_stage.entry_point = entry_point;
  shader_stage.stage = stage;
  return shader_stage;
}

UniformBuffer
create_uniform_buffer_explicit(const VulkanContext *context, uint32_t binding,
                               VkShaderStageFlags stage_flags,
                               uint32_t descriptor_count, uint32_t buffer_size,
                               uint32_t binding_count,
                               VkDescriptorPool descriptor_pool, bool dynamic) {
  UniformBuffer uniform_buffer;

  VkDescriptorSetLayoutBinding ubo_layout_binding;
  ubo_layout_binding.binding = binding;
  ubo_layout_binding.descriptorType =
      dynamic ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC
              : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = descriptor_count;
  ubo_layout_binding.stageFlags = stage_flags;
  ubo_layout_binding.pImmutableSamplers = NULL;

  VkDescriptorSetLayout descriptor_set_layout = create_descriptor_set_layout(
      context->device, &ubo_layout_binding, binding_count);
  VkDescriptorSet descriptor_set = create_descriptor_set(
      context->device, descriptor_pool, descriptor_set_layout);

  VulkanBuffer vulkan_buffer =
      create_buffer(context, BUFFER_TYPE_UNIFORM, buffer_size);
  update_uniform_descriptor_sets(context->device, vulkan_buffer.buffer, 0,
                                 buffer_size, descriptor_set, 0);

  uniform_buffer.vulkan_buffer = vulkan_buffer;
  uniform_buffer.descriptor_set_layout = descriptor_set_layout;
  uniform_buffer.descriptor_set = descriptor_set;
  uniform_buffer.size = buffer_size;
  return uniform_buffer;
}

// TODO better understand bindings and what is necessary
UniformBuffer create_uniform_buffer(const VulkanContext *context,
                                    uint32_t buffer_size,
                                    VkDescriptorPool descriptor_pool) {
  UniformBuffer uniform_buffer = create_uniform_buffer_explicit(
      context, 0, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 1,
      buffer_size, 1, descriptor_pool, false);

  vkMapMemory(context->device, uniform_buffer.vulkan_buffer.memory, 0,
              buffer_size, 0, &uniform_buffer.mapped);

  return uniform_buffer;
}

// TODO finish when I actually need dynamic buffers
UniformBuffer create_dynamic_uniform_buffer(const VulkanContext *context,
                                            uint32_t c_struct_size,
                                            VkDescriptorPool descriptor_pool) {

  UniformBuffer uniform_buffer = create_uniform_buffer_explicit(
      context, 0, VK_SHADER_STAGE_FRAGMENT_BIT | VK_SHADER_STAGE_VERTEX_BIT, 1,
      c_struct_size, 1, descriptor_pool, true);

  return uniform_buffer;
}

void destroy_uniform_buffer(const VulkanContext *context,
                            UniformBuffer *uniform_buffer) {
  vkDestroyDescriptorSetLayout(context->device,
                               uniform_buffer->descriptor_set_layout, NULL);
  destroy_vulkan_buffer(context, uniform_buffer->vulkan_buffer);
}

void write_to_uniform_buffer(UniformBuffer *uniform_buffer, const void *data,
                             uint32_t size) {
  memcpy(uniform_buffer->mapped, data, size);
}

VertexLayoutBuilder create_vertex_layout_builder() {
  VertexLayoutBuilder builder;
  builder.binding_description_count = 0;
  memset(builder.binding_descriptions, 0, sizeof(builder.binding_descriptions));
  builder.attribute_description_count = 0;
  memset(builder.attribute_descriptions, 0,
         sizeof(builder.attribute_descriptions));
  return builder;
}

void push_vertex_binding(VertexLayoutBuilder *builder, uint32_t binding,
                         uint32_t stride, VkVertexInputRate input_rate) {
  assert(builder->binding_description_count < MAX_VERTEX_BINDINGS);

  VkVertexInputBindingDescription *description =
      &builder->binding_descriptions[builder->binding_description_count];
  description->binding = binding;
  description->stride = stride;
  description->inputRate = input_rate;

  builder->binding_description_count++;
}

void push_vertex_attribute(VertexLayoutBuilder *builder, uint32_t location,
                           uint32_t binding, VkFormat format, uint32_t offset) {

  assert(builder->attribute_description_count < MAX_VERTEX_ATTRIBUTES);

  VkVertexInputAttributeDescription *description =
      &builder->attribute_descriptions[builder->attribute_description_count];
  description->location = location;
  description->binding = binding;
  description->format = format;
  description->offset = offset;

  builder->attribute_description_count++;
}

VkPipelineVertexInputStateCreateInfo
build_vertex_input_state(VertexLayoutBuilder *builder) {
  return create_vertex_input_state(
      builder->binding_description_count, builder->binding_descriptions,
      builder->attribute_description_count, builder->attribute_descriptions);
}
