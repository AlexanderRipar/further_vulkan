#pragma once

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "och_vulkan_debug_callback.h"
#include "och_error_handling.h"
#include "och_heap_buffer.h"
#include "och_helpers.h"

namespace och
{
	void vulkan_context_resize_callback(GLFWwindow* window, int width, int height);

	struct required_feature_list
	{
		static constexpr uint32_t max_cnt = 8;

		const char* m_inst_extensions[max_cnt]{
	#ifdef OCH_VALIDATE
				"VK_EXT_debug_utils"
	#endif // OCH_VALIDATE
		};

		const char* m_inst_layers[max_cnt]
		{
	#ifdef OCH_VALIDATE
				"VK_LAYER_KHRONOS_validation",
	#endif // OCH_VALIDATE
		};

#ifdef OCH_VALIDATE
		uint32_t m_inst_extension_cnt = 1;

		uint32_t m_inst_layer_cnt = 1;
#else
		uint32_t instance_extension_cnt = 0;

		uint32_t instance_layer_cnt = 0;
#endif // OCH_VALIDATE

		const char* m_dev_extensions[max_cnt]
		{
			"VK_KHR_swapchain",
		};

		const char* m_dev_layers[max_cnt]
		{
			"VK_LAYER_KHRONOS_validation",
		};

		uint32_t m_dev_extension_cnt = 1;

		uint32_t m_dev_layer_cnt = 1;

		err_info add_instance_extensions(const char** exts, uint32_t cnt)
		{
			if (m_inst_extension_cnt + cnt > max_cnt)
				return MSG_ERROR("Too many instance extensions requested");

			for (uint32_t i = 0; i != cnt; ++i)
				m_inst_extensions[m_inst_extension_cnt++] = exts[i];

			return{};
		};

		err_info add_instance_layers(const char** layers, uint32_t cnt)
		{
			if (m_inst_layer_cnt + cnt > max_cnt)
				return MSG_ERROR("Too many instance layers requested");

			for (uint32_t i = 0; i != cnt; ++i)
				m_inst_layers[m_inst_layer_cnt++] = layers[i];

			return{};
		};

		err_info add_device_extensions(const char** exts, uint32_t cnt)
		{
			if (m_dev_extension_cnt + cnt > max_cnt)
				return MSG_ERROR("Too many device extensions requested");

			for (uint32_t i = 0; i != cnt; ++i)
				m_dev_extensions[m_dev_extension_cnt++] = exts[i];

			return{};
		};

		err_info add_device_layers(const char** layers, uint32_t cnt)
		{
			if (m_dev_layer_cnt + cnt > max_cnt)
				return MSG_ERROR("Too many device layers requested");

			for (uint32_t i = 0; i != cnt; ++i)
				m_dev_layers[m_dev_layer_cnt++] = layers[i];

			return{};
		};

		const char* const* inst_extensions() const noexcept { return m_inst_extension_cnt ? m_inst_extensions : nullptr; }

		uint32_t inst_extension_cnt() const noexcept { return m_inst_extension_cnt; }

		const char* const* inst_layers() const noexcept { return m_inst_layer_cnt ? m_inst_layers : nullptr; }

		uint32_t inst_layer_cnt() const noexcept { return m_inst_layer_cnt; }

		const char* const* dev_extensions() const noexcept { return m_dev_extension_cnt ? m_dev_extensions : nullptr; }

		uint32_t dev_extension_cnt() const noexcept { return m_dev_extension_cnt; }

		const char* const* dev_layers() const noexcept { return m_dev_layer_cnt ? m_dev_layers : nullptr; }

		uint32_t dev_layer_cnt() const noexcept { return m_dev_layer_cnt; }

