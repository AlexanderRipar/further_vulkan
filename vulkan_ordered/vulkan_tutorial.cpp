#include "vulkan_tutorial.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <cstdint>
#include <vector>
#include <unordered_map>

#include "bitmap_header.h"
#include "och_timer.h"
#include "och_fmt.h"
#include "och_fio.h"
#include "och_vulkan_base.h"
#include "och_error_handling.h"
#include "och_matmath.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm.hpp>
#include <gtc\matrix_transform.hpp>
#define GLM_ENABLE_EXTERIMANTAL
#include <gtx\hash.hpp>



#define OCH_ASSET_NAME "viking_room"
//#define OCH_ASSET_NAME "vase"

#define OCH_ASSET_OFFSET {0.0F, 0.0F, 0.3F}
#define OCH_ASSET_SCALE 2.0F

#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define OCH_ASSET_NAME "viking_room"
//#define OCH_ASSET_NAME "vase"

#define OCH_ASSET_OFFSET {0.0F, 0.0F, 0.3F}
#define OCH_ASSET_SCALE 2.0F


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
		size_t operator()(const vertex& vertex) const {
			return (
				(hash<glm::vec3>()(vertex.pos) ^
					(hash<glm::vec3>()(vertex.col) << 1)) >> 1) ^
				(hash<glm::vec2>()(vertex.tex_pos) << 1);
		}
	};
}

struct uniform_buffer_obj
{
	och::mat4 transform;
};

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



struct vulkan_tutorial
{
	static constexpr uint32_t max_frames_in_flight = 2;



	std::vector<vertex> vertices;

	std::vector<uint32_t> indices;

	uint32_t window_width = 1440;
	uint32_t window_height = 810;



	och::vulkan_context context{};



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

	size_t curr_frame = 0;

#ifdef OCH_VALIDATE
	VkDebugUtilsMessengerEXT vk_debug_messenger = nullptr;
#endif // OCH_VALIDATE



	och::err_info run()
	{
		check(init());

		check(main_loop());

		cleanup();

		return {};
	}

	och::err_info init()
	{
		och::print("Starting initialization\n\n");

		och::timer init_timer;

		check(context.create("Hello Vulkan", window_width, window_height));

		check(create_vk_render_pass());

		check(create_vk_descriptor_set_layout());

		check(create_vk_graphics_pipeline_layout());

		check(create_vk_graphics_pipeline());

		check(create_vk_command_pool());

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

		och::print("Finished initialization. Time taken: {}\n\n", init_timer.read());

		return {};
	}

	och::err_info create_vk_render_pass()
	{
		VkAttachmentDescription color_attachment{};
		color_attachment.format = context.m_swapchain_format;
		color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
		color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

		VkAttachmentDescription depth_attachment{};
		depth_attachment.format = VK_FORMAT_D32_SFLOAT;
		depth_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
		depth_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
		depth_attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
		depth_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
		depth_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		depth_attachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkAttachmentReference color_ref{};
		color_ref.attachment = 0;
		color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

		VkAttachmentReference depth_ref{};
		depth_ref.attachment = 1;
		depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

		VkSubpassDescription subpass{};
		subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
		subpass.colorAttachmentCount = 1;
		subpass.pColorAttachments = &color_ref;
		subpass.pDepthStencilAttachment = &depth_ref;

		VkSubpassDependency dependency{};
		dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
		dependency.dstSubpass = 0;
		dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.srcAccessMask = 0;
		dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
		dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

		VkAttachmentDescription attachment_descs[]{ color_attachment, depth_attachment };

		VkRenderPassCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
		create_info.attachmentCount = static_cast<uint32_t>(sizeof(attachment_descs) / sizeof(*attachment_descs));
		create_info.pAttachments = attachment_descs;
		create_info.subpassCount = 1;
		create_info.pSubpasses = &subpass;
		create_info.dependencyCount = 1;
		create_info.pDependencies = &dependency;

		check(vkCreateRenderPass(context.m_device, &create_info, nullptr, &vk_render_pass));

		return {};
	}

