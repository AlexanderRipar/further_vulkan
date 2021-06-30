#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>
#include <unordered_map>

#include "och_fmt.h"
#include "och_fio.h"
#include "och_time.h"

#include "och_error_handling.h"
#include "och_bmp_header.h"
#include "och_matmath.h"
#include "och_vulkan_debug_callback.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc\matrix_transform.hpp>
#define GLM_ENABLE_EXTERIMANTAL
#include <gtx\hash.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#include "och_vulkan_base.h"

#define OCH_ASSET_NAME "viking_room"
//#define OCH_ASSET_NAME "vase"

#define OCH_ASSET_OFFSET {0.0F, 0.0F, 0.3F}
#define OCH_ASSET_SCALE 2.0F

using err_info = och::err_info;

void framebuffer_resize_callback_fn(GLFWwindow* window, int width, int height);

VkDebugUtilsMessengerCreateInfoEXT populate_messenger_create_info() noexcept
{
	VkDebugUtilsMessengerCreateInfoEXT create_info{};
	create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	create_info.pfnUserCallback = och_vulkan_debug_callback;
	create_info.pUserData = nullptr;

	return create_info;
}

struct queue_family_indices
{
	uint32_t graphics_idx = ~0u;
	uint32_t compute_idx = ~0u;
	uint32_t transfer_idx = ~0u;
	uint32_t present_idx = ~0u;

	operator bool() const noexcept { return graphics_idx != ~0u && compute_idx != ~0u && transfer_idx != ~0u && present_idx != ~0u; }

	bool discrete_present_family() const noexcept { return graphics_idx != present_idx; }
};

struct swapchain_support_details
{
	VkSurfaceCapabilitiesKHR capabilites{};
	std::vector<VkSurfaceFormatKHR> formats{};
	std::vector<VkPresentModeKHR> present_modes{};
};

struct uniform_buffer_obj
{
	och::mat4 model;
	och::mat4 view;
	och::mat4 projection;
};

struct vertex
{
	glm::vec3 pos;
	glm::vec3 col;
	glm::vec2 tex_pos;

	bool operator==(const vertex& rhs) const noexcept
	{
		return pos == rhs.pos && col == rhs.col && tex_pos == rhs.tex_pos;
	}

	static constexpr VkVertexInputBindingDescription binding_desc{ 0, 32, VK_VERTEX_INPUT_RATE_VERTEX };

	static constexpr VkVertexInputAttributeDescription attribute_descs[]{
		{ 0, 0, VK_FORMAT_R32G32B32_SFLOAT,  0 },
		{ 1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12 },
		{ 2, 0, VK_FORMAT_R32G32_SFLOAT   , 24 },
	};
};

static_assert(vertex::binding_desc.stride == sizeof(vertex));
static_assert(vertex::attribute_descs[0].offset == offsetof(vertex, vertex::pos));
static_assert(vertex::attribute_descs[1].offset == offsetof(vertex, vertex::col));
static_assert(vertex::attribute_descs[2].offset == offsetof(vertex, vertex::tex_pos));

namespace std {
	template<> struct hash<vertex> {
		size_t operator()(vertex const& vertex) const {
			return (
				(hash<glm::vec3>()(vertex.pos) ^
				(hash<glm::vec3>()(vertex.col) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.tex_pos) << 1);
		}
	};
}



struct hello_vulkan
{
	static constexpr uint32_t max_frames_in_flight = 2;

#ifdef OCH_VALIDATE
	static constexpr const char* required_validation_layers[]{ "VK_LAYER_KHRONOS_validation" };
#endif // OCH_VALIDATE

	static constexpr const char* required_device_extensions[]{ VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	std::vector<vertex> vertices;

	std::vector<uint32_t> indices;

	uint32_t window_width = 1440;
	uint32_t window_height = 810;

	GLFWwindow* window = nullptr;

	VkInstance vk_instance = nullptr;

	VkSurfaceKHR vk_surface = nullptr;

	VkPhysicalDevice vk_physical_device = nullptr;

	VkDevice vk_device = nullptr;

	VkQueue vk_graphics_queue = nullptr;

	VkQueue vk_present_queue = nullptr;

	VkSwapchainKHR vk_swapchain = nullptr;

	std::vector<VkImage> vk_swapchain_images;

	VkFormat vk_swapchain_format;

	VkExtent2D vk_swapchain_extent;

	std::vector<VkImageView> vk_swapchain_views;

	VkRenderPass vk_render_pass = nullptr;

	VkDescriptorSetLayout vk_descriptor_set_layout = nullptr;

	VkPipelineLayout vk_pipeline_layout = nullptr;

	VkPipeline vk_graphics_pipeline = nullptr;

	std::vector<VkFramebuffer> vk_swapchain_framebuffers;

	VkCommandPool vk_command_pool = nullptr;

	std::vector<VkCommandBuffer> vk_command_buffers;

	VkSemaphore vk_image_available_semaphores[max_frames_in_flight];
	VkSemaphore vk_render_complete_semaphores[max_frames_in_flight];
	VkFence vk_inflight_fences[max_frames_in_flight];
	std::vector<VkFence> vk_images_inflight_fences;

	VkBuffer vk_vertex_buffer = nullptr;

	VkDeviceMemory vk_vertex_buffer_memory = nullptr;

	VkBuffer vk_index_buffer = nullptr;

	VkDeviceMemory vk_index_buffer_memory = nullptr;

	std::vector<VkBuffer> vk_uniform_buffers;
	
	std::vector<VkDeviceMemory> vk_uniform_buffers_memory;

	VkDescriptorPool vk_descriptor_pool = nullptr;

	std::vector<VkDescriptorSet> vk_descriptor_sets;

	uint32_t vk_texture_image_mipmap_levels;

	VkImage vk_texture_image = nullptr;

	VkDeviceMemory vk_texture_image_memory = nullptr;

	VkImageView vk_texture_image_view = nullptr;

	VkSampler vk_texture_sampler = nullptr;

	VkImage vk_depth_image = nullptr;

	VkDeviceMemory vk_depth_image_memory = nullptr;

	VkImageView vk_depth_image_view = nullptr;

	VkSampleCountFlagBits vk_msaa_samples = VK_SAMPLE_COUNT_1_BIT;

	VkImage vk_colour_image = nullptr;

	VkDeviceMemory vk_colour_image_memory = nullptr;

	VkImageView vk_colour_image_view = nullptr;

	size_t curr_frame = 0;

	bool framebuffer_resized = false;

#ifdef OCH_VALIDATE
	VkDebugUtilsMessengerEXT vk_debug_messenger = nullptr;
#endif // OCH_VALIDATE

	err_info run()
	{
		init_window();

		check(init_vulkan());

		check(main_loop());

		cleanup();

		return {};
	}

	void init_window()
	{
		glfwInit();

		glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

		// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

		window = glfwCreateWindow(window_width, window_height, "Hello Vulkan", nullptr, nullptr);

		glfwSetWindowUserPointer(window, this);

		glfwSetFramebufferSizeCallback(window, framebuffer_resize_callback_fn);
	}

	err_info init_vulkan()
	{
		check(create_vk_instance());

		check(create_vk_debug_messenger());

		check(create_vk_surface());

		check(select_vk_physical_device());

		check(create_vk_logical_device());

		check(create_vk_swapchain());

		check(get_vk_swapchain_views());

		check(create_vk_render_pass());

		check(create_vk_descriptor_set_layout());

		check(create_vk_graphics_pipeline());

		check(create_vk_command_pool());

		check(create_vk_colour_resources());

		check(create_vk_depth_resources());

		check(create_vk_swapchain_framebuffers());

		check(create_vk_texture_image());

		check(create_vk_texture_image_view());

		check(create_vk_texture_sampler());

		check(load_obj_model());

		check(create_vk_vertex_buffer());

		check(create_vk_index_buffer());

		check(create_vk_uniform_buffers());

		check(create_vk_descriptor_pool());

		check(create_vk_descriptor_sets());

		check(create_vk_command_buffers());

		check(create_vk_sync_objects());

		return {};
	}

	err_info create_vk_instance()
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = "Hello Vulkan";
		app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		std::vector<const char*> extensions;

		och::range<const char* const> layers;

		check(get_required_instance_extensions(extensions));

		check(get_required_instance_validation_layers(layers));

		VkInstanceCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		create_info.pApplicationInfo = &app_info;
		create_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
		create_info.ppEnabledExtensionNames = extensions.data();
		create_info.enabledLayerCount = static_cast<uint32_t>(layers.len());
		create_info.ppEnabledLayerNames = layers.beg;

#ifdef OCH_VALIDATE

		VkDebugUtilsMessengerCreateInfoEXT messenger_info = populate_messenger_create_info();
		create_info.pNext = &messenger_info;

#endif // OCH_VALIDATE

		check(vkCreateInstance(&create_info, nullptr, &vk_instance));

		return {};
	}

