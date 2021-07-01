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

	struct queue_info
	{
		uint32_t index;

		VkQueueFamilyProperties properties;
	};

	struct vulkan_context
	{
		static inline required_feature_list s_feats;

		static constexpr uint32_t max_swapchain_images = 8;



		struct
		{
			bool framebuffer_resized : 1;
		} flags{};

		queue_info m_render_queue_info;

		queue_info m_transfer_queue_info;



		GLFWwindow* m_window{};

		VkQueue m_render_queue;

		VkQueue m_transfer_queue;

		VkInstance m_instance{};

		VkPhysicalDevice m_physical_device{};

		VkDevice m_device{};

		VkSurfaceKHR m_surface{};

		VkDebugUtilsMessengerEXT m_debug_messenger;



		VkSwapchainKHR m_swapchain;

		VkFormat m_swapchain_format;

		VkColorSpaceKHR m_swapchain_colorspace;

		VkPresentModeKHR m_swapchain_present_mode;

		VkExtent2D m_swapchain_extent;

		uint32_t m_swapchain_image_cnt;

		VkImage m_swapchain_images[max_swapchain_images]{};

		VkImageView m_swapchain_image_views[max_swapchain_images]{};



		err_info create(const char* app_name, uint32_t window_width, uint32_t window_height)
		{
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
					och::heap_buffer<VkQueueFamilyProperties> families(queue_family_cnt);
					vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_cnt, families.data());

					uint32_t transfer_queue_index = VK_QUEUE_FAMILY_IGNORED;
					uint32_t render_queue_index = VK_QUEUE_FAMILY_IGNORED;

					for (uint32_t f = 0; f != queue_family_cnt; ++f)
					{
						VkBool32 supports_present;
						check(vkGetPhysicalDeviceSurfaceSupportKHR(dev, f, m_surface, &supports_present));

						if ((families[f].queueFlags & (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT)) == (VK_QUEUE_COMPUTE_BIT | VK_QUEUE_GRAPHICS_BIT) && supports_present)
							render_queue_index = f;
						else if (families[f].queueFlags == VK_QUEUE_TRANSFER_BIT || ((families[f].queueFlags & VK_QUEUE_TRANSFER_BIT) == VK_QUEUE_TRANSFER_BIT && transfer_queue_index == VK_QUEUE_FAMILY_IGNORED))
							transfer_queue_index = f;
					}

					if (transfer_queue_index == VK_QUEUE_FAMILY_IGNORED || render_queue_index == VK_QUEUE_FAMILY_IGNORED)
						continue;

					m_transfer_queue_info = { transfer_queue_index, families[transfer_queue_index] };
					m_render_queue_info = { render_queue_index, families[render_queue_index] };

					m_physical_device = dev;

					goto DEVICE_SELECTED;
				}

				return MSG_ERROR("Could not locate a suitable physical device");

			DEVICE_SELECTED:;
			}

			// Create logical device
			{
				float render_queue_priority = 1.0F, transfer_queue_priority = 0.75F;

				VkDeviceQueueCreateInfo queue_cis[2]{};

				queue_cis[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_cis[0].queueFamilyIndex = m_render_queue_info.index;
				queue_cis[0].queueCount = 1;
				queue_cis[0].pQueuePriorities = &render_queue_priority;

				queue_cis[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				queue_cis[1].queueFamilyIndex = m_transfer_queue_info.index;
				queue_cis[1].queueCount = 1;
				queue_cis[1].pQueuePriorities = &transfer_queue_priority;

				VkPhysicalDeviceFeatures enabled_features{};

				VkDeviceCreateInfo device_ci{};
				device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
				device_ci.queueCreateInfoCount = 2;
				device_ci.pQueueCreateInfos = queue_cis;
				device_ci.enabledLayerCount = s_feats.dev_layer_cnt();
				device_ci.ppEnabledLayerNames = s_feats.dev_layers();
				device_ci.enabledExtensionCount = s_feats.dev_extension_cnt();
				device_ci.ppEnabledExtensionNames = s_feats.dev_extensions();
				device_ci.pEnabledFeatures = &enabled_features;

				check(vkCreateDevice(m_physical_device, &device_ci, nullptr, &m_device));

				vkGetDeviceQueue(m_device, m_render_queue_info.index, 0, &m_render_queue);

				vkGetDeviceQueue(m_device, m_render_queue_info.index, 0, &m_transfer_queue);
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

			// Get surface's capabilites

			VkSurfaceCapabilitiesKHR surface_capabilites;
			check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilites));

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

				if (surface_capabilites.minImageCount == max_swapchain_images || surface_capabilites.maxImageCount == surface_capabilites.minImageCount)
					--requested_img_cnt;
				else if (surface_capabilites.minImageCount > max_swapchain_images)
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
				swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
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

				if (m_swapchain_image_cnt > max_swapchain_images)
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

			auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

			if (destroy_fn)
				destroy_fn(m_instance, m_debug_messenger, nullptr);
			else
				och::print("\nERROR DURING CLEANUP: Could not load vkDestroyDebugUtilsMessengerEXT\n");

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

			//Actually recreate swapchain

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
			swapchain_ci.oldSwapchain = nullptr; //m_swapchain;

			for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
				vkDestroyImageView(m_device, m_swapchain_image_views[i], nullptr);

			vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

			check(vkCreateSwapchainKHR(m_device, &swapchain_ci, nullptr, &m_swapchain));

			// Get swapchain images (and the number thereof)
			{
				check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, nullptr));

				if (m_swapchain_image_cnt > max_swapchain_images)
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

			// Delete old swapchain

			//vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

			return {};
		}
	};

	/*
	struct swapchain_context
	{
		static constexpr uint32_t max_images = 8;

		VkFormat m_format;

		VkColorSpaceKHR m_colorspace;

		VkPresentModeKHR m_present_mode;

		VkExtent2D m_extent;

		uint32_t m_image_cnt;

		VkImage m_images[max_images]{};

		VkImageView m_image_views[max_images]{};

		VkSwapchainKHR m_swapchain;

		err_info create (const vulkan_context& context) noexcept
		{
			// Get swapchain settings supported by context
			{
				uint32_t format_cnt;
				check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.m_physical_device, context.m_surface, &format_cnt, nullptr));
				och::heap_buffer<VkSurfaceFormatKHR> formats(format_cnt);
				check(vkGetPhysicalDeviceSurfaceFormatsKHR(context.m_physical_device, context.m_surface, &format_cnt, formats.data()));

				m_format = formats[0].format;
				m_colorspace = formats[0].colorSpace;
				for (uint32_t i = 0; i != format_cnt; ++i)
					if (formats[i].format == VK_FORMAT_B8G8R8A8_SRGB && formats[i].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
					{
						m_format = formats[i].format;
						m_colorspace = formats[i].colorSpace;

						break;
					}

				uint32_t present_mode_cnt;
				check(vkGetPhysicalDeviceSurfacePresentModesKHR(context.m_physical_device, context.m_surface, &present_mode_cnt, nullptr));
				och::heap_buffer<VkPresentModeKHR> present_modes(present_mode_cnt);
				check(vkGetPhysicalDeviceSurfacePresentModesKHR(context.m_physical_device, context.m_surface, &present_mode_cnt, present_modes.data()));

				m_present_mode = VK_PRESENT_MODE_FIFO_KHR;
				for (uint32_t i = 0; i != present_mode_cnt; ++i)
					if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) // TODO: VK_PRESENT_MODE_IMMEDIATE_KHR
					{
						m_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

						break;
					}
			}

			// Get surface's capabilites

			VkSurfaceCapabilitiesKHR surface_capabilites;
			check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.m_physical_device, context.m_surface, &surface_capabilites));

			// Choose initial swapchain extent
			{
				if (surface_capabilites.currentExtent.width == ~0u)
				{
					uint32_t width, height;
					glfwGetFramebufferSize(context.m_window, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));

					m_extent.width = och::clamp(width, surface_capabilites.minImageExtent.width, surface_capabilites.maxImageExtent.width);
					m_extent.height = och::clamp(height, surface_capabilites.minImageExtent.height, surface_capabilites.maxImageExtent.height);
				}
				else
					m_extent = surface_capabilites.currentExtent;
			}

			// Create swapchain
			{
				// Choose image count
				uint32_t requested_img_cnt = surface_capabilites.minImageCount + 1;

				if (surface_capabilites.minImageCount == max_images || surface_capabilites.maxImageCount == surface_capabilites.minImageCount)
					--requested_img_cnt;
				else if (surface_capabilites.minImageCount > max_images)
					return MSG_ERROR("Minimum number of images supported by swapchain exceeds engine's maximum");

				VkSwapchainCreateInfoKHR swapchain_ci{};
				swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
				swapchain_ci.pNext = nullptr;
				swapchain_ci.flags = 0;
				swapchain_ci.surface = context.m_surface;
				swapchain_ci.minImageCount = requested_img_cnt;
				swapchain_ci.imageFormat = m_format;
				swapchain_ci.imageColorSpace = m_colorspace;
				swapchain_ci.imageExtent = m_extent;
				swapchain_ci.imageArrayLayers = 1;
				swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
				swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				swapchain_ci.queueFamilyIndexCount = 0;
				swapchain_ci.pQueueFamilyIndices = nullptr;
				swapchain_ci.preTransform = surface_capabilites.currentTransform;
				swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
				swapchain_ci.presentMode = m_present_mode;
				swapchain_ci.clipped = VK_TRUE; // TODO: VK_FALSE?
				swapchain_ci.oldSwapchain = nullptr;

				VkResult create_rst = vkCreateSwapchainKHR(context.m_device, &swapchain_ci, nullptr, &m_swapchain);

				check(create_rst);
			}

			// Get swapchain images (and the number thereof)
			{
				check(vkGetSwapchainImagesKHR(context.m_device, m_swapchain, &m_image_cnt, nullptr));

				if (m_image_cnt > max_images)
					return MSG_ERROR("Created swapchain contains too many images");

				check(vkGetSwapchainImagesKHR(context.m_device, m_swapchain, &m_image_cnt, m_images));
			}

			// Create swapchain image views
			{
				for (uint32_t i = 0; i != m_image_cnt; ++i)
				{
					VkImageViewCreateInfo imview_ci{};
					imview_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
					imview_ci.pNext = nullptr;
					imview_ci.flags = 0;
					imview_ci.image = m_images[i];
					imview_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
					imview_ci.format = m_format;
					imview_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
					imview_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
					imview_ci.subresourceRange.baseMipLevel = 0;
					imview_ci.subresourceRange.levelCount = 1;
					imview_ci.subresourceRange.baseArrayLayer = 0;
					imview_ci.subresourceRange.layerCount = 1;

					check(vkCreateImageView(context.m_device, &imview_ci, nullptr, &m_image_views[i]));
				}
			}

			return {};
		}

		void destroy(const vulkan_context& context) noexcept
		{
			for (uint32_t i = 0; i != m_image_cnt; ++i)
				vkDestroyImageView(context.m_device, m_image_views[i], nullptr);

			vkDestroySwapchainKHR(context.m_device, m_swapchain, nullptr);
		}

		err_info recreate(const vulkan_context& context)
		{
			// Get current window size

			glfwGetFramebufferSize(context.m_window, reinterpret_cast<int*>(&m_extent.width), reinterpret_cast<int*>(&m_extent.height));

			// Get surface capabilities for current pre-transform

			VkSurfaceCapabilitiesKHR surface_capabilites;
			check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(context.m_physical_device, context.m_surface, &surface_capabilites));

			//Actually recreate swapchain

			VkSwapchainCreateInfoKHR swapchain_ci{};
			swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			swapchain_ci.pNext = nullptr;
			swapchain_ci.flags = 0;
			swapchain_ci.surface = context.m_surface;
			swapchain_ci.minImageCount = m_image_cnt;
			swapchain_ci.imageFormat = m_format;
			swapchain_ci.imageColorSpace = m_colorspace;
			swapchain_ci.imageExtent = m_extent;
			swapchain_ci.imageArrayLayers = 1;
			swapchain_ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
			swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
			swapchain_ci.queueFamilyIndexCount = 0;
			swapchain_ci.pQueueFamilyIndices = nullptr;
			swapchain_ci.preTransform = surface_capabilites.currentTransform;
			swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
			swapchain_ci.presentMode = m_present_mode;
			swapchain_ci.clipped = VK_TRUE; // TODO: VK_FALSE?
			swapchain_ci.oldSwapchain = m_swapchain;

			VkSwapchainKHR tmp_swapchain;

			check(vkCreateSwapchainKHR(context.m_device, &swapchain_ci, nullptr, &tmp_swapchain));

			// Delete old swapchain

			vkDestroySwapchainKHR(context.m_device, m_swapchain, nullptr);

			m_swapchain = tmp_swapchain;
		}
	};
	*/

	void vulkan_context_resize_callback(GLFWwindow* window, int width, int height)
	{
		width, height;

		reinterpret_cast<vulkan_context*>(glfwGetWindowUserPointer(window))->flags.framebuffer_resized = true;
	}
}