		err_info check_instance_support(bool& has_support) const noexcept
		{
			{
				uint32_t avl_ext_cnt;
				check(vkEnumerateInstanceExtensionProperties(nullptr, &avl_ext_cnt, nullptr));
				och::heap_buffer<VkExtensionProperties> avl_exts(avl_ext_cnt);
				check(vkEnumerateInstanceExtensionProperties(nullptr, &avl_ext_cnt, avl_exts.data()));

				for (uint32_t i = 0; i != m_inst_extension_cnt; ++i)
				{
					for (uint32_t j = 0; j != avl_ext_cnt; ++j)
						if (!strcmp(m_inst_extensions[i], avl_exts[j].extensionName))
							goto EXT_AVAILABLE;

					has_support = false;

					return{};

				EXT_AVAILABLE:;
				}
			}

			{
				uint32_t avl_layer_cnt;
				check(vkEnumerateInstanceLayerProperties(&avl_layer_cnt, nullptr));
				och::heap_buffer<VkLayerProperties> avl_layers(avl_layer_cnt);
				check(vkEnumerateInstanceLayerProperties(&avl_layer_cnt, avl_layers.data()));

				for (uint32_t i = 0; i != m_inst_layer_cnt; ++i)
				{
					for (uint32_t j = 0; j != avl_layer_cnt; ++j)
						if (!strcmp(m_inst_layers[i], avl_layers[j].layerName))
							goto LAYER_AVAILABLE;

					has_support = false;

					return{};

				LAYER_AVAILABLE:;
				}
			}

			has_support = true;

			return {};
		}

		err_info check_device_support(VkPhysicalDevice dev, bool& has_support) const noexcept
		{
			{
				uint32_t avl_ext_cnt;
				check(vkEnumerateDeviceExtensionProperties(dev, nullptr, &avl_ext_cnt, nullptr));
				och::heap_buffer<VkExtensionProperties> avl_exts(avl_ext_cnt);
				check(vkEnumerateDeviceExtensionProperties(dev, nullptr, &avl_ext_cnt, avl_exts.data()));

				for (uint32_t i = 0; i != m_dev_extension_cnt; ++i)
				{
					for (uint32_t j = 0; j != avl_ext_cnt; ++j)
						if (!strcmp(m_dev_extensions[i], avl_exts[j].extensionName))
							goto EXT_AVAILABLE;

					has_support = false;

					return {};

				EXT_AVAILABLE:;
				}
			}

			{
				uint32_t avl_layer_cnt;
				check(vkEnumerateDeviceLayerProperties(dev, &avl_layer_cnt, nullptr));
				och::heap_buffer<VkLayerProperties> avl_layers(avl_layer_cnt);
				check(vkEnumerateDeviceLayerProperties(dev, &avl_layer_cnt, avl_layers.data()));

				for (uint32_t i = 0; i != m_dev_layer_cnt; ++i)
				{
					for (uint32_t j = 0; j != avl_layer_cnt; ++j)
						if (!strcmp(m_dev_layers[i], avl_layers[j].layerName))
							goto LAYER_AVAILABLE;

					has_support = false;

					return {};

				LAYER_AVAILABLE:;
				}
			}

			has_support = true;

			return {};
		}
	};

	struct queue_family_info
	{
		static constexpr uint32_t MAX_QUEUE_CNT = 4;

		uint32_t index;

		uint32_t cnt;

		VkQueue queues[MAX_QUEUE_CNT];

		VkQueue& operator[](size_t n) noexcept { return queues[n]; }

		const VkQueue& operator[](size_t n) const noexcept { return queues[n]; }
	};

	struct memory_info
	{
		uint32_t heap_index;
		uint32_t type_index;
	};

	struct vulkan_context
	{
		static inline required_feature_list s_feats;

		static constexpr uint32_t MAX_SWAPCHAIN_IMAGE_CNT = 4;



		struct
		{
			bool framebuffer_resized : 1;
			bool fully_initialized : 1;
		} m_flags{};

		queue_family_info m_general_queues{};

		queue_family_info m_compute_queues{};

		queue_family_info m_transfer_queues{};

		VkExtent3D m_min_image_transfer_granularity{};



		memory_info m_device_memory{};

		memory_info m_staging_memory{};



		GLFWwindow* m_window{};

		VkInstance m_instance{};

		VkPhysicalDevice m_physical_device{};

		VkDevice m_device{};

		VkSurfaceKHR m_surface{};

		VkDebugUtilsMessengerEXT m_debug_messenger{};



		VkSwapchainKHR m_swapchain{};

		VkFormat m_swapchain_format{};