	err_info get_required_instance_extensions(std::vector<const char*>& required_extensions)
	{
		uint32_t glfw_cnt;

		const char** glfw_names = glfwGetRequiredInstanceExtensions(&glfw_cnt);

		for (uint32_t i = 0; i != glfw_cnt; ++i)
			och::print("{}\n", glfw_names[i]);

		required_extensions = std::vector(glfw_names, glfw_names + glfw_cnt);

#ifdef OCH_VALIDATE
		required_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif // OCH_VALIDATE

		uint32_t vk_cnt;

		check(vkEnumerateInstanceExtensionProperties(nullptr, &vk_cnt, nullptr));

		std::vector<VkExtensionProperties> available_exts(vk_cnt);

		check(vkEnumerateInstanceExtensionProperties(nullptr, &vk_cnt, available_exts.data()));

		for (const auto& req : required_extensions)
		{
			bool is_available = false;

			for (const auto& avl : available_exts)
				if (!strcmp(req, avl.extensionName))
				{
					is_available = true;
					break;
				}

			if (!is_available)
				return MAKE_ERROR(1);
		}

		return {};
	}

	err_info get_required_instance_validation_layers(och::range<const char* const>& required_layers)
	{
#ifdef OCH_VALIDATE
		uint32_t layer_cnt;

		check(vkEnumerateInstanceLayerProperties(&layer_cnt, nullptr));

		std::vector<VkLayerProperties> available_layers(layer_cnt);

		check(vkEnumerateInstanceLayerProperties(&layer_cnt, available_layers.data()));

		for (const auto& req : required_validation_layers)
		{
			bool is_available = false;

			for (const auto& avl : available_layers)
				if (!strcmp(req, avl.layerName))
				{
					is_available = true;
					break;
				}

			if (!is_available)
				return MAKE_ERROR(1);
		}

		required_layers = och::range<const char* const>(required_validation_layers);

#endif // OCH_VALIDATE

		return {};
	}

	err_info create_vk_debug_messenger()
	{
#ifdef OCH_VALIDATE

		VkDebugUtilsMessengerCreateInfoEXT create_info = populate_messenger_create_info();

		auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(vk_instance, "vkCreateDebugUtilsMessengerEXT"));

		if (create_fn)
		{
			check(create_fn(vk_instance, &create_info, nullptr, &vk_debug_messenger));
		}
		else
			return MAKE_ERROR(1);

#endif // OCH_VALIDATE

		return {};
	}

	err_info create_vk_surface()
	{
		check(glfwCreateWindowSurface(vk_instance, window, nullptr, &vk_surface));

		return {};
	}

	err_info select_vk_physical_device()
	{
		uint32_t device_cnt;

		check(vkEnumeratePhysicalDevices(vk_instance, &device_cnt, nullptr));

		if (!device_cnt)
			return MAKE_ERROR(1);

		std::vector<VkPhysicalDevice> devices(device_cnt);

		check(vkEnumeratePhysicalDevices(vk_instance, &device_cnt, devices.data()));

		for (auto& dev : devices)
		{
			VkPhysicalDeviceProperties props;

			vkGetPhysicalDeviceProperties(dev, &props);

			VkPhysicalDeviceFeatures feats;

			vkGetPhysicalDeviceFeatures(dev, &feats);
			
			if (props.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				continue;

			if (!feats.geometryShader || !feats.samplerAnisotropy)
				continue;

			queue_family_indices queue_families;

			check(query_queue_families(dev, vk_surface, queue_families));

			if (!queue_families)
				continue;

			bool supports_required_dev_extensions;

			check(check_required_device_extension_support(dev, supports_required_dev_extensions));

			if (!supports_required_dev_extensions)
				continue;
			
			swapchain_support_details swapchain_details;

			check(query_swapchain_support(dev, vk_surface, swapchain_details));

			if (swapchain_details.formats.empty() || swapchain_details.present_modes.empty())
				continue;

			vk_physical_device = dev;

			vk_msaa_samples = query_max_msaa_samples();

			break;
		}

		if (!vk_physical_device)
			return MAKE_ERROR(1);

		return {};
	}

	err_info check_required_device_extension_support(VkPhysicalDevice physical_dev, bool& all_supported)
	{
		uint32_t extension_cnt;

		check(vkEnumerateDeviceExtensionProperties(physical_dev, nullptr, &extension_cnt, nullptr));

		std::vector<VkExtensionProperties> available_dev_extensions(extension_cnt);

		check(vkEnumerateDeviceExtensionProperties(physical_dev, nullptr, &extension_cnt, available_dev_extensions.data()));

		all_supported = true;

		for (const auto& req : required_device_extensions)
		{
			bool is_available = false;

			for (const auto& avl : available_dev_extensions)
				if (!strcmp(req, avl.extensionName))
				{
					is_available = true;
					break;
				}

			if (!is_available)
			{
				all_supported = false;
				return {};
			}
		}

		return {};
	}

	err_info create_vk_logical_device()
	{
		queue_family_indices family_indices;

		check(query_queue_families(vk_physical_device, vk_surface, family_indices));

		float graphics_queue_priority = 1.0F;

		VkDeviceQueueCreateInfo queue_infos[2]{};
		queue_infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_infos[0].queueFamilyIndex = family_indices.graphics_idx;
		queue_infos[0].queueCount = 1;
		queue_infos[0].pQueuePriorities = &graphics_queue_priority;
		queue_infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_infos[1].queueFamilyIndex = family_indices.present_idx;
		queue_infos[1].queueCount = 1;
		queue_infos[1].pQueuePriorities = &graphics_queue_priority;

		VkPhysicalDeviceFeatures enabled_dev_features{};
		enabled_dev_features.samplerAnisotropy = VK_TRUE;

		VkDeviceCreateInfo dev_info{};
		dev_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		dev_info.pQueueCreateInfos = queue_infos;
		dev_info.queueCreateInfoCount = 1 + family_indices.discrete_present_family();
		dev_info.pEnabledFeatures = &enabled_dev_features;
		dev_info.ppEnabledExtensionNames = required_device_extensions;
		dev_info.enabledExtensionCount = sizeof(required_device_extensions) / sizeof(*required_device_extensions);
#ifdef OCH_VALIDATE
		dev_info.ppEnabledLayerNames = required_validation_layers;
		dev_info.enabledLayerCount = sizeof(required_validation_layers) / sizeof(*required_validation_layers);
#else
		dev_info.ppEnabledLayerNames = nullptr;
		dev_info.enabledLayerCount = 0;
#endif // OCH_VALIDATE

		check(vkCreateDevice(vk_physical_device, &dev_info, nullptr, &vk_device));

		vkGetDeviceQueue(vk_device, family_indices.graphics_idx, 0, &vk_graphics_queue);

		vkGetDeviceQueue(vk_device, family_indices.present_idx, 0, &vk_present_queue);

		return {};
	}

	err_info create_vk_swapchain()
	{
		swapchain_support_details swapchain_details;

		check(query_swapchain_support(vk_physical_device, vk_surface, swapchain_details));

		VkSurfaceFormatKHR chosen_format = swapchain_details.formats[0];

		for (const auto& format : swapchain_details.formats)
			if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
			{
				chosen_format = format;

				break;
			}

		VkPresentModeKHR chosen_present_mode = VK_PRESENT_MODE_FIFO_KHR;

		for(const auto& present_mode : swapchain_details.present_modes)
			if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
			{
				chosen_present_mode = present_mode;

				break;
			}

		VkExtent2D chosen_extent = swapchain_details.capabilites.currentExtent;

		if (swapchain_details.capabilites.currentExtent.width == ~0u)
		{
			uint32_t width, height;

			glfwGetFramebufferSize(window, reinterpret_cast<int*>(&width), reinterpret_cast<int*>(&height));

			if (width > swapchain_details.capabilites.maxImageExtent.width)
				width = swapchain_details.capabilites.maxImageExtent.width;
			else if (width < swapchain_details.capabilites.minImageExtent.width)
				width = swapchain_details.capabilites.minImageExtent.width;

			if (height > swapchain_details.capabilites.maxImageExtent.height)
				height = swapchain_details.capabilites.maxImageExtent.height;
			else if (height < swapchain_details.capabilites.minImageExtent.height)
				height = swapchain_details.capabilites.minImageExtent.height;

			chosen_extent.width = static_cast<uint32_t>(width);
			chosen_extent.height = static_cast<uint32_t>(height);
		}

		och::print("width: {}; height: {}\n\n", chosen_extent.width, chosen_extent.height);

		uint32_t chosen_image_count = swapchain_details.capabilites.minImageCount + 1;

		if (swapchain_details.capabilites.maxImageCount && chosen_image_count > swapchain_details.capabilites.maxImageCount)
			chosen_image_count = swapchain_details.capabilites.maxImageCount;

		VkSwapchainCreateInfoKHR info{};

		info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		info.imageFormat = chosen_format.format;
		info.imageColorSpace = chosen_format.colorSpace;
		info.presentMode = chosen_present_mode;
		info.imageExtent = chosen_extent;
		info.minImageCount = chosen_image_count;
		info.surface = vk_surface;
		info.imageArrayLayers = 1;
		info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		info.preTransform = swapchain_details.capabilites.currentTransform;
		info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		info.clipped = VK_TRUE;
		info.oldSwapchain = nullptr;

		queue_family_indices family_indices;

		check(query_queue_families(vk_physical_device, vk_surface, family_indices));

		if (family_indices.discrete_present_family())
		{
			uint32_t queue_indices[]{ family_indices.graphics_idx, family_indices.present_idx };

			info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
			info.pQueueFamilyIndices = queue_indices;
			info.queueFamilyIndexCount = sizeof(queue_indices) / sizeof(*queue_indices);
		}
		else
			info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		
		check(vkCreateSwapchainKHR(vk_device, &info, nullptr, &vk_swapchain));

		uint32_t swapchain_image_cnt;

		check(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_cnt, nullptr));

		vk_swapchain_images.resize(swapchain_image_cnt);

		check(vkGetSwapchainImagesKHR(vk_device, vk_swapchain, &swapchain_image_cnt, vk_swapchain_images.data()));

		vk_swapchain_format = chosen_format.format;

		vk_swapchain_extent = chosen_extent;

		return {};
	}

