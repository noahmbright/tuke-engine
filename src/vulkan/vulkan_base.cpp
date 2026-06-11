#include "vulkan_base.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vulkan/vulkan_beta.h>
#include <vulkan/vulkan_core.h>

void update_frame_index(VulkanContext *context) {
  context->current_frame_index = (context->current_frame_index + 1) % MAX_FRAMES_IN_FLIGHT;
}

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
    printf("%s(): Tried to destroy null swapchain\n", __func__);
  }

  memset(storage, 0, sizeof(SwapchainStorage));
}

void destroy_vulkan_context(VulkanContext *ctx) {
  VkResult result = vkDeviceWaitIdle(ctx->device);
  VK_CHECK(result, "Failed to wait idle");

  // Descriptors set layouts, then pipeline layouts, then pipelines
  reset_descriptor_set_layouts(ctx);
  vkDestroyPipelineCache(ctx->device, ctx->pipeline_cache, NULL);

  // Command pools
  vkDestroyCommandPool(ctx->device, ctx->transient_cmd_pool, NULL);
  vkDestroyCommandPool(ctx->device, ctx->graphics_cmd_pool, NULL);
  if (ctx->present_queue_index_is_different_than_graphics) {
    vkDestroyCommandPool(ctx->device, ctx->present_cmd_pool, NULL);
  }
  if (ctx->compute_queue_index_is_different_than_graphics) {
    vkDestroyCommandPool(ctx->device, ctx->compute_cmd_pool, NULL);
  }

  // sync primitives
  for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(ctx->device, ctx->frame_sync_objects[i].image_available_semaphore, NULL);
    vkDestroySemaphore(ctx->device, ctx->frame_sync_objects[i].render_finished_semaphore, NULL);
    vkDestroyFence(ctx->device, ctx->frame_sync_objects[i].in_flight_fence, NULL);
  }

  destroy_swapchain(ctx);

  vkDestroyDescriptorPool(ctx->device, ctx->descriptor_pool, NULL);
  vkDestroyRenderPass(ctx->device, ctx->render_pass, NULL);
  vkDestroyDevice(ctx->device, NULL);
  vkDestroySurfaceKHR(ctx->instance, ctx->surface, NULL);

#ifdef TUKE_DISABLE_VULKAN_VALIDATION
#else
  PFN_vkDestroyDebugUtilsMessengerEXT fpDestroyDebugUtilsMessengerEXT =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(ctx->instance, "vkDestroyDebugUtilsMessengerEXT");
  if (fpDestroyDebugUtilsMessengerEXT) {
    fpDestroyDebugUtilsMessengerEXT(ctx->instance, ctx->debug_messenger, NULL);
  }
#endif
  vkDestroyInstance(ctx->instance, NULL);
}

VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT sev,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data
) {
  (void)message_type;
  (void)callback_data;
  (void)user_data;

  const char *severity = "UNKNOWN";
  if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
    severity = "VERBOSE";
  else if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT)
    severity = "INFO";
  else if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT)
    severity = "WARNING";
  else if (sev & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
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

VkInstance create_instance(const char *name, VulkanWindowInfo window_info) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkApplicationInfo.html
  VkApplicationInfo application_info;
  application_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  application_info.pNext = NULL;
  application_info.pApplicationName = name;
  application_info.applicationVersion = 1;
  application_info.pEngineName = NULL;
  application_info.engineVersion = 0;
  application_info.apiVersion = VK_API_VERSION_1_3;

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
  const u32 total_extension_count = extra_extension_count + window_info.extension_count;
  if (total_extension_count > MAX_EXTENSIONS) {
    fprintf(
        stderr, "%s(): MAX_EXTENSIONS is %d, but total_extension_count is %d\n", __func__, MAX_EXTENSIONS,
        total_extension_count
    );
    exit(1);
  }

  const char *enabled_extensions[MAX_EXTENSIONS];
  memcpy(enabled_extensions, window_info.extensions, sizeof(char *) * window_info.extension_count);

  for (u32 i = 0; i < extra_extension_count; i++) {
    enabled_extensions[window_info.extension_count + i] = extra_extensions[i];
  }

  VkDebugUtilsMessengerCreateInfoEXT debug_msngr_create_info;
  populate_debug_msngr_create_info(&debug_msngr_create_info);

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkInstanceCreateInfo.html
#ifdef TUKE_DISABLE_VULKAN_VALIDATION
  void *next = NULL;
#else
  void *next = &debug_msngr_create_info;
#endif
  VkInstanceCreateInfo instance_create_info = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = next,
      .flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR,
      .pApplicationInfo = &application_info,
      .enabledLayerCount = layer_count,
      .ppEnabledLayerNames = validation_layers,
      .enabledExtensionCount = total_extension_count,
      .ppEnabledExtensionNames = enabled_extensions,
  };

  VkInstance instance;
  VkResult result = vkCreateInstance(&instance_create_info, NULL, &instance);
  VK_CHECK(result, "Failed to create instance");
  return instance;
}

VkDebugUtilsMessengerEXT create_debug_messenger(VkInstance instance) {
  VkDebugUtilsMessengerCreateInfoEXT debug_msngr_create_info;
  populate_debug_msngr_create_info(&debug_msngr_create_info);

  PFN_vkCreateDebugUtilsMessengerEXT create_fn =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

  if (!create_fn) {
    fprintf(stderr, "%s(): Failed to load vkCreateDebugUtilsMessengerEXT\n", __func__);
    exit(1);
  }
  VkDebugUtilsMessengerEXT debug_messenger;
  VkResult result = create_fn(instance, &debug_msngr_create_info, NULL, &debug_messenger);
  VK_CHECK(result, "Failed to create VkDebugUtilsMessengerEXT");
  return debug_messenger;
}

u32 get_physical_devices(VkInstance instance, VkPhysicalDevice *physical_devices) {
  u32 device_count;
  VkResult result = vkEnumeratePhysicalDevices(instance, &device_count, NULL);
  VK_CHECK(result, "Failed to enumerate physical device count");
  if (device_count == 0) {
    fprintf(stderr, "%s(): Failed to find a physical device. device_count = 0\n", __func__);
    exit(1);
  }

  if (device_count > MAX_PHYSICAL_DEVICES) {
    printf(
        "%s(): Found more physical devices (%u) than MAX_PHYSICAL_DEVICES (%u)\n", __func__, device_count,
        MAX_PHYSICAL_DEVICES
    );
    device_count = MAX_PHYSICAL_DEVICES;
  }

  result = vkEnumeratePhysicalDevices(instance, &device_count, physical_devices);
  VK_CHECK(result, "Failed to enumerate physical devices");

  return device_count;
}

QueueFamilyIndices get_queue_family_indices(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
  u32 queue_family_count = 0;
  VkQueueFamilyProperties queue_families[MAX_QUEUE_FAMILIES];
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, NULL);
  vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, queue_families);

  i32 graphics_family = -1;
  i32 present_family = -1;
  i32 compute_family = -1;

  if (queue_family_count > MAX_QUEUE_FAMILIES) {
    printf(
        "%s(): MAX_QUEUE_FAMILIES is %d but queue_family_count is %d\n", __func__, MAX_QUEUE_FAMILIES,
        queue_family_count
    );
    queue_family_count = MAX_QUEUE_FAMILIES;
  }

  for (u32 i = 0; i < queue_family_count; i++) {
    if (queue_families[i].queueCount <= 0) {
      continue;
    }

    if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      graphics_family = i;
    }

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

// TODO May want to extend this to support compute only.
//      Not sure if we'd ever want graphics without present.
bool is_valid_queue_family_indices(QueueFamilyIndices indices) {
  bool is_invalid = indices.compute_family == -1 || indices.present_family == -1 || indices.graphics_family == -1;
  return !is_invalid;
}

VkPhysicalDevice
pick_physical_device(VkInstance instance, VkSurfaceKHR surface, QueueFamilyIndices *queue_family_indices) {

  VkPhysicalDevice physical_devices[MAX_PHYSICAL_DEVICES];
  u32 num_physical_devices = get_physical_devices(instance, physical_devices);

  VkPhysicalDevice res = NULL;
  for (u32 i = 0; i < num_physical_devices; i++) {
    VkPhysicalDevice physical_device = physical_devices[i];
    *queue_family_indices = get_queue_family_indices(physical_device, surface);

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
    fprintf(stderr, "%s(): result is NULL failed to find suitable physical device\n", __func__);
    exit(1);
  }

  VkPhysicalDeviceDriverProperties driver_props = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES,
  };
  VkPhysicalDeviceProperties2 device_properties = {
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2,
      .pNext = &driver_props,
  };
  vkGetPhysicalDeviceProperties2(res, &device_properties);
  VkPhysicalDeviceProperties *p = &device_properties.properties;
  printf("%s(): Selected device:      %s\n", __func__, p->deviceName);
  printf(
      "%s(): Vulkan API version:   %u.%u.%u\n", __func__, VK_API_VERSION_MAJOR(p->apiVersion),
      VK_API_VERSION_MINOR(p->apiVersion), VK_API_VERSION_PATCH(p->apiVersion)
  );
  printf("%s(): Driver:               %s (%s)\n", __func__, driver_props.driverName, driver_props.driverInfo);

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

    // https://registry.khronos.org/vulkan/specs/latest/man/html/VkDeviceQueueCreateInfo.html
    f32 queue_priority = 1.0f;
    VkDeviceQueueCreateInfo device_queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .pNext = NULL,
        .flags = 0,
        .queueFamilyIndex = indices[i],
        .queueCount = 1,
        .pQueuePriorities = &queue_priority,
    };
    out_infos[num_unique_queue_families++] = device_queue_create_info;
  }

  return num_unique_queue_families;
}

