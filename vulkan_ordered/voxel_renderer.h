#pragma once

#include <vulkan/vulkan.h>

#include "och_timer.h"
#include "och_fmt.h"
#include "och_fio.h"
#include "och_error_handling.h"
#include "och_matmath.h"

#include "och_vulkan_base.h"

using och::err_info;

struct ui_vertex
{
	och::vec2 tex_coord;
	och::vec2 position;

	static constexpr VkVertexInputBindingDescription binding{ 0, 16, VK_VERTEX_INPUT_RATE_VERTEX };

	static constexpr VkVertexInputAttributeDescription attributes[]{
		{0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
		{0, 1, VK_FORMAT_R32G32_SFLOAT, 8}
	};

	static constexpr uint32_t attribute_cnt = 2;
};

struct compute_push_constant_data
{
	och::mat3 transform;
};

struct voxel_renderer
{
	static constexpr uint32_t max_frames_in_flight = 2;

	static constexpr uint32_t initial_window_width = 1440, initial_window_height = 810;

	och::vulkan_context context;

	VkRenderPass renderpass;

	VkPipelineCache pipeline_cache;

	VkCommandPool command_pool;

	VkCommandBuffer command_buffers[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkFramebuffer framebuffers[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkDeviceMemory device_memory;



	VkPipeline graphics_pipeline;

	VkPipelineLayout graphics_pipeline_layout;

	VkBuffer ui_vertices; // TODO

	VkBuffer ui_indices; // TODO

	uint32_t ui_vertex_cnt; // TODO

	uint32_t ui_index_cnt; // TODO

	VkImage ui_overlay_image; // TODO

	VkImageView ui_overlay_image_view; // TODO
	
	VkShaderModule ui_frag_shader;

	VkShaderModule ui_vert_shader;



	VkPipeline compute_pipeline;

	VkPipelineLayout compute_pipeline_layout;

	VkPushConstantRange compute_push_constant_range;

	VkDescriptorSetLayout compute_descriptor_set_layout;

	VkDescriptorPool compute_descriptor_pool;

	VkDescriptorSet compute_descriptor_sets[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkShaderModule volume_shader;

	compute_push_constant_data compute_push_constants;

	VkImage volume_image; // TODO

	VkImageView volume_image_view; // TODO

	VkDeviceMemory volume_image_memory; // TODO

	VkBuffer voxel_color_buffer; // TODO

	uint32_t voxel_color_cnt; // TODO



	VkCommandPool transfer_command_pool;


	VkSemaphore image_available_semaphores[max_frames_in_flight];

	VkSemaphore render_complete_semaphores[max_frames_in_flight];

	VkFence inflight_fences[max_frames_in_flight];

	VkFence swapchain_image_inflight_fences[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};



	uint32_t curr_frame = 0;



	err_info create()
	{
		och::print("Starting initialization\n\n");

		och::timer init_timer;

		check(context.create("Voxel Renderer", initial_window_width, initial_window_height, 1, 0, 1, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT));

		// Create Renderpass
		{
			VkAttachmentDescription color_attachment{};
			color_attachment.flags = 0;
			color_attachment.format = context.m_swapchain_format;
			color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
			color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference color_reference{};
			color_reference.attachment = 0;
			color_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass_desc{};
			subpass_desc.flags = 0;
			subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass_desc.inputAttachmentCount = 0;
			subpass_desc.pInputAttachments = nullptr;
			subpass_desc.colorAttachmentCount = 1;
			subpass_desc.pColorAttachments = &color_reference;
			subpass_desc.pResolveAttachments = nullptr;
			subpass_desc.pDepthStencilAttachment = nullptr;
			subpass_desc.preserveAttachmentCount = 0;
			subpass_desc.pPreserveAttachments = nullptr;

			VkSubpassDependency subpass_dependency{};
			subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			subpass_dependency.dstSubpass = 0;
			subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpass_dependency.srcAccessMask = 0;
			subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			subpass_dependency.dependencyFlags = 0;

			VkRenderPassCreateInfo renderpass_ci{};
			renderpass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderpass_ci.pNext = nullptr;
			renderpass_ci.flags = 0;
			renderpass_ci.attachmentCount = 1;
			renderpass_ci.pAttachments = &color_attachment;
			renderpass_ci.subpassCount = 1;
			renderpass_ci.pSubpasses = &subpass_desc;
			renderpass_ci.dependencyCount = 1;
			renderpass_ci.pDependencies = &subpass_dependency;

			check(vkCreateRenderPass(context.m_device, &renderpass_ci, nullptr, &renderpass));
		}

		// Create Compute Descriptor Set Layout
		{
			VkDescriptorSetLayoutBinding bindings[3]{};
			// Volume
			bindings[0].binding = 0;
			bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindings[0].descriptorCount = 1;
			bindings[0].stageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			bindings[0].pImmutableSamplers = nullptr;
			// Voxel Colors
			bindings[1].binding = 1;
			bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[1].descriptorCount = 1;
			bindings[1].stageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			bindings[1].pImmutableSamplers = nullptr;
			// Output Image
			bindings[2].binding = 2;
			bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			bindings[2].descriptorCount = 1;
			bindings[2].stageFlags = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
			bindings[2].pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci{};
			descriptor_set_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptor_set_layout_ci.pNext = nullptr;
			descriptor_set_layout_ci.flags = 0;
			descriptor_set_layout_ci.bindingCount = 3;
			descriptor_set_layout_ci.pBindings = bindings;

			check(vkCreateDescriptorSetLayout(context.m_device, &descriptor_set_layout_ci, nullptr, &compute_descriptor_set_layout));
		}

		// Load Pipeline Cache
		{
			och::mapped_file<uint8_t> cache_file("cache/pipeline_cache.bin", och::fio::access_read, och::fio::open_normal, och::fio::open_fail);

			VkPipelineCacheCreateInfo pipeline_cache_ci{};
			pipeline_cache_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
			pipeline_cache_ci.pNext = nullptr;
			pipeline_cache_ci.flags = 0;
			pipeline_cache_ci.initialDataSize = cache_file.bytes();
			pipeline_cache_ci.pInitialData = cache_file.data();

			check(vkCreatePipelineCache(context.m_device, &pipeline_cache_ci, nullptr, &pipeline_cache));
		}

		// Load Shaders
		{
			check(load_shader(ui_vert_shader, "shaders/ui_vert.spv"));

			check(load_shader(ui_frag_shader, "shaders/ui_frag.spv"));

			check(load_shader(volume_shader, "shader/volume_comp.spv"));
		}

		// Create Graphics Pipeline Layout
		{
			VkPipelineLayoutCreateInfo layout_ci;
			layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layout_ci.pNext = nullptr;
			layout_ci.flags = 0;
			layout_ci.setLayoutCount = 0;
			layout_ci.pSetLayouts = nullptr;
			layout_ci.pushConstantRangeCount = 0;
			layout_ci.pPushConstantRanges = nullptr;

			check(vkCreatePipelineLayout(context.m_device, &layout_ci, nullptr, &graphics_pipeline_layout));
		}

		check(create_graphics_pipeline());

		// Create Push Constant Range for Compute Pipeline
		{
			compute_push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			compute_push_constant_range.offset = 0;
			compute_push_constant_range.size = sizeof(compute_push_constant_data);
		}

		// Create Compute Pipeline Layout
		{
			VkPipelineLayoutCreateInfo layout_ci{};
			layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			layout_ci.pNext = nullptr;
			layout_ci.flags = 0;
			layout_ci.setLayoutCount = 1;
			layout_ci.pSetLayouts = &compute_descriptor_set_layout;
			layout_ci.pushConstantRangeCount = 1;
			layout_ci.pPushConstantRanges = &compute_push_constant_range;

			check(vkCreatePipelineLayout(context.m_device, &layout_ci, nullptr, &compute_pipeline_layout));
		}

		check(create_compute_pipeline());

		// Create Transfer Command Pool
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			command_pool_ci.queueFamilyIndex = context.m_transfer_queues.index;

			check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &transfer_command_pool));
		}

		// Create Render Command Pool
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = 0;
			command_pool_ci.queueFamilyIndex = context.m_general_queues.index;

			check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &command_pool));
		}

		check(create_framebuffers());

		// TODO // Load ui overlay vertices
		{
			och::mapped_file<ui_vertex> vertex_file("models/ui_overlay", och::fio::access_read, och::fio::open_normal, och::fio::open_fail);

			if (!vertex_file)
				return MSG_ERROR("Could not load vertices for UI-Overlay");

			VkBufferCreateInfo buffer_ci{};
			

			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &ui_vertices));
		}

		// TODO // Load ui overlay image
		{

		}

		// TODO // Load volume
		{

		}

		// TODO // Load voxel colors
		{

		}
		
		// Create Compute Descriptor Pool
		{
			VkDescriptorPoolSize pool_sizes[3]
			{
				{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
				{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1},
				{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1},
			};

			VkDescriptorPoolCreateInfo descriptor_pool_ci{};
			descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptor_pool_ci.pNext = nullptr;
			descriptor_pool_ci.flags = 0;
			descriptor_pool_ci.maxSets = context.m_swapchain_image_cnt;
			descriptor_pool_ci.poolSizeCount = 3;
			descriptor_pool_ci.pPoolSizes = pool_sizes;

			check(vkCreateDescriptorPool(context.m_device, &descriptor_pool_ci, nullptr, &compute_descriptor_pool));
		}

		// Create Compute Descriptor Sets
		{
			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = compute_descriptor_pool;
			descriptor_set_ai.descriptorSetCount = context.m_swapchain_image_cnt;
			descriptor_set_ai.pSetLayouts = &compute_descriptor_set_layout;

			check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, compute_descriptor_sets));

			for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			{
				VkDescriptorImageInfo volume_info{};
				volume_info.sampler = nullptr;
				volume_info.imageView = volume_image_view;
				volume_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

				VkDescriptorBufferInfo voxel_color_info{};
				voxel_color_info.buffer = voxel_color_buffer;
				voxel_color_info.offset = 0;
				voxel_color_info.range = voxel_color_cnt;

				VkDescriptorImageInfo output_image_info{};
				output_image_info.imageView = context.m_swapchain_image_views[i];
				output_image_info.sampler = nullptr;
				output_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

				VkWriteDescriptorSet writes[2]{};
				// Volume
				writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[0].pNext = nullptr;
				writes[0].dstSet = compute_descriptor_sets[i];
				writes[0].dstBinding = 0;
				writes[0].dstArrayElement = 0;
				writes[0].descriptorCount = 1;
				writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				writes[0].pImageInfo = &volume_info;
				writes[0].pBufferInfo = nullptr;
				writes[0].pTexelBufferView = nullptr;
				// Voxel Colors
				writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[1].pNext = nullptr;
				writes[1].dstSet = compute_descriptor_sets[i];
				writes[1].dstBinding = 1;
				writes[1].dstArrayElement = 0;
				writes[1].descriptorCount = 1;
				writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
				writes[1].pImageInfo = nullptr;
				writes[1].pBufferInfo = &voxel_color_info;
				writes[1].pTexelBufferView = nullptr;
				// Output Image
				writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[1].pNext = nullptr;
				writes[1].dstSet = compute_descriptor_sets[i];
				writes[1].dstBinding = 2;
				writes[1].dstArrayElement = 0;
				writes[1].descriptorCount = 1;
				writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				writes[1].pImageInfo = &output_image_info;
				writes[1].pBufferInfo = nullptr;
				writes[1].pTexelBufferView = nullptr;

				vkUpdateDescriptorSets(context.m_device, 3, writes, 0, nullptr);
			}
		}

		// Create Sync Objects
		{
			VkSemaphoreCreateInfo semaphore_ci{};
			semaphore_ci.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
			semaphore_ci.pNext = nullptr;
			semaphore_ci.flags = 0;

			VkFenceCreateInfo fence_ci{};
			fence_ci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
			fence_ci.pNext = nullptr;
			fence_ci.flags = VK_FENCE_CREATE_SIGNALED_BIT;

			for (uint32_t i = 0; i != max_frames_in_flight; ++i)
			{
				check(vkCreateSemaphore(context.m_device, &semaphore_ci, nullptr, &image_available_semaphores[i]));

				check(vkCreateSemaphore(context.m_device, &semaphore_ci, nullptr, &render_complete_semaphores[i]));

				check(vkCreateFence(context.m_device, &fence_ci, nullptr, &inflight_fences[i]));
			}
		}

		och::print("Finished initialization. Time taken: {}\n\n", init_timer.read());

		return {};
	}

	err_info load_shader(VkShaderModule& out_shader_module, const char* filename) noexcept
	{
		och::mapped_file<uint32_t> shader_file(filename, och::fio::access_read, och::fio::open_normal, och::fio::open_fail);

		if (!shader_file)
			return MSG_ERROR("Could not open shader file");

		VkShaderModuleCreateInfo shader_module_ci{};
		shader_module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		shader_module_ci.pNext = nullptr;
		shader_module_ci.flags = 0;
		shader_module_ci.codeSize = shader_file.bytes();
		shader_module_ci.pCode = shader_file.data();

		check(vkCreateShaderModule(context.m_device, &shader_module_ci, nullptr, &out_shader_module));

		return {};
	}

	err_info create_graphics_pipeline() noexcept
	{
		VkPipelineShaderStageCreateInfo shader_ci[2]{};
		shader_ci[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_ci[0].pNext = nullptr;
		shader_ci[0].flags = 0;
		shader_ci[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_ci[0].module = ui_vert_shader;
		shader_ci[0].pName = "main";
		shader_ci[0].pSpecializationInfo = nullptr;

		shader_ci[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_ci[1].pNext = nullptr;
		shader_ci[1].flags = 0;
		shader_ci[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_ci[1].module = ui_frag_shader;
		shader_ci[1].pName = "main";
		shader_ci[1].pSpecializationInfo = nullptr;

		VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
		vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_ci.pNext = nullptr;
		vertex_input_ci.flags = 0;
		vertex_input_ci.vertexBindingDescriptionCount = 1;
		vertex_input_ci.pVertexBindingDescriptions = &ui_vertex::binding;
		vertex_input_ci.vertexAttributeDescriptionCount = ui_vertex::attribute_cnt;
		vertex_input_ci.pVertexAttributeDescriptions = ui_vertex::attributes;

		VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
		input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_ci.pNext = nullptr;
		input_assembly_ci.flags = 0;
		input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_ci.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0.0F;
		viewport.y = 0.0F;
		viewport.width = static_cast<float>(context.m_swapchain_extent.width);
		viewport.height = static_cast<float>(context.m_swapchain_extent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;

		VkRect2D scissor_rect{};
		scissor_rect.offset = { 0, 0 };
		scissor_rect.extent = context.m_swapchain_extent;

		VkPipelineViewportStateCreateInfo viewport_ci{};
		viewport_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_ci.pNext = nullptr;
		viewport_ci.flags = 0;
		viewport_ci.viewportCount = 1;
		viewport_ci.pViewports = &viewport;
		viewport_ci.scissorCount = 1;
		viewport_ci.pScissors = &scissor_rect;

		VkPipelineRasterizationStateCreateInfo rasterization_ci{};
		rasterization_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
		rasterization_ci.pNext = nullptr;
		rasterization_ci.flags = 0;
		rasterization_ci.depthClampEnable = VK_FALSE;
		rasterization_ci.rasterizerDiscardEnable = VK_FALSE;
		rasterization_ci.polygonMode = VK_POLYGON_MODE_FILL;
		rasterization_ci.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterization_ci.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
		rasterization_ci.depthBiasEnable = VK_FALSE;
		rasterization_ci.depthBiasConstantFactor = 0.0F;
		rasterization_ci.depthBiasClamp = 0.0F;
		rasterization_ci.depthBiasSlopeFactor = 0.0F;
		rasterization_ci.lineWidth = 1.0F;

		VkPipelineColorBlendAttachmentState color_blend_attachment{};
		color_blend_attachment.blendEnable = VK_TRUE;
		color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
		color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
		color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		color_blend_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo color_blend_ci{};
		color_blend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		color_blend_ci.pNext = nullptr;
		color_blend_ci.logicOpEnable = VK_FALSE;
		color_blend_ci.logicOp = VK_LOGIC_OP_COPY;
		color_blend_ci.attachmentCount = 1;
		color_blend_ci.pAttachments = &color_blend_attachment;
		color_blend_ci.blendConstants[0] = color_blend_ci.blendConstants[1] = color_blend_ci.blendConstants[2] = color_blend_ci.blendConstants[3] = 0.0F;

		VkGraphicsPipelineCreateInfo pipeline_ci{};
		pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_ci.pNext = nullptr;
		pipeline_ci.flags = 0;
		pipeline_ci.stageCount = 2;
		pipeline_ci.pStages = shader_ci;
		pipeline_ci.pVertexInputState = &vertex_input_ci;
		pipeline_ci.pInputAssemblyState = &input_assembly_ci;
		pipeline_ci.pTessellationState = nullptr;
		pipeline_ci.pViewportState = &viewport_ci;
		pipeline_ci.pRasterizationState = &rasterization_ci;
		pipeline_ci.pMultisampleState = nullptr;
		pipeline_ci.pDepthStencilState = nullptr;
		pipeline_ci.pColorBlendState = &color_blend_ci;
		pipeline_ci.pDynamicState = nullptr;
		pipeline_ci.layout = graphics_pipeline_layout;
		pipeline_ci.renderPass = renderpass;
		pipeline_ci.subpass = 0;
		pipeline_ci.basePipelineHandle = nullptr;
		pipeline_ci.basePipelineIndex = -1;

		check(vkCreateGraphicsPipelines(context.m_device, pipeline_cache, 1, &pipeline_ci, nullptr, &graphics_pipeline));

		return {};
	}

	err_info create_compute_pipeline() noexcept
	{
		VkPipelineShaderStageCreateInfo shader_ci{};
		shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_ci.pNext = nullptr;
		shader_ci.flags = 0;
		shader_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
		shader_ci.module = volume_shader;
		shader_ci.pName = "main";
		shader_ci.pSpecializationInfo = nullptr;

		VkComputePipelineCreateInfo pipeline_ci{};
		pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
		pipeline_ci.pNext = nullptr;
		pipeline_ci.flags = 0;
		pipeline_ci.stage = shader_ci;
		pipeline_ci.layout = compute_pipeline_layout;
		pipeline_ci.basePipelineHandle = nullptr;
		pipeline_ci.basePipelineIndex = -1;
	}

	err_info create_framebuffers()
	{
		for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
		{
			VkFramebufferCreateInfo framebuffer_ci{};
			framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_ci.pNext = nullptr;
			framebuffer_ci.flags = 0;
			framebuffer_ci.renderPass = renderpass;
			framebuffer_ci.attachmentCount = 1;
			framebuffer_ci.pAttachments = &context.m_swapchain_image_views[i];
			framebuffer_ci.width = context.m_swapchain_extent.width;
			framebuffer_ci.height = context.m_swapchain_extent.height;
			framebuffer_ci.layers = 1;

			check(vkCreateFramebuffer(context.m_device, &framebuffer_ci, nullptr, &framebuffers[i]));
		}

		return {};
	}

	err_info create_command_buffers()
	{
		VkCommandBufferAllocateInfo command_buffer_ai{};
		command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		command_buffer_ai.pNext = nullptr;
		command_buffer_ai.commandPool = command_pool;
		command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		command_buffer_ai.commandBufferCount = context.m_swapchain_image_cnt;

		check(vkAllocateCommandBuffers(context.m_device, &command_buffer_ai, command_buffers));

		for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
		{
			VkCommandBufferBeginInfo buffer_beg_info{};
			buffer_beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			buffer_beg_info.pNext = nullptr;
			buffer_beg_info.flags = 0;
			buffer_beg_info.pInheritanceInfo = nullptr;

			check(vkBeginCommandBuffer(command_buffers[i], &buffer_beg_info));

			// Record Compute Work

			vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_COMPUTE, compute_pipeline);

			vkCmdPushConstants(command_buffers[i], compute_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(compute_push_constant_range), &compute_push_constants);

			vkCmdDispatch(command_buffers[i], context.m_swapchain_extent.width, context.m_swapchain_extent.height, 1);

			// Record Graphics Work

			VkRenderPassBeginInfo pass_beg_info{};
			pass_beg_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
			pass_beg_info.renderPass = renderpass;
			pass_beg_info.framebuffer = framebuffers[i];
			pass_beg_info.renderArea.offset = { 0, 0 };
			pass_beg_info.renderArea.extent = context.m_swapchain_extent;
			pass_beg_info.clearValueCount = 0;
			pass_beg_info.pClearValues = nullptr;

			vkCmdBeginRenderPass(command_buffers[i], &pass_beg_info, VK_SUBPASS_CONTENTS_INLINE);

			vkCmdBindPipeline(command_buffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline);

			VkDeviceSize offsets[]{ 0 };
			vkCmdBindVertexBuffers(command_buffers[i], 0, 1, &ui_vertices, offsets);

			vkCmdBindIndexBuffer(command_buffers[i], ui_indices, 0, VK_INDEX_TYPE_UINT32);

			vkCmdDrawIndexed(command_buffers[i], ui_index_cnt, 1, 0, 0, 0);

			vkCmdEndRenderPass(command_buffers[i]);

			check(vkEndCommandBuffer(command_buffers[i]));
		}

		return {};
	}

	err_info recreate_swapchain()
	{
		// Wait for Window to be visible

		int width, height;

		glfwGetFramebufferSize(context.m_window, &width, &height);

		while (!width || !height)
		{
			glfwWaitEvents();

			glfwGetFramebufferSize(context.m_window, &width, &height);
		}



		// Wait for Device to become idle

		vkDeviceWaitIdle(context.m_device);



		// Free Resources

		vkFreeCommandBuffers(context.m_device, command_pool, context.m_swapchain_image_cnt, command_buffers);

		vkDestroyPipeline(context.m_device, graphics_pipeline, nullptr);

		for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			vkDestroyFramebuffer(context.m_device, framebuffers[i], nullptr);



		// Recreate Resources

		context.recreate_swapchain();

		check(create_framebuffers());

		check(create_graphics_pipeline());

		check(create_command_buffers());

		return {};
	}

	err_info draw_frame() noexcept
	{
		check(vkWaitForFences(context.m_device, 1, &inflight_fences[curr_frame], VK_TRUE, UINT64_MAX));

		uint32_t swapchain_image_idx;
		if (VkResult acquire_rst = vkAcquireNextImageKHR(context.m_device, context.m_swapchain, UINT64_MAX, image_available_semaphores[curr_frame], nullptr, &swapchain_image_idx); acquire_rst = VK_ERROR_OUT_OF_DATE_KHR)
		{
			recreate_swapchain();

			return {};
		}
		else if (acquire_rst != VK_SUBOPTIMAL_KHR)
			check(acquire_rst);

		if (swapchain_image_inflight_fences[swapchain_image_idx])
			check(vkWaitForFences(context.m_device, 1, &swapchain_image_inflight_fences[swapchain_image_idx], VK_TRUE, UINT64_MAX));

		check(vkResetFences(context.m_device, 1, &inflight_fences[curr_frame]));

		swapchain_image_inflight_fences[swapchain_image_idx] = inflight_fences[curr_frame];

		VkPipelineStageFlags submit_wait_stages = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = nullptr;
		submit_info.waitSemaphoreCount = 1;
		submit_info.pWaitSemaphores = &image_available_semaphores[curr_frame];
		submit_info.pWaitDstStageMask = &submit_wait_stages;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &command_buffers[swapchain_image_idx];
		submit_info.signalSemaphoreCount = 1;
		submit_info.pSignalSemaphores = &render_complete_semaphores[curr_frame];

		check(vkQueueSubmit(context.m_general_queues[0], 1, &submit_info, inflight_fences[curr_frame]));

		VkPresentInfoKHR present_info{};
		present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present_info.pNext = nullptr;
		present_info.waitSemaphoreCount = 1;
		present_info.pWaitSemaphores = &render_complete_semaphores[curr_frame];
		present_info.swapchainCount = 1;
		present_info.pSwapchains = &context.m_swapchain;
		present_info.pImageIndices = &swapchain_image_idx;
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

	err_info upload_image_data(VkImage dst, VkBuffer src, uint32_t width, uint32_t height, VkImageLayout dst_layout) const noexcept
	{
		VkCommandBuffer cmd;

		VkCommandBufferAllocateInfo cmd_ai{};
		cmd_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
		cmd_ai.pNext = nullptr;
		cmd_ai.commandPool = transfer_command_pool;
		cmd_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
		cmd_ai.commandBufferCount = 1;

		check(vkAllocateCommandBuffers(context.m_device, &cmd_ai, &cmd));

		VkCommandBufferBeginInfo beg_info{};
		beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beg_info.pNext = nullptr;
		beg_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
		beg_info.pInheritanceInfo = nullptr;

		check(vkBeginCommandBuffer(cmd, &beg_info));

		VkBufferImageCopy copy{};
		copy.bufferOffset = 0;
		copy.bufferRowLength = width;
		copy.bufferImageHeight = height;
		copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy.imageSubresource.mipLevel = 0;
		copy.imageSubresource.baseArrayLayer = 0;
		copy.imageSubresource.layerCount = 1;
		copy.imageOffset = { 0, 0, 0 };
		copy.imageExtent = { 0, 0, 0 };

		vkCmdCopyBufferToImage(cmd, src, dst, dst_layout, 1, &copy);

		check(vkEndCommandBuffer(cmd));

		VkSubmitInfo submit_info{};
		submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
		submit_info.pNext = nullptr;
		submit_info.waitSemaphoreCount = 0;
		submit_info.pWaitSemaphores = nullptr;
		submit_info.pWaitDstStageMask = 0;
		submit_info.commandBufferCount = 1;
		submit_info.pCommandBuffers = &cmd;
		submit_info.signalSemaphoreCount = 0;
		submit_info.pSignalSemaphores = nullptr;

		check(vkQueueSubmit(context.m_transfer_queues[0], 1, &submit_info, nullptr));

		vkFreeCommandBuffers(context.m_device, transfer_command_pool, 1, &cmd);

		return {};
	}
};