	err_info get_vk_swapchain_views()
	{
		vk_swapchain_views.resize(vk_swapchain_images.size());

		for (uint32_t i = 0; i != static_cast<uint32_t>(vk_swapchain_images.size()); ++i)
			check(allocate_image_view(vk_swapchain_images[i], vk_swapchain_format, VK_IMAGE_ASPECT_COLOR_BIT, vk_swapchain_views[i]));

		return {};
	}

	err_info create_vk_render_pass()
	{
		VkAttachmentDescription color_attachment{};
		color_attachment.format = vk_swapchain_format;
		color_attachment.samples = vk_msaa_samples;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription depth_attachment{};
		depth_attachment.format = VK_FORMAT_D32_SFLOAT;
		depth_attachment.samples = vk_msaa_samples;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentDescription color_attachment_resolve{};
		color_attachment_resolve.format = vk_swapchain_format;
		color_attachment_resolve.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment_resolve.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment_resolve.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment_resolve.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment_resolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment_resolve.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment_resolve.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentReference color_ref{};
		color_ref.attachment = 0;
		color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_ref{};
		depth_ref.attachment = 1;
		depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference color_resolve_ref{};
		color_resolve_ref.attachment = 2;
		color_resolve_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_ref;
		subpass.pDepthStencilAttachment = &depth_ref;
		subpass.pResolveAttachments = &color_resolve_ref;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachment_descs[]{ color_attachment, depth_attachment, color_attachment_resolve };

		VkRenderPassCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		create_info.attachmentCount = static_cast<uint32_t>(sizeof(attachment_descs) / sizeof(*attachment_descs));
		create_info.pAttachments = attachment_descs;
		create_info.subpassCount = 1;
		create_info.pSubpasses = &subpass;
		create_info.dependencyCount = 1;
		create_info.pDependencies = &dependency;

		check(vkCreateRenderPass(vk_device, &create_info, nullptr, &vk_render_pass));

		return {};
	}

	err_info create_vk_descriptor_set_layout()
	{
		VkDescriptorSetLayoutBinding ubo_layout_binding{};
		ubo_layout_binding.binding = 0;
		ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		ubo_layout_binding.descriptorCount = 1;
		ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
		ubo_layout_binding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding sampler_layout_binding{};
		sampler_layout_binding.binding = 1;
		sampler_layout_binding.descriptorCount = 1;
		sampler_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
		sampler_layout_binding.pImmutableSamplers = nullptr;

		VkDescriptorSetLayoutBinding bindings[]{ ubo_layout_binding, sampler_layout_binding };

		VkDescriptorSetLayoutCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		create_info.bindingCount = static_cast<uint32_t>(sizeof(bindings) / sizeof(*bindings));
		create_info.pBindings = bindings;

		check(vkCreateDescriptorSetLayout(vk_device, &create_info, nullptr, &vk_descriptor_set_layout));

		return {};
	}