VkDevice create_device(QueueFamilyIndices queue_family_indices, VkPhysicalDevice physical_device) {

  VkDeviceQueueCreateInfo queue_create_infos[NUM_QUEUE_FAMILY_INDICES];
  u32 num_unique_queue_families = get_unique_queue_infos(queue_family_indices, queue_create_infos);

  u32 device_extension_count = 1;
  const char *device_extensions[2] = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };

  // Check to see if we are on MacOS and need the portability subset extension
  // The Vulkan spec states: If the VK_KHR_portability_subset extension is included in pProperties of
  // vkEnumerateDeviceExtensionProperties, ppEnabledExtensionNames must include "VK_KHR_portability_subset"
  u32 ext_count = 0;
  vkEnumerateDeviceExtensionProperties(physical_device, NULL, &ext_count, NULL);
  VkExtensionProperties exts[256]; // or however you handle it
  vkEnumerateDeviceExtensionProperties(physical_device, NULL, &ext_count, exts);

  bool need_portability = false;
  bool has_dynamic_rendering = false;
  for (u32 i = 0; i < ext_count; i++) {
    if (strcmp(exts[i].extensionName, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME) == 0) {
      need_portability = true;
    }
    if (strcmp(exts[i].extensionName, VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME) == 0) {
      has_dynamic_rendering = true;
    }
  }
  printf("%s(): VK_KHR_dynamic_rendering: %s\n", __func__, has_dynamic_rendering ? "supported" : "NOT supported");

  if (need_portability) {
    device_extensions[device_extension_count++] = VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME;
  }

  // Versioned features — only chain 1.3 struct if the device actually supports 1.3.
  // Chaining a versioned features struct for a version the device doesn't support is a validation error.
  VkPhysicalDeviceProperties device_props;
  vkGetPhysicalDeviceProperties(physical_device, &device_props);

  VkPhysicalDeviceVulkan12Features vk_1_2_features{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
  };
  VkPhysicalDeviceVulkan13Features vk_1_3_features{
      .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
      .pNext = &vk_1_2_features,
  };
  VkPhysicalDeviceFeatures vk_1_0_features = {};

  u32 minor_version = VK_VERSION_MINOR(device_props.apiVersion);
  void *features_pnext = (minor_version >= 3) ? (void *)&vk_1_3_features : (void *)&vk_1_2_features;

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkDeviceCreateInfo.html
  VkDeviceCreateInfo device_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .pNext = features_pnext,
      .flags = 0,
      .queueCreateInfoCount = num_unique_queue_families,
      .pQueueCreateInfos = queue_create_infos,
      .enabledLayerCount = 0,      // enabledLayerCount is deprecated and should not be used
      .ppEnabledLayerNames = NULL, // ppEnabledLayerNames is deprecated and should not be used
      .enabledExtensionCount = device_extension_count,
      .ppEnabledExtensionNames = device_extensions,
      .pEnabledFeatures = &vk_1_0_features,
  };

  // https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateDevice.html
  VkDevice device;
  VkResult result = vkCreateDevice(physical_device, &device_create_info, NULL, &device);
  VK_CHECK(result, "Failed to vkCreateDevice()");
  return device;
}

VkSurfaceFormatKHR get_surface_format(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {

  u32 format_count = 0;
  vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, NULL);

  const u32 MAX_FORMAT_COUNT = 128;
  if (format_count > MAX_FORMAT_COUNT) {
    printf(
        "%s(): found more formats than expected. Max is %d, found %d. Clamping.\n", __func__, MAX_FORMAT_COUNT,
        format_count
    );
    format_count = MAX_FORMAT_COUNT;
  }
  VkSurfaceFormatKHR formats[MAX_FORMAT_COUNT];
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

static u32 clamp_u32(u32 x, u32 min, u32 max) {
  if (x < min) {
    return min;
  }
  if (x > max) {
    return max;
  }
  return x;
}

VkExtent2D get_swapchain_extent(u32 width, u32 height, VkSurfaceCapabilitiesKHR surface_capabilities) {
  if (surface_capabilities.currentExtent.width != UINT32_MAX) {
    return surface_capabilities.currentExtent;
  }

  VkExtent2D min_extent = surface_capabilities.minImageExtent;
  VkExtent2D max_extent = surface_capabilities.maxImageExtent;
  VkExtent2D extent;
  extent.width = clamp_u32(width, min_extent.width, max_extent.width);
  extent.height = clamp_u32(height, min_extent.height, max_extent.height);
  return extent;
}

VkSurfaceCapabilitiesKHR get_surface_capabilities(VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
  VkSurfaceCapabilitiesKHR surface_capabilities;
  VkResult result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &surface_capabilities);
  VK_CHECK(result, "Failed to get surface capabilities");
  return surface_capabilities;
}

VkSwapchainKHR create_swapchain(
    VkDevice device,
    VkPhysicalDevice physical_device,
    VkSurfaceKHR surface,
    QueueFamilyIndices queue_family_indices,
    VkSurfaceFormatKHR surface_format,
    VkSurfaceCapabilitiesKHR surface_capabilities,
    VkExtent2D swapchain_extent
) {
  // TODO why am I adding 1 here?
  u32 image_count = surface_capabilities.minImageCount + 1;
  if (surface_capabilities.maxImageCount > 0 && image_count > surface_capabilities.maxImageCount) {
    image_count = surface_capabilities.maxImageCount;
  }

  const u32 MAX_PRESENT_MODE_COUNT = 16;
  VkPresentModeKHR present_modes[MAX_PRESENT_MODE_COUNT];
  u32 present_mode_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, NULL);
  vkGetPhysicalDeviceSurfacePresentModesKHR(physical_device, surface, &present_mode_count, present_modes);

  printf("Physical Device Surface has %u present modes.\n", present_mode_count);
  if (present_mode_count > MAX_PRESENT_MODE_COUNT) {
    present_mode_count = MAX_PRESENT_MODE_COUNT;
    printf("Clamping Physical Device Surface present modes to %u.\n", MAX_PRESENT_MODE_COUNT);
  }

  // VK_PRESENT_MODE_FIFO_KHR is the only mode guaranteed by the spec to be always available.
  VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
  for (u32 i = 0; i < present_mode_count; i++) {
    // TODO Why do I prefer this mode when available?
    if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      present_mode = present_modes[i];
      break;
    }
  }

  // TODO Need to better understand all these fields, and better understand swapchain creation in
  //      general. I may want to move away from the static/dynamic storage thing in the context.
  // TODO What is v-sync?
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSwapchainCreateInfoKHR.html
  VkSwapchainCreateInfoKHR swapchain_ci = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .pNext = NULL,
      .flags = 0,
      .surface = surface,
      .minImageCount = image_count,
      .imageFormat = surface_format.format,
      .imageColorSpace = surface_format.colorSpace,
      .imageExtent = swapchain_extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .preTransform = surface_capabilities.currentTransform,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = present_mode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };

  if (queue_family_indices.graphics_family != queue_family_indices.present_family) {
    u32 queue_family_indices_array[] = {
        (u32)queue_family_indices.graphics_family,
        (u32)queue_family_indices.present_family,
    };
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    swapchain_ci.queueFamilyIndexCount = 2;
    swapchain_ci.pQueueFamilyIndices = queue_family_indices_array;
  } else {
    swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchain_ci.queueFamilyIndexCount = 0;
    swapchain_ci.pQueueFamilyIndices = NULL;
  }

  // https://registry.khronos.org/vulkan/specs/latest/man/html/vkCreateSwapchainKHR.html
  VkSwapchainKHR swapchain;
  VkResult result = vkCreateSwapchainKHR(device, &swapchain_ci, NULL, &swapchain);
  VK_CHECK(result, "Failed to create swapchain");
  return swapchain;
}

void transition_image_layout(VkCommandBuffer cmd, VkImage image, VkImageLayout old_layout, VkImageLayout new_layout) {
  VkImageMemoryBarrier image_memory_barrier{
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .pNext = NULL,
      .srcAccessMask = 0,
      .dstAccessMask = 0,
      .oldLayout = old_layout,
      .newLayout = new_layout,
      .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
      .image = image,
      .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1,
  };

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

    // Always include stencil bit: formats like D32_SFLOAT_S8_UINT require both
    // unless separateDepthStencilLayouts is enabled (we don't enable it).
    image_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;

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
  vkCmdPipelineBarrier(cmd, src_stage, dst_stage, 0, 0, NULL, 0, NULL, 1, &image_memory_barrier);
}

