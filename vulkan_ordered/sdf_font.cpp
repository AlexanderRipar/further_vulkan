#include "sdf_font.h"

#include "och_vulkan_base.h"
#include "och_fmt.h"
#include "och_heap_buffer.h"
#include "och_helpers.h"
#include "och_matmath.h"

#include "sdf_image.h"

struct compute_font
{
	struct font_vertex
	{
		och::vec3 pos;
	};

	static constexpr uint32_t GROUP_SZ_X = 8;
	static constexpr uint32_t GROUP_SZ_Y = 8;
	static constexpr uint32_t GROUP_SZ_Z = 1;
	static constexpr uint32_t MAX_FRAMES_INFLIGHT = 2;
	static constexpr uint32_t MAX_ATLAS_CHAR_CNT = 128;
	static constexpr uint32_t MAX_ATLAS_CHAR_SZ = 32;

	och::vulkan_context context;



	VkShaderModule vert_shader_module;

	VkShaderModule frag_shader_module;

	VkRenderPass render_pass;

	VkDescriptorSetLayout descriptor_set_layout;

	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;



	VkDescriptorPool descriptor_pool;

	VkDescriptorSet descriptor_sets[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkCommandPool command_pool;

	VkCommandBuffer command_buffers[MAX_FRAMES_INFLIGHT];



	VkSemaphore render_complete_semaphores[MAX_FRAMES_INFLIGHT];

	VkSemaphore image_available_semaphores[MAX_FRAMES_INFLIGHT];

	VkFence frame_inflight_fences[MAX_FRAMES_INFLIGHT];

	VkFence image_inflight_fences[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};



	VkImage font_image;

	VkImageView font_image_view;

	VkDeviceMemory font_image_memory;



	och::err_info create()
	{
		check(context.create("Compute Font", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));

		// Create Render Pass
		{
			VkAttachmentDescription color_attachment_description{};
			color_attachment_description.flags = 0;
			color_attachment_description.format = context.m_swapchain_format;
			color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
			color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
			color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			color_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			color_attachment_description.initialLayout = VK_IMAGE_LAYOUT_GENERAL;
			color_attachment_description.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

			VkAttachmentReference color_attachment_reference{};
			color_attachment_reference.attachment = 0;
			color_attachment_reference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass_description{};
			subpass_description.flags = 0;
			subpass_description.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
			subpass_description.inputAttachmentCount = 0;
			subpass_description.pInputAttachments = nullptr;
			subpass_description.colorAttachmentCount = 1;
			subpass_description.pColorAttachments = &color_attachment_reference;
			subpass_description.pResolveAttachments = nullptr;
			subpass_description.pDepthStencilAttachment = nullptr;
			subpass_description.preserveAttachmentCount = 0;
			subpass_description.pPreserveAttachments = nullptr;

			VkRenderPassCreateInfo renderpass_ci{};
			renderpass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderpass_ci.pNext = nullptr;
			renderpass_ci.flags = 0;
			renderpass_ci.attachmentCount = 1;
			renderpass_ci.pAttachments = &color_attachment_description;
			renderpass_ci.subpassCount = 1;
			renderpass_ci.pSubpasses = &subpass_description;
			renderpass_ci.dependencyCount = 0;									// TODO
			renderpass_ci.pDependencies = nullptr;								// TODO

			check(vkCreateRenderPass(context.m_device, &renderpass_ci, nullptr, &render_pass));
		}

		// Create Graphics Pipeline
		{
			check(context.load_shader_module_file(vert_shader_module, "shaders/sdf_font.vert.spv"));

			check(context.load_shader_module_file(frag_shader_module, "shaders/sdf_font.frag.spv"));

			VkDescriptorSetLayoutBinding descriptor_binding{};
			// Font SDF-Atlas
			descriptor_binding.binding = 0;
			descriptor_binding.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
			descriptor_binding.descriptorCount = 1;
			descriptor_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
			descriptor_binding.pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci{};
			descriptor_set_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptor_set_layout_ci.pNext = nullptr;
			descriptor_set_layout_ci.flags = 0;
			descriptor_set_layout_ci.bindingCount = 1;
			descriptor_set_layout_ci.pBindings = &descriptor_binding;

			check(vkCreateDescriptorSetLayout(context.m_device, &descriptor_set_layout_ci, nullptr, &descriptor_set_layout));

			VkPipelineLayoutCreateInfo pipeline_layout_ci{};
			pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipeline_layout_ci.pNext = nullptr;
			pipeline_layout_ci.flags = 0;
			pipeline_layout_ci.setLayoutCount = 1;
			pipeline_layout_ci.pSetLayouts = &descriptor_set_layout;
			pipeline_layout_ci.pushConstantRangeCount = 0;
			pipeline_layout_ci.pPushConstantRanges = nullptr;

			check(vkCreatePipelineLayout(context.m_device, &pipeline_layout_ci, nullptr, &pipeline_layout));

			check(create_pipeline());
		}

		// Create Descriptor Sets
		{
			VkDescriptorPoolSize pool_size{};
			pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			pool_size.descriptorCount = context.m_swapchain_image_cnt;

			VkDescriptorPoolCreateInfo descriptor_pool_ci{};
			descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptor_pool_ci.pNext = nullptr;
			descriptor_pool_ci.flags = 0;
			descriptor_pool_ci.maxSets = context.m_swapchain_image_cnt;
			descriptor_pool_ci.poolSizeCount = 1;
			descriptor_pool_ci.pPoolSizes = &pool_size;

			check(vkCreateDescriptorPool(context.m_device, &descriptor_pool_ci, nullptr, &descriptor_pool));

			VkDescriptorSetLayout set_layouts[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];
			for (auto& l : set_layouts) l = descriptor_set_layout;

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = descriptor_pool;
			descriptor_set_ai.descriptorSetCount = context.m_swapchain_image_cnt;
			descriptor_set_ai.pSetLayouts = set_layouts;

			check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, descriptor_sets));

			VkDescriptorImageInfo image_infos[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

			VkWriteDescriptorSet writes[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

			for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			{
				image_infos[i].sampler = nullptr;
				image_infos[i].imageView = context.m_swapchain_image_views[i];
				image_infos[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

				writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
				writes[i].pNext = nullptr;
				writes[i].dstSet = descriptor_sets[i];
				writes[i].dstBinding = 0;
				writes[i].dstArrayElement = 0;
				writes[i].descriptorCount = 1;
				writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
				writes[i].pImageInfo = &image_infos[i];
				writes[i].pBufferInfo = nullptr;
				writes[i].pTexelBufferView = nullptr;
			}

			vkUpdateDescriptorSets(context.m_device, context.m_swapchain_image_cnt, writes, 0, nullptr);
		}

		// Create Command Buffers
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
			command_pool_ci.queueFamilyIndex = context.m_general_queues.family_index;

			check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &command_pool));

			VkCommandBufferAllocateInfo command_buffer_ai{};
			command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_ai.pNext = nullptr;
			command_buffer_ai.commandPool = command_pool;
			command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_ai.commandBufferCount = MAX_FRAMES_INFLIGHT;

			check(vkAllocateCommandBuffers(context.m_device, &command_buffer_ai, command_buffers));
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

			for (uint32_t i = 0; i != MAX_FRAMES_INFLIGHT; ++i)
			{
				check(vkCreateSemaphore(context.m_device, &semaphore_ci, nullptr, &image_available_semaphores[i]));

				check(vkCreateSemaphore(context.m_device, &semaphore_ci, nullptr, &render_complete_semaphores[i]));

				check(vkCreateFence(context.m_device, &fence_ci, nullptr, &frame_inflight_fences[i]));
			}
		}

