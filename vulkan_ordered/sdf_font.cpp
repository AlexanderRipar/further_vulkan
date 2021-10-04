#include "sdf_font.h"

#include <cmath>

#include "vulkan_base.h"
#include "och_heap_buffer.h"
#include "och_helpers.h"
#include "och_matmath.h"
#include "och_timer.h"

#include "sdf_glyph_atlas.h"

#include "och_fmt.h"

struct sdf_font
{
	struct font_vertex
	{
		och::vec2 atlas_pos;
		och::vec2 screen_pos;
	};

	static constexpr uint32_t MAX_FRAMES_INFLIGHT = 2;

	static constexpr uint32_t MAX_DISPLAY_CHARS = 1024;
	static_assert(MAX_DISPLAY_CHARS < UINT16_MAX / 6);

	static constexpr float DISPLAY_SCALE = 0.25F;

	static constexpr float DISPLAY_MIN_X = -1.0F;
	static constexpr float DISPLAY_MIN_Y = -1.0F;
	static constexpr float DISPLAY_MAX_X =  1.0F;
	static constexpr float DISPLAY_MAX_Y =  1.0F;

	vulkan_context context;



	VkShaderModule vert_shader_module;

	VkShaderModule frag_shader_module;

	VkRenderPass render_pass;

	VkDescriptorSetLayout descriptor_set_layout;

	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;



	VkDescriptorPool descriptor_pool;

	VkDescriptorSet descriptor_set;

	VkCommandPool command_pool;