VkImageView create_default_image_view(
    const VulkanContext *context, VkImage image, VkFormat format, VkImageAspectFlags aspect_flags
) {
  // TODO allow for depth and stencil attachments
  VkImageViewCreateInfo image_view_create_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
      .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
      .subresourceRange.aspectMask = aspect_flags,
      .subresourceRange.baseMipLevel = 0,
      .subresourceRange.levelCount = 1,
      .subresourceRange.baseArrayLayer = 0,
      .subresourceRange.layerCount = 1,
  };

  VkImageView image_view;
  VkResult result = vkCreateImageView(context->device, &image_view_create_info, NULL, &image_view);
  VK_CHECK(result, "Failed to create image view");

  return image_view;
}

VkImage
create_default_image(const VulkanContext *context, u32 width, u32 height, VkImageUsageFlags usage, VkFormat format) {
  // TODO Better understand all these fields
  //      When do I need more samples? Need mipmap support
  //      Don't understand queue families
  //      What is needed for a texture image vs other images?
  VkImageCreateInfo texture_image_info = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
      .pNext = 0,
      .flags = 0,
      .imageType = VK_IMAGE_TYPE_2D,
      .format = format, // TODO fallback for HW where formats are not supported?
      .extent.width = width,
      .extent.height = height,
      .extent.depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .tiling = VK_IMAGE_TILING_OPTIMAL,
      .usage = usage,
      .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .queueFamilyIndexCount = 0,
      .pQueueFamilyIndices = NULL,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
  };

  VkImage image;
  VkResult result = vkCreateImage(context->device, &texture_image_info, NULL, &image);
  VK_CHECK(result, "Failed to create image");

  return image;
}

VkDeviceMemory allocate_and_bind_image_memory(const VulkanContext *ctx, VkImage image) {
  // create memory
  VkMemoryRequirements memory_reqs;
  vkGetImageMemoryRequirements(ctx->device, image, &memory_reqs);

  VkMemoryAllocateInfo memory_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
      .pNext = 0,
      .allocationSize = memory_reqs.size,
      .memoryTypeIndex =
          find_memory_type(ctx->physical_device, memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT),
  };

  VkDeviceMemory device_memory;
  VkResult result = vkAllocateMemory(ctx->device, &memory_allocate_info, NULL, &device_memory);
  VK_CHECK(result, "Failed to allocate image memory");

  result = vkBindImageMemory(ctx->device, image, device_memory, 0);
  VK_CHECK(result, "Failed to bind image memory");

  return device_memory;
}

VkFormat find_depth_format(VkPhysicalDevice physical_device) {
  VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
  for (u32 i = 0; i < sizeof(candidates) / sizeof(VkFormat); i++) {
    VkFormatProperties2 props{.sType = VK_STRUCTURE_TYPE_FORMAT_PROPERTIES_2};
    vkGetPhysicalDeviceFormatProperties2(physical_device, candidates[i], &props);
    if (props.formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
      return candidates[i];
  }
  fprintf(stderr, "%s(): no supported depth format found\n", __func__);
  exit(1);
}

DepthBuffer create_depth_buffer(VulkanContext *context, VkFormat depth_format) {

  VkImageUsageFlags usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  VkImageAspectFlagBits aspect = VK_IMAGE_ASPECT_DEPTH_BIT;
  VkExtent2D extent = context->swapchain_extent;

  DepthBuffer depth_buffer;
  depth_buffer.image = create_default_image(context, extent.width, extent.height, usage, depth_format);
  depth_buffer.device_memory = allocate_and_bind_image_memory(context, depth_buffer.image);
  depth_buffer.image_view = create_default_image_view(context, depth_buffer.image, depth_format, aspect);

  VkCommandBuffer cmd = begin_single_use_command_buffer(context);
  VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout new_layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  transition_image_layout(cmd, depth_buffer.image, old_layout, new_layout);
  end_single_use_command_buffer(context, cmd);

  return depth_buffer;
}

SwapchainStorage create_swapchain_storage(VulkanContext *context, VkFormat depth_format) {
  SwapchainStorage storage;
  storage.use_static = true;

  // Query swapchain for image count. The swapchain owns the images we present to, so it is necessary
  // to ask for them. We cannot hardcode them in the application.
  u32 swapchain_image_count;
  VkResult result = vkGetSwapchainImagesKHR(context->device, context->swapchain, &swapchain_image_count, NULL);

  VkImage *images;
  VkImageView *image_views;
  if (swapchain_image_count > NUM_SWAPCHAIN_IMAGES) {
    storage.use_static = false;
    storage.as.dynamic_storage.images = (VkImage *)malloc(swapchain_image_count * sizeof(VkImage));
    storage.as.dynamic_storage.image_views = (VkImageView *)malloc(swapchain_image_count * sizeof(VkImageView));

    if (!storage.as.dynamic_storage.images || !storage.as.dynamic_storage.image_views) {
      fprintf(stderr, "Failed to allocate memory for swapchain storage\n");
      exit(1);
    }
    images = storage.as.dynamic_storage.images;
    image_views = storage.as.dynamic_storage.image_views;
  } else {
    images = storage.as.static_storage.images;
    image_views = storage.as.static_storage.image_views;
  }

  result = vkGetSwapchainImagesKHR(context->device, context->swapchain, &swapchain_image_count, images);
  VK_CHECK(result, "Failed to get swapchain images");
  storage.image_count = swapchain_image_count;

  for (u32 i = 0; i < swapchain_image_count; i++) {
    VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
    VkFormat surface_format = context->surface_format.format;

    if (storage.use_static) {
      VkImage image = images[i];
      image_views[i] = create_default_image_view(context, image, surface_format, aspect_flags);
    } else {
      VkImage image = images[i];
      image_views[i] = create_default_image_view(context, image, surface_format, aspect_flags);
    }
  }

  // TODO Want to better manage depth buffers and not have a hardcoded depth/color framebuffer in the context
  // TODO why am I using NUM_SWAPCHAIN_IMAGES instead of swapchain_image_count?
  for (u32 i = 0; i < NUM_SWAPCHAIN_IMAGES; i++) {
    storage.depth_buffers[i] = create_depth_buffer(context, depth_format);
  }

  return storage;
}

VkAttachmentDescription make_color_attachment(VkFormat format) {
  VkAttachmentDescription color_attachment = {
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
  };
  return color_attachment;
}

VkAttachmentDescription make_depth_attachment(VkFormat format) {
  VkAttachmentDescription depth_attachment = {
      .flags = 0,
      .format = format,
      .samples = VK_SAMPLE_COUNT_1_BIT,
      .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
      .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };
  return depth_attachment;
}

VkRenderPass create_render_pass(
    VkDevice device,
    u32 num_attachment_descriptions,
    const VkAttachmentDescription *attachment_descriptions,
    u32 num_subpass_descriptions,
    const VkSubpassDescription *subpass_descriptions,
    u32 num_dependencies,
    const VkSubpassDependency *dependencies
) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkRenderPassCreateInfo.html
  VkRenderPassCreateInfo render_pass_create_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .attachmentCount = num_attachment_descriptions,
      .pAttachments = attachment_descriptions,
      .subpassCount = num_subpass_descriptions,
      .pSubpasses = subpass_descriptions,
      .dependencyCount = num_dependencies,
      .pDependencies = dependencies,
  };

  VkRenderPass render_pass;
  VkResult result = vkCreateRenderPass(device, &render_pass_create_info, NULL, &render_pass);
  VK_CHECK(result, "Failed to create render pass");
  return render_pass;
}

VkRenderPass create_color_depth_render_pass(VkDevice device, VkFormat color_format, VkFormat depth_format) {
  VkAttachmentDescription color_attachment = make_color_attachment(color_format);
  VkAttachmentDescription depth_attachment = make_depth_attachment(depth_format);
  VkAttachmentDescription attachment_descriptions[NUM_ATTACHMENTS] = {color_attachment, depth_attachment};

  VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  VkAttachmentReference depth_attachment_ref = {
      .attachment = 1,
      .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
  };

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSubpassDescription.html
  VkSubpassDescription subpass = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = NULL,
      .pColorAttachments = &color_attachment_ref,
      .colorAttachmentCount = 1,
      .pResolveAttachments = NULL,
      .pDepthStencilAttachment = &depth_attachment_ref,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = NULL,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
      .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
  };

  return create_render_pass(device, NUM_ATTACHMENTS, attachment_descriptions, 1, &subpass, 1, &dependency);
}

VkRenderPass create_color_render_pass(VkDevice device, VkFormat format) {
  const u32 num_attachments = 1;
  VkAttachmentDescription color_attachment = make_color_attachment(format);

  VkAttachmentDescription attachment_descriptions[num_attachments] = {color_attachment};

  VkAttachmentReference color_attachment_ref = {
      .attachment = 0,
      .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
  };

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSubpassDescription.html
  VkSubpassDescription subpass = {
      .flags = 0,
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .inputAttachmentCount = 0,
      .pInputAttachments = NULL,
      .pColorAttachments = &color_attachment_ref,
      .colorAttachmentCount = 1,
      .pResolveAttachments = NULL,
      .pDepthStencilAttachment = NULL,
      .preserveAttachmentCount = 0,
      .pPreserveAttachments = NULL,
  };

  VkSubpassDependency dependency = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
      .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT,
  };

  return create_render_pass(device, num_attachments, attachment_descriptions, 1, &subpass, 1, &dependency);
}

