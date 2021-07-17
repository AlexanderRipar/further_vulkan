#include "och_vulkan_base.h"

#include "och_heap_buffer.h"
#include "och_helpers.h"
#include "och_fmt.h"
#include "och_fio.h"

VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
	user_data; type;

	if (severity != VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT)
		if (callback_data->messageIdNumber == 0) // Ignore Loader Message (loaderAddLayerProperties invalid layer manifest file version)
			return VK_FALSE;

	och::print("{}\n\n", callback_data->pMessage);

	return VK_FALSE;
}

void vulkan_context_resize_callback(GLFWwindow* window, int width, int height)
{
	width, height;

	reinterpret_cast<och::vulkan_context*>(glfwGetWindowUserPointer(window))->m_flags.framebuffer_resized = true;
}

och::err_info och::vulkan_context::create(const char* app_name, uint32_t window_width, uint32_t window_height, uint32_t requested_general_queues, uint32_t requested_compute_queues, uint32_t requested_transfer_queues, VkImageUsageFlags swapchain_image_usage, const VkPhysicalDeviceFeatures* enabled_device_features, bool allow_compute_graphics_merge) noexcept
{
	if (requested_general_queues > queue_family_info::MAX_QUEUE_CNT || requested_compute_queues > queue_family_info::MAX_QUEUE_CNT || requested_transfer_queues > queue_family_info::MAX_QUEUE_CNT)
		return MSG_ERROR("Too many queues requested");

	// Create window
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		m_window = glfwCreateWindow(window_width, window_height, app_name, nullptr, nullptr);

		glfwSetWindowUserPointer(m_window, this);

		glfwSetFramebufferSizeCallback(m_window, vulkan_context_resize_callback);

		uint32_t glfw_extension_cnt;

		const char** glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_cnt);

		check(s_feats.add_instance_extensions(glfw_extensions, glfw_extension_cnt));
	}

	// Fill debug messenger creation info
	VkDebugUtilsMessengerCreateInfoEXT messenger_ci{};
	messenger_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messenger_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messenger_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	messenger_ci.pfnUserCallback = vulkan_debug_callback;

	// Create instance
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = app_name;
		app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(0, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		bool supports_extensions;
		check(s_feats.check_instance_support(supports_extensions));

		if (!supports_extensions)
			return MSG_ERROR("Not all required instance extensions / layers are supported");

		VkInstanceCreateInfo instance_ci{};
		instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_ci.pNext = &messenger_ci;
		instance_ci.enabledLayerCount = s_feats.inst_layer_cnt();
		instance_ci.ppEnabledLayerNames = s_feats.inst_layers();
		instance_ci.pApplicationInfo = &app_info;
		instance_ci.enabledExtensionCount = s_feats.inst_extension_cnt();
		instance_ci.ppEnabledExtensionNames = s_feats.inst_extensions();

		check(vkCreateInstance(&instance_ci, nullptr, &m_instance));
	}

	// Create debug messenger
	{
#ifdef OCH_VALIDATE

		auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

		if (create_fn)
		{
			check(create_fn(m_instance, &messenger_ci, nullptr, &m_debug_messenger));
		}
		else
			return MSG_ERROR("Could not load function vkCreateDebugUtilsMessengerEXT");

#endif // OCH_VALIDATE
	}

	// Create surface
	{
		check(glfwCreateWindowSurface(m_instance, m_window, nullptr, &m_surface));
	}

	// Surface Capabilites for use throughout the create() Function
	VkSurfaceCapabilitiesKHR surface_capabilites;

	// Select physical device and save queue family info
	{
		uint32_t avl_dev_cnt;
		check(vkEnumeratePhysicalDevices(m_instance, &avl_dev_cnt, nullptr));
		och::heap_buffer<VkPhysicalDevice> avl_devs(avl_dev_cnt);
		check(vkEnumeratePhysicalDevices(m_instance, &avl_dev_cnt, avl_devs.data()));

		for (uint32_t i = 0; i != avl_dev_cnt; ++i)
		{
			VkPhysicalDevice dev = avl_devs[i];

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(dev, &properties);

			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(dev, &features);

			if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				continue;

			// Check support for required extensions and layers

			bool supports_extensions;
			check(s_feats.check_device_support(dev, supports_extensions));

			if (!supports_extensions)
				continue;

			// Check queue families

			uint32_t queue_family_cnt;
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_cnt, nullptr);
			och::heap_buffer<VkQueueFamilyProperties> family_properties(queue_family_cnt);
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_cnt, family_properties.data());

			uint32_t general_queue_index = VK_QUEUE_FAMILY_IGNORED;
			uint32_t compute_queue_index = VK_QUEUE_FAMILY_IGNORED;
			uint32_t transfer_queue_index = VK_QUEUE_FAMILY_IGNORED;

			for (uint32_t f = 0; f != queue_family_cnt; ++f)
			{
				VkBool32 supports_present;
				check(vkGetPhysicalDeviceSurfaceSupportKHR(dev, f, m_surface, &supports_present));

				VkQueueFlags flags = family_properties[f].queueFlags;

				if (och::contains_all(flags, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) && supports_present)
					general_queue_index = f;
				else if (och::contains_all(flags, VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT) &&
					och::contains_none(flags, VK_QUEUE_GRAPHICS_BIT))
					compute_queue_index = f;
				else if (och::contains_all(flags, VK_QUEUE_TRANSFER_BIT) &&
					och::contains_none(flags, VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT))
					transfer_queue_index = f;
			}

			if (compute_queue_index == VK_QUEUE_FAMILY_IGNORED)
			{
				if (allow_compute_graphics_merge && general_queue_index != VK_QUEUE_FAMILY_IGNORED && requested_general_queues + requested_compute_queues <= och::min(queue_family_info::MAX_QUEUE_CNT, family_properties[general_queue_index].queueCount))
				{
					compute_queue_index = general_queue_index;

					m_flags.separate_compute_and_general_queue = false;
				}
				else
					continue;
			}
			else
				m_flags.separate_compute_and_general_queue = true;

			if (((general_queue_index == VK_QUEUE_FAMILY_IGNORED && requested_general_queues) || (requested_general_queues && family_properties[general_queue_index].queueCount < requested_general_queues)) ||
				((compute_queue_index == VK_QUEUE_FAMILY_IGNORED && requested_compute_queues) || (requested_compute_queues && family_properties[compute_queue_index].queueCount < requested_compute_queues)) ||
				((transfer_queue_index == VK_QUEUE_FAMILY_IGNORED && requested_transfer_queues) || (requested_transfer_queues && family_properties[transfer_queue_index].queueCount < requested_transfer_queues)))
				continue;

			// Check support for window surface

			uint32_t surface_format_cnt;
			check(vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &surface_format_cnt, nullptr));

			uint32_t present_mode_cnt;
			check(vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface, &present_mode_cnt, nullptr));

			if (!surface_format_cnt || !present_mode_cnt)
				continue;

			// Check if requested Image Usage is available for the Swapchain

			check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, m_surface, &surface_capabilites));

			if ((surface_capabilites.supportedUsageFlags & swapchain_image_usage) != swapchain_image_usage)
				continue;

			// Find a Format that supports the requested Image Usage, preferably VK_FORMAT_B8G8R8A8_SRGB

			och::heap_buffer<VkSurfaceFormatKHR> surface_formats(surface_format_cnt);
			check(vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &surface_format_cnt, surface_formats.data()));

			bool format_found = false;

			for (uint32_t j = 0; j != surface_format_cnt; ++j)
			{
				VkImageFormatProperties format_props;

				if (VK_ERROR_FORMAT_NOT_SUPPORTED == vkGetPhysicalDeviceImageFormatProperties(dev, surface_formats[j].format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, swapchain_image_usage, 0, &format_props))
					continue;

				if (!format_found || (surface_formats[j].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[j].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR))
				{
					format_found = true;

					m_swapchain_format = surface_formats[j].format;
					m_swapchain_colorspace = surface_formats[j].colorSpace;

					if (surface_formats[j].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[j].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
						break;
				}
			}

			if (!format_found)
				continue;

			// suitable Device found; Initialize member Variables

			if (requested_general_queues)
			{
				m_general_queues.family_index = general_queue_index;
				m_general_queues.cnt = requested_general_queues;
			}

			if (requested_compute_queues)
			{
				m_compute_queues.family_index = compute_queue_index;
				m_compute_queues.cnt = requested_compute_queues;
			}

			if (requested_transfer_queues)
			{
				m_transfer_queues.family_index = transfer_queue_index;
				m_transfer_queues.cnt = requested_transfer_queues;

				m_min_image_transfer_granularity = family_properties[transfer_queue_index].minImageTransferGranularity;
			}

			m_physical_device = dev;

			goto DEVICE_SELECTED;
		}

		return MSG_ERROR("Could not locate a suitable physical device");

	DEVICE_SELECTED:;
	}

	// Create logical device
	{
		float general_queue_priorities[queue_family_info::MAX_QUEUE_CNT];
		for (uint32_t i = 0; i != queue_family_info::MAX_QUEUE_CNT; ++i)
			general_queue_priorities[i] = 1.0F;

		float compute_queue_priorities[queue_family_info::MAX_QUEUE_CNT];

		for (uint32_t i = 0; i != queue_family_info::MAX_QUEUE_CNT; ++i)
			compute_queue_priorities[i] = 1.0F;

		float transfer_queue_priorities[queue_family_info::MAX_QUEUE_CNT];

		for (uint32_t i = 0; i != queue_family_info::MAX_QUEUE_CNT; ++i)
			transfer_queue_priorities[i] = 1.0F;

		VkDeviceQueueCreateInfo queue_cis[3]{};

		uint32_t ci_idx = 0;

		if (requested_general_queues)
		{
			queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_cis[ci_idx].queueFamilyIndex = m_general_queues.family_index;
			queue_cis[ci_idx].queueCount = requested_general_queues + (m_flags.separate_compute_and_general_queue ? 0 : requested_compute_queues);

			queue_cis[ci_idx].pQueuePriorities = general_queue_priorities;

			++ci_idx;
		}

		if (requested_compute_queues && m_flags.separate_compute_and_general_queue)
		{
			queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_cis[ci_idx].queueFamilyIndex = m_compute_queues.family_index;
			queue_cis[ci_idx].queueCount = requested_compute_queues;
			queue_cis[ci_idx].pQueuePriorities = compute_queue_priorities;

			++ci_idx;
		}

		if (requested_transfer_queues)
		{
			queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_cis[ci_idx].queueFamilyIndex = m_transfer_queues.family_index;
			queue_cis[ci_idx].queueCount = requested_transfer_queues;
			queue_cis[ci_idx].pQueuePriorities = transfer_queue_priorities;

			++ci_idx;
		}

		VkPhysicalDeviceFeatures default_enabled_device_features{};

		VkDeviceCreateInfo device_ci{};
		device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_ci.queueCreateInfoCount = ci_idx;
		device_ci.pQueueCreateInfos = queue_cis;
		device_ci.enabledLayerCount = s_feats.dev_layer_cnt();
		device_ci.ppEnabledLayerNames = s_feats.dev_layers();
		device_ci.enabledExtensionCount = s_feats.dev_extension_cnt();
		device_ci.ppEnabledExtensionNames = s_feats.dev_extensions();
		device_ci.pEnabledFeatures = enabled_device_features ? enabled_device_features : &default_enabled_device_features;

		check(vkCreateDevice(m_physical_device, &device_ci, nullptr, &m_device));

		for (uint32_t i = 0; i != requested_general_queues; ++i)
			vkGetDeviceQueue(m_device, m_general_queues.family_index, i, m_general_queues.queues + i);

		if (m_flags.separate_compute_and_general_queue)
			for (uint32_t i = 0; i != requested_compute_queues; ++i)
				vkGetDeviceQueue(m_device, m_compute_queues.family_index, i, m_compute_queues.queues + i);
		else
		{
			for (uint32_t i = 0; i != requested_compute_queues; ++i)
				vkGetDeviceQueue(m_device, m_compute_queues.family_index, requested_general_queues + i, m_compute_queues.queues + i);

			m_compute_queues.offset = requested_general_queues;
		}


		for (uint32_t i = 0; i != requested_transfer_queues; ++i)
			vkGetDeviceQueue(m_device, m_transfer_queues.family_index, i, m_transfer_queues.queues + i);
	}

	// Get supported swapchain settings
	{
		uint32_t present_mode_cnt;
		check(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_cnt, nullptr));
		och::heap_buffer<VkPresentModeKHR> present_modes(present_mode_cnt);
		check(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_cnt, present_modes.data()));

		m_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
		for (uint32_t i = 0; i != present_mode_cnt; ++i)
			if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) // TODO: VK_PRESENT_MODE_IMMEDIATE_KHR
			{
				m_swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

				break;
			}
	}

	// Choose initial swapchain extent
	{
		if (surface_capabilites.currentExtent.width == ~0u)
		{
			uint32_t width, height;
			glfwGetFramebufferSize(m_window, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));

			m_swapchain_extent.width = och::clamp(width, surface_capabilites.minImageExtent.width, surface_capabilites.maxImageExtent.width);
			m_swapchain_extent.height = och::clamp(height, surface_capabilites.minImageExtent.height, surface_capabilites.maxImageExtent.height);
		}
		else
			m_swapchain_extent = surface_capabilites.currentExtent;
	}

	// Create swapchain
	{
		// Choose image count
		uint32_t requested_img_cnt = surface_capabilites.minImageCount + 1;

		if (surface_capabilites.minImageCount == MAX_SWAPCHAIN_IMAGE_CNT || surface_capabilites.maxImageCount == surface_capabilites.minImageCount)
			--requested_img_cnt;
		else if (surface_capabilites.minImageCount > MAX_SWAPCHAIN_IMAGE_CNT)
			return MSG_ERROR("Minimum number of images supported by swapchain exceeds engine's maximum");

		VkSwapchainCreateInfoKHR swapchain_ci{};
		swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_ci.pNext = nullptr;
		swapchain_ci.flags = 0;
		swapchain_ci.surface = m_surface;
		swapchain_ci.minImageCount = requested_img_cnt;
		swapchain_ci.imageFormat = m_swapchain_format;
		swapchain_ci.imageColorSpace = m_swapchain_colorspace;
		swapchain_ci.imageExtent = m_swapchain_extent;
		swapchain_ci.imageArrayLayers = 1;
		swapchain_ci.imageUsage = swapchain_image_usage;
		swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_ci.queueFamilyIndexCount = 0;
		swapchain_ci.pQueueFamilyIndices = nullptr;
		swapchain_ci.preTransform = surface_capabilites.currentTransform;
		swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchain_ci.presentMode = m_swapchain_present_mode;
		swapchain_ci.clipped = VK_TRUE; // TODO: VK_FALSE?
		swapchain_ci.oldSwapchain = nullptr;

		VkResult create_rst = vkCreateSwapchainKHR(m_device, &swapchain_ci, nullptr, &m_swapchain);

		check(create_rst);
	}

	// Get swapchain images (and the number thereof)
	{
		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, nullptr));

		if (m_swapchain_image_cnt > MAX_SWAPCHAIN_IMAGE_CNT)
			return MSG_ERROR("Created swapchain contains too many images");

		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, m_swapchain_images));
	}

	// Create swapchain image views
	{
		for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		{
			VkImageViewCreateInfo imview_ci{};
			imview_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imview_ci.pNext = nullptr;
			imview_ci.flags = 0;
			imview_ci.image = m_swapchain_images[i];
			imview_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imview_ci.format = m_swapchain_format;
			imview_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
			imview_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imview_ci.subresourceRange.baseMipLevel = 0;
			imview_ci.subresourceRange.levelCount = 1;
			imview_ci.subresourceRange.baseArrayLayer = 0;
			imview_ci.subresourceRange.layerCount = 1;

			check(vkCreateImageView(m_device, &imview_ci, nullptr, &m_swapchain_image_views[i]));
		}
	}

	// Set allowed swapchain usages
	{
		m_image_swapchain_usage = swapchain_image_usage;
	}

	// Get memory heap- and type-indices for device- and staging-memory
	{
		vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);
	}

	return {};
}