	VkCommandBuffer command_buffers[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkFramebuffer frame_buffers[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];



	VkSemaphore render_complete_semaphores[MAX_FRAMES_INFLIGHT];

	VkSemaphore image_available_semaphores[MAX_FRAMES_INFLIGHT];

	VkFence frame_inflight_fences[MAX_FRAMES_INFLIGHT];

	VkFence image_inflight_fences[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};



	VkImage font_image{};

	VkImageView font_image_view{};

	VkDeviceMemory font_image_memory{};

	VkSampler font_sampler{};

	glyph_atlas atlas;



	VkBuffer vertex_buffer{};

	VkDeviceMemory vertex_buffer_memory{};

	VkBuffer index_buffer{};

	VkDeviceMemory index_buffer_memory{};

	VkBuffer staging_buffer{};

	VkDeviceMemory staging_buffer_memory{};

	VkCommandPool staging_command_pool{};

	och::time begin_time;

	uint32_t frame_idx{};

	uint32_t input_cnt{};

	och::vec2 input_pos{ DISPLAY_MIN_X, DISPLAY_MIN_Y };

	uint32_t pos_history_idx{};

	char32_t input_buffer[MAX_DISPLAY_CHARS]{};

	och::vec2 pos_history[MAX_DISPLAY_CHARS]{};



	och::status create()
	{
		och::mat4 test_mat = och::mat4::translate(1.0F, 2.0F, 3.0F);

		test_mat = transpose(test_mat);

		och::print("{}\n", test_mat);

		check(context.create("Compute Font", 1440, 810, 1, 0, 0));

		// Create Staging Command Pool
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
			command_pool_ci.queueFamilyIndex = context.m_general_queues.family_index;

			check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &staging_command_pool));
		}

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
			VkExtent2D loaded_swapchain_extent = context.m_swapchain_extent;

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
			
			VkPushConstantRange push_constant_range{};
			push_constant_range.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
			push_constant_range.offset = 0;
			push_constant_range.size = sizeof(och::mat4);

			VkPipelineLayoutCreateInfo pipeline_layout_ci{};
			pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipeline_layout_ci.pNext = nullptr;
			pipeline_layout_ci.flags = 0;
			pipeline_layout_ci.setLayoutCount = 1;
			pipeline_layout_ci.pSetLayouts = &descriptor_set_layout;
			pipeline_layout_ci.pushConstantRangeCount = 1;
			pipeline_layout_ci.pPushConstantRanges = &push_constant_range;

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

			if (atlas.load_glfatl("textures/renderer_atlas.glfatl"))
			{
				check(atlas.create("C:/Windows/Fonts/calibri.ttf", 64, 2, clamp, 1024, och::range(ranges)));

				check(atlas.save_glfatl("textures/renderer_atlas.glfatl", true));

				check(atlas.save_bmp("textures/renderer_atlas.bmp", true));
			}
		}

		// Allocate Device Image and Imageview to hold SDF Glyph Atlas
		{
			VkImageCreateInfo image_ci{};
			image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			image_ci.pNext = nullptr;
			image_ci.flags = 0;
			image_ci.imageType = VK_IMAGE_TYPE_2D;
			image_ci.format = VK_FORMAT_R8_UNORM;
			image_ci.extent.width = atlas.width();
			image_ci.extent.height = atlas.height();
			image_ci.extent.depth = 1;
			image_ci.mipLevels = 1;
			image_ci.arrayLayers = 1;
			image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
			image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
			image_ci.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
			image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			image_ci.queueFamilyIndexCount = 1;
			image_ci.pQueueFamilyIndices = &context.m_general_queues.family_index;
			image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			check(vkCreateImage(context.m_device, &image_ci, nullptr, &font_image));

			VkMemoryRequirements image_memory_reqs{};
			vkGetImageMemoryRequirements(context.m_device, font_image, &image_memory_reqs);

			uint32_t image_mem_type_idx;
			
			check(context.suitable_memory_type_idx(image_mem_type_idx, image_memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

			VkMemoryAllocateInfo image_mem_ai{};
			image_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			image_mem_ai.pNext = nullptr;
			image_mem_ai.allocationSize = image_memory_reqs.size;
			image_mem_ai.memoryTypeIndex = image_mem_type_idx;
			check(vkAllocateMemory(context.m_device, &image_mem_ai, nullptr, &font_image_memory));

			check(vkBindImageMemory(context.m_device, font_image, font_image_memory, 0));
		}

		// Push SDF Glyph Atlas Image to Device
		{
			// Create a staging buffer

			VkBuffer img_staging_buffer;

			VkDeviceMemory img_staging_buffer_memory;

			VkBufferCreateInfo buffer_ci{};
			buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_ci.pNext = nullptr;
			buffer_ci.flags = 0;
			buffer_ci.size = atlas.width() * atlas.height();
			buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer_ci.queueFamilyIndexCount = 1;
			buffer_ci.pQueueFamilyIndices = &context.m_general_queues.family_index;
			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &img_staging_buffer));

			VkMemoryRequirements buffer_memory_reqs{};
			vkGetBufferMemoryRequirements(context.m_device, img_staging_buffer, &buffer_memory_reqs);

			uint32_t buffer_mem_type_idx;
			
			check(context.suitable_memory_type_idx(buffer_mem_type_idx, buffer_memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
			
			VkMemoryAllocateInfo buffer_mem_ai{};
			buffer_mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			buffer_mem_ai.pNext = nullptr;
			buffer_mem_ai.allocationSize = buffer_memory_reqs.size;
			buffer_mem_ai.memoryTypeIndex = buffer_mem_type_idx;

			check(vkAllocateMemory(context.m_device, &buffer_mem_ai, nullptr, &img_staging_buffer_memory));

			check(vkBindBufferMemory(context.m_device, img_staging_buffer, img_staging_buffer_memory, 0));

			// Copy data to the staging buffer

			void* buffer_ptr;
			check(vkMapMemory(context.m_device, img_staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &buffer_ptr));

			memcpy(buffer_ptr, atlas.data(), atlas.width() * atlas.height());

			vkUnmapMemory(context.m_device, img_staging_buffer_memory);

			// Copy data from staging buffer to image

			VkCommandBuffer staging_command_buffer;

			check(context.begin_onetime_command(staging_command_buffer, staging_command_pool));

			VkImageMemoryBarrier transfer_dst_transition_barrier{};
			transfer_dst_transition_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			transfer_dst_transition_barrier.pNext = nullptr;
			transfer_dst_transition_barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
			transfer_dst_transition_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			transfer_dst_transition_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			transfer_dst_transition_barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			transfer_dst_transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transfer_dst_transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			transfer_dst_transition_barrier.image = font_image;
			transfer_dst_transition_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			transfer_dst_transition_barrier.subresourceRange.baseMipLevel = 0;
			transfer_dst_transition_barrier.subresourceRange.levelCount = 1;
			transfer_dst_transition_barrier.subresourceRange.baseArrayLayer = 0;
			transfer_dst_transition_barrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(staging_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &transfer_dst_transition_barrier);

			VkBufferImageCopy buf_img_copy{};
			buf_img_copy.bufferOffset = 0;
			buf_img_copy.bufferRowLength = atlas.width();
			buf_img_copy.bufferImageHeight = atlas.height();
			buf_img_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			buf_img_copy.imageSubresource.mipLevel = 0;
			buf_img_copy.imageSubresource.baseArrayLayer = 0;
			buf_img_copy.imageSubresource.layerCount = 1;
			buf_img_copy.imageOffset.x = 0;
			buf_img_copy.imageOffset.y = 0;
			buf_img_copy.imageOffset.z = 0;
			buf_img_copy.imageExtent.width = atlas.width();
			buf_img_copy.imageExtent.height = atlas.height();
			buf_img_copy.imageExtent.depth = 1;
			vkCmdCopyBufferToImage(staging_command_buffer, img_staging_buffer, font_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &buf_img_copy);

			VkImageMemoryBarrier shader_read_transition_barrier{};
			shader_read_transition_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			shader_read_transition_barrier.pNext = nullptr;
			shader_read_transition_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			shader_read_transition_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
			shader_read_transition_barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			shader_read_transition_barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			shader_read_transition_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			shader_read_transition_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			shader_read_transition_barrier.image = font_image;
			shader_read_transition_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			shader_read_transition_barrier.subresourceRange.baseMipLevel = 0;
			shader_read_transition_barrier.subresourceRange.levelCount = 1;
			shader_read_transition_barrier.subresourceRange.baseArrayLayer = 0;
			shader_read_transition_barrier.subresourceRange.layerCount = 1;
			vkCmdPipelineBarrier(staging_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &shader_read_transition_barrier);

			context.submit_onetime_command(staging_command_buffer, staging_command_pool, context.m_general_queues[0], true);

			vkDestroyBuffer(context.m_device, img_staging_buffer, nullptr);

			vkFreeMemory(context.m_device, img_staging_buffer_memory, nullptr);
		}

		// Create Image View from atlas Image
		{
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
			image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			image_view_ci.subresourceRange.baseMipLevel = 0;
			image_view_ci.subresourceRange.levelCount = 1;
			image_view_ci.subresourceRange.baseArrayLayer = 0;
			image_view_ci.subresourceRange.layerCount = 1;

			check(vkCreateImageView(context.m_device, &image_view_ci, nullptr, &font_image_view));
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
			buffer_ci.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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
			buffer_ci.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
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

		// Create Staging Buffer
		{
			constexpr uint32_t staging_buffer_bytes = sizeof(font_vertex) * 4 + sizeof(uint16_t) * 6;

			VkBufferCreateInfo buffer_ci{};
			buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_ci.pNext = nullptr;
			buffer_ci.flags = 0;
			buffer_ci.size = staging_buffer_bytes;
			buffer_ci.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
			buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer_ci.queueFamilyIndexCount = 1;
			buffer_ci.pQueueFamilyIndices = &context.m_general_queues.family_index;

			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &staging_buffer));

			VkMemoryRequirements memory_reqs{};

			vkGetBufferMemoryRequirements(context.m_device, staging_buffer, &memory_reqs);

			uint32_t memory_type_idx;

			check(context.suitable_memory_type_idx(memory_type_idx, memory_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

			VkMemoryAllocateInfo memory_ai{};
			memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			memory_ai.pNext = nullptr;
			memory_ai.allocationSize = memory_reqs.size;
			memory_ai.memoryTypeIndex = memory_type_idx;

			check(vkAllocateMemory(context.m_device, &memory_ai, nullptr, &staging_buffer_memory));

			check(vkBindBufferMemory(context.m_device, staging_buffer, staging_buffer_memory, 0));
		}

		// Create Command Pool and -Buffers
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
			command_buffer_ai.commandBufferCount = context.m_swapchain_image_cnt;
			check(vkAllocateCommandBuffers(context.m_device, &command_buffer_ai, command_buffers));
		}

		begin_time = och::time::now();

		return {};
	}

	och::status run()
	{
		check(context.begin_message_processing());

		while (!context.is_window_closed())
		{
			if (char32_t c = context.get_input_char(); c && input_cnt < MAX_DISPLAY_CHARS)
				if (c == L'\r')
				{
					pos_history[pos_history_idx++] = input_pos;

					input_pos = { DISPLAY_MIN_X, input_pos.y + atlas.line_height() * DISPLAY_SCALE };
				}
				else if (c == L'\b')
				{
					if (pos_history_idx)
					{
						if (pos_history[pos_history_idx - 1].y == input_pos.y)
							--input_cnt;

						input_pos = pos_history[--pos_history_idx];
					}
				}
				else
				{
					input_buffer[input_cnt] = c;

					glyph_atlas::glyph_index glf = atlas(c);

					if (input_pos.x + DISPLAY_SCALE * (glf.real_extent.x + glf.real_bearing.x) > DISPLAY_MAX_X)
					{
						pos_history[pos_history_idx++] = input_pos;

						input_pos = { DISPLAY_MIN_X, input_pos.y + atlas.line_height() * DISPLAY_SCALE };
					}

					VkCommandBuffer staging_command_buffer;

					struct staging_buf_struct
					{
						font_vertex verts[4];

						uint16_t inds[6];
					};

					void* staging_buffer_ptr;

					check(vkMapMemory(context.m_device, staging_buffer_memory, 0, VK_WHOLE_SIZE, 0, &staging_buffer_ptr));

					staging_buf_struct& sb = *static_cast<staging_buf_struct*>(staging_buffer_ptr);

					sb.verts[0].atlas_pos  = glf.atlas_position;
					sb.verts[1].atlas_pos  = glf.atlas_position + och::vec2(glf.atlas_extent.x, 0.0F);
					sb.verts[2].atlas_pos  = glf.atlas_position + och::vec2(0.0F, glf.atlas_extent.y);
					sb.verts[3].atlas_pos  = glf.atlas_position + glf.atlas_extent;

					sb.verts[0].screen_pos = input_pos + DISPLAY_SCALE *  glf.real_bearing;
					sb.verts[1].screen_pos = input_pos + DISPLAY_SCALE * (glf.real_bearing + och::vec2(glf.real_extent.x, 0));
					sb.verts[2].screen_pos = input_pos + DISPLAY_SCALE * (glf.real_bearing + och::vec2(0, glf.real_extent.y));
					sb.verts[3].screen_pos = input_pos + DISPLAY_SCALE * (glf.real_bearing + glf.real_extent);

					const uint16_t start_idx = static_cast<uint16_t>(input_cnt * 4);

					sb.inds[0] = start_idx + 0;
					sb.inds[1] = start_idx + 2;
					sb.inds[2] = start_idx + 1;
					sb.inds[3] = start_idx + 1;
					sb.inds[4] = start_idx + 2;
					sb.inds[5] = start_idx + 3;

					och::print("CHAR {} (0x{:4>~0X})\n({}, {}) -> ({}, {})\n({}, {}) -> ({}, {})\n({}, {}) -> ({}, {})\n({}, {}) -> ({}, {})\n\n",
						c, static_cast<uint32_t>(c),
						sb.verts[0].atlas_pos.x,
						sb.verts[0].atlas_pos.y,
						sb.verts[0].screen_pos.x,
						sb.verts[0].screen_pos.y,
						sb.verts[1].atlas_pos.x,
						sb.verts[1].atlas_pos.y,
						sb.verts[1].screen_pos.x,
						sb.verts[1].screen_pos.y,
						sb.verts[2].atlas_pos.x,
						sb.verts[2].atlas_pos.y,
						sb.verts[2].screen_pos.x,
						sb.verts[2].screen_pos.y,
						sb.verts[3].atlas_pos.x,
						sb.verts[3].atlas_pos.y,
						sb.verts[3].screen_pos.x,
						sb.verts[3].screen_pos.y
					);

					vkUnmapMemory(context.m_device, staging_buffer_memory);



					check(context.begin_onetime_command(staging_command_buffer, staging_command_pool));

					VkBufferMemoryBarrier before_barriers[2]{};
					// vertex_buffer
					before_barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					before_barriers[0].pNext = nullptr;
					before_barriers[0].srcAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
					before_barriers[0].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					before_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					before_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					before_barriers[0].buffer = vertex_buffer;
					before_barriers[0].offset = 0;
					before_barriers[0].size = VK_WHOLE_SIZE;
					// index_buffer
					before_barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					before_barriers[1].pNext = nullptr;
					before_barriers[1].srcAccessMask = VK_ACCESS_INDEX_READ_BIT;
					before_barriers[1].dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					before_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					before_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					before_barriers[1].buffer = index_buffer;
					before_barriers[1].offset = 0;
					before_barriers[1].size = VK_WHOLE_SIZE;

					vkCmdPipelineBarrier(staging_command_buffer, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 2, before_barriers, 0, nullptr);

					VkBufferCopy vertex_copy{};
					vertex_copy.srcOffset = 0;
					vertex_copy.dstOffset = input_cnt * 4 * sizeof(font_vertex);
					vertex_copy.size = 4 * sizeof(font_vertex);

					vkCmdCopyBuffer(staging_command_buffer, staging_buffer, vertex_buffer, 1, &vertex_copy);

					VkBufferCopy index_copy{};
					index_copy.srcOffset = 4 * sizeof(font_vertex);
					index_copy.dstOffset = input_cnt * 6 * sizeof(uint16_t);
					index_copy.size = 6 * sizeof(uint16_t);

					vkCmdCopyBuffer(staging_command_buffer, staging_buffer, index_buffer, 1, &index_copy);

					VkBufferMemoryBarrier after_barriers[2]{};
					// vertex_buffer
					after_barriers[0].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					after_barriers[0].pNext = nullptr;
					after_barriers[0].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					after_barriers[0].dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT;
					after_barriers[0].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					after_barriers[0].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					after_barriers[0].buffer = vertex_buffer;
					after_barriers[0].offset = 0;
					after_barriers[0].size = VK_WHOLE_SIZE;
					// index_buffer
					after_barriers[1].sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
					after_barriers[1].pNext = nullptr;
					after_barriers[1].srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
					after_barriers[1].dstAccessMask = VK_ACCESS_INDEX_READ_BIT;
					after_barriers[1].srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					after_barriers[1].dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
					after_barriers[1].buffer = index_buffer;
					after_barriers[1].offset = 0;
					after_barriers[1].size = VK_WHOLE_SIZE;

					vkCmdPipelineBarrier(staging_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, 0, 0, nullptr, 2, after_barriers, 0, nullptr);

					check(context.submit_onetime_command(staging_command_buffer, staging_command_pool, context.m_general_queues[0]));

					++input_cnt;

					pos_history[pos_history_idx++] = input_pos;

					input_pos.x += glf.real_advance * DISPLAY_SCALE;
				}

			check(vkWaitForFences(context.m_device, 1, &frame_inflight_fences[frame_idx], VK_FALSE, UINT64_MAX));

			uint32_t swapchain_idx;

			VkResult acquire_rst = vkAcquireNextImageKHR(context.m_device, context.m_swapchain, UINT64_MAX, image_available_semaphores[frame_idx], nullptr, &swapchain_idx);

			if (acquire_rst == VK_ERROR_OUT_OF_DATE_KHR)
			{
				check(recreate_swapchain());

				continue;
			}
			else if (acquire_rst != VK_SUBOPTIMAL_KHR)
				check(acquire_rst);

			if (image_inflight_fences[swapchain_idx] != nullptr)
				check(vkWaitForFences(context.m_device, 1, &image_inflight_fences[swapchain_idx], VK_FALSE, UINT64_MAX));

			image_inflight_fences[swapchain_idx] = frame_inflight_fences[frame_idx];

			check(record_command_buffer(command_buffers[frame_idx], swapchain_idx));

			VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;

			VkSubmitInfo submit_info{};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info.pNext = nullptr;
			submit_info.waitSemaphoreCount = 1;
			submit_info.pWaitSemaphores = &image_available_semaphores[frame_idx];
			submit_info.pWaitDstStageMask = &wait_stage;
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &command_buffers[frame_idx];
			submit_info.signalSemaphoreCount = 1;
			submit_info.pSignalSemaphores = &render_complete_semaphores[frame_idx];

			check(vkResetFences(context.m_device, 1, &frame_inflight_fences[frame_idx]));

			check(vkQueueSubmit(context.m_general_queues[0], 1, &submit_info, frame_inflight_fences[frame_idx]));

			VkPresentInfoKHR present_info{};
			present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			present_info.pNext = nullptr;
			present_info.waitSemaphoreCount = 1;
			present_info.pWaitSemaphores = &render_complete_semaphores[frame_idx];
			present_info.swapchainCount = 1;
			present_info.pSwapchains = &context.m_swapchain;
			present_info.pImageIndices = &swapchain_idx;
			present_info.pResults = nullptr;

			VkResult present_rst = vkQueuePresentKHR(context.m_general_queues[0], &present_info);

			if (present_rst == VK_ERROR_OUT_OF_DATE_KHR || present_rst == VK_SUBOPTIMAL_KHR || context.is_framebuffer_resized())
			{
				context.m_flags.framebuffer_resized.store(false, std::memory_order::release);

				recreate_swapchain();
			}
			else
				check(present_rst);

			frame_idx = (frame_idx + 1) % MAX_FRAMES_INFLIGHT;
		}

		return {};
	}

	void destroy()
	{
		if (!context.m_device || vkDeviceWaitIdle(context.m_device) != VK_SUCCESS)
			return;

		vkDestroySampler(context.m_device, font_sampler, nullptr);

		vkDestroyImageView(context.m_device, font_image_view, nullptr);

		vkDestroyImage(context.m_device, font_image, nullptr);

		vkFreeMemory(context.m_device, font_image_memory, nullptr);

		vkDestroyBuffer(context.m_device, staging_buffer, nullptr);

		vkFreeMemory(context.m_device, staging_buffer_memory, nullptr);

		vkDestroyBuffer(context.m_device, vertex_buffer, nullptr);

		vkFreeMemory(context.m_device, vertex_buffer_memory, nullptr);

		vkDestroyBuffer(context.m_device, index_buffer, nullptr);

		vkFreeMemory(context.m_device, index_buffer_memory, nullptr);

		for (auto& framebuffer : frame_buffers)
			vkDestroyFramebuffer(context.m_device, framebuffer, nullptr);

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

		vkDestroyCommandPool(context.m_device, staging_command_pool, nullptr);

		vkDestroyDescriptorPool(context.m_device, descriptor_pool, nullptr);

		context.destroy();
	}

	och::status record_command_buffer(VkCommandBuffer command_buffer, uint32_t swapchain_idx) noexcept
	{
		VkCommandBufferBeginInfo command_buffer_bi{};
		command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		command_buffer_bi.pNext = nullptr;
		command_buffer_bi.flags = 0;
		command_buffer_bi.pInheritanceInfo = nullptr;
		check(vkBeginCommandBuffer(command_buffer, &command_buffer_bi));

		VkClearValue clear_value = { 0.8F, 0.8F, 0.8F, 1.0F };

		VkRenderPassBeginInfo render_pass_bi{};
		render_pass_bi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		render_pass_bi.pNext = nullptr;
		render_pass_bi.renderPass = render_pass;
		render_pass_bi.framebuffer = frame_buffers[swapchain_idx];
		render_pass_bi.renderArea.offset.x = 0;
		render_pass_bi.renderArea.offset.y = 0;
		render_pass_bi.renderArea.extent = context.m_swapchain_extent;
		render_pass_bi.clearValueCount = 1;
		render_pass_bi.pClearValues = &clear_value;

		vkCmdBeginRenderPass(command_buffer, &render_pass_bi, VK_SUBPASS_CONTENTS_INLINE);

		const float w = static_cast<float>(context.m_swapchain_extent.width);
		const float h = static_cast<float>(context.m_swapchain_extent.height);
		const float scale_x = 512.0F / w;
		const float scale_y = 512.0F / h;

		const och::timespan delta_ts = och::time::now() - begin_time;
		const float delta_t = static_cast<float>(delta_ts.milliseconds()) / 1024.0F;
		const float rot = sinf(delta_t);

		const och::mat4 push_constant_mat = och::mat4::translate(1.0F, 1.0F, 0.0F) * och::mat4::scale(scale_x, scale_y, 1.0F) * och::mat4::translate(-1.0F, -1.0F, 0.0F);

		vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(och::mat4), &push_constant_mat);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

		VkDeviceSize buffer_offset = 0;

		vkCmdBindVertexBuffers(command_buffer, 0, 1, &vertex_buffer, &buffer_offset);

		vkCmdBindIndexBuffer(command_buffer, index_buffer, 0, VK_INDEX_TYPE_UINT16);

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

		vkCmdDrawIndexed(command_buffer, input_cnt * 6, 1, 0, 0, 0);

		vkCmdEndRenderPass(command_buffer);

		check(vkEndCommandBuffer(command_buffer));

		return {};
	}

	och::status recreate_swapchain()
	{
		check(vkDeviceWaitIdle(context.m_device));


		// Cleanup
		
		for (auto& framebuffer : frame_buffers)
			vkDestroyFramebuffer(context.m_device, framebuffer, nullptr);

		vkDestroyPipeline(context.m_device, pipeline, nullptr);


		// Actual Recreation

		context.recreate_swapchain();

		check(create_pipeline());

		for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
		{
			VkFramebufferCreateInfo framebuffer_ci{};
			framebuffer_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
			framebuffer_ci.pNext = nullptr;
			framebuffer_ci.flags = 0;
			framebuffer_ci.renderPass = render_pass;
			framebuffer_ci.attachmentCount = 1;
			framebuffer_ci.pAttachments = context.m_swapchain_image_views + i;
			framebuffer_ci.width = context.m_swapchain_extent.width;
			framebuffer_ci.height = context.m_swapchain_extent.height;
			framebuffer_ci.layers = 1;

			check(vkCreateFramebuffer(context.m_device, &framebuffer_ci, nullptr, frame_buffers + i));
		}

		return {};
	}

	och::status create_pipeline() noexcept
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
		shader_stages[1].module = frag_shader_module;
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
		vertex_attribute_descs[1].location = 1;
		vertex_attribute_descs[1].binding = 0;
		vertex_attribute_descs[1].format = VK_FORMAT_R32G32_SFLOAT;
		vertex_attribute_descs[1].offset = offsetof(font_vertex, screen_pos);

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
};

och::status run_sdf_font()
{
	sdf_font program{};

	och::status err = program.create();

	if (!err)
		err = program.run();

	program.destroy();

	check(err);

	return {};
}