VkFramebuffer create_framebuffer(
    VkDevice device,
    VkRenderPass render_pass,
    u32 num_attachments,
    VkImageView *image_view_attachments,
    VkExtent2D extent
) {

  VkFramebufferCreateInfo framebuffer_create_info = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .renderPass = render_pass,
      .attachmentCount = num_attachments,
      .pAttachments = image_view_attachments,
      .width = extent.width,
      .height = extent.height,
      .layers = 1,
  };

  VkFramebuffer framebuffer;
  VkResult result = vkCreateFramebuffer(device, &framebuffer_create_info, NULL, &framebuffer);
  VK_CHECK(result, "Failed to create framebuffer");
  return framebuffer;
}

void init_framebuffers(VulkanContext *ctx) {
  for (u32 i = 0; i < ctx->swapchain_storage.image_count; i++) {

    const SwapchainStorage *storage = &ctx->swapchain_storage;
    VkImageView color_view =
        storage->use_static ? storage->as.static_storage.image_views[i] : storage->as.dynamic_storage.image_views[i];

    VkImageView views[] = {color_view, ctx->swapchain_storage.depth_buffers[i].image_view};
    ctx->framebuffers[i] =
        create_framebuffer(ctx->device, ctx->render_pass, NUM_ATTACHMENTS, views, ctx->swapchain_extent);
  }
}

void recreate_swapchain(VulkanContext *context) {
  vkDeviceWaitIdle(context->device);
  destroy_swapchain(context);

  VkSurfaceCapabilitiesKHR surface_capabilities = get_surface_capabilities(context->physical_device, context->surface);
  context->swapchain_extent = get_swapchain_extent(0, 0, surface_capabilities);

  // TODO Passing every single parameter here by a context deref is ugly lol
  //      Also, does this just take too many parameters?
  context->swapchain = create_swapchain(
      context->device, context->physical_device, context->surface, context->queue_family_indices,
      context->surface_format, surface_capabilities, context->swapchain_extent
  );

  VkFormat format = find_depth_format(context->physical_device);

  // Why am I not assigning the result of this?
  // Why am I doing it after creating the swapchain?
  create_swapchain_storage(context, format);

  init_framebuffers(context);
}

void create_frame_sync_objects(VkDevice device, FrameSyncObjects *frame_sync_objects) {
  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkSemaphoreCreateInfo.html
  VkSemaphoreCreateInfo semaphore_create_info = {
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
  };

  VkFenceCreateInfo fence_create_info = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .pNext = NULL,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };

  VkResult result;
  for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    result = vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_sync_objects[i].image_available_semaphore);
    VK_CHECK_VARIADIC(result, "Failed to create image_available_semaphore %d", i);

    // TODO should these be per frame in flight or per swapchain image?
    result = vkCreateSemaphore(device, &semaphore_create_info, NULL, &frame_sync_objects[i].render_finished_semaphore);
    VK_CHECK_VARIADIC(result, "Failed to create render_finished_semaphore %d", i);

    result = vkCreateFence(device, &fence_create_info, NULL, &frame_sync_objects[i].in_flight_fence);
    VK_CHECK_VARIADIC(result, "Failed to create in_flight_fence %d", i);
  }
}

// Command pool creation is cheap, and recording commands from multiple threads requires a pool per thread.
VkCommandPool create_command_pool(VkDevice device, u32 queue_family_index, bool transient) {
  // Command buffers allocated from pools with reset_flag may be reset using vkResetCommandBuffer -
  // and may not be reset with vkResetCommandBuffer if not
  // When calling vkBeginCommandBuffer without this flag, the command buffer passed to it must
  // be in the initial state. This flag being set allows vkBeginCommandBuffer to do the resetting.
  const VkCommandPoolCreateFlags reset_flag = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

  // transient_flag set allows for the driver to make memory optimizations
  const VkCommandPoolCreateFlags transient_flag = (transient ? VK_COMMAND_POOL_CREATE_TRANSIENT_BIT : 0);

  VkCommandPoolCreateInfo command_pool_create_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = (reset_flag | transient_flag),
      .queueFamilyIndex = queue_family_index,
  };

  VkCommandPool command_pool;
  VkResult result = vkCreateCommandPool(device, &command_pool_create_info, NULL, &command_pool);
  VK_CHECK(result, "Failed to create command pool");
  return command_pool;
}

void allocate_command_buffers(
    VkDevice device, VkCommandPool command_pool, u32 command_buffer_count, VkCommandBuffer *command_buffers
) {
  VkCommandBufferAllocateInfo allocate_info{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = 0,
      .commandPool = command_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = command_buffer_count,
  };

  VkResult result = vkAllocateCommandBuffers(device, &allocate_info, command_buffers);
  VK_CHECK(result, "Failed to allocate command buffers");
}

VkPipelineCache create_pipeline_cache(VkDevice device) {
  // TODO add serialization to disk
  VkPipelineCache pipeline_cache;
  VkPipelineCacheCreateInfo pipeline_cache_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .initialDataSize = 0,
      .pInitialData = NULL,
  };

  VkResult result = vkCreatePipelineCache(device, &pipeline_cache_create_info, NULL, &pipeline_cache);
  VK_CHECK(result, "Failed to create pipeline cache");
  return pipeline_cache;
}

VkVertexInputBindingDescription create_instanced_vertex_binding_description(u32 binding, u32 stride) {
  VkVertexInputBindingDescription description = {
      .binding = binding,
      .stride = stride,
      .inputRate = VK_VERTEX_INPUT_RATE_INSTANCE,
  };
  return description;
}

VkVertexInputBindingDescription create_vertex_binding_description(u32 binding, u32 stride) {
  VkVertexInputBindingDescription description = {
      .binding = binding,
      .stride = stride,
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  return description;
}

// https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#vkCreateDescriptorPool
static VkDescriptorPool create_descriptor_pool(VkDevice device) {
  // https://docs.vulkan.org/spec/latest/chapters/descriptorsets.html#VkDescriptorPoolCreateFlagBits
  VkDescriptorPoolCreateFlags flags = 0;

  VkDescriptorPoolSize pool_sizes[] = {
      {
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = POOL_SIZE_DESCRIPTOR_COUNT,
      },
      {
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = POOL_SIZE_DESCRIPTOR_COUNT,
      },
  };

  VkDescriptorPoolCreateInfo descriptor_pool_ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .pNext = NULL,
      .flags = flags,
      .maxSets = POOL_SIZE_DESCRIPTOR_COUNT,
      .poolSizeCount = sizeof(pool_sizes) / sizeof(pool_sizes[0]),
      .pPoolSizes = pool_sizes,
  };

  VkDescriptorPool descriptor_pool;
  VkResult result = vkCreateDescriptorPool(device, &descriptor_pool_ci, NULL, &descriptor_pool);
  VK_CHECK(result, "Failed to create descriptor pool");
  return descriptor_pool;
}

