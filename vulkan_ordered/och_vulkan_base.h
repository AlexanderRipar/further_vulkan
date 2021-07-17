#pragma once

#include <vulkan/vulkan.h>

#include <GLFW/glfw3.h>

#include "och_error_handling.h"
#include "och_heap_buffer.h"
#include "och_helpers.h"

namespace och
{
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

		uint32_t family_index;

		uint32_t cnt;

		uint32_t offset; // Used if compute and graphics queue families are merged.

		VkQueue queues[MAX_QUEUE_CNT];

		VkQueue& operator[](size_t n) noexcept { return queues[n + offset]; }

		const VkQueue& operator[](size_t n) const noexcept { return queues[n - offset]; }
	};

	struct vulkan_context
	{
		static inline required_feature_list s_feats;

		static constexpr uint32_t MAX_SWAPCHAIN_IMAGE_CNT = 4;



		struct
		{
			bool framebuffer_resized : 1;
			bool fully_initialized : 1;
			bool separate_compute_and_general_queue : 1;
		} m_flags{};

		queue_family_info m_general_queues{};

		queue_family_info m_compute_queues{};

		queue_family_info m_transfer_queues{};

		VkExtent3D m_min_image_transfer_granularity{};


		
		VkPhysicalDeviceMemoryProperties m_memory_properties{};


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

		VkImageUsageFlags m_image_swapchain_usage{};

		VkExtent2D m_swapchain_extent{};

		uint32_t m_swapchain_image_cnt{};

		VkImage m_swapchain_images[MAX_SWAPCHAIN_IMAGE_CNT]{};

		VkImageView m_swapchain_image_views[MAX_SWAPCHAIN_IMAGE_CNT]{};



		err_info create(const char* app_name, uint32_t window_width, uint32_t window_height, uint32_t requested_general_queues = 1, uint32_t requested_compute_queues = 0, uint32_t requested_transfer_queues = 0, VkImageUsageFlags swapchain_image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, const VkPhysicalDeviceFeatures* enabled_device_features = nullptr, bool allow_compute_graphics_merge = true) noexcept;

		void destroy() const noexcept;

		err_info recreate_swapchain() noexcept;

		uint32_t suitable_memory_type_idx(uint32_t memory_type_mask, VkMemoryPropertyFlags property_flags) const noexcept;

		err_info load_shader_module_file(VkShaderModule& out_shader_module, const char* filename) const noexcept;
	};
}
