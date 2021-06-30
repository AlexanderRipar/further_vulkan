#include "och_vulkan_debug_callback.h"

#include "och_fmt.h"

VKAPI_ATTR VkBool32 VKAPI_CALL och_vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
	user_data;

	const char* severity_msg = nullptr, * type_msg = nullptr;

	switch (severity)
	{
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
		severity_msg = "verbose"; break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
		severity_msg = "info"; break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
		severity_msg = "WARNING"; break;
	case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
		severity_msg = "ERROR"; break;
	}

	switch (type)
	{
	case VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT:
		type_msg = "general"; break;
	case VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT:
		type_msg = "performance"; break;
	case VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT:
		type_msg = "validation"; break;
	}

	och::print("{}/{}: {}\n\n", severity_msg, type_msg, callback_data->pMessage);

	return VK_FALSE;
}