// May want this to be create_instance_graphics(), or something. Or need to have
// VulkanWindowInfo be something that can equally well describe compute pipelines.
VulkanContext create_vulkan_context(const char *title, VulkanWindowInfo window_info) {
  VulkanContext ctx = {};

  ctx.instance = create_instance(title, window_info);
#ifdef TUKE_DISABLE_VULKAN_VALIDATION
  context.debug_messenger = VK_NULL_HANDLE;
#else
  ctx.debug_messenger = create_debug_messenger(ctx.instance);
#endif
  ctx.surface = window_info.create_surface(ctx.instance, window_info.window);

  // pick_physical_device() initializes context.queue_family_indices through a side effect.
  ctx.physical_device = pick_physical_device(ctx.instance, ctx.surface, &ctx.queue_family_indices);
  vkGetPhysicalDeviceProperties(ctx.physical_device, &ctx.physical_device_properties);

  const QueueFamilyIndices indices = ctx.queue_family_indices;
  ctx.present_queue_index_is_different_than_graphics = (indices.graphics_family != indices.present_family);
  ctx.compute_queue_index_is_different_than_graphics = (indices.graphics_family != indices.compute_family);

  ctx.device = create_device(ctx.queue_family_indices, ctx.physical_device);
  ctx.surface_format = get_surface_format(ctx.physical_device, ctx.surface);
  ctx.descriptor_pool = create_descriptor_pool(ctx.device);

  create_frame_sync_objects(ctx.device, ctx.frame_sync_objects);
  ctx.current_frame_index = 0;

  // boolean argument is is_transient command pool
  ctx.transient_cmd_pool = create_command_pool(ctx.device, indices.graphics_family, true);
  ctx.graphics_cmd_pool = create_command_pool(ctx.device, indices.graphics_family, false);

  if (ctx.compute_queue_index_is_different_than_graphics) {
    ctx.compute_cmd_pool = create_command_pool(ctx.device, indices.compute_family, false);
  } else {
    ctx.compute_cmd_pool = ctx.graphics_cmd_pool;
  }

  if (ctx.present_queue_index_is_different_than_graphics) {
    ctx.present_cmd_pool = create_command_pool(ctx.device, indices.present_family, false);
  } else {
    ctx.present_cmd_pool = ctx.graphics_cmd_pool;
  }

  // swapchain creation needs to happen after the transient command pool is
  // allocated because depth buffer creation requires an image transition
  VkSurfaceCapabilitiesKHR surface_capabilities = get_surface_capabilities(ctx.physical_device, ctx.surface);
  ctx.swapchain_extent = get_swapchain_extent(window_info.width, window_info.height, surface_capabilities);
  ctx.swapchain = create_swapchain(
      ctx.device, ctx.physical_device, ctx.surface, ctx.queue_family_indices, ctx.surface_format, surface_capabilities,
      ctx.swapchain_extent
  );

  vkGetDeviceQueue(ctx.device, indices.graphics_family, 0, &ctx.graphics_queue);
  vkGetDeviceQueue(ctx.device, indices.present_family, 0, &ctx.present_queue);
  vkGetDeviceQueue(ctx.device, indices.compute_family, 0, &ctx.compute_queue);
  VkFormat depth_format = find_depth_format(ctx.physical_device);
  ctx.swapchain_storage = create_swapchain_storage(&ctx, depth_format);

  // TODO sync this with init_framebuffers()
  // don't need a depth attachment here
  ctx.render_pass = create_color_depth_render_pass(ctx.device, ctx.surface_format.format, depth_format);
  init_framebuffers(&ctx);

  // Regular command buffers need to be created after the swapchain, which depends on the transient command buffers.
  allocate_command_buffers(
      ctx.device, ctx.graphics_cmd_pool, ctx.swapchain_storage.image_count, ctx.graphics_cmd_buffers
  );

  allocate_command_buffers(
      ctx.device, ctx.compute_cmd_pool, ctx.swapchain_storage.image_count, ctx.compute_cmd_buffers
  );

  ctx.pipeline_cache = create_pipeline_cache(ctx.device);

  VkResult result = vkWaitForFences(
      ctx.device, 1, &ctx.frame_sync_objects[ctx.current_frame_index].in_flight_fence, VK_TRUE, UINT64_MAX
  );
  VK_CHECK(result, "Failed to wait for fences");

  return ctx;
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

VulkanBuffer create_buffer_explicit(
    const VulkanContext *context, VkBufferUsageFlags usage, VkDeviceSize size, VkMemoryPropertyFlags properties
) {
  assert(size > 0);
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
  VkResult result = vkCreateBuffer(context->device, &buffer_create_info, NULL, &buffer);
  VK_CHECK(result, "Failed to create buffer");
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

  result = vkAllocateMemory(context->device, &memory_allocate_info, NULL, &vulkan_buffer.memory);
  VK_CHECK(result, "Failed to allocate buffer memory");

  VkPhysicalDeviceMemoryProperties memory_properties;
  vkGetPhysicalDeviceMemoryProperties(context->physical_device, &memory_properties);
  vulkan_buffer.memory_property_flags =
      memory_properties.memoryTypes[memory_allocate_info.memoryTypeIndex].propertyFlags;

  result = vkBindBufferMemory(context->device, buffer, vulkan_buffer.memory, 0);
  VK_CHECK(result, "Failed to bind buffer memory");

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
    return create_buffer_explicit(
        context, readonly_storage_buffer_usage, size, readonly_storage_buffer_memory_properties
    );
  }

  case BUFFER_TYPE_READ_WRITE_STORAGE: {
    VkBufferUsageFlags read_write_storage_buffer_usage =
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VkMemoryPropertyFlags read_write_storage_buffer_memory_properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    return create_buffer_explicit(
        context, read_write_storage_buffer_usage, size, read_write_storage_buffer_memory_properties
    );
  }

  default:
    assert(false && "create_buffer: got an invalid buffer_type");
  }
}

void write_to_vulkan_buffer(
    const VulkanContext *ctx, const void *src_data, VkDeviceSize size, VkDeviceSize offset, VulkanBuffer vulkan_buffer
) {
  if (!(vulkan_buffer.memory_property_flags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)) {
    fprintf(
        stderr, "write_to_vulkan_buffer: attempting to write to host "
                "invisible memory\n"
    );
    exit(1);
  }

  VkDeviceSize aligned_offset = offset;
  VkDeviceSize aligned_size = size;
  VkDeviceSize atom_size = ctx->physical_device_properties.limits.nonCoherentAtomSize;
  bool not_coherent = !(vulkan_buffer.memory_property_flags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
  if (not_coherent) {
    aligned_offset = offset & ~(atom_size - 1);
    aligned_size = ((offset + size + atom_size - 1) & ~(atom_size - 1)) - aligned_offset;
  }

  void *dest_data;
  VkResult result = vkMapMemory(ctx->device, vulkan_buffer.memory, aligned_offset, aligned_size, 0, &dest_data);
  VK_CHECK(result, "Failed to map memory");
  memcpy((char *)dest_data + (offset - aligned_offset), src_data, size);

  if (not_coherent) {
    VkMappedMemoryRange range;
    range.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
    range.pNext = NULL;
    range.memory = vulkan_buffer.memory;
    range.offset = aligned_offset;
    range.size = aligned_size;

    result = vkFlushMappedMemoryRanges(ctx->device, 1, &range);
    VK_CHECK(result, "Failed to flush mapped memory ranges");
  }

  vkUnmapMemory(ctx->device, vulkan_buffer.memory);
}

void destroy_vulkan_buffer(const VulkanContext *ctx, VulkanBuffer buffer) {
  vkDestroyBuffer(ctx->device, buffer.buffer, NULL);
  vkFreeMemory(ctx->device, buffer.memory, NULL);
}

VkCommandBuffer begin_single_use_command_buffer(const VulkanContext *context) {
  assert(context);
  assert(context->device != VK_NULL_HANDLE);
  assert(context->transient_cmd_pool != VK_NULL_HANDLE);

  VkCommandBufferAllocateInfo allocate_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .pNext = 0,
      .commandPool = context->transient_cmd_pool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = 1,
  };

  VkCommandBuffer command_buffer;
  VkResult result = vkAllocateCommandBuffers(context->device, &allocate_info, &command_buffer);
  VK_CHECK(result, "Failed to allocate command buffer");

  VkCommandBufferBeginInfo begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = 0,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = NULL,
  };

  result = vkBeginCommandBuffer(command_buffer, &begin_info);
  VK_CHECK(result, "Failed to begin command buffer");

  return command_buffer;
}

void end_single_use_command_buffer(const VulkanContext *context, VkCommandBuffer command_buffer) {
  VkResult result = vkEndCommandBuffer(command_buffer);
  VK_CHECK(result, "Failed to end command buffer");

  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = 0,
      .waitSemaphoreCount = 0,
      .pWaitSemaphores = 0,
      .pWaitDstStageMask = 0,
      .commandBufferCount = 1,
      .pCommandBuffers = &command_buffer,
      .signalSemaphoreCount = 0,
      .pSignalSemaphores = 0,
  };

  result = vkQueueSubmit(context->graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
  VK_CHECK(result, "Failed to submit queue");
  // TODO Why wait idle here?
  result = vkQueueWaitIdle(context->graphics_queue);
  VK_CHECK(result, "Failed to wait for queue idle");

  vkFreeCommandBuffers(context->device, context->transient_cmd_pool, 1, &command_buffer);
}

VkShaderModule create_shader_module(VkDevice device, const u32 *code, u32 code_size) {
  VkShaderModuleCreateInfo shader_module_ci = {
      .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .codeSize = code_size,
      .pCode = code,
  };

  VkShaderModule module;
  VkResult result = vkCreateShaderModule(device, &shader_module_ci, NULL, &module);
  VK_CHECK(result, "Failed to create shader module");
  return module;
}

ViewportState create_viewport_state_offset(VkExtent2D swapchain_extent, VkOffset2D offset) {
  ViewportState viewport_state = {
      .viewport.x = (f32)offset.x,
      .viewport.y = (f32)offset.y,
      .viewport.width = (f32)swapchain_extent.width,
      .viewport.height = (f32)swapchain_extent.height,
      .viewport.minDepth = 0.0f,
      .viewport.maxDepth = 1.0f,
      .scissor.extent = swapchain_extent,
      .scissor.offset.x = offset.x,
      .scissor.offset.y = offset.y,
  };

  return viewport_state;
}

// TODO this is ugly and I want to get rid of it
ViewportState create_viewport_state_xy(VkExtent2D swapchain_extent, u32 x, u32 y) {
  VkOffset2D offset = {.x = (i32)x, .y = (i32)y};
  return create_viewport_state_offset(swapchain_extent, offset);
}

VkPipelineColorBlendAttachmentState create_color_blend_attachment_state(BlendMode blend_mode) {
  VkBool32 blending_enabled = (blend_mode == BLEND_MODE_ALPHA) ? VK_TRUE : VK_FALSE;

  VkPipelineColorBlendAttachmentState state = {
      .blendEnable = blending_enabled,
      .colorWriteMask =
          VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      .colorBlendOp = VK_BLEND_OP_ADD,
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      .alphaBlendOp = VK_BLEND_OP_ADD,
  };

  switch (blend_mode) {
  case BLEND_MODE_ALPHA:
    state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    break;
  case BLEND_MODE_OPAQUE:
    state.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
    state.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
    break;
  default:
    assert(false && "create_blend_mode_state got invalid blend_mode");
  }

  return state;
}