void och::vulkan_context::destroy() const noexcept
{
	for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		vkDestroyImageView(m_device, m_swapchain_image_views[i], nullptr);

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

	vkDestroyDevice(m_device, nullptr);

	vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

#ifdef OCH_VALIDATE

	// If m_instance is null at this point, this cleanup must be the result of a failed initialization, 
	// and m_debug_messenger cannot have been created yet either. Hence, we skip this to avoid loader issues.
	if (m_instance)
	{
		auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

		if (destroy_fn)
			destroy_fn(m_instance, m_debug_messenger, nullptr);
		else if (m_instance)
			och::print("\nERROR DURING CLEANUP: Could not load vkDestroyDebugUtilsMessengerEXT\n");
	}

#endif // OCH_VALIDATE

	vkDestroyInstance(m_instance, nullptr);

	glfwDestroyWindow(m_window);

	glfwTerminate();
}

och::err_info och::vulkan_context::recreate_swapchain() noexcept
{
	// Get current window size

	glfwGetFramebufferSize(m_window, reinterpret_cast<int*>(&m_swapchain_extent.width), reinterpret_cast<int*>(&m_swapchain_extent.height));

	while (!m_swapchain_extent.width || !m_swapchain_extent.height)
	{
		glfwWaitEvents();

		glfwGetFramebufferSize(m_window, reinterpret_cast<int*>(&m_swapchain_extent.width), reinterpret_cast<int*>(&m_swapchain_extent.height));
	}

	check(vkDeviceWaitIdle(m_device));

	// Get surface capabilities for current pre-transform

	VkSurfaceCapabilitiesKHR surface_capabilites;
	check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilites));

	// Destroy old swapchain's image views

	for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		vkDestroyImageView(m_device, m_swapchain_image_views[i], nullptr);

	// Temporary swapchain handle
	VkSwapchainKHR tmp_swapchain;

	// Actually recreate swapchain

	VkSwapchainCreateInfoKHR swapchain_ci{};
	swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_ci.pNext = nullptr;
	swapchain_ci.flags = 0;
	swapchain_ci.surface = m_surface;
	swapchain_ci.minImageCount = m_swapchain_image_cnt;
	swapchain_ci.imageFormat = m_swapchain_format;
	swapchain_ci.imageColorSpace = m_swapchain_colorspace;
	swapchain_ci.imageExtent = m_swapchain_extent;
	swapchain_ci.imageArrayLayers = 1;
	swapchain_ci.imageUsage = m_image_swapchain_usage;
	swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_ci.queueFamilyIndexCount = 0;
	swapchain_ci.pQueueFamilyIndices = nullptr;
	swapchain_ci.preTransform = surface_capabilites.currentTransform;
	swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_ci.presentMode = m_swapchain_present_mode;
	swapchain_ci.clipped = VK_TRUE; // TODO: VK_FALSE?
	swapchain_ci.oldSwapchain = m_swapchain;

	check(vkCreateSwapchainKHR(m_device, &swapchain_ci, nullptr, &tmp_swapchain));

	// Now that the new swapchain has been created, the old one can be destroyed and the new one assigned to the struct member

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

	m_swapchain = tmp_swapchain;

	// Get swapchain images (and the number thereof) for new swapchain
	{
		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, nullptr));

		if (m_swapchain_image_cnt > MAX_SWAPCHAIN_IMAGE_CNT)
			return MSG_ERROR("Recreated swapchain contains too many images");

		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, m_swapchain_images));
	}

	// Create swapchain image views for new swapchain
	{
		for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		{
			VkImageViewCreateInfo imview_ci{};
			imview_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imview_ci.pNext = nullptr;
			imview_ci.flags = 0;
			imview_ci.image = m_swapchain_images[i];
			imview_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imview_ci.format = m_swapchain_format;
			imview_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
			imview_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imview_ci.subresourceRange.baseMipLevel = 0;
			imview_ci.subresourceRange.levelCount = 1;
			imview_ci.subresourceRange.baseArrayLayer = 0;
			imview_ci.subresourceRange.layerCount = 1;

			check(vkCreateImageView(m_device, &imview_ci, nullptr, &m_swapchain_image_views[i]));
		}
	}

	return {};
}

uint32_t och::vulkan_context:: suitable_memory_type_idx(uint32_t memory_type_mask, VkMemoryPropertyFlags property_flags) const noexcept
{
	for (uint32_t i = 0; i != m_memory_properties.memoryTypeCount; ++i)
		if ((memory_type_mask & (1 << i)) && och::contains_all(m_memory_properties.memoryTypes[i].propertyFlags, property_flags))
			return i;

	return ~0u;
}

och::err_info och::vulkan_context::load_shader_module_file(VkShaderModule& out_shader_module, const char* filename) const noexcept
{
	och::mapped_file<uint32_t> shader_file(filename, och::fio::access_read, och::fio::open_normal, och::fio::open_fail);

	if (!shader_file)
		return MSG_ERROR("Could not find shader file");

	VkShaderModuleCreateInfo module_ci{};
	module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	module_ci.pNext = nullptr;
	module_ci.flags = 0;
	module_ci.codeSize = shader_file.bytes();
	module_ci.pCode = shader_file.data();

	check(vkCreateShaderModule(m_device, &module_ci, nullptr, &out_shader_module));

	return {};
}