		// Create SDF Font Data
		check(create_sdf_image());

		return {};
	}

	och::err_info run()
	{


		return {};
	}

	void destroy()
	{
		if (!context.m_device || vkDeviceWaitIdle(context.m_device) != VK_SUCCESS)
			return;

		for(auto& s : image_available_semaphores)
			vkDestroySemaphore(context.m_device, s, nullptr);

		for(auto& s : render_complete_semaphores)
			vkDestroySemaphore(context.m_device, s, nullptr);

		for(auto& f : frame_inflight_fences)
			vkDestroyFence(context.m_device, f, nullptr);
		
		vkDestroyShaderModule(context.m_device, vert_shader_module, nullptr);

		vkDestroyShaderModule(context.m_device, frag_shader_module, nullptr);

		vkDestroyRenderPass(context.m_device, render_pass, nullptr);

		vkDestroyDescriptorSetLayout(context.m_device, descriptor_set_layout, nullptr);

		vkDestroyPipelineLayout(context.m_device, pipeline_layout, nullptr);

		vkDestroyPipeline(context.m_device, pipeline, nullptr);

		vkDestroyCommandPool(context.m_device, command_pool, nullptr);

		vkDestroyDescriptorPool(context.m_device, descriptor_pool, nullptr);

		context.destroy();
	}

	och::err_info recreate_swapchain()
	{
		return {};
	}

	och::err_info create_pipeline() noexcept
	{
		VkPipelineShaderStageCreateInfo shader_stages[2];
		// Vertex
		shader_stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[0].pNext = nullptr;
		shader_stages[0].flags = 0;
		shader_stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
		shader_stages[0].module = vert_shader_module;
		shader_stages[0].pName = "main";
		shader_stages[0].pSpecializationInfo = nullptr;
		// Fragment
		shader_stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		shader_stages[1].pNext = nullptr;
		shader_stages[1].flags = 0;
		shader_stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
		shader_stages[1].module; frag_shader_module;
		shader_stages[1].pName = "main";
		shader_stages[1].pSpecializationInfo = nullptr;

		VkVertexInputBindingDescription vertex_binding_description{};
		vertex_binding_description.binding = 0;
		vertex_binding_description.stride = sizeof(font_vertex);
		vertex_binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		VkVertexInputAttributeDescription vertex_attribute_description{};
		vertex_attribute_description.location = 0;
		vertex_attribute_description.binding = 0;
		vertex_attribute_description.format = VK_FORMAT_R32G32B32_SFLOAT;
		vertex_attribute_description.offset = offsetof(font_vertex, pos);

		VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
		vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_ci.pNext = nullptr;
		vertex_input_ci.flags = 0;
		vertex_input_ci.vertexBindingDescriptionCount = 1;
		vertex_input_ci.pVertexBindingDescriptions = &vertex_binding_description;
		vertex_input_ci.vertexAttributeDescriptionCount = 1;
		vertex_input_ci.pVertexAttributeDescriptions = &vertex_attribute_description;

		VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
		input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_ci.pNext = nullptr;
		input_assembly_ci.flags = 0;
		input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_ci.primitiveRestartEnable = VK_FALSE;

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(context.m_swapchain_extent.width);
		viewport.height = static_cast<float>(context.m_swapchain_extent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = context.m_swapchain_extent;

		VkPipelineViewportStateCreateInfo viewport_ci{};
		viewport_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
		viewport_ci.pNext = nullptr;
		viewport_ci.flags = 0;
		viewport_ci.viewportCount = 1;
		viewport_ci.pViewports = &viewport;
		viewport_ci.scissorCount = 1;
		viewport_ci.pScissors = &scissor;

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

		VkPipelineMultisampleStateCreateInfo multisample_ci{};
		multisample_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
		multisample_ci.pNext = nullptr;
		multisample_ci.flags = 0;
		multisample_ci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
		multisample_ci.sampleShadingEnable = VK_FALSE;
		multisample_ci.minSampleShading = 1.0F;
		multisample_ci.pSampleMask = nullptr;
		multisample_ci.alphaToCoverageEnable = VK_FALSE;
		multisample_ci.alphaToOneEnable = VK_FALSE;

		VkPipelineColorBlendAttachmentState colorblend_state_attachment{};
		colorblend_state_attachment.blendEnable = VK_FALSE;
		colorblend_state_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
		colorblend_state_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorblend_state_attachment.colorBlendOp = VK_BLEND_OP_ADD;
		colorblend_state_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
		colorblend_state_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
		colorblend_state_attachment.alphaBlendOp = VK_BLEND_OP_ADD;
		colorblend_state_attachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

		VkPipelineColorBlendStateCreateInfo colorblend_ci{};
		colorblend_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
		colorblend_ci.pNext = nullptr;
		colorblend_ci.flags = 0;
		colorblend_ci.logicOpEnable = VK_FALSE;
		colorblend_ci.logicOp = VK_LOGIC_OP_COPY;
		colorblend_ci.attachmentCount = 1;
		colorblend_ci.pAttachments = &colorblend_state_attachment;
		colorblend_ci.blendConstants[0] = 0.0F;
		colorblend_ci.blendConstants[1] = 0.0F;
		colorblend_ci.blendConstants[2] = 0.0F;
		colorblend_ci.blendConstants[3] = 0.0F;

		VkGraphicsPipelineCreateInfo pipeline_ci{};
		pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
		pipeline_ci.pNext = nullptr;
		pipeline_ci.flags = 0;
		pipeline_ci.stageCount = 2;
		pipeline_ci.pStages = shader_stages;
		pipeline_ci.pVertexInputState = &vertex_input_ci;
		pipeline_ci.pInputAssemblyState = &input_assembly_ci;
		pipeline_ci.pTessellationState = nullptr;
		pipeline_ci.pViewportState = &viewport_ci;
		pipeline_ci.pRasterizationState = &rasterization_ci;
		pipeline_ci.pMultisampleState = &multisample_ci;
		pipeline_ci.pDepthStencilState = nullptr;
		pipeline_ci.pColorBlendState = &colorblend_ci;
		pipeline_ci.pDynamicState = nullptr;
		pipeline_ci.layout = pipeline_layout;
		pipeline_ci.renderPass = render_pass;
		pipeline_ci.subpass = 0;
		pipeline_ci.basePipelineHandle = nullptr;
		pipeline_ci.basePipelineIndex = 0;

		check(vkCreateGraphicsPipelines(context.m_device, nullptr, 1, &pipeline_ci, nullptr, &pipeline));

		return {};
	}

	och::err_info create_sdf_image() noexcept
	{
		return {};
	}
};

och::err_info run_sdf_font()
{
	compute_font program{};

	och::err_info err = program.create();

	if (!err)
		err = program.run();

	program.destroy();

	check(err);

	return {};
}