// TODO Need flexibility for dynamic rendering vs render passes. More architecture to do.
//      - Pipeline config takes a render pass
VkPipeline create_graphics_pipeline(VkDevice device, const PipelineConfig *config, VkPipelineCache pipeline_cache) {
  assert(config->stage_count <= MAX_SHADER_STAGE_COUNT);
  assert(
      config->vertex_input_state_create_info && "create_graphics_pipeline: vertex_input_state_create_info is NULL. Did "
                                                "you mean to fill out this structure manually?"
  );

  VkPipelineShaderStageCreateInfo vert_stage_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = config->vertex_shader,
      .pName = "main",
      .pSpecializationInfo = NULL,
  };

  VkPipelineShaderStageCreateInfo frag_stage_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = config->fragment_shader,
      .pName = "main",
      .pSpecializationInfo = NULL,
  };

  const VkPipelineShaderStageCreateInfo stages[] = {vert_stage_ci, frag_stage_ci};

  // https://registry.khronos.org/vulkan/specs/latest/man/html/VkPipelineInputAssemblyStateCreateInfo.html
  VkPipelineInputAssemblyStateCreateInfo input_assembly_state = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .topology = config->topology,
      .primitiveRestartEnable = config->primitive_restart_enabled,
  };

  VkOffset2D offset = {.x = 0, .y = 0};
  ViewportState viewport_state = create_viewport_state_offset(config->swapchain_extent, offset);

  VkPipelineViewportStateCreateInfo viewport_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .viewportCount = 1,
      .pViewports = &viewport_state.viewport,
      .scissorCount = 1,
      .pScissors = &viewport_state.scissor,
  };

  // TODO Will need some updates here when I want to do shadow mapping
  //      Set depthClampEnable = VK_TRUE for shadow support
  //      Understand how depthBias* is used for shadow mapping
  VkPipelineRasterizationStateCreateInfo rasterization_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = config->polygon_mode,
      .cullMode = config->cull_mode,
      .frontFace = config->front_face,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,
      .depthBiasClamp = 0.0f,
      .depthBiasSlopeFactor = 0.0f,
      .lineWidth = 1.0f,
  };

  VkPipelineMultisampleStateCreateInfo multisample_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .rasterizationSamples = config->sample_count_flag,
      // TODO if sample count flag is not VK_SAMPLE_COUNT_1_BIT, make this true?
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,
      .pSampleMask = NULL,
      // TODO understand alpha to coverage
      .alphaToCoverageEnable = VK_FALSE,
      .alphaToOneEnable = VK_FALSE,
  };

  VkStencilOpState empty_stencil_op_state;
  memset(&empty_stencil_op_state, 0, sizeof(empty_stencil_op_state));
  VkPipelineDepthStencilStateCreateInfo depth_stencil_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .depthTestEnable = VK_TRUE,
      .depthWriteEnable = VK_TRUE,
      .depthCompareOp = VK_COMPARE_OP_LESS,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = empty_stencil_op_state,
      .back = empty_stencil_op_state,
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };

  const u32 num_color_blend_attachments = 1;
  VkPipelineColorBlendAttachmentState color_blend_state = create_color_blend_attachment_state(config->blend_mode);
  VkPipelineColorBlendStateCreateInfo color_blend_state_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,
      .attachmentCount = num_color_blend_attachments,
      .pAttachments = &color_blend_state,
  };

  f32 blend_constants[4] = {0.0f, 0.0f, 0.0f, 0.0f};
  memcpy(color_blend_state_ci.blendConstants, blend_constants, sizeof(blend_constants));

  // anticipating use of vkCmdSetViewport(), vkCmdSetScissor()
  const u32 num_dynamic_states = 2;
  VkDynamicState dynamic_states[num_dynamic_states] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .dynamicStateCount = num_dynamic_states,
      .pDynamicStates = dynamic_states,
  };

  // https://docs.vulkan.org/refpages/latest/refpages/source/VkGraphicsPipelineCreateInfo.html
  VkGraphicsPipelineCreateInfo pipeline_ci = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = 0,
      .flags = 0,
      .stageCount = config->stage_count,
      .pStages = stages,
      .pVertexInputState = config->vertex_input_state_create_info,
      .pInputAssemblyState = &input_assembly_state,
      .pTessellationState = NULL, // TOOD?
      .pViewportState = &viewport_state_ci,
      .pRasterizationState = &rasterization_state_ci,
      .pMultisampleState = &multisample_state_ci,
      .pDepthStencilState = &depth_stencil_state_ci,
      .pColorBlendState = &color_blend_state_ci,
      .pDynamicState = &dynamic_state_create_info,
      .layout = config->pipeline_layout,
      .renderPass = config->render_pass,
      .subpass = 0,
      .basePipelineHandle = VK_NULL_HANDLE,
      .basePipelineIndex = -1,

  };

  VkPipeline graphics_pipeline;
  VkResult result = vkCreateGraphicsPipelines(device, pipeline_cache, 1, &pipeline_ci, NULL, &graphics_pipeline);
  VK_CHECK(result, "Failed to create graphics pipeline");
  return graphics_pipeline;
}

VkPipeline create_default_graphics_pipeline(
    const VulkanContext *ctx,
    VkRenderPass render_pass,
    VkShaderModule vertex_shader,
    VkShaderModule fragment_shader,
    const VkPipelineVertexInputStateCreateInfo *vertex_input_state,
    VkPipelineLayout pipeline_layout
) {

  PipelineConfig config = {
      .vertex_shader = vertex_shader,
      .fragment_shader = fragment_shader,
      .stage_count = 2,
      .vertex_input_state_create_info = vertex_input_state,
      .render_pass = render_pass,
      .pipeline_layout = pipeline_layout,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitive_restart_enabled = VK_FALSE,
      .polygon_mode = VK_POLYGON_MODE_FILL,
      .cull_mode = VK_CULL_MODE_NONE,
      .front_face = VK_FRONT_FACE_COUNTER_CLOCKWISE,
      .sample_count_flag = VK_SAMPLE_COUNT_1_BIT,
      .blend_mode = BLEND_MODE_ALPHA,
  };

  return create_graphics_pipeline(ctx->device, &config, ctx->pipeline_cache);
}

void begin_frame(VulkanContext *ctx) {
  u32 fence_count = 1;

  // Wait for fence. CPU waits for GPU to signal, indicating work is done.
  u64 timeout = UINT64_MAX;    // nanoseconds
  VkBool32 wait_all = VK_TRUE; // When true, we wait for ALL fenceCount fences to signal. False, just one.
  const VkFence *fence = &ctx->frame_sync_objects[ctx->current_frame_index].in_flight_fence;

  // https://docs.vulkan.org/refpages/latest/refpages/source/vkWaitForFences.html
  VkResult result = vkWaitForFences(ctx->device, fence_count, fence, wait_all, timeout);
  VK_CHECK(result, "Failed to wait for fence");

  // Reset fence to unsignalled state, meaning we must wait on it again for the next frame.
  vkResetFences(ctx->device, fence_count, fence);

  // https://docs.vulkan.org/refpages/latest/refpages/source/vkAcquireNextImageKHR.html
  // TODO Why do I need a semaphore here? What is a semaphore?
  VkSemaphore semaphore = ctx->frame_sync_objects[ctx->current_frame_index].image_available_semaphore;
  result = vkAcquireNextImageKHR(ctx->device, ctx->swapchain, timeout, semaphore, VK_NULL_HANDLE, &ctx->image_index);
  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    // TODO do I need to redo this until recreation succeeds?
    recreate_swapchain(ctx);
  }

  if (result == VK_ERROR_DEVICE_LOST) {
    fprintf(stderr, "%s(): result = VK_ERROR_DEVICE_LOST", __func__);
    exit(1);
  }
}

VkCommandBuffer begin_command_buffer(const VulkanContext *context) {
  VkCommandBuffer command_buffer = context->graphics_cmd_buffers[context->image_index];
  assert(command_buffer != VK_NULL_HANDLE);

  VkCommandBufferBeginInfo command_buffer_begin_info = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = NULL,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      .pInheritanceInfo = NULL,
  };

  VkResult result = vkBeginCommandBuffer(command_buffer, &command_buffer_begin_info);
  VK_CHECK(result, "Failed to vkBeginCommandBuffer()");
  return command_buffer;
}

void begin_render_pass(
    const VulkanContext *context,
    VkCommandBuffer cmd,
    VkRenderPass render_pass,
    VkFramebuffer framebuffer,
    const VkClearValue *clear_value,
    u32 clear_value_count,
    ViewportState viewport_state
) {
  VkRenderPassBeginInfo render_pass_begin_info = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .pNext = NULL,
      .renderPass = render_pass,
      .framebuffer = framebuffer,
      .renderArea.offset = viewport_state.scissor.offset,
      .renderArea.extent = context->swapchain_extent,
      .clearValueCount = clear_value_count,
      .pClearValues = clear_value,
  };

  vkCmdBeginRenderPass(cmd, &render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
  vkCmdSetViewport(cmd, 0, 1, &viewport_state.viewport);
  vkCmdSetScissor(cmd, 0, 1, &viewport_state.scissor);
}