	err_info create_vk_graphics_pipeline()
	{
		VkShaderModule vert_shader_module;

		check(create_shader_module_from_file("shaders/vert.spv", vert_shader_module));

		VkShaderModule frag_shader_module;

		check(create_shader_module_from_file("shaders/frag.spv", frag_shader_module));

		VkPipelineShaderStageCreateInfo shader_info[2]{};
		shader_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_info[0].module = vert_shader_module;
		shader_info[0].pName = "main";

		shader_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_info[1].module = frag_shader_module;
		shader_info[1].pName = "main";
		
		VkPipelineVertexInputStateCreateInfo vert_input_info{};
		vert_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vert_input_info.vertexBindingDescriptionCount = 1;
		vert_input_info.pVertexBindingDescriptions = &vertex::binding_desc;
		vert_input_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(sizeof(vertex::attribute_descs) / sizeof(*vertex::attribute_descs));
		vert_input_info.pVertexAttributeDescriptions = vertex::attribute_descs;

		VkPipelineInputAssemblyStateCreateInfo input_asm_info{};
		input_asm_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_asm_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_asm_info.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = static_cast<float>(vk_swapchain_extent.width);
		viewport.height = static_cast<float>(vk_swapchain_extent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = vk_swapchain_extent;

		VkPipelineViewportStateCreateInfo view_info{};
		view_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		view_info.viewportCount = 1;
		view_info.pViewports = &viewport;
		view_info.scissorCount = 1;
		view_info.pScissors = &scissor;

		VkPipelineRasterizationStateCreateInfo raster_info{};
		raster_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		raster_info.depthClampEnable = VK_FALSE;
		raster_info.rasterizerDiscardEnable = VK_FALSE;
		raster_info.polygonMode = VK_POLYGON_MODE_FILL;
		raster_info.lineWidth = 1.0F;
		raster_info.cullMode = VK_CULL_MODE_BACK_BIT;
		raster_info.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		raster_info.depthBiasEnable = VK_FALSE;
		raster_info.depthBiasConstantFactor = 0.0F;
		raster_info.depthBiasClamp = 0.0F;
		raster_info.depthBiasSlopeFactor = 0.0F;

		VkPipelineMultisampleStateCreateInfo multisample_info{};
		multisample_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_info.sampleShadingEnable = VK_FALSE;
		multisample_info.rasterizationSamples = vk_msaa_samples;
		multisample_info.minSampleShading = 1.0F;
		multisample_info.pSampleMask = nullptr;
		multisample_info.alphaToCoverageEnable = VK_FALSE;
		multisample_info.alphaToOneEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState blend_attachment{};
		blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
		blend_attachment.blendEnable = VK_FALSE;
		blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;

		VkPipelineColorBlendStateCreateInfo blend_info{};
		blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		blend_info.logicOpEnable = VK_FALSE;
		blend_info.logicOp = VK_LOGIC_OP_COPY;
		blend_info.attachmentCount = 1;
		blend_info.pAttachments = &blend_attachment;
		blend_info.blendConstants[0] = 0.0F;
		blend_info.blendConstants[1] = 0.0F;
		blend_info.blendConstants[2] = 0.0F;
		blend_info.blendConstants[3] = 0.0F;

		VkDynamicState dynamic_states[]{ VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };
		VkPipelineDynamicStateCreateInfo dynamic_info{};
		dynamic_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
		dynamic_info.dynamicStateCount = sizeof(dynamic_states) / sizeof(*dynamic_states);
		dynamic_info.pDynamicStates = dynamic_states;

		VkPipelineDepthStencilStateCreateInfo depth_stencil_info{};
		depth_stencil_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
		depth_stencil_info.depthTestEnable = VK_TRUE;
		depth_stencil_info.depthWriteEnable = VK_TRUE;
		depth_stencil_info.depthCompareOp = VK_COMPARE_OP_LESS;
		depth_stencil_info.depthBoundsTestEnable = VK_FALSE;
		depth_stencil_info.minDepthBounds = 0.0F;
		depth_stencil_info.maxDepthBounds = 1.0F;
		depth_stencil_info.stencilTestEnable = VK_FALSE;
		depth_stencil_info.front = {};
		depth_stencil_info.back = {};

		VkPipelineLayoutCreateInfo layout_info{};
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_info.setLayoutCount = 1;
		layout_info.pSetLayouts = &vk_descriptor_set_layout;
		layout_info.pushConstantRangeCount = 0;
		layout_info.pPushConstantRanges = nullptr;

		check(vkCreatePipelineLayout(vk_device, &layout_info, nullptr, &vk_pipeline_layout));

		VkGraphicsPipelineCreateInfo pipeline_info{};
		pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_info.stageCount = 2;
		pipeline_info.pStages = shader_info;
		pipeline_info.pVertexInputState = &vert_input_info;
		pipeline_info.pInputAssemblyState = &input_asm_info;
		pipeline_info.pTessellationState = nullptr;
		pipeline_info.pViewportState = &view_info;
		pipeline_info.pRasterizationState = &raster_info;
		pipeline_info.pMultisampleState = &multisample_info;
		pipeline_info.pDepthStencilState = &depth_stencil_info;
		pipeline_info.pColorBlendState = &blend_info;
		pipeline_info.pDynamicState = nullptr;
		pipeline_info.layout = vk_pipeline_layout;
		pipeline_info.renderPass = vk_render_pass;
		pipeline_info.subpass = 0;
		pipeline_info.basePipelineHandle = nullptr;
		pipeline_info.basePipelineIndex = -1;

		check(vkCreateGraphicsPipelines(vk_device, nullptr, 1, &pipeline_info, nullptr, &vk_graphics_pipeline));

		vkDestroyShaderModule(vk_device, vert_shader_module, nullptr);

		vkDestroyShaderModule(vk_device, frag_shader_module, nullptr);

		return {};
	}
	
	err_info create_vk_command_pool()
	{
		queue_family_indices family_indices;

		check(query_queue_families(vk_physical_device, vk_surface, family_indices));

		VkCommandPoolCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		create_info.queueFamilyIndex = family_indices.graphics_idx;
		
		check(vkCreateCommandPool(vk_device, &create_info, nullptr, &vk_command_pool));

		return {};
	}

	err_info create_vk_colour_resources()
	{
		VkFormat colour_format = vk_swapchain_format;

		check(allocate_image(vk_swapchain_extent.width, vk_swapchain_extent.height, colour_format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_colour_image, vk_colour_image_memory, vk_msaa_samples));

		check(allocate_image_view(vk_colour_image, colour_format, VK_IMAGE_ASPECT_COLOR_BIT, vk_colour_image_view));

		return {};
	}

	err_info create_vk_depth_resources()
	{
		check(allocate_image(vk_swapchain_extent.width, vk_swapchain_extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_depth_image, vk_depth_image_memory, vk_msaa_samples));

		check(allocate_image_view(vk_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, vk_depth_image_view));

		check(transition_image_layout(vk_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

		return {};
	}

	err_info create_vk_swapchain_framebuffers()
	{
		vk_swapchain_framebuffers.resize(vk_swapchain_views.size());

		for (size_t i = 0; i != vk_swapchain_views.size(); ++i)
		{
			VkImageView attachments[]{ vk_colour_image_view, vk_depth_image_view, vk_swapchain_views[i] };

			VkFramebufferCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			create_info.renderPass = vk_render_pass;
			create_info.attachmentCount = static_cast<uint32_t>(sizeof(attachments) / sizeof(*attachments));
			create_info.pAttachments = attachments;
			create_info.width = vk_swapchain_extent.width;
			create_info.height = vk_swapchain_extent.height;
			create_info.layers = 1;

			check(vkCreateFramebuffer(vk_device, &create_info, nullptr, &vk_swapchain_framebuffers[i]));
		}

		return {};
	}

	err_info create_vk_texture_image()
	{
		och::mapped_file<bitmap_header> texture_file(och::stringview("textures/" OCH_ASSET_NAME ".bmp"), och::fio::access_read, och::fio::open_normal, och::fio::open_fail);

		if (!texture_file)
			return MAKE_ERROR(1);

		const bitmap_header& header = texture_file[0];

		const uint8_t* pixels = header.pixel_data();

		const size_t pixel_cnt = static_cast<size_t>(header.width * header.height);

		if (header.bits_per_pixel == 24)
		{
			uint8_t* with_alpha = new uint8_t[pixel_cnt * 4];

			for (size_t i = 0; i != pixel_cnt; ++i)
			{
				for (size_t j = 0; j != 3; ++j)
					with_alpha[i * 4 + j] = pixels[i * 3 + j];

				with_alpha[i * 4 + 3] = 0xFF;
			}

			pixels = with_alpha;
		}
		else if (header.bits_per_pixel != 32)
			return MAKE_ERROR(1);

		VkBuffer staging_buf;
		VkDeviceMemory staging_buf_mem;

		uint32_t img_sz = static_cast<uint32_t>(header.height > header.width ? header.height : header.width);

		uint32_t mip_levels = 0;

		while (img_sz)
		{
			img_sz >>= 1;
			++mip_levels;
		}

		vk_texture_image_mipmap_levels = mip_levels;

		check(allocate_buffer(pixel_cnt * 4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_buf_mem));

		void* data;

		check(vkMapMemory(vk_device, staging_buf_mem, 0, pixel_cnt * 4, 0, &data));

		memcpy(data, pixels, pixel_cnt * 4);

		vkUnmapMemory(vk_device, staging_buf_mem);

		if (header.bits_per_pixel == 24)
			delete[] pixels;

		check(allocate_image(header.width, header.height, VK_FORMAT_B8G8R8A8_SRGB, 
			                 VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
			                 VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_texture_image, vk_texture_image_memory, VK_SAMPLE_COUNT_1_BIT, mip_levels));


		check(transition_image_layout(vk_texture_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels));

		check(copy_buffer_to_image(vk_texture_image, staging_buf, header.width, header.height));

		check(generate_mipmap(vk_texture_image, VK_FORMAT_B8G8R8A8_SRGB, header.width, header.height, mip_levels));

		vkDestroyBuffer(vk_device, staging_buf, nullptr);

		vkFreeMemory(vk_device, staging_buf_mem, nullptr);

		return {};
	}

	err_info create_vk_texture_image_view()
	{
		check(allocate_image_view(vk_texture_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, vk_texture_image_view, vk_texture_image_mipmap_levels));

		return {};
	}

	err_info create_vk_texture_sampler()
	{
		VkPhysicalDeviceProperties dev_props{};

		vkGetPhysicalDeviceProperties(vk_physical_device, &dev_props);

		VkSamplerCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		create_info.anisotropyEnable = VK_TRUE;
		create_info.maxAnisotropy = dev_props.limits.maxSamplerAnisotropy;
		create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		create_info.unnormalizedCoordinates = VK_FALSE;
		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_ALWAYS;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.mipLodBias = 0.0F;
		create_info.minLod = 0.0F;
		create_info.maxLod = static_cast<float>(vk_texture_image_mipmap_levels);

		check(vkCreateSampler(vk_device, &create_info, nullptr, &vk_texture_sampler));

		return {};
	}

	err_info load_obj_model()
	{
		tinyobj::attrib_t attrib;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;
		std::string warn, err;

		if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "models/" OCH_ASSET_NAME ".obj"))
		{
			och::print("Failed to load .obj File:\n\tWarning: {}\n\tError: {}\n", warn.c_str(), err.c_str());

			return MAKE_ERROR(1);
		}

		std::unordered_map<vertex, uint32_t> uniqueVertices{};

		for (const auto& shape : shapes)
		{
			for (const auto& index : shape.mesh.indices)
			{
				vertex vert{};

				vert.pos = {
					attrib.vertices[3 * index.vertex_index + 0],
					attrib.vertices[3 * index.vertex_index + 1],
					attrib.vertices[3 * index.vertex_index + 2],
				};

				vert.tex_pos = {
					attrib.texcoords[2 * index.texcoord_index + 0],
					attrib.texcoords[2 * index.texcoord_index + 1],
				};

				vert.col = { 1.0f, 1.0f, 1.0f };

				if (uniqueVertices.count(vert) == 0)
				{
					uniqueVertices[vert] = static_cast<uint32_t>(vertices.size());

					vertices.push_back(vert);
				}
				
				indices.push_back(uniqueVertices[vert]);
			}
		}

		och::print("\tTotal number of vertices loaded: {}\n\tTotal number of indices loaded: {}\n\n", vertices.size(), indices.size());

		normalize_model(vertices, OCH_ASSET_OFFSET, OCH_ASSET_SCALE);

		return {};
	}

	err_info create_vk_vertex_buffer()
	{
		VkBuffer staging_buf = nullptr;

		VkDeviceMemory staging_buf_mem = nullptr;

		check(allocate_buffer(vertices.size() * sizeof(vertices[0]), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_buf_mem));

		void* staging_data = nullptr;

		check(vkMapMemory(vk_device, staging_buf_mem, 0, vertices.size() * sizeof(vertices[0]), 0, &staging_data));

		memcpy(staging_data, vertices.data(), vertices.size() * sizeof(vertices[0]));

		vkUnmapMemory(vk_device, staging_buf_mem);

		check(allocate_buffer(vertices.size() * sizeof(vertices[0]), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_vertex_buffer, vk_vertex_buffer_memory));

		copy_buffer_to_buffer(vk_vertex_buffer, staging_buf, vertices.size() * sizeof(vertices[0]));

		vkDestroyBuffer(vk_device, staging_buf, nullptr);

		vkFreeMemory(vk_device, staging_buf_mem, nullptr);

		return {};
	}

	err_info create_vk_index_buffer()
	{
		VkBuffer staging_buf = nullptr;

		VkDeviceMemory staging_buf_mem = nullptr;

		check(allocate_buffer(indices.size() * sizeof(indices[0]), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging_buf, staging_buf_mem));

		void* data = nullptr;

		check(vkMapMemory(vk_device, staging_buf_mem, 0, indices.size() * sizeof(indices[0]), 0, &data));

		memcpy(data, indices.data(), indices.size() * sizeof(indices[0]));

		vkUnmapMemory(vk_device, staging_buf_mem);

		check(allocate_buffer(indices.size() * sizeof(indices[0]), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_index_buffer, vk_index_buffer_memory));

		check(copy_buffer_to_buffer(vk_index_buffer, staging_buf, indices.size() * sizeof(indices[0])));

		vkDestroyBuffer(vk_device, staging_buf, nullptr);

		vkFreeMemory(vk_device, staging_buf_mem, nullptr);

		return {};
	}

	err_info create_vk_uniform_buffers()
	{
		vk_uniform_buffers.resize(vk_swapchain_images.size());
		vk_uniform_buffers_memory.resize(vk_swapchain_images.size());

		for (size_t i = 0; i != vk_swapchain_images.size(); ++i)
			check(allocate_buffer(sizeof(uniform_buffer_obj), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vk_uniform_buffers[i], vk_uniform_buffers_memory[i]));

		return {};
	}

	err_info create_vk_descriptor_pool()
	{
		VkDescriptorPoolSize pool_sizes[]{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, static_cast<uint32_t>(vk_swapchain_images.size())},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, static_cast<uint32_t>(vk_swapchain_images.size())},
		};

		VkDescriptorPoolCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		create_info.poolSizeCount = static_cast<uint32_t>(sizeof(pool_sizes) / sizeof(*pool_sizes));
		create_info.pPoolSizes = pool_sizes;
		create_info.maxSets = static_cast<uint32_t>(vk_swapchain_images.size());

		check(vkCreateDescriptorPool(vk_device, &create_info, nullptr, &vk_descriptor_pool));

		return {};
	}

	err_info create_vk_descriptor_sets()
	{
		std::vector<VkDescriptorSetLayout> desc_set_layouts(vk_swapchain_images.size(), vk_descriptor_set_layout);

		VkDescriptorSetAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = vk_descriptor_pool;
		alloc_info.descriptorSetCount = static_cast<uint32_t>(desc_set_layouts.size());
		alloc_info.pSetLayouts = desc_set_layouts.data();

		vk_descriptor_sets.resize(desc_set_layouts.size());

		check(vkAllocateDescriptorSets(vk_device, &alloc_info, vk_descriptor_sets.data()));

		for (size_t i = 0; i != vk_descriptor_sets.size(); ++i)
		{
			VkDescriptorBufferInfo buf_info{};
			buf_info.buffer = vk_uniform_buffers[i];
			buf_info.offset = 0;
			buf_info.range = sizeof(uniform_buffer_obj);

			VkDescriptorImageInfo img_info{};
			img_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			img_info.imageView = vk_texture_image_view;
			img_info.sampler = vk_texture_sampler;

			VkWriteDescriptorSet ubo_write{};
			ubo_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			ubo_write.dstSet = vk_descriptor_sets[i];
			ubo_write.dstBinding = 0;
			ubo_write.dstArrayElement = 0;
			ubo_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
			ubo_write.descriptorCount = 1;
			ubo_write.pBufferInfo = &buf_info;
			ubo_write.pImageInfo = nullptr;
			ubo_write.pTexelBufferView = nullptr;

			VkWriteDescriptorSet sampler_write{};
			sampler_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			sampler_write.dstSet = vk_descriptor_sets[i];
			sampler_write.dstBinding = 1;
			sampler_write.dstArrayElement = 0;
			sampler_write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			sampler_write.descriptorCount = 1;
			sampler_write.pBufferInfo = nullptr;
			sampler_write.pImageInfo = &img_info;
			sampler_write.pTexelBufferView = nullptr;

			VkWriteDescriptorSet writes[]{ ubo_write, sampler_write };

			vkUpdateDescriptorSets(vk_device, static_cast<uint32_t>(sizeof(writes) / sizeof(*writes)), writes, 0, nullptr);
		}

		return{};
	}

	err_info create_vk_command_buffers()
	{
		vk_command_buffers.resize(vk_swapchain_views.size());

		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = vk_command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = static_cast<uint32_t>(vk_command_buffers.size());

		check(vkAllocateCommandBuffers(vk_device, &alloc_info, vk_command_buffers.data()));

		for (size_t i = 0; i != vk_command_buffers.size(); ++i)
		{
			VkCommandBufferBeginInfo buffer_beg_info{};
			buffer_beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			buffer_beg_info.flags = 0;
			buffer_beg_info.pInheritanceInfo = nullptr;
			
			check(vkBeginCommandBuffer(vk_command_buffers[i], &buffer_beg_info));

			VkClearValue clear_values[]{ {0.0F, 0.0F, 0.0F, 1.0F}, {1.0F, 0.0F, 0.0F, 0.0F} };

			VkRenderPassBeginInfo pass_beg_info{};
			pass_beg_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			pass_beg_info.renderPass = vk_render_pass;
			pass_beg_info.framebuffer = vk_swapchain_framebuffers[i];
			pass_beg_info.renderArea.offset = { 0, 0 };
			pass_beg_info.renderArea.extent = vk_swapchain_extent;
			pass_beg_info.clearValueCount = static_cast<uint32_t>(sizeof(clear_values) / sizeof(*clear_values));
			pass_beg_info.pClearValues = clear_values;

			vkCmdBeginRenderPass(vk_command_buffers[i], &pass_beg_info, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(vk_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_graphics_pipeline);
				
				VkDeviceSize offsets[]{ 0 };
				vkCmdBindVertexBuffers(vk_command_buffers[i], 0, 1, &vk_vertex_buffer, offsets);

				vkCmdBindIndexBuffer(vk_command_buffers[i], vk_index_buffer, 0, VK_INDEX_TYPE_UINT32);

				vkCmdBindDescriptorSets(vk_command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline_layout, 0, 1, &vk_descriptor_sets[i], 0, nullptr);

				vkCmdDrawIndexed(vk_command_buffers[i], static_cast<uint32_t>(indices.size()), 1, 0, 0, 0);

			vkCmdEndRenderPass(vk_command_buffers[i]);

			check(vkEndCommandBuffer(vk_command_buffers[i]));
		}

		return {};
	}

	err_info create_vk_sync_objects()
	{
		vk_images_inflight_fences.resize(vk_swapchain_images.size(), nullptr);

		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (uint32_t i = 0; i != max_frames_in_flight; ++i)
		{
			check(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &vk_image_available_semaphores[i]));

			check(vkCreateSemaphore(vk_device, &semaphore_info, nullptr, &vk_render_complete_semaphores[i]));

			check(vkCreateFence(vk_device, &fence_info, nullptr, &vk_inflight_fences[i]));
		}

		return {};
	}

	err_info main_loop()
	{
		while (!glfwWindowShouldClose(window))
		{
			check(draw_frame());

			glfwPollEvents();
		}

		check(vkDeviceWaitIdle(vk_device));

		return {};
	}

	err_info draw_frame()
	{
		check(vkWaitForFences(vk_device, 1, &vk_inflight_fences[curr_frame], VK_FALSE, UINT64_MAX));

		uint32_t image_idx;

		if (VkResult acquire_rst = vkAcquireNextImageKHR(vk_device, vk_swapchain, UINT64_MAX, vk_image_available_semaphores[curr_frame], nullptr, &image_idx); acquire_rst == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreate_swapchain();
			return {};
		}
		else if(acquire_rst != VK_SUBOPTIMAL_KHR)
			check(acquire_rst);

		if (vk_images_inflight_fences[image_idx])
			check(vkWaitForFences(vk_device, 1, &vk_images_inflight_fences[image_idx], VK_FALSE, UINT64_MAX));

		vk_images_inflight_fences[image_idx] = vk_inflight_fences[curr_frame];

		VkSemaphore wait_semaphores[]{ vk_image_available_semaphores[curr_frame] };

		VkSemaphore signal_semaphores[]{ vk_render_complete_semaphores[curr_frame] };

		VkPipelineStageFlags wait_stages[]{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		update_uniforms(image_idx);

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = wait_semaphores;
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &vk_command_buffers[image_idx];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = signal_semaphores;

		check(vkResetFences(vk_device, 1, &vk_inflight_fences[curr_frame]));

		check(vkQueueSubmit(vk_graphics_queue, 1, &submit_info, vk_inflight_fences[curr_frame]));

		VkSwapchainKHR present_swapchains[]{ vk_swapchain };

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = signal_semaphores;
		present_info.swapchainCount = 1;
		present_info.pSwapchains = present_swapchains;
		present_info.pImageIndices = &image_idx;
		present_info.pResults = nullptr;

		if (VkResult present_rst = vkQueuePresentKHR(vk_present_queue, &present_info); present_rst == VK_ERROR_OUT_OF_DATE_KHR || present_rst == VK_SUBOPTIMAL_KHR || framebuffer_resized)
		{
			framebuffer_resized = false;
			recreate_swapchain();
		}
		else
			check(present_rst);

		curr_frame = (curr_frame + 1) % max_frames_in_flight;

		return {};
	}

	err_info recreate_swapchain()
	{
		cleanup_swapchain();

		check(vkDeviceWaitIdle(vk_device));

		check(create_vk_swapchain());

		check(create_vk_depth_resources());

		check(create_vk_colour_resources());

		check(get_vk_swapchain_views());

		check(create_vk_render_pass());

		check(create_vk_graphics_pipeline());

		check(create_vk_swapchain_framebuffers());

		check(create_vk_uniform_buffers());

		check(create_vk_descriptor_pool());

		check(create_vk_descriptor_sets());

		check(create_vk_command_buffers());

		return {};
	}

	void cleanup_swapchain()
	{
		int width, height;
		
		glfwGetFramebufferSize(window, &width, &height);
		
		while (!width || !height)
		{
			glfwWaitEvents();

			glfwGetFramebufferSize(window, &width, &height);
		}

		vkDeviceWaitIdle(vk_device);

		vkDestroyImageView(vk_device, vk_colour_image_view, nullptr);

		vkDestroyImage(vk_device, vk_colour_image, nullptr);

		vkFreeMemory(vk_device, vk_colour_image_memory, nullptr);

		for (auto& framebuffer : vk_swapchain_framebuffers)
			vkDestroyFramebuffer(vk_device, framebuffer, nullptr);

		vkFreeCommandBuffers(vk_device, vk_command_pool, static_cast<uint32_t>(vk_command_buffers.size()), vk_command_buffers.data());

		vkDestroyPipeline(vk_device, vk_graphics_pipeline, nullptr);

		vkDestroyPipelineLayout(vk_device, vk_pipeline_layout, nullptr);

		vkDestroyRenderPass(vk_device, vk_render_pass, nullptr);

		for (auto& view : vk_swapchain_views)
			vkDestroyImageView(vk_device, view, nullptr);

		vkDestroySwapchainKHR(vk_device, vk_swapchain, nullptr);

		for (auto& buffer : vk_uniform_buffers)
			vkDestroyBuffer(vk_device, buffer, nullptr);

		for (auto& memory : vk_uniform_buffers_memory)
			vkFreeMemory(vk_device, memory, nullptr);

		vkDestroyDescriptorPool(vk_device, vk_descriptor_pool, nullptr);

		vkDestroyImageView(vk_device, vk_depth_image_view, nullptr);

		vkDestroyImage(vk_device, vk_depth_image, nullptr);

		vkFreeMemory(vk_device, vk_depth_image_memory, nullptr);
	}

	void cleanup()
	{
		cleanup_swapchain();

		vkDestroySampler(vk_device, vk_texture_sampler, nullptr);

		vkDestroyImageView(vk_device, vk_texture_image_view, nullptr);

		vkDestroyImage(vk_device, vk_texture_image, nullptr);

		vkFreeMemory(vk_device, vk_texture_image_memory, nullptr);

		vkDestroyDescriptorSetLayout(vk_device, vk_descriptor_set_layout, nullptr);

		vkDestroyBuffer(vk_device, vk_index_buffer, nullptr);

		vkFreeMemory(vk_device, vk_index_buffer_memory, nullptr);

		vkDestroyBuffer(vk_device, vk_vertex_buffer, nullptr);

		vkFreeMemory(vk_device, vk_vertex_buffer_memory, nullptr);

		for(auto& sem : vk_render_complete_semaphores)
			vkDestroySemaphore(vk_device, sem, nullptr);

		for (auto& sem : vk_image_available_semaphores)
			vkDestroySemaphore(vk_device, sem, nullptr);

		for (auto& fence : vk_inflight_fences)
			vkDestroyFence(vk_device, fence, nullptr);

		vkDestroyCommandPool(vk_device, vk_command_pool, nullptr);

		vkDestroyDevice(vk_device, nullptr);

		vkDestroySurfaceKHR(vk_instance, vk_surface, nullptr);

		// Destroy Vulkan debug utils messenger
		{
#ifdef OCH_VALIDATE

			auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(vk_instance, "vkDestroyDebugUtilsMessengerEXT"));

			if (destroy_fn)
				destroy_fn(vk_instance, vk_debug_messenger, nullptr);
			else
				och::print("\nERROR DURING CLEANUP: Could not load vkDestroyDebugUtilsMessengerEXT\n");

#endif // OCH_VALIDATE
		}

		vkDestroyInstance(vk_instance, nullptr);

		glfwDestroyWindow(window);

		glfwTerminate();
	}

	err_info find_first_supported_format(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags required_features, VkFormat& out_format)
	{
		for (auto format : candidates)
		{
			VkFormatProperties props;
			vkGetPhysicalDeviceFormatProperties(vk_physical_device, format, &props);

			if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & required_features) == required_features)
			{
				out_format = format;

				return {};
			}
			else if (tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & required_features) == required_features)
			{
				out_format = format;

				return {};
			}
		}

		return MAKE_ERROR(1);
	}

	err_info create_shader_module_from_file(const char* filename, VkShaderModule& out_shader_module)
	{
		och::mapped_file<uint8_t> shader_file(filename, och::fio::access_read, och::fio::open_normal, och::fio::open_fail);

		if (!shader_file)
			return MAKE_ERROR(1);

		VkShaderModuleCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = shader_file.bytes;
		create_info.pCode = reinterpret_cast<const uint32_t*>(shader_file.get_data().beg);

		check(vkCreateShaderModule(vk_device, &create_info, nullptr, &out_shader_module));

		return {};
	}

	err_info query_queue_families(VkPhysicalDevice physical_dev, VkSurfaceKHR surface, queue_family_indices& out_indices)
	{
		uint32_t queue_family_cnt;

		vkGetPhysicalDeviceQueueFamilyProperties(physical_dev, &queue_family_cnt, nullptr);

		std::vector<VkQueueFamilyProperties> available(queue_family_cnt);

		vkGetPhysicalDeviceQueueFamilyProperties(physical_dev, &queue_family_cnt, available.data());

		bool has_graphics_and_present_in_one = false;

		for (uint32_t i = 0; i != queue_family_cnt; ++i)
		{
			VkQueueFamilyProperties& avl = available[i];

			VkBool32 supports_present;

			check(vkGetPhysicalDeviceSurfaceSupportKHR(physical_dev, i, surface, &supports_present));

			if (avl.queueFlags & VK_QUEUE_GRAPHICS_BIT && !has_graphics_and_present_in_one)
			{
				out_indices.graphics_idx = i;

				if (supports_present)
				{
					out_indices.present_idx = i;

					has_graphics_and_present_in_one = true;
				}
			}

			if (avl.queueFlags & VK_QUEUE_COMPUTE_BIT)
				out_indices.compute_idx = i;

			if (avl.queueFlags & VK_QUEUE_TRANSFER_BIT)
				out_indices.transfer_idx = i;

			if (supports_present && !has_graphics_and_present_in_one)
				out_indices.present_idx = i;
		}

		return {};
	}

	err_info query_swapchain_support(VkPhysicalDevice physical_dev, VkSurfaceKHR surface, swapchain_support_details& out_support_details)
	 {
		 check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_dev, surface, &out_support_details.capabilites));

		 uint32_t format_cnt;

		 check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_dev, surface, &format_cnt, nullptr));

		 out_support_details.formats.resize(format_cnt);

		 check(vkGetPhysicalDeviceSurfaceFormatsKHR(physical_dev, surface, &format_cnt, out_support_details.formats.data()));

		 uint32_t present_mode_cnt;

		 check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_dev, surface, &present_mode_cnt, nullptr));

		 out_support_details.present_modes.resize(present_mode_cnt);

		 check(vkGetPhysicalDeviceSurfacePresentModesKHR(physical_dev, surface, &present_mode_cnt, out_support_details.present_modes.data()));

		 return {};
	 }

	err_info query_memory_type_index(uint32_t type_filter, VkMemoryPropertyFlags properties, uint32_t& out_type_index)
	{
		VkPhysicalDeviceMemoryProperties mem_props{};

		vkGetPhysicalDeviceMemoryProperties(vk_physical_device, &mem_props);

		for (uint32_t i = 0; i != mem_props.memoryTypeCount; ++i)
			if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
			{
				out_type_index = i;

				return{};
			}

		return MAKE_ERROR(1);
	}

	VkSampleCountFlagBits query_max_msaa_samples()
	{
		VkPhysicalDeviceProperties props;

		vkGetPhysicalDeviceProperties(vk_physical_device, &props);

		VkSampleCountFlags compat = props.limits.framebufferDepthSampleCounts & props.limits.framebufferColorSampleCounts;

		if (compat & VK_SAMPLE_COUNT_64_BIT)
			return VK_SAMPLE_COUNT_64_BIT;
		if (compat & VK_SAMPLE_COUNT_32_BIT)
			return VK_SAMPLE_COUNT_32_BIT;
		if (compat & VK_SAMPLE_COUNT_16_BIT)
			return VK_SAMPLE_COUNT_16_BIT;
		if (compat & VK_SAMPLE_COUNT_8_BIT)
			return VK_SAMPLE_COUNT_8_BIT;
		if (compat & VK_SAMPLE_COUNT_4_BIT)
			return VK_SAMPLE_COUNT_4_BIT;
		if (compat & VK_SAMPLE_COUNT_2_BIT)
			return VK_SAMPLE_COUNT_2_BIT;

		return VK_SAMPLE_COUNT_1_BIT;
	}

	err_info allocate_buffer(VkDeviceSize bytes, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkBuffer& out_buffer, VkDeviceMemory& out_buffer_memory, VkSharingMode share_mode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queue_family_cnt = 0, const uint32_t* queue_family_ptr = nullptr)
	{
		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = bytes;
		buffer_info.usage = usage_flags;
		buffer_info.sharingMode = share_mode;
		buffer_info.queueFamilyIndexCount = queue_family_cnt;
		buffer_info.pQueueFamilyIndices = queue_family_ptr;

		check(vkCreateBuffer(vk_device, &buffer_info, nullptr, &out_buffer));

		VkMemoryRequirements mem_reqs{};

		vkGetBufferMemoryRequirements(vk_device, out_buffer, &mem_reqs);

		uint32_t memory_type_index;

		check(query_memory_type_index(mem_reqs.memoryTypeBits, property_flags, memory_type_index));

		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_reqs.size;
		alloc_info.memoryTypeIndex = memory_type_index;

		check(vkAllocateMemory(vk_device, &alloc_info, nullptr, &out_buffer_memory));

		check(vkBindBufferMemory(vk_device, out_buffer, out_buffer_memory, 0ull));

		return {};
	}

	err_info allocate_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImage& out_image, VkDeviceMemory& out_image_memory, VkSampleCountFlagBits sample_cnt = VK_SAMPLE_COUNT_1_BIT, uint32_t mip_levels = 1, VkSharingMode share_mode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queue_family_cnt = 0, const uint32_t* queue_family_ptr = nullptr)
	{
		VkImageCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		create_info.imageType = VK_IMAGE_TYPE_2D;
		create_info.extent.width = width;
		create_info.extent.height = height;
		create_info.extent.depth = 1;
		create_info.mipLevels = mip_levels;
		create_info.arrayLayers = 1;
		create_info.format = format;
		create_info.tiling = tiling;
		create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		create_info.usage = usage_flags;
		create_info.samples = sample_cnt;
		create_info.flags = 0;
		create_info.sharingMode = share_mode;
		create_info.queueFamilyIndexCount = queue_family_cnt;
		create_info.pQueueFamilyIndices = queue_family_ptr;

		check(vkCreateImage(vk_device, &create_info, nullptr, &out_image));

		VkMemoryRequirements mem_reqs;

		vkGetImageMemoryRequirements(vk_device, out_image, &mem_reqs);

		uint32_t mem_type_idx;

		check(query_memory_type_index(mem_reqs.memoryTypeBits, property_flags, mem_type_idx));

		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_reqs.size;
		alloc_info.memoryTypeIndex = mem_type_idx;

		check(vkAllocateMemory(vk_device, &alloc_info, nullptr, &out_image_memory));

		check(vkBindImageMemory(vk_device, out_image, out_image_memory, 0));

		return {};
	}
	
	err_info allocate_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask, VkImageView& out_image_view, uint32_t mip_levels = 1)
	{
		VkImageViewCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		create_info.image = image;
		create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		create_info.format = format;
		create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
		create_info.subresourceRange.aspectMask = aspect_mask;
		create_info.subresourceRange.baseMipLevel = 0;
		create_info.subresourceRange.levelCount = mip_levels;
		create_info.subresourceRange.baseArrayLayer = 0;
		create_info.subresourceRange.layerCount = 1;
		
		check(vkCreateImageView(vk_device, &create_info, nullptr, &out_image_view));

		return {};
	}

	err_info copy_buffer_to_buffer(VkBuffer dst, VkBuffer src, VkDeviceSize size, VkDeviceSize dst_offset = 0, VkDeviceSize src_offset = 0)
	{
		VkCommandBuffer cmd_buffer;

		check(beg_single_command(cmd_buffer));

		VkBufferCopy copy_region{};
		copy_region.size = size;
		copy_region.dstOffset = dst_offset;
		copy_region.srcOffset = src_offset;

		vkCmdCopyBuffer(cmd_buffer, src, dst, 1, &copy_region);

		check(end_single_command(cmd_buffer));

		return {};
	}

	err_info copy_buffer_to_image(VkImage dst, VkBuffer src, uint32_t width, uint32_t height)
	{
		VkCommandBuffer cmd_buffer;

		check(beg_single_command(cmd_buffer));

		VkBufferImageCopy copy_region{};
		copy_region.bufferOffset = 0;
		copy_region.bufferRowLength = 0;
		copy_region.bufferImageHeight = 0;
		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageOffset = { 0, 0, 0 };
		copy_region.imageExtent = { width, height, 1 };

		vkCmdCopyBufferToImage(cmd_buffer, src, dst, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

		check(end_single_command(cmd_buffer));

		return {};
	}

	err_info transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t mip_levels = 1)
	{
		VkCommandBuffer transit_cmd_buffer;

		check(beg_single_command(transit_cmd_buffer));

		VkImageMemoryBarrier img_barrier{};
		img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		img_barrier.image = image;
		img_barrier.subresourceRange.aspectMask = format == VK_FORMAT_D32_SFLOAT ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;
		img_barrier.subresourceRange.baseMipLevel = 0;
		img_barrier.subresourceRange.levelCount = mip_levels;
		img_barrier.subresourceRange.baseArrayLayer = 0;
		img_barrier.subresourceRange.layerCount = 1;
		img_barrier.oldLayout = old_layout;
		img_barrier.newLayout = new_layout;
		img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		img_barrier.srcAccessMask = 0;
		img_barrier.dstAccessMask = 0;

		VkPipelineStageFlags src_stage;
		VkPipelineStageFlags dst_stage;

		if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			img_barrier.srcAccessMask = 0;
			img_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dst_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			img_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			img_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			src_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			dst_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		}
		else if (old_layout == VK_IMAGE_LAYOUT_UNDEFINED && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			img_barrier.srcAccessMask = 0;
			img_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			src_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
			dst_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		}
		else
			return MAKE_ERROR(1);

		vkCmdPipelineBarrier(transit_cmd_buffer, src_stage, dst_stage, 0, 0, nullptr, 0, nullptr, 1, &img_barrier);

		check(end_single_command(transit_cmd_buffer));

		return {};
	}

	err_info generate_mipmap(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mip_levels)
	{
		VkFormatProperties props;
		
		vkGetPhysicalDeviceFormatProperties(vk_physical_device, format, &props);

		if (!(props.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT))
			return MAKE_ERROR(1);

		VkCommandBuffer buf;

		check(beg_single_command(buf));

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = image;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mip_width = width;
		int32_t mip_height = height;

		for (uint32_t i = 0; i != mip_levels - 1; ++i)
		{
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.subresourceRange.baseMipLevel = i;

			vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			VkImageBlit blit{};
			blit.srcOffsets[0] = { 0, 0, 0 };
			blit.srcOffsets[1] = { mip_width, mip_height, 1 };
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.srcSubresource.mipLevel = i;
			blit.dstOffsets[0] = { 0, 0, 0 };
			blit.dstOffsets[1] = { mip_width > 1 ? mip_width >> 1 : 1, mip_height > 1 ? mip_height >> 1 : 1, 1 };
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;
			blit.dstSubresource.mipLevel = i + 1;

			vkCmdBlitImage(buf, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

			if (mip_width > 1)
				mip_width >>= 1;

			if (mip_height > 1)
				mip_height >>= 1;
		}

		barrier.subresourceRange.baseMipLevel = mip_levels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

		check(end_single_command(buf));

		return {};
	}

	err_info beg_single_command(VkCommandBuffer& out_command_buffer)
	{
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = vk_command_pool;

		check(vkAllocateCommandBuffers(vk_device, &alloc_info, &out_command_buffer));

		VkCommandBufferBeginInfo beg_info{};
		beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beg_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

		check(vkBeginCommandBuffer(out_command_buffer, &beg_info));

		return {};
	}

	err_info end_single_command(VkCommandBuffer command_buffer)
	{
		check(vkEndCommandBuffer(command_buffer));

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
		
		check(vkQueueSubmit(vk_graphics_queue, 1, &submit_info, nullptr));

		check(vkDeviceWaitIdle(vk_device));

		vkFreeCommandBuffers(vk_device, vk_command_pool, 1, &command_buffer);

		return {};
	}

	err_info update_uniforms(uint32_t image_idx)
	{
		static och::time start_t = och::time::now();

		float seconds = (och::time::now() - start_t).microseconds() / 1'000'000.0F;

		uniform_buffer_obj ubo;

		//ubo.model = glm::rotate(glm::mat4(1.0F), seconds * glm::radians(90.0F), glm::vec3(0.0F, 0.0F, 1.0F));
		ubo.model = och::mat4::rotate_z(seconds * 0.785398F);

		// ubo.view = glm::lookAt(glm::vec3(2.0F, 2.0F, 2.0F), glm::vec3(0.0F, 0.0F, 0.0F), glm::vec3(0.0F, 0.0F, 1.0F));
		ubo.view = och::look_at(och::vec3(2.0F), och::vec3(0.0F), och::vec3(0.0F, 0.0F, 1.0F));

		// ubo.projection = glm::perspective(glm::radians(45.0F), static_cast<float>(vk_swapchain_extent.width) / vk_swapchain_extent.height, 0.1F, 10.0F); ubo.projection[1][1] *= -1;
		ubo.projection = och::perspective(0.785398F, static_cast<float>(vk_swapchain_extent.width) / vk_swapchain_extent.height, 0.1F, 10.0F);

		void* uniform_data;

		check(vkMapMemory(vk_device, vk_uniform_buffers_memory[image_idx], 0, sizeof(uniform_buffer_obj), 0, &uniform_data));

		memcpy(uniform_data, &ubo, sizeof(uniform_buffer_obj));

		vkUnmapMemory(vk_device, vk_uniform_buffers_memory[image_idx]);

		return {};
	}

	void normalize_model(std::vector<vertex>& verts, glm::vec3 center = { 0.0F, 0.0F, 0.0F }, float scale = 2.0F)
	{
		float max_x = -INFINITY, max_y = -INFINITY, max_z = -INFINITY, min_x = INFINITY, min_y = INFINITY, min_z = INFINITY;

		for (const auto& v : verts)
		{
			if (v.pos.x > max_x)
				max_x = v.pos.x;
			if (v.pos.y > max_y)
				max_y = v.pos.y;
			if (v.pos.z > max_z)
				max_z = v.pos.z;

			if (v.pos.x < min_x)
				min_x = v.pos.x;
			if (v.pos.y < min_y)
				min_y = v.pos.y;
			if (v.pos.z < min_z)
				min_z = v.pos.z;
		}

		const float max = fmaxf(fmaxf(max_x, max_y), max_z);
		const float min = fminf(fminf(min_x, min_y), min_z);

		const float inv_scale = scale / (max - min);

		glm::vec3 offset((max_x + min_x) * 0.5F * inv_scale, (max_y + min_y) * 0.5F * inv_scale, (max_z + min_z) * 0.5F * inv_scale);

		offset -= center;

		och::print("min/max:\nx: {:8.4>_} / {:8.4>_}\ny: {:8.4>_} / {:8.4>_}\nz: {:8.4>_} / {:8.4>_}\nD: {:8.4>_} / {:8.4>_}\n\n", min_x, max_x, min_y, max_y, min_z, max_z, min, max);

		och::print("inverse scale: {}\n\n", inv_scale);

		och::print("offset: ({:8.4>_}, {:8.4>_}, {:8.4>_})\n\n", offset.x, offset.y, offset.z);

		for (auto& v : verts)
			v.pos = (v.pos * inv_scale) - (offset);
	}
};