		VkColorSpaceKHR m_swapchain_colorspace{};

		VkPresentModeKHR m_swapchain_present_mode{};

		VkExtent2D m_swapchain_extent{};

		uint32_t m_swapchain_image_cnt{};

		VkImage m_swapchain_images[MAX_SWAPCHAIN_IMAGE_CNT]{};

		VkImageView m_swapchain_image_views[MAX_SWAPCHAIN_IMAGE_CNT]{};



		err_info create(const char* app_name, uint32_t window_width, uint32_t window_height, uint32_t requested_general_queues = 1, uint32_t requested_compute_queues = 0, uint32_t requested_transfer_queues = 0, VkImageUsageFlags swapchain_image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, const VkPhysicalDeviceFeatures* enabled_device_features = nullptr)
		{
			if (requested_general_queues > queue_family_info::MAX_QUEUE_CNT || requested_compute_queues > queue_family_info::MAX_QUEUE_CNT || requested_transfer_queues > queue_family_info::MAX_QUEUE_CNT)
				return MSG_ERROR("Too many queues requested");

			// Create window
			{
				glfwInit();

				glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

				// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

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
			messenger_ci.pfnUserCallback = och_vulkan_debug_callback;

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

					// Check support for window surface

					uint32_t format_cnt;
					check(vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &format_cnt, nullptr));

					uint32_t present_mode_cnt;
					check(vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface, &present_mode_cnt, nullptr));

					if (!format_cnt || !present_mode_cnt)
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

						if ((family_properties[f].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT) && supports_present)
							general_queue_index = f;
						else if (family_properties[f].queueFlags == VK_QUEUE_TRANSFER_BIT || ((family_properties[f].queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT && transfer_queue_index == VK_QUEUE_FAMILY_IGNORED))
							transfer_queue_index = f;
					}

					if ((( general_queue_index == VK_QUEUE_FAMILY_IGNORED &&  requested_general_queues) || ( requested_general_queues && family_properties[ general_queue_index].queueCount <  requested_general_queues)) ||
						(( compute_queue_index == VK_QUEUE_FAMILY_IGNORED &&  requested_compute_queues) || ( requested_compute_queues && family_properties[ compute_queue_index].queueCount <  requested_compute_queues)) ||
						((transfer_queue_index == VK_QUEUE_FAMILY_IGNORED && requested_transfer_queues) || (requested_transfer_queues && family_properties[transfer_queue_index].queueCount < requested_transfer_queues)))
						continue;

					// Check if requested Image Usage is available for the Swapchain