// Assuming a model where a frame's drawing commands can all be issued through a single command buffer.
void end_frame(VulkanContext *ctx, VkCommandBuffer cmd) {
  VK_CHECK(vkEndCommandBuffer(cmd), "Failed to end command buffer");
  submit_and_present(ctx, cmd);
  update_frame_index(ctx);
}

// TODO May want to pass the raw vulkan primitives here instead of the context
void submit_and_present(const VulkanContext *ctx, VkCommandBuffer cmd) {
  const FrameSyncObjects *fso = &ctx->frame_sync_objects[ctx->current_frame_index];

  VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  VkSubmitInfo submit_info = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .pNext = NULL,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &fso->image_available_semaphore,
      .pWaitDstStageMask = &wait_stage,
      .commandBufferCount = 1,
      .pCommandBuffers = &cmd,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &fso->render_finished_semaphore,
  };

  VkResult result = vkQueueSubmit(ctx->graphics_queue, 1, &submit_info, fso->in_flight_fence);
  VK_CHECK(result, "Failed to submit queue");

  VkPresentInfoKHR present_info = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .pNext = NULL,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &fso->render_finished_semaphore,
      .swapchainCount = 1,
      .pSwapchains = &ctx->swapchain,
      .pImageIndices = &ctx->image_index,
      .pResults = NULL,
  };

  // TODO handle swapchain recreation here if need be
  result = vkQueuePresentKHR(ctx->graphics_queue, &present_info);
  VK_CHECK(result, "Failed to present queue");
}

VkPipelineLayout
create_pipeline_layout(VkDevice device, const VkDescriptorSetLayout *set_layouts, u32 set_layout_count) {
  VkPipelineLayoutCreateInfo pipeline_layout_ci = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .setLayoutCount = set_layout_count,
      .pSetLayouts = set_layouts,
      .pushConstantRangeCount = 0, // TODO push constants
      .pPushConstantRanges = NULL,
  };

  VkPipelineLayout pipeline_layout;
  VkResult result = vkCreatePipelineLayout(device, &pipeline_layout_ci, NULL, &pipeline_layout);
  VK_CHECK(result, "Failed to create pipeline layout");
  return pipeline_layout;
}

static void
stage_data(const VulkanContext *ctx, StagingArena *arena, const void *data, u32 size, VkBuffer dst, u32 dst_offset) {
  assert(arena->offset + size <= arena->total_size);
  assert(arena->num_copy_regions < MAX_COPY_REGIONS);

  VkBufferCopy copy_region = {
      .srcOffset = arena->offset,
      .dstOffset = dst_offset,
      .size = size,
  };

  arena->copy_regions[arena->num_copy_regions] = copy_region;
  arena->dst_buffers[arena->num_copy_regions++] = dst;

  write_to_vulkan_buffer(ctx, data, size, arena->offset, arena->buffer);
  arena->offset += size;
}

static void flush_staging_arena(const VulkanContext *context, StagingArena *arena) {
  if (arena->num_copy_regions == 0) {
    return;
  }

  VkCommandBuffer cmd = begin_single_use_command_buffer(context);
  VkBuffer current_destination = arena->dst_buffers[0];
  u32 start = 0;

  for (u32 i = 1; i < arena->num_copy_regions; i++) {
    if (arena->dst_buffers[i] == current_destination) {
      continue;
    }

    vkCmdCopyBuffer(cmd, arena->buffer.buffer, current_destination, i - start, arena->copy_regions + start);
    current_destination = arena->dst_buffers[i];
    start = i;
  }

  vkCmdCopyBuffer(
      cmd, arena->buffer.buffer, current_destination, arena->num_copy_regions - start, arena->copy_regions + start
  );

  end_single_use_command_buffer(context, cmd);
}

UniformBuffer create_uniform_buffer(const VulkanContext *ctx, u32 size) {
  UniformBuffer ub = {
      .vulkan_buffer = create_buffer(ctx, BUFFER_TYPE_UNIFORM, size),
      .size = size,
  };

  VkResult result = vkMapMemory(ctx->device, ub.vulkan_buffer.memory, 0, size, 0, (void **)&ub.mapped);
  VK_CHECK(result, "Failed to map uniform buffer memory");
  return ub;
}

void destroy_uniform_buffer(const VulkanContext *context, UniformBuffer *uniform_buffer) {
  destroy_vulkan_buffer(context, uniform_buffer->vulkan_buffer);
}

void write_to_uniform_buffer(UniformBuffer *uniform_buffer, const void *data, UniformWrite uniform_write) {
  memcpy(uniform_buffer->mapped + uniform_write.offset, data, uniform_write.size);
}

ReadOnlyStorageBuffer create_readonly_storage_buffer(const VulkanContext *ctx, u32 buffer_size) {
  ReadOnlyStorageBuffer readonly_storage_buffer = {
      .vulkan_buffer = create_buffer(ctx, BUFFER_TYPE_READONLY_STORAGE, buffer_size),
      .size = buffer_size,
  };

  VkResult result = vkMapMemory(
      ctx->device, readonly_storage_buffer.vulkan_buffer.memory, 0, buffer_size, 0,
      (void **)&readonly_storage_buffer.mapped
  );
  VK_CHECK(result, "Failed to map storage buffer memory");

  return readonly_storage_buffer;
}

// Can make textures in a GPU native format KTX: https://developer.imaginationtech.com/solutions/pvrtextool/
VulkanTexture create_vulkan_texture(
    VulkanContext *ctx,
    u32 width,
    u32 height,
    u32 n_channels,
    u8 *data,
    VulkanBuffer staging_buffer,
    void *ptr_to_mapped_memory
) {
  VulkanTexture texture = {
      .width = width,
      .height = height,
  };

  VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  VkFormat format = VK_FORMAT_R8G8B8A8_SRGB;
  texture.image = create_default_image(ctx, width, height, usage, format);

  // Allocating device memory must come after image creation.
  texture.device_memory = allocate_and_bind_image_memory(ctx, texture.image);

  // Map memory
  assert(data);
  assert(ptr_to_mapped_memory);
  u32 size = n_channels * width * height;
  memcpy(ptr_to_mapped_memory, data, size);

  // First transition
  VkCommandBuffer cmd = begin_single_use_command_buffer(ctx);
  VkImageLayout old_layout = VK_IMAGE_LAYOUT_UNDEFINED;
  VkImageLayout new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  transition_image_layout(cmd, texture.image, old_layout, new_layout);

  // Copy buffer to image
  VkBufferImageCopy buffer_image_copy = {
      .bufferOffset = 0,
      .bufferRowLength = 0, // 0 indicates tightly packed - could we also used handle width/height?
      .bufferImageHeight = 0,
      .imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .imageSubresource.mipLevel = 0,
      .imageSubresource.baseArrayLayer = 0,
      .imageSubresource.layerCount = 1,
      .imageOffset.x = 0,
      .imageOffset.y = 0,
      .imageOffset.z = 0,
      .imageExtent.width = width,
      .imageExtent.height = height,
      .imageExtent.depth = 1,
  };

  VkImageLayout image_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  u32 region_count = 1;
  vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, texture.image, image_layout, region_count, &buffer_image_copy);

  // Second transition
  old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  transition_image_layout(cmd, texture.image, old_layout, new_layout);
  end_single_use_command_buffer(ctx, cmd);

  VkImageAspectFlags aspect_flags = VK_IMAGE_ASPECT_COLOR_BIT;
  format = VK_FORMAT_R8G8B8A8_SRGB; // TODO should be determined from image
  texture.image_view = create_default_image_view(ctx, texture.image, format, aspect_flags);

  return texture;
}

void destroy_vulkan_texture(VkDevice device, VulkanTexture *vulkan_texture) {
  vkDestroyImage(device, vulkan_texture->image, NULL);
  vkDestroyImageView(device, vulkan_texture->image_view, NULL);
  vkFreeMemory(device, vulkan_texture->device_memory, NULL);
}

// https://docs.vulkan.org/spec/latest/chapters/textures.html
// TODO give this any modularity at all
VkSampler create_sampler(VkDevice device) {
  VkSamplerCreateInfo sampler_create_info = {
      .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .magFilter = VK_FILTER_LINEAR,
      .minFilter = VK_FILTER_LINEAR,
      .mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR,
      .addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      .mipLodBias = 0.0f,
      .anisotropyEnable = VK_FALSE,
      .maxAnisotropy = 0.0f,
      .compareEnable = VK_FALSE,
      .compareOp = VK_COMPARE_OP_LESS,
      .minLod = 1.0f,
      .maxLod = 1.0f,
      .borderColor = VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK,
      .unnormalizedCoordinates = VK_FALSE,
  };

  VkSampler sampler;
  VkResult result = vkCreateSampler(device, &sampler_create_info, NULL, &sampler);
  VK_CHECK(result, "Failed to create sampler");
  return sampler;
}

void reset_descriptor_set_layouts(VulkanContext *ctx) {
  if (!ctx->descriptor_set_layouts) {
    return;
  }
  for (u32 i = 0; i < ctx->num_descriptor_set_layouts; i++) {
    vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptor_set_layouts[i], NULL);
    ctx->descriptor_set_layouts[i] = VK_NULL_HANDLE;
  }
}