int main()
{
	//glm::mat4 mglm = glm::perspective(0.25F, 1440.0F / 810.0F, 0.1F, 10.0F); mglm[1][1] *= -1;
	//och::mat4 moch = och::perspective(0.25F, 1440.0F / 810.0F, 0.1F, 10.0F);
	//
	////glm::mat4 mglm = glm::scale(glm::translate(glm::mat4(1), { 4.0F, 5.0F, 6.0F }), { 1.0F, 2.0F, 3.0F });
	////och::mat4 moch = och::mat4::translate(4.0F, 5.0F, 6.0F) * och::mat4::scale(1.0F, 2.0F, 3.0F);
	//
	////glm::mat4 mglm(.0, .1, .2, .3, .4, .5, .6, .7, .8, .9, .10, .11, .12, .13, .14, .15);
	////och::mat4 moch(.0, .1, .2, .3, .4, .5, .6, .7, .8, .9, .10, .11, .12, .13, .14, .15);
	//
	//och::print("glm:\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n\n", 
	//	mglm[0][0], mglm[1][0], mglm[2][0], mglm[3][0], mglm[0][1], mglm[1][1], mglm[2][1], mglm[3][1], mglm[0][2], mglm[1][2], mglm[2][2], mglm[3][2], mglm[0][3], mglm[1][3], mglm[2][3], mglm[3][3]);
	//
	//och::print("och:\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n{:8.3>_} {:8.3>_} {:8.3>_} {:8.3>_}\n\n", 
	//	moch(0, 0), moch(1, 0), moch(2, 0), moch(3, 0), moch(0, 1), moch(1, 1), moch(2, 1), moch(3, 1), moch(0, 2), moch(1, 2), moch(2, 2), moch(3, 2), moch(0, 3), moch(1, 3), moch(2, 3), moch(3, 3));
	//
	//if (memcmp(&mglm, &moch, 64))
	//	och::print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n!!!!!!!!!!!!!!!!!!NOT EQUAL!!!!!!!!!!!!!!!!!!\n!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n\n");
	//else
	//	och::print("Equal\n\n");

	hello_vulkan vk;
	
	err_info err = vk.run();

	if (err)
	{
		och::print("An Error occurred!\n");

		auto stack = och::get_stacktrace();

		for (auto e : stack)
			och::print("Function {} on Line {}: \"{}\"\n\n", e->function, e->line_num, e->call);
	}
	else
		och::print("\nProcess terminated successfully\n");
}

void framebuffer_resize_callback_fn(GLFWwindow* window, int width, int height)
{
	width, height;

	reinterpret_cast<hello_vulkan*>(glfwGetWindowUserPointer(window))->framebuffer_resized = true;
}
