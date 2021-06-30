#pragma once

#include <vulkan/vulkan.h>

VKAPI_ATTR VkBool32 VKAPI_CALL och_vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data);