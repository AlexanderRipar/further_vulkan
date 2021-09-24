#include "sdf_font.h"

#include "och_vulkan_base.h"
#include "och_fmt.h"
#include "och_heap_buffer.h"
#include "och_helpers.h"
#include "och_matmath.h"

#include "sdf_glyph_atlas.h"

struct compute_font
{
	struct font_vertex
	{
		och::vec2 atlas_pos;
		och::vec2 screen_pos;
	};

	static constexpr uint32_t MAX_FRAMES_INFLIGHT = 2;

	static constexpr uint32_t MAX_DISPLAY_CHARS = 1024;
	static_assert(MAX_DISPLAY_CHARS < UINT16_MAX / 4);

	och::vulkan_context context;



	VkShaderModule vert_shader_module;

	VkShaderModule frag_shader_module;

	VkRenderPass render_pass;

	VkDescriptorSetLayout descriptor_set_layout;

	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;



	VkDescriptorPool descriptor_pool;

	VkDescriptorSet descriptor_set;

	VkCommandPool command_pool;

	VkCommandBuffer command_buffers[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkFramebuffer frame_buffers[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];



	VkSemaphore render_complete_semaphores[MAX_FRAMES_INFLIGHT];

	VkSemaphore image_available_semaphores[MAX_FRAMES_INFLIGHT];

	VkFence frame_inflight_fences[MAX_FRAMES_INFLIGHT];

	VkFence image_inflight_fences[och::vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};



	VkImage font_image{};

	VkImageView font_image_view{};

	VkDeviceMemory font_image_memory{};

	VkSampler font_sampler{};

	glyph_atlas m_glf_atlas;



	VkBuffer vertex_buffer{};

	VkDeviceMemory vertex_buffer_memory{};

	VkBuffer index_buffer{};

	VkDeviceMemory index_buffer_memory{};



	uint32_t input_cnt{};

	char32_t input_buffer[MAX_DISPLAY_CHARS]{};



	och::err_info create()
	{
		check(context.create("Compute Font", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));

		// Create Render Pass
		{
			VkAttachmentDescription color_attachment_description{};
			color_attachment_description.flags = 0;
			color_attachment_description.format = context.m_swapchain_format;
			color_attachment_description.samples = VK_SAMPLE_COUNT_1_BIT;
			color_attachment_description.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			color_attachment_description.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			color_attachment_description.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			color_attachment_description.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			color_attachment_description.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
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

			VkSubpassDependency subpass_dependency{};
			subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			subpass_dependency.dstSubpass = 0;
			subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			subpass_dependency.srcAccessMask = 0;
			subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
			subpass_dependency.dependencyFlags = 0;

			VkRenderPassCreateInfo renderpass_ci{};
			renderpass_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderpass_ci.pNext = nullptr;
			renderpass_ci.flags = 0;
			renderpass_ci.attachmentCount = 1;
			renderpass_ci.pAttachments = &color_attachment_description;
			renderpass_ci.subpassCount = 1;
			renderpass_ci.pSubpasses = &subpass_description;
			renderpass_ci.dependencyCount = 1;
			renderpass_ci.pDependencies = &subpass_dependency;

			check(vkCreateRenderPass(context.m_device, &renderpass_ci, nullptr, &render_pass));
		}

		// Create Framebuffers
		{
			VkExtent2D loaded_swapchain_extent = context.m_swapchain_extent.load(std::memory_order::acquire);

			for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			{
				VkFramebufferCreateInfo framebuffer_ci{};
				framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebuffer_ci.pNext = nullptr;
				framebuffer_ci.flags = 0;
				framebuffer_ci.renderPass = render_pass;
				framebuffer_ci.attachmentCount = 1;
				framebuffer_ci.pAttachments = context.m_swapchain_image_views + i;
				framebuffer_ci.width = loaded_swapchain_extent.width;
				framebuffer_ci.height = loaded_swapchain_extent.height;
				framebuffer_ci.layers = 1;

				check(vkCreateFramebuffer(context.m_device, &framebuffer_ci, nullptr, frame_buffers + i));
			}
		}

		// Create Graphics Pipeline
		{
			check(context.load_shader_module_file(vert_shader_module, "shaders/sdf_font.vert.spv"));

			check(context.load_shader_module_file(frag_shader_module, "shaders/sdf_font.frag.spv"));

			VkDescriptorSetLayoutBinding descriptor_binding{};
			// Font SDF-Atlas
			descriptor_binding.binding = 0;
			descriptor_binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
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

		// Create SDF Glyph Atlas
		{
			codept_range ranges[1]{ {32, 95} };

			constexpr float clamp = 0.015625F * 2.0F;

			check(m_glf_atlas.create("C:/Windows/Fonts/consola.ttf", 64, 2, clamp, 1024, och::range(ranges)));
		}

		// Allocate Device Image and Imageview to hold SDF Glyph Atlas
		{
			VkImageCreateInfo image_ci{};
			image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image_ci.pNext = nullptr;
			image_ci.flags = 0;
			image_ci.imageType = VK_IMAGE_TYPE_2D;
			image_ci.format = VK_FORMAT_R8_UNORM;
			image_ci.extent.width = m_glf_atlas.width();
			image_ci.extent.height = m_glf_atlas.height();
			image_ci.extent.depth = 1;
			image_ci.mipLevels = 1;
			image_ci.arrayLayers = 1;
			image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
			image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
			image_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
			image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image_ci.queueFamilyIndexCount = 1;
			image_ci.pQueueFamilyIndices = &context.m_general_queues.family_index;
			image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			check(vkCreateImage(context.m_device, &image_ci, nullptr, &font_image));

			VkMemoryRequirements image_memory_reqs{};
			vkGetImageMemoryRequirements(context.m_device, font_image, &image_memory_reqs);

			uint32_t image_mem_type_idx;
			
			check(context.suitable_memory_type_idx(image_mem_type_idx, image_memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

			VkMemoryAllocateInfo image_mem_ai{};
			image_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			image_mem_ai.pNext = nullptr;
			image_mem_ai.allocationSize = image_memory_reqs.size;
			image_mem_ai.memoryTypeIndex = image_mem_type_idx;
			check(vkAllocateMemory(context.m_device, &image_mem_ai, nullptr, &font_image_memory));

			check(vkBindImageMemory(context.m_device, font_image, font_image_memory, 0));



			VkImageViewCreateInfo image_view_ci{};
			image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			image_view_ci.pNext = nullptr;
			image_view_ci.flags = 0;
			image_view_ci.image = font_image;
			image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			image_view_ci.format = VK_FORMAT_R8_UNORM;
			image_view_ci.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_view_ci.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_view_ci.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_view_ci.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
			image_view_ci.subresourceRange;
			check(vkCreateImageView(context.m_device, &image_view_ci, nullptr, &font_image_view));
		}

		// Push SDF Glyph Atlas Image to Device
		{
			// Create a staging buffer

			VkBuffer staging_buffer;

			VkDeviceMemory staging_buffer_memory;

			VkBufferCreateInfo buffer_ci{};
			buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_ci.pNext = nullptr;
			buffer_ci.flags = 0;
			buffer_ci.size = m_glf_atlas.width() * m_glf_atlas.height();
			buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer_ci.queueFamilyIndexCount = 1;
			buffer_ci.pQueueFamilyIndices = &context.m_general_queues.family_index;
			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &staging_buffer));

			VkMemoryRequirements buffer_memory_reqs{};
			vkGetBufferMemoryRequirements(context.m_device, staging_buffer, &buffer_memory_reqs);

			uint32_t buffer_mem_type_idx;
			
			check(context.suitable_memory_type_idx(buffer_mem_type_idx, buffer_memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
			
			VkMemoryAllocateInfo buffer_mem_ai{};
			buffer_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			buffer_mem_ai.pNext = nullptr;
			buffer_mem_ai.allocationSize = buffer_memory_reqs.size;
			buffer_mem_ai.memoryTypeIndex = buffer_mem_type_idx;

			check(vkAllocateMemory(context.m_device, &buffer_mem_ai, nullptr, &staging_buffer_memory));

			check(vkBindBufferMemory(context.m_device, staging_buffer, staging_buffer_memory, 0));

			// Copy data to the staging buffer

			void* buffer_ptr;
			check(vkMapMemory(context.m_device, staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &buffer_ptr));

			memcpy(buffer_ptr, m_glf_atlas.data(), m_glf_atlas.width() * m_glf_atlas.height());

			vkUnmapMemory(context.m_device, staging_buffer_memory);

			// Copy data from staging buffer to image

			VkCommandBuffer staging_command_buffer;

			check(context.begin_onetime_command(staging_command_buffer, command_pool));

			VkImageMemoryBarrier transition_barrier{};
			transition_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			transition_barrier.pNext = nullptr;
			transition_barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
			transition_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			transition_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			transition_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transition_barrier.image = font_image;
			transition_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			transition_barrier.subresourceRange.baseMipLevel = 0;
			transition_barrier.subresourceRange.levelCount = 1;
			transition_barrier.subresourceRange.baseArrayLayer = 0;
			transition_barrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(staging_command_buffer, VK_PIPELINE_STAGE_NONE_KHR, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transition_barrier);

			VkBufferImageCopy buf_img_copy{};
			buf_img_copy.bufferOffset = 0;
			buf_img_copy.bufferRowLength = m_glf_atlas.width();
			buf_img_copy.bufferImageHeight = m_glf_atlas.height();
			buf_img_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			buf_img_copy.imageSubresource.mipLevel = 1;
			buf_img_copy.imageSubresource.baseArrayLayer = 1;
			buf_img_copy.imageSubresource.layerCount = 1;
			buf_img_copy.imageOffset.x = 0;
			buf_img_copy.imageOffset.y = 0;
			buf_img_copy.imageOffset.z = 0;
			buf_img_copy.imageExtent.width = m_glf_atlas.width();
			buf_img_copy.imageExtent.height = m_glf_atlas.height();
			buf_img_copy.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(staging_command_buffer, staging_buffer, font_image, VK_IMAGE_LAYOUT_UNDEFINED, 0, nullptr);

			context.submit_onetime_command(staging_command_buffer, command_pool, context.m_general_queues[0], true);

			vkDestroyBuffer(context.m_device, staging_buffer, nullptr);

			vkFreeMemory(context.m_device, staging_buffer_memory, nullptr);
		}

		// Create Sampler for SDF
		{
			VkSamplerCreateInfo sampler_ci{};
			sampler_ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
			sampler_ci.pNext = nullptr;
			sampler_ci.flags = 0;
			sampler_ci.magFilter = VK_FILTER_LINEAR;
			sampler_ci.minFilter = VK_FILTER_LINEAR;
			sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
			sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
			sampler_ci.mipLodBias = 0.0F;
			sampler_ci.anisotropyEnable = VK_FALSE;
			sampler_ci.maxAnisotropy = 0.0F;
			sampler_ci.compareEnable = VK_FALSE;
			sampler_ci.compareOp = VK_COMPARE_OP_NEVER;
			sampler_ci.minLod = 0.0F;
			sampler_ci.maxLod = VK_LOD_CLAMP_NONE;
			sampler_ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
			sampler_ci.unnormalizedCoordinates = VK_FALSE;

			check(vkCreateSampler(context.m_device, &sampler_ci, nullptr, &font_sampler));
		}

		// Create Descriptor Pool and -Set
		{
			VkDescriptorPoolSize pool_size{};
			pool_size.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			pool_size.descriptorCount = 1;

			VkDescriptorPoolCreateInfo descriptor_pool_ci{};
			descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptor_pool_ci.pNext = nullptr;
			descriptor_pool_ci.flags = 0;
			descriptor_pool_ci.maxSets = 1;
			descriptor_pool_ci.poolSizeCount = 1;
			descriptor_pool_ci.pPoolSizes = &pool_size;
			check(vkCreateDescriptorPool(context.m_device, &descriptor_pool_ci, nullptr, &descriptor_pool));

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = descriptor_pool;
			descriptor_set_ai.descriptorSetCount = 1;
			descriptor_set_ai.pSetLayouts = &descriptor_set_layout;

			check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, &descriptor_set));

			VkDescriptorImageInfo image_info;
			image_info.sampler = font_sampler;
			image_info.imageView = font_image_view;
			image_info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

			VkWriteDescriptorSet write;
			write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write.pNext = nullptr;
			write.dstSet = descriptor_set;
			write.dstBinding = 0;
			write.dstArrayElement = 0;
			write.descriptorCount = 1;
			write.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
			write.pImageInfo = &image_info;
			write.pBufferInfo = nullptr;
			write.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(context.m_device, 1, &write, 0, nullptr);
		}

		// Create Vertex Buffer
		{
			VkBufferCreateInfo buffer_ci{};
			buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_ci.pNext = nullptr;
			buffer_ci.flags = 0;
			buffer_ci.size = sizeof(font_vertex) * MAX_DISPLAY_CHARS * 4;
			buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer_ci.queueFamilyIndexCount = 1;
			buffer_ci.pQueueFamilyIndices = &context.m_general_queues.family_index;

			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &vertex_buffer));

			VkMemoryRequirements memory_reqs{};

			vkGetBufferMemoryRequirements(context.m_device, vertex_buffer, &memory_reqs);

			uint32_t memory_type_idx;

			check(context.suitable_memory_type_idx(memory_type_idx, memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

			VkMemoryAllocateInfo memory_ai{};
			memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memory_ai.pNext = nullptr;
			memory_ai.allocationSize = memory_reqs.size;
			memory_ai.memoryTypeIndex = memory_type_idx;

			check(vkAllocateMemory(context.m_device, &memory_ai, nullptr, &vertex_buffer_memory));

			check(vkBindBufferMemory(context.m_device, vertex_buffer, vertex_buffer_memory, 0));
		}

		// Create Index Buffer
		{
			VkBufferCreateInfo buffer_ci{};
			buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_ci.pNext = nullptr;
			buffer_ci.flags = 0;
			buffer_ci.size = 2 * 6 * MAX_DISPLAY_CHARS;
			buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
			buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer_ci.queueFamilyIndexCount = 1;
			buffer_ci.pQueueFamilyIndices = &context.m_general_queues.family_index;

			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &index_buffer));

			VkMemoryRequirements memory_reqs{};

			vkGetBufferMemoryRequirements(context.m_device, index_buffer, &memory_reqs);

			uint32_t memory_type_idx;

			check(context.suitable_memory_type_idx(memory_type_idx, memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

			VkMemoryAllocateInfo memory_ai{};
			memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memory_ai.pNext = nullptr;
			memory_ai.allocationSize = memory_reqs.size;
			memory_ai.memoryTypeIndex = memory_type_idx;

			check(vkAllocateMemory(context.m_device, &memory_ai, nullptr, &index_buffer_memory));

			check(vkBindBufferMemory(context.m_device, index_buffer, index_buffer_memory, 0));
		}

		// Create Command Buffers
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = 0;
			command_pool_ci.queueFamilyIndex = context.m_general_queues.family_index;
			check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &command_pool));

			VkCommandBufferAllocateInfo command_buffer_ai{};
			command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_ai.pNext = nullptr;
			command_buffer_ai.commandPool = command_pool;
			command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_ai.commandBufferCount = context.m_swapchain_image_cnt;
			check(vkAllocateCommandBuffers(context.m_device, &command_buffer_ai, command_buffers));
		}

		

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