void set_descriptor_set_layouts(VulkanContext *ctx, VkDescriptorSetLayout *layouts, u32 num_layouts) {
  ctx->descriptor_set_layouts = layouts;
  ctx->num_descriptor_set_layouts = num_layouts;
  memset(ctx->descriptor_set_layouts, 0, num_layouts * sizeof(VkDescriptorSetLayout));
}

VkDescriptorSetLayout
create_descriptor_set_layout(VkDevice device, const VkDescriptorSetLayoutBinding *bindings, u32 binding_count) {
  VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .pNext = NULL,
      .flags = 0,
      .bindingCount = binding_count,
      .pBindings = bindings,
  };

  VkDescriptorSetLayout descriptor_set_layout;
  VkResult result = vkCreateDescriptorSetLayout(device, &descriptor_set_layout_ci, NULL, &descriptor_set_layout);
  VK_CHECK(result, "Failed to create descriptor set layout");
  return descriptor_set_layout;
}

VkDescriptorSet
create_descriptor_set(VkDevice device, const VkDescriptorSetLayout *set_layouts, VkDescriptorPool pool) {
  VkDescriptorSetAllocateInfo descriptor_set_allocate_info = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .pNext = NULL,
      .descriptorPool = pool,
      .descriptorSetCount = 1,
      .pSetLayouts = set_layouts,
  };

  VkDescriptorSet descriptor_set;
  VkResult result = vkAllocateDescriptorSets(device, &descriptor_set_allocate_info, &descriptor_set);
  VK_CHECK(result, "Failed to allocate descriptor set");
  return descriptor_set;
}

void destroy_vulkan_material(VkDevice device, VulkanMaterial *mat) {
  vkDestroyPipelineLayout(device, mat->pipeline_layout, NULL);
  vkDestroyPipeline(device, mat->pipeline, NULL);
}

void render_mesh_material(VkCommandBuffer cmd, const VulkanMesh *mesh, const VulkanMaterial *mat) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline);

  // Still not really sure when any of these first_* are meaningful.
  u32 first_binding = 0;
  if (mesh->num_vertex_buffers > 0) {
    vkCmdBindVertexBuffers(
        cmd, first_binding, mesh->num_vertex_buffers, mesh->vertex_buffers, mesh->vertex_buffer_offsets
    );
  }

  if (mat->num_descriptor_sets > 0) {
    u32 first_set = 0;
    u32 dynamic_offset_count = 0;
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, mat->pipeline_layout, first_set, mat->num_descriptor_sets,
        mat->descriptor_sets, dynamic_offset_count, NULL
    );
  }

  u32 first_instance = 0;
  i32 vertex_offset = 0;
  if (mesh->num_indices > 0) {
    vkCmdBindIndexBuffer(cmd, mesh->index_buffer, mesh->index_buffer_offset, VK_INDEX_TYPE_UINT16);
    u32 first_index = 0;
    vkCmdDrawIndexed(cmd, mesh->num_indices, mesh->instance_count, first_index, vertex_offset, first_instance);
  } else {
    u32 first_vertex = 0;
    vkCmdDraw(cmd, mesh->num_vertices, mesh->instance_count, first_vertex, first_instance);
  }
}

void render_mesh(VkCommandBuffer cmd, RenderCall *call) {
  vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, call->pipeline);

  u32 first_binding = 0;
  if (call->num_vertex_buffers > 0) {
    vkCmdBindVertexBuffers(
        cmd, first_binding, call->num_vertex_buffers, call->vertex_buffers, call->vertex_buffer_offsets
    );
  }

  if (call->num_descriptor_sets > 0) {
    u32 first_set = 0;
    u32 dynamic_offset_count = 0;
    vkCmdBindDescriptorSets(
        cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, call->pipeline_layout, first_set, call->num_descriptor_sets,
        call->descriptor_sets, dynamic_offset_count, NULL
    );
  }

  u32 first_instance = 0;
  i32 vertex_offset = 0;
  if (call->is_indexed) {
    vkCmdBindIndexBuffer(cmd, call->index_buffer, call->index_buffer_offset, VK_INDEX_TYPE_UINT16);
    u32 first_index = 0;
    vkCmdDrawIndexed(cmd, call->num_indices, call->instance_count, first_index, vertex_offset, first_instance);
  } else {
    u32 first_vertex = 0;
    vkCmdDraw(cmd, call->num_vertices, call->instance_count, first_vertex, first_instance);
  }
}

BufferUploadQueue create_buffer_upload_queue() {
  BufferUploadQueue queue = {
      .vertex_buffer_offset = 0,
      .index_buffer_offset = 0,
      .num_slices = 0,
  };
  memset(queue.slices, 0, sizeof(queue.slices));
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

BufferManager flush_buffers(VulkanContext *ctx, BufferUploadQueue *queue) {
  assert(queue->num_slices > 0);
  u64 total_size = queue->vertex_buffer_offset + queue->index_buffer_offset;

  VulkanBuffer staging_buffer = create_buffer(ctx, BUFFER_TYPE_STAGING, total_size);
  StagingArena staging_arena = {
      .buffer = staging_buffer,
      .total_size = total_size,
      .offset = 0,
      .num_copy_regions = 0,
      .copy_regions = {},
  };

  BufferManager manager = {
      .context = ctx,
      .vertex_buffer = create_buffer(ctx, BUFFER_TYPE_VERTEX, queue->vertex_buffer_offset),
      .index_buffer = create_buffer(ctx, BUFFER_TYPE_INDEX, queue->index_buffer_offset),
      .staging_arena = staging_arena,
  };

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

    stage_data(ctx, &manager.staging_arena, handle.data, handle.size, destination, handle.offset);
  }

  flush_staging_arena(ctx, &manager.staging_arena);
  return manager;
}

void destroy_buffer_manager(BufferManager *buffer_manager) {
  VulkanContext *ctx = buffer_manager->context;
  destroy_vulkan_buffer(ctx, buffer_manager->vertex_buffer);
  destroy_vulkan_buffer(ctx, buffer_manager->index_buffer);
  destroy_vulkan_buffer(ctx, buffer_manager->staging_arena.buffer);
}

UniformBufferManager create_uniform_buffer_manager() {
  UniformBufferManager uniform_buffer_manager = {
      .current_offset = 0,
  };
  return uniform_buffer_manager;
}

UniformWrite push_uniform(UniformBufferManager *uniform_buffer_manager, u32 size) {
  UniformWrite uniform_write = {
      .size = size,
      .offset = uniform_buffer_manager->current_offset,
  };
  uniform_buffer_manager->current_offset += size;
  return uniform_write;
}

CoherentStreamingBuffer create_coherent_streaming_buffer(const VulkanContext *ctx, u32 size) {
  CoherentStreamingBuffer coherent_streaming_buffer;

  coherent_streaming_buffer.vulkan_buffer = create_buffer(ctx, BUFFER_TYPE_COHERENT_STREAMING, size);

  vkMapMemory(
      ctx->device, coherent_streaming_buffer.vulkan_buffer.memory, 0, size, 0, (void **)&coherent_streaming_buffer.data
  );

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

ColorDepthFramebuffer create_color_depth_framebuffer(
    const VulkanContext *context, VkExtent2D extent, VkFormat color_format, VkFormat depth_format
) {
  ColorDepthFramebuffer fb;
  fb.render_pass = create_color_depth_render_pass(context->device, color_format, depth_format);

  // Color
  VkImageUsageFlags usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  fb.color_image = create_default_image(context, extent.width, extent.height, usage, color_format);
  fb.color_image_device_memory = allocate_and_bind_image_memory(context, fb.color_image);
  fb.color_image_view = create_default_image_view(context, fb.color_image, color_format, VK_IMAGE_ASPECT_COLOR_BIT);

  // Depth
  usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
  fb.depth_image = create_default_image(context, extent.width, extent.height, usage, depth_format);
  fb.depth_image_device_memory = allocate_and_bind_image_memory(context, fb.depth_image);
  fb.depth_image_view = create_default_image_view(context, fb.depth_image, depth_format, VK_IMAGE_ASPECT_DEPTH_BIT);

  VkImageView offscreen_image_views[] = {fb.color_image_view, fb.depth_image_view};
  fb.framebuffer = create_framebuffer(context->device, fb.render_pass, 2, offscreen_image_views, extent);

  return fb;
}

void destroy_color_depth_framebuffer(VkDevice device, ColorDepthFramebuffer *color_depth_framebuffer) {
  vkDestroyFramebuffer(device, color_depth_framebuffer->framebuffer, NULL);
  vkDestroyImageView(device, color_depth_framebuffer->color_image_view, NULL);
  vkDestroyImage(device, color_depth_framebuffer->color_image, NULL);
  vkFreeMemory(device, color_depth_framebuffer->color_image_device_memory, NULL);
  vkDestroyImageView(device, color_depth_framebuffer->depth_image_view, NULL);
  vkDestroyImage(device, color_depth_framebuffer->depth_image, NULL);
  vkFreeMemory(device, color_depth_framebuffer->depth_image_device_memory, NULL);
  vkDestroyRenderPass(device, color_depth_framebuffer->render_pass, NULL);
}