					check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, m_surface, &surface_capabilites));

					if ((surface_capabilites.supportedUsageFlags & swapchain_image_usage) != swapchain_image_usage)
						continue;

					if (requested_general_queues)
					{
						m_general_queues.index = general_queue_index;
						m_general_queues.cnt = requested_general_queues;
					}

					if (requested_compute_queues)
					{
						m_compute_queues.index = compute_queue_index;
						m_compute_queues.cnt = requested_compute_queues;
					}

					if (requested_transfer_queues)
					{
						m_transfer_queues.index = transfer_queue_index;
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
					queue_cis[ci_idx].queueFamilyIndex = m_general_queues.index;
					queue_cis[ci_idx].queueCount = requested_general_queues;
					queue_cis[ci_idx].pQueuePriorities = general_queue_priorities;

					++ci_idx;
				}

				if (requested_compute_queues)
				{
					queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					queue_cis[ci_idx].queueFamilyIndex = m_compute_queues.index;
					queue_cis[ci_idx].queueCount = requested_compute_queues;
					queue_cis[ci_idx].pQueuePriorities = compute_queue_priorities;

					++ci_idx;
				}

				if (requested_transfer_queues)
				{
					queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
					queue_cis[ci_idx].queueFamilyIndex = m_transfer_queues.index;
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

				m_general_queues.cnt = requested_general_queues;
				
				m_compute_queues.cnt = requested_compute_queues;

				m_transfer_queues.cnt = requested_transfer_queues;

				for(uint32_t i = 0; i != requested_general_queues; ++i)
					vkGetDeviceQueue(m_device, m_general_queues.index, i, m_general_queues.queues + i);

				for (uint32_t i = 0; i != requested_compute_queues; ++i)
					vkGetDeviceQueue(m_device, m_compute_queues.index, i, m_compute_queues.queues + i);

				for (uint32_t i = 0; i != requested_transfer_queues; ++i)
					vkGetDeviceQueue(m_device, m_transfer_queues.index, i, m_transfer_queues.queues + i);
			}

			// Get supported swapchain settings
			{
				uint32_t format_cnt;
				check(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_cnt, nullptr));
				och::heap_buffer<VkSurfaceFormatKHR> formats(format_cnt);
				check(vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface, &format_cnt, formats.data()));

				m_swapchain_format = formats[0].format;
				m_swapchain_colorspace = formats[0].colorSpace;
				for (uint32_t i = 0; i != format_cnt; ++i)
					if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
					{
						m_swapchain_format = formats[i].format;
						m_swapchain_colorspace = formats[i].colorSpace;

						break;
					}
				
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

			// Get memory heap- and type-indices for device- and staging-memory
			{
				VkPhysicalDeviceMemoryProperties mem_props;
				vkGetPhysicalDeviceMemoryProperties(m_physical_device, &mem_props);

				for (uint32_t i = 0; i != mem_props.memoryTypeCount; ++i)
					if ((mem_props.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
					{
						m_device_memory.type_index = i;
						m_device_memory.heap_index = mem_props.memoryTypes[i].heapIndex;

						goto DEVICE_MEM_FOUND;
					}

				return MSG_ERROR("Could not find a memory type suitable as device memory");

			DEVICE_MEM_FOUND:

				for (uint32_t i = 0; i != mem_props.memoryTypeCount; ++i)
					if ((mem_props.memoryTypes[i].propertyFlags & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT))
					{
						m_staging_memory.type_index = i;
						m_staging_memory.heap_index = mem_props.memoryTypes[i].heapIndex;
						
						goto STAGING_MEM_FOUND;
					}

				return MSG_ERROR("Could not find a memory type suitable as staging memory");

			STAGING_MEM_FOUND:;
			}

			return {};
		}

		void destroy()
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

		err_info recreate_swapchain()
		{
			// Get current window size

			glfwGetFramebufferSize(m_window, reinterpret_cast<int*>(&m_swapchain_extent.width), reinterpret_cast<int*>(&m_swapchain_extent.height));

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
			swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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

		/*
		err_info allocate(	VkImage& out_image, 
							VkImageType type, 
							VkFormat format, 
							VkExtent3D extent, 
							VkImageUsageFlags usage, 
							VkImageLayout initial_layout,
							memory_info memory,
							uint32_t mip_levels = 1,
							VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL, 
							VkSharingMode share_mode = VK_SHARING_MODE_EXCLUSIVE, 
							uint32_t queue_family_cnt = 0, 
							const uint32_t* queue_families_ptr = nullptr)
	{
			VkImageCreateInfo image_ci{};
			image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image_ci.pNext = nullptr;
			image_ci.flags = 0;
			image_ci.imageType = type;
			image_ci.format = format;
			image_ci.extent = extent;
			image_ci.mipLevels = mip_levels;
			image_ci.arrayLayers = 1;
			image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
			image_ci.tiling = tiling;
			image_ci.usage = usage;
			image_ci.sharingMode = share_mode;
			image_ci.queueFamilyIndexCount = queue_family_cnt;
			image_ci.pQueueFamilyIndices = queue_families_ptr;
			image_ci.initialLayout = initial_layout;

			check(vkCreateImage(m_device, &image_ci, nullptr, &out_image));

			VkMemoryRequirements mem_reqs;
			vkGetImageMemoryRequirements(m_device, out_image, &mem_reqs);

			if (!(mem_reqs.memoryTypeBits & (1 << memory.type_index)))
				return MSG_ERROR("Image not compatible with requested memory type");




			return {};
		}
		*/
	};

	void vulkan_context_resize_callback(GLFWwindow* window, int width, int height)
	{
		width, height;

		reinterpret_cast<vulkan_context*>(glfwGetWindowUserPointer(window))->m_flags.framebuffer_resized = true;
	}
}
