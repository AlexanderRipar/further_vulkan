#include "och_vulkan_debug_callback.h"

#include "och_fmt.h"

VKAPI_ATTR VkBool32 VKAPI_CALL och_vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
	user_data; type;

	if(severity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		if (callback_data->messageIdNumber == 0) // Ignore Loader Message (loaderAddLayerProperties invalid layer manifest file version)
			return VK_FALSE;

	och::print("{}\n\n", callback_data->pMessage);

	return VK_FALSE;
}