	och::err_info create_vk_descriptor_set_layout()
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

		check(vkCreateDescriptorSetLayout(context.m_device, &create_info, nullptr, &vk_descriptor_set_layout));

		return {};
	}

	och::err_info create_vk_graphics_pipeline_layout()
	{
		VkPipelineLayoutCreateInfo layout_info{};
		layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
		layout_info.setLayoutCount = 1;
		layout_info.pSetLayouts = &vk_descriptor_set_layout;
		layout_info.pushConstantRangeCount = 0;
		layout_info.pPushConstantRanges = nullptr;

		check(vkCreatePipelineLayout(context.m_device, &layout_info, nullptr, &vk_pipeline_layout));

		return {};
	}

	och::err_info create_vk_graphics_pipeline()
	{
		VkShaderModule vert_shader_module;

		check(create_shader_module_from_file("shaders/tutorial.vert.spv", vert_shader_module));

		VkShaderModule frag_shader_module;

		check(create_shader_module_from_file("shaders/tutorial.frag.spv", frag_shader_module));

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
		viewport.width = static_cast<float>(context.m_swapchain_extent.width);
		viewport.height = static_cast<float>(context.m_swapchain_extent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = context.m_swapchain_extent;

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
		multisample_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
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

		check(vkCreateGraphicsPipelines(context.m_device, nullptr, 1, &pipeline_info, nullptr, &vk_graphics_pipeline));

		vkDestroyShaderModule(context.m_device, vert_shader_module, nullptr);

		vkDestroyShaderModule(context.m_device, frag_shader_module, nullptr);

		return {};
	}

	och::err_info create_vk_command_pool()
	{
		VkCommandPoolCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		create_info.queueFamilyIndex = context.m_general_queues.family_index;

		check(vkCreateCommandPool(context.m_device, &create_info, nullptr, &vk_command_pool));

		return {};
	}

	och::err_info create_vk_depth_resources()
	{
		check(allocate_image(context.m_swapchain_extent.width, context.m_swapchain_extent.height, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_depth_image, vk_depth_image_memory, VK_SAMPLE_COUNT_1_BIT));

		check(allocate_image_view(vk_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT, vk_depth_image_view));

		check(transition_image_layout(vk_depth_image, VK_FORMAT_D32_SFLOAT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL));

		return {};
	}

	och::err_info create_vk_swapchain_framebuffers()
	{
		vk_swapchain_framebuffers.resize(context.m_swapchain_image_cnt);

		for (size_t i = 0; i != context.m_swapchain_image_cnt; ++i)
		{
			VkImageView attachments[]{ context.m_swapchain_image_views[i], vk_depth_image_view };

			VkFramebufferCreateInfo create_info{};
			create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			create_info.renderPass = vk_render_pass;
			create_info.attachmentCount = static_cast<uint32_t>(sizeof(attachments) / sizeof(*attachments));
			create_info.pAttachments = attachments;
			create_info.width = context.m_swapchain_extent.width;
			create_info.height = context.m_swapchain_extent.height;
			create_info.layers = 1;

			check(vkCreateFramebuffer(context.m_device, &create_info, nullptr, &vk_swapchain_framebuffers[i]));
		}

		return {};
	}

	och::err_info create_vk_texture_image()
	{
		och::mapped_file<bitmap_header> texture_file(och::stringview("textures/" OCH_ASSET_NAME ".bmp"), och::fio::access_read, och::fio::open_normal, och::fio::open_fail);

		if (!texture_file)
			return MAKE_ERROR(1);

		bitmap_header& header = texture_file[0];

		const uint8_t* pixels = header.raw_image_data();

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

		check(vkMapMemory(context.m_device, staging_buf_mem, 0, pixel_cnt * 4, 0, &data));

		memcpy(data, pixels, pixel_cnt * 4);

		vkUnmapMemory(context.m_device, staging_buf_mem);

		if (header.bits_per_pixel == 24)
			delete[] pixels;

		check(allocate_image(header.width, header.height, VK_FORMAT_B8G8R8A8_SRGB,
			VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_texture_image, vk_texture_image_memory, VK_SAMPLE_COUNT_1_BIT, mip_levels));


		check(transition_image_layout(vk_texture_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, mip_levels));

		check(copy_buffer_to_image(vk_texture_image, staging_buf, header.width, header.height));

		check(generate_mipmap(vk_texture_image, VK_FORMAT_B8G8R8A8_SRGB, header.width, header.height, mip_levels));

		vkDestroyBuffer(context.m_device, staging_buf, nullptr);

		vkFreeMemory(context.m_device, staging_buf_mem, nullptr);

		return {};
	}

	och::err_info create_vk_texture_image_view()
	{
		check(allocate_image_view(vk_texture_image, VK_FORMAT_B8G8R8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, vk_texture_image_view, vk_texture_image_mipmap_levels));

		return {};
	}

	och::err_info create_vk_texture_sampler()
	{
		VkPhysicalDeviceProperties dev_props{};

		vkGetPhysicalDeviceProperties(context.m_physical_device, &dev_props);

		VkSamplerCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		create_info.magFilter = VK_FILTER_LINEAR;
		create_info.minFilter = VK_FILTER_LINEAR;
		create_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		create_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		create_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		create_info.anisotropyEnable = VK_FALSE; // VK_TRUE;
		create_info.maxAnisotropy = dev_props.limits.maxSamplerAnisotropy;
		create_info.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
		create_info.unnormalizedCoordinates = VK_FALSE;
		create_info.compareEnable = VK_FALSE;
		create_info.compareOp = VK_COMPARE_OP_ALWAYS;
		create_info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
		create_info.mipLodBias = 0.0F;
		create_info.minLod = 0.0F;
		create_info.maxLod = static_cast<float>(vk_texture_image_mipmap_levels);

		check(vkCreateSampler(context.m_device, &create_info, nullptr, &vk_texture_sampler));

		return {};
	}

	och::err_info load_obj_model()
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
			for (const auto& family_index : shape.mesh.indices)
			{
				vertex vert{};

				vert.pos = {
					attrib.vertices[3 * family_index.vertex_index + 0],
					attrib.vertices[3 * family_index.vertex_index + 1],
					attrib.vertices[3 * family_index.vertex_index + 2],
				};

				vert.tex_pos = {
					attrib.texcoords[2 * family_index.texcoord_index + 0],
					attrib.texcoords[2 * family_index.texcoord_index + 1],
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

	och::err_info create_vk_vertex_buffer()
	{
		VkBuffer staging_buf = nullptr;

		VkDeviceMemory staging_buf_mem = nullptr;

		check(allocate_buffer(vertices.size() * sizeof(vertices[0]), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, staging_buf, staging_buf_mem));

		void* staging_data = nullptr;

		check(vkMapMemory(context.m_device, staging_buf_mem, 0, vertices.size() * sizeof(vertices[0]), 0, &staging_data));

		memcpy(staging_data, vertices.data(), vertices.size() * sizeof(vertices[0]));

		vkUnmapMemory(context.m_device, staging_buf_mem);

		check(allocate_buffer(vertices.size() * sizeof(vertices[0]), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_vertex_buffer, vk_vertex_buffer_memory));

		copy_buffer_to_buffer(vk_vertex_buffer, staging_buf, vertices.size() * sizeof(vertices[0]));

		vkDestroyBuffer(context.m_device, staging_buf, nullptr);

		vkFreeMemory(context.m_device, staging_buf_mem, nullptr);

		return {};
	}

	och::err_info create_vk_index_buffer()
	{
		VkBuffer staging_buf = nullptr;

		VkDeviceMemory staging_buf_mem = nullptr;

		check(allocate_buffer(indices.size() * sizeof(indices[0]), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, staging_buf, staging_buf_mem));

		void* data = nullptr;

		check(vkMapMemory(context.m_device, staging_buf_mem, 0, indices.size() * sizeof(indices[0]), 0, &data));

		memcpy(data, indices.data(), indices.size() * sizeof(indices[0]));

		vkUnmapMemory(context.m_device, staging_buf_mem);

		check(allocate_buffer(indices.size() * sizeof(indices[0]), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, vk_index_buffer, vk_index_buffer_memory));

		check(copy_buffer_to_buffer(vk_index_buffer, staging_buf, indices.size() * sizeof(indices[0])));

		vkDestroyBuffer(context.m_device, staging_buf, nullptr);

		vkFreeMemory(context.m_device, staging_buf_mem, nullptr);

		return {};
	}

	och::err_info create_vk_uniform_buffers()
	{
		vk_uniform_buffers.resize(context.m_swapchain_image_cnt);
		vk_uniform_buffers_memory.resize(context.m_swapchain_image_cnt);

		for (size_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			check(allocate_buffer(sizeof(uniform_buffer_obj), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_COHERENT_BIT | VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, vk_uniform_buffers[i], vk_uniform_buffers_memory[i]));

		return {};
	}

	och::err_info create_vk_descriptor_pool()
	{
		VkDescriptorPoolSize pool_sizes[]{
			{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, context.m_swapchain_image_cnt},
			{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, context.m_swapchain_image_cnt},
		};

		VkDescriptorPoolCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		create_info.poolSizeCount = static_cast<uint32_t>(sizeof(pool_sizes) / sizeof(*pool_sizes));
		create_info.pPoolSizes = pool_sizes;
		create_info.maxSets = static_cast<uint32_t>(context.m_swapchain_image_cnt);

		check(vkCreateDescriptorPool(context.m_device, &create_info, nullptr, &vk_descriptor_pool));

		return {};
	}

	och::err_info create_vk_descriptor_sets()
	{
		std::vector<VkDescriptorSetLayout> desc_set_layouts(context.m_swapchain_image_cnt, vk_descriptor_set_layout);

		VkDescriptorSetAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		alloc_info.descriptorPool = vk_descriptor_pool;
		alloc_info.descriptorSetCount = static_cast<uint32_t>(desc_set_layouts.size());
		alloc_info.pSetLayouts = desc_set_layouts.data();

		vk_descriptor_sets.resize(desc_set_layouts.size());

		check(vkAllocateDescriptorSets(context.m_device, &alloc_info, vk_descriptor_sets.data()));

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

			vkUpdateDescriptorSets(context.m_device, static_cast<uint32_t>(sizeof(writes) / sizeof(*writes)), writes, 0, nullptr);
		}

		return{};
	}

	och::err_info create_vk_command_buffers()
	{
		vk_command_buffers.resize(context.m_swapchain_image_cnt);

		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.commandPool = vk_command_pool;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = static_cast<uint32_t>(vk_command_buffers.size());

		check(vkAllocateCommandBuffers(context.m_device, &alloc_info, vk_command_buffers.data()));

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
			pass_beg_info.renderArea.extent = context.m_swapchain_extent;
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

	och::err_info create_vk_sync_objects()
	{
		vk_images_inflight_fences.resize(context.m_swapchain_image_cnt, nullptr);

		VkSemaphoreCreateInfo semaphore_info{};
		semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		VkFenceCreateInfo fence_info{};
		fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

		for (uint32_t i = 0; i != max_frames_in_flight; ++i)
		{
			check(vkCreateSemaphore(context.m_device, &semaphore_info, nullptr, &vk_image_available_semaphores[i]));

			check(vkCreateSemaphore(context.m_device, &semaphore_info, nullptr, &vk_render_complete_semaphores[i]));

			check(vkCreateFence(context.m_device, &fence_info, nullptr, &vk_inflight_fences[i]));
		}

		return {};
	}

	och::err_info main_loop()
	{
		while (!glfwWindowShouldClose(context.m_window))
		{
			check(draw_frame());

			glfwPollEvents();
		}

		check(vkDeviceWaitIdle(context.m_device));

		return {};
	}

	och::err_info draw_frame()
	{
		check(vkWaitForFences(context.m_device, 1, &vk_inflight_fences[curr_frame], VK_FALSE, UINT64_MAX));

		uint32_t image_idx;

		if (VkResult acquire_rst = vkAcquireNextImageKHR(context.m_device, context.m_swapchain, UINT64_MAX, vk_image_available_semaphores[curr_frame], nullptr, &image_idx); acquire_rst == VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreate_swapchain();
			return {};
		}
		else if (acquire_rst != VK_SUBOPTIMAL_KHR)
			check(acquire_rst);

		if (vk_images_inflight_fences[image_idx])
			check(vkWaitForFences(context.m_device, 1, &vk_images_inflight_fences[image_idx], VK_FALSE, UINT64_MAX));

		vk_images_inflight_fences[image_idx] = vk_inflight_fences[curr_frame];

		update_uniforms(image_idx);

		VkPipelineStageFlags wait_stages[]{ VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &vk_image_available_semaphores[curr_frame];
		submit_info.pWaitDstStageMask = wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &vk_command_buffers[image_idx];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &vk_render_complete_semaphores[curr_frame];

		check(vkResetFences(context.m_device, 1, &vk_inflight_fences[curr_frame]));

		check(vkQueueSubmit(context.m_general_queues[0], 1, &submit_info, vk_inflight_fences[curr_frame]));

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &vk_render_complete_semaphores[curr_frame];
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &context.m_swapchain;
		present_info.pImageIndices = &image_idx;
		present_info.pResults = nullptr;

		if (VkResult present_rst = vkQueuePresentKHR(context.m_general_queues[0], &present_info); present_rst == VK_ERROR_OUT_OF_DATE_KHR || present_rst == VK_SUBOPTIMAL_KHR || context.m_flags.framebuffer_resized)
		{
			context.m_flags.framebuffer_resized = false;
			recreate_swapchain();
		}
		else
			check(present_rst);

		curr_frame = (curr_frame + 1) % max_frames_in_flight;

		return {};
	}

	och::err_info recreate_swapchain()
	{
		cleanup_swapchain();

		check(context.recreate_swapchain());

		check(create_vk_depth_resources());

		check(create_vk_graphics_pipeline());

		check(create_vk_swapchain_framebuffers());

		check(create_vk_command_buffers());

		return {};
	}

	void cleanup_swapchain()
	{
		int width, height;

		glfwGetFramebufferSize(context.m_window, &width, &height);

		while (!width || !height)
		{
			glfwWaitEvents();

			glfwGetFramebufferSize(context.m_window, &width, &height);
		}

		vkDeviceWaitIdle(context.m_device);



		vkFreeCommandBuffers(context.m_device, vk_command_pool, static_cast<uint32_t>(vk_command_buffers.size()), vk_command_buffers.data());

		for (auto& framebuffer : vk_swapchain_framebuffers)
			vkDestroyFramebuffer(context.m_device, framebuffer, nullptr);

		vkDestroyPipeline(context.m_device, vk_graphics_pipeline, nullptr);



		vkDestroyImageView(context.m_device, vk_depth_image_view, nullptr);

		vkDestroyImage(context.m_device, vk_depth_image, nullptr);

		vkFreeMemory(context.m_device, vk_depth_image_memory, nullptr);
	}

	void cleanup()
	{
		cleanup_swapchain();
	
		vkDestroyDescriptorPool(context.m_device, vk_descriptor_pool, nullptr);
	
		vkDestroyPipelineLayout(context.m_device, vk_pipeline_layout, nullptr);
	
		vkDestroyRenderPass(context.m_device, vk_render_pass, nullptr);
	
		for (auto& buffer : vk_uniform_buffers)
			vkDestroyBuffer(context.m_device, buffer, nullptr);
	
		for (auto& memory : vk_uniform_buffers_memory)
			vkFreeMemory(context.m_device, memory, nullptr);
	
		vkDestroySampler(context.m_device, vk_texture_sampler, nullptr);
	
		vkDestroyImageView(context.m_device, vk_texture_image_view, nullptr);
	
		vkDestroyImage(context.m_device, vk_texture_image, nullptr);
	
		vkFreeMemory(context.m_device, vk_texture_image_memory, nullptr);
	
		vkDestroyDescriptorSetLayout(context.m_device, vk_descriptor_set_layout, nullptr);
	
		vkDestroyBuffer(context.m_device, vk_index_buffer, nullptr);
	
		vkFreeMemory(context.m_device, vk_index_buffer_memory, nullptr);
	
		vkDestroyBuffer(context.m_device, vk_vertex_buffer, nullptr);
	
		vkFreeMemory(context.m_device, vk_vertex_buffer_memory, nullptr);
	
		for (auto& sem : vk_render_complete_semaphores)
			vkDestroySemaphore(context.m_device, sem, nullptr);
	
		for (auto& sem : vk_image_available_semaphores)
			vkDestroySemaphore(context.m_device, sem, nullptr);
	
		for (auto& fence : vk_inflight_fences)
			vkDestroyFence(context.m_device, fence, nullptr);
	
		vkDestroyCommandPool(context.m_device, vk_command_pool, nullptr);
	
		context.destroy();
	}

	och::err_info create_shader_module_from_file(const char* filename, VkShaderModule& out_shader_module)
	{
		och::mapped_file<uint8_t> shader_file(filename, och::fio::access_read, och::fio::open_normal, och::fio::open_fail);
	
		if (!shader_file)
			return MAKE_ERROR(1);
	
		VkShaderModuleCreateInfo create_info{};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.codeSize = shader_file.bytes();
		create_info.pCode = reinterpret_cast<const uint32_t*>(shader_file.data());
	
		check(vkCreateShaderModule(context.m_device, &create_info, nullptr, &out_shader_module));
	
		return {};
	}
	
	och::err_info query_memory_type_index(uint32_t type_filter, VkMemoryPropertyFlags properties, uint32_t& out_type_index)
	{
		VkPhysicalDeviceMemoryProperties mem_props{};
	
		vkGetPhysicalDeviceMemoryProperties(context.m_physical_device, &mem_props);
	
		for (uint32_t i = 0; i != mem_props.memoryTypeCount; ++i)
			if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & properties) == properties)
			{
				out_type_index = i;
	
				return{};
			}
	
		return MAKE_ERROR(1);
	}
	
	och::err_info allocate_buffer(VkDeviceSize bytes, VkBufferUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkBuffer& out_buffer, VkDeviceMemory& out_buffer_memory, VkSharingMode share_mode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queue_family_cnt = 0, const uint32_t* queue_family_ptr = nullptr)
	{
		VkBufferCreateInfo buffer_info{};
		buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		buffer_info.size = bytes;
		buffer_info.usage = usage_flags;
		buffer_info.sharingMode = share_mode;
		buffer_info.queueFamilyIndexCount = queue_family_cnt;
		buffer_info.pQueueFamilyIndices = queue_family_ptr;
	
		check(vkCreateBuffer(context.m_device, &buffer_info, nullptr, &out_buffer));
	
		VkMemoryRequirements mem_reqs{};
	
		vkGetBufferMemoryRequirements(context.m_device, out_buffer, &mem_reqs);
	
		uint32_t memory_type_index;
	
		check(query_memory_type_index(mem_reqs.memoryTypeBits, property_flags, memory_type_index));
	
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_reqs.size;
		alloc_info.memoryTypeIndex = memory_type_index;
	
		check(vkAllocateMemory(context.m_device, &alloc_info, nullptr, &out_buffer_memory));
	
		check(vkBindBufferMemory(context.m_device, out_buffer, out_buffer_memory, 0ull));
	
		return {};
	}
	
	och::err_info allocate_image(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage_flags, VkMemoryPropertyFlags property_flags, VkImage& out_image, VkDeviceMemory& out_image_memory, VkSampleCountFlagBits sample_cnt = VK_SAMPLE_COUNT_1_BIT, uint32_t mip_levels = 1, VkSharingMode share_mode = VK_SHARING_MODE_EXCLUSIVE, uint32_t queue_family_cnt = 0, const uint32_t* queue_family_ptr = nullptr)
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
	
		check(vkCreateImage(context.m_device, &create_info, nullptr, &out_image));
	
		VkMemoryRequirements mem_reqs;
	
		vkGetImageMemoryRequirements(context.m_device, out_image, &mem_reqs);
	
		uint32_t mem_type_idx;
	
		check(query_memory_type_index(mem_reqs.memoryTypeBits, property_flags, mem_type_idx));
	
		VkMemoryAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		alloc_info.allocationSize = mem_reqs.size;
		alloc_info.memoryTypeIndex = mem_type_idx;
	
		check(vkAllocateMemory(context.m_device, &alloc_info, nullptr, &out_image_memory));
	
		check(vkBindImageMemory(context.m_device, out_image, out_image_memory, 0));
	
		return {};
	}
	
	och::err_info allocate_image_view(VkImage image, VkFormat format, VkImageAspectFlags aspect_mask, VkImageView& out_image_view, uint32_t mip_levels = 1)
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
	
		check(vkCreateImageView(context.m_device, &create_info, nullptr, &out_image_view));
	
		return {};
	}
	
	och::err_info copy_buffer_to_buffer(VkBuffer dst, VkBuffer src, VkDeviceSize size, VkDeviceSize dst_offset = 0, VkDeviceSize src_offset = 0)
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
	
	och::err_info copy_buffer_to_image(VkImage dst, VkBuffer src, uint32_t width, uint32_t height)
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
	
	och::err_info transition_image_layout(VkImage image, VkFormat format, VkImageLayout old_layout, VkImageLayout new_layout, uint32_t mip_levels = 1)
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
	
	och::err_info generate_mipmap(VkImage image, VkFormat format, int32_t width, int32_t height, uint32_t mip_levels)
	{
		VkFormatProperties props;
	
		vkGetPhysicalDeviceFormatProperties(context.m_physical_device, format, &props);
	
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

	och::err_info beg_single_command(VkCommandBuffer& out_command_buffer)
	{
		VkCommandBufferAllocateInfo alloc_info{};
		alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		alloc_info.commandBufferCount = 1;
		alloc_info.commandPool = vk_command_pool;
	
		check(vkAllocateCommandBuffers(context.m_device, &alloc_info, &out_command_buffer));
	
		VkCommandBufferBeginInfo beg_info{};
		beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beg_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
	
		check(vkBeginCommandBuffer(out_command_buffer, &beg_info));
	
		return {};
	}
	
	och::err_info end_single_command(VkCommandBuffer command_buffer)
	{
		check(vkEndCommandBuffer(command_buffer));
	
		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffer;
	
		check(vkQueueSubmit(context.m_general_queues[0], 1, &submit_info, nullptr));
	
		check(vkDeviceWaitIdle(context.m_device));
	
		vkFreeCommandBuffers(context.m_device, vk_command_pool, 1, &command_buffer);
	
		return {};
	}
	
	och::err_info update_uniforms(uint32_t image_idx)
	{
		static och::time start_t = och::time::now();
	
		float seconds = (och::time::now() - start_t).microseconds() / 1'000'000.0F;
	
		uniform_buffer_obj ubo;
	
		ubo.transform =
			och::perspective(0.785398F, static_cast<float>(context.m_swapchain_extent.width) / context.m_swapchain_extent.height, 0.1F, 10.0F) *
			och::look_at(och::vec3(2.0F), och::vec3(0.0F), och::vec3(0.0F, 0.0F, 1.0F)) *
			och::mat4::rotate_z(seconds * 0.785398F);
	
		void* uniform_data;
	
		check(vkMapMemory(context.m_device, vk_uniform_buffers_memory[image_idx], 0, sizeof(uniform_buffer_obj), 0, &uniform_data));
	
		memcpy(uniform_data, &ubo, sizeof(uniform_buffer_obj));
	
		vkUnmapMemory(context.m_device, vk_uniform_buffers_memory[image_idx]);
	
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
	
		for (auto& v : verts)
			v.pos = (v.pos * inv_scale) - (offset);
	}
};

och::err_info run_vulkan_tutorial() noexcept
{
	vulkan_tutorial program;

	check(program.run());

	return {};
}