	och::err_info record_command_buffer(VkCommandBuffer command_buffer, uint32_t swapchain_idx) noexcept
	{
		VkCommandBufferBeginInfo command_buffer_bi{};
		command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_bi.pNext = nullptr;
		command_buffer_bi.flags = 0;
		command_buffer_bi.pInheritanceInfo = nullptr;
		check(vkBeginCommandBuffer(command_buffer, &command_buffer_bi));

		VkClearValue clear_value = { 0.0F, 0.0F, 0.0F, 1.0F };

		VkRenderPassBeginInfo render_pass_bi{};
		render_pass_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_bi.pNext = nullptr;
		render_pass_bi.renderPass = render_pass;
		render_pass_bi.framebuffer = frame_buffers[swapchain_idx];
		render_pass_bi.renderArea.offset.x = 0;
		render_pass_bi.renderArea.offset.y = 0;
		render_pass_bi.renderArea.extent = context.m_swapchain_extent.load(std::memory_order::acquire);
		render_pass_bi.clearValueCount = 1;
		render_pass_bi.pClearValues = &clear_value;

		vkCmdBeginRenderPass(command_buffer, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		// TODO
		// vkCmdBindVertexBuffers(command_buffer, ...);

		// TODO
		// vkCmdBindIndexBuffer(command_buffer, ...);

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

		// TODO
		// vkCmdDrawIndexed(command_buffer, ...);

		vkCmdEndRenderPass(command_buffer);

		check(vkEndCommandBuffer(command_buffer));
	}

	och::err_info recreate_swapchain()
	{
		context.recreate_swapchain();

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

		VkVertexInputAttributeDescription vertex_attribute_descs[2]{};
		// atlas_pos
		vertex_attribute_descs[0].location = 0;
		vertex_attribute_descs[0].binding = 0;
		vertex_attribute_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descs[0].offset = offsetof(font_vertex, atlas_pos);
		// screen_pos
		vertex_attribute_descs[0].location = 0;
		vertex_attribute_descs[0].binding = 0;
		vertex_attribute_descs[0].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descs[0].offset = offsetof(font_vertex, screen_pos);

		VkPipelineVertexInputStateCreateInfo vertex_input_ci{};
		vertex_input_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
		vertex_input_ci.pNext = nullptr;
		vertex_input_ci.flags = 0;
		vertex_input_ci.vertexBindingDescriptionCount = 1;
		vertex_input_ci.pVertexBindingDescriptions = &vertex_binding_description;
		vertex_input_ci.vertexAttributeDescriptionCount = 2;
		vertex_input_ci.pVertexAttributeDescriptions = vertex_attribute_descs;

		VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
		input_assembly_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
		input_assembly_ci.pNext = nullptr;
		input_assembly_ci.flags = 0;
		input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
		input_assembly_ci.primitiveRestartEnable = VK_FALSE;

		VkExtent2D loaded_swapchain_extent = context.m_swapchain_extent.load(std::memory_order::acquire);

		VkViewport viewport{};
		viewport.x = 0;
		viewport.y = 0;
		viewport.width = static_cast<float>(loaded_swapchain_extent.width);
		viewport.height = static_cast<float>(loaded_swapchain_extent.height);
		viewport.minDepth = 0.0F;
		viewport.maxDepth = 1.0F;

		VkRect2D scissor{};
		scissor.offset = { 0, 0 };
		scissor.extent = context.m_swapchain_extent.load(std::memory_order::acquire);

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
