#include "compute_to_swapchain.h"

#include "directory_constants.h"

#include "vulkan_base.h"
#include "och_fmt.h"
#include "och_matmath.h"
#include "heap_buffer.h"

#define USE_SIMPLEX

struct compute_image_to_swapchain
{
	static constexpr uint32_t GROUP_SZ_X = 8;
	static constexpr uint32_t GROUP_SZ_Y = 8;
	static constexpr uint32_t GROUP_SZ_Z = 1;

	static constexpr uint32_t MAX_FRAMES_INFLIGHT = 2;

	uint32_t frame_idx = 0;



	vulkan_context context;



	VkShaderModule shader_module{};

	VkDescriptorSetLayout descriptor_set_layout{};

	VkPipelineLayout pipeline_layout{};
	
	VkPipeline pipeline{};



	VkDescriptorPool descriptor_pool;

	VkDescriptorSet descriptor_sets[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};

	VkCommandPool command_pool{};

	VkCommandBuffer command_buffers[MAX_FRAMES_INFLIGHT]{};


	VkSemaphore image_available_semaphores[MAX_FRAMES_INFLIGHT]{};

	VkSemaphore render_complete_semaphores[MAX_FRAMES_INFLIGHT]{};

	VkFence frame_inflight_fences[MAX_FRAMES_INFLIGHT]{};

	VkFence image_inflight_fences[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};



	och::time creation_time;



	bool is_using_simplex;



	och::status create(bool use_simplex) noexcept
	{
		is_using_simplex = use_simplex;

		check(context.create("Compute to Swapchain", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));

		// Create Compute Pipeline
		{
			VkDescriptorSetLayoutBinding binding{};
			binding.binding = 0;
			binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			binding.descriptorCount = 1;
			binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			binding.pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci{};
			descriptor_set_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptor_set_layout_ci.pNext = nullptr;
			descriptor_set_layout_ci.flags = 0;
			descriptor_set_layout_ci.bindingCount = 1;
			descriptor_set_layout_ci.pBindings = &binding;

			check(vkCreateDescriptorSetLayout(context.m_device, &descriptor_set_layout_ci, nullptr, &descriptor_set_layout));

			VkPushConstantRange push_constant_range{};
			push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			push_constant_range.offset = 0;
			push_constant_range.size = sizeof(och::vec4);

			VkPipelineLayoutCreateInfo pipeline_layout_ci{};
			pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipeline_layout_ci.pNext = nullptr;
			pipeline_layout_ci.flags = 0;
			pipeline_layout_ci.setLayoutCount = 1;
			pipeline_layout_ci.pSetLayouts = &descriptor_set_layout;
			pipeline_layout_ci.pushConstantRangeCount = 1;
			pipeline_layout_ci.pPushConstantRanges = &push_constant_range;

			check(vkCreatePipelineLayout(context.m_device, &pipeline_layout_ci, nullptr, &pipeline_layout));

			check(context.load_shader_module_file(shader_module, is_using_simplex ? OCH_DIR "shaders\\simplex3d_slice.comp.spv" : OCH_DIR "shaders\\swapchain.comp.spv"));

			struct { uint32_t x, y, z; } group_size{ GROUP_SZ_X, GROUP_SZ_Y, GROUP_SZ_Z };

			VkSpecializationMapEntry specialization_map_entries[3]{};
			specialization_map_entries[0] = { 1, offsetof(decltype(group_size), x), sizeof(group_size.x) };
			specialization_map_entries[1] = { 2, offsetof(decltype(group_size), y), sizeof(group_size.y) };
			specialization_map_entries[2] = { 3, offsetof(decltype(group_size), z), sizeof(group_size.z) };

			VkSpecializationInfo specialization_info{};
			specialization_info.mapEntryCount = 3;
			specialization_info.pMapEntries = specialization_map_entries;
			specialization_info.dataSize = sizeof(group_size);
			specialization_info.pData = &group_size;

			VkPipelineShaderStageCreateInfo shader_stage_ci{};
			shader_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shader_stage_ci.pNext = nullptr;
			shader_stage_ci.flags = 0;
			shader_stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shader_stage_ci.module = shader_module;
			shader_stage_ci.pName = "main";
			shader_stage_ci.pSpecializationInfo = &specialization_info;
				
			VkComputePipelineCreateInfo pipeline_ci{};
			pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipeline_ci.pNext = nullptr;
			pipeline_ci.flags = 0;
			pipeline_ci.stage = shader_stage_ci;
			pipeline_ci.layout = pipeline_layout;
			pipeline_ci.basePipelineHandle = nullptr;
			pipeline_ci.basePipelineIndex = 0;

			check(vkCreateComputePipelines(context.m_device, nullptr, 1, &pipeline_ci, nullptr, &pipeline));
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

			VkDescriptorSetLayout set_layouts[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];
			for (auto& l : set_layouts) l = descriptor_set_layout;

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = descriptor_pool;
			descriptor_set_ai.descriptorSetCount = context.m_swapchain_image_cnt;
			descriptor_set_ai.pSetLayouts = set_layouts;

			check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, descriptor_sets));

			VkDescriptorImageInfo image_infos[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

			VkWriteDescriptorSet writes[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];
			
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

		creation_time = och::time::now();

		return {};
	}
	
	och::status run() noexcept
	{
		check(context.begin_message_processing());

		while (!context.is_window_closed())
		{
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

	void destroy() const noexcept
	{
		if (!context.m_device || vkDeviceWaitIdle(context.m_device) != VK_SUCCESS)
			return;

		for (auto& s : image_available_semaphores)
			vkDestroySemaphore(context.m_device, s, nullptr);

		for (auto& s : render_complete_semaphores)
			vkDestroySemaphore(context.m_device, s, nullptr);

		for (auto& f : frame_inflight_fences)
			vkDestroyFence(context.m_device, f, nullptr);

		vkDestroyShaderModule(context.m_device, shader_module, nullptr);

		vkDestroyDescriptorSetLayout(context.m_device, descriptor_set_layout, nullptr);

		vkDestroyPipelineLayout(context.m_device, pipeline_layout, nullptr);

		vkDestroyPipeline(context.m_device, pipeline, nullptr);

		vkDestroyCommandPool(context.m_device, command_pool, nullptr);

		vkDestroyDescriptorPool(context.m_device, descriptor_pool, nullptr);

		context.destroy();
	}

	och::status record_command_buffer(VkCommandBuffer command_buffer, uint32_t swapchain_idx)
	{
		och::timespan delta_t = och::time::now() - creation_time;

		och::vec4 push_constant_data;

		if (is_using_simplex)
		{
			push_constant_data = { static_cast<float>(delta_t.milliseconds()) / 3000.0F, static_cast<float>(delta_t.milliseconds()) / 2048.0F, static_cast<float>(delta_t.milliseconds()) / 8192.0F, 0.02F };
		}
		else
		{
			constexpr float cycle_len = 512.0F;

			float r = cosf((delta_t.milliseconds() + cycle_len / 3.0F * 0.0F) / cycle_len);
			float b = cosf((delta_t.milliseconds() + cycle_len / 3.0F * 1.0F) / cycle_len);
			float g = cosf((delta_t.milliseconds() + cycle_len / 3.0F * 2.0F) / cycle_len);

			push_constant_data = { r, g, b, 1.0F };
		}

		VkCommandBufferBeginInfo begin_info{};
		begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		begin_info.pNext = nullptr;
		begin_info.flags = 0;
		begin_info.pInheritanceInfo = nullptr;

		check(vkBeginCommandBuffer(command_buffer, &begin_info));

		VkImageMemoryBarrier to_storage_img_barrier{};
		to_storage_img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		to_storage_img_barrier.pNext = nullptr;
		to_storage_img_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		to_storage_img_barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		to_storage_img_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
		to_storage_img_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
		to_storage_img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_storage_img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_storage_img_barrier.image = context.m_swapchain_images[swapchain_idx];
		to_storage_img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		to_storage_img_barrier.subresourceRange.baseMipLevel = 0;
		to_storage_img_barrier.subresourceRange.levelCount = 1;
		to_storage_img_barrier.subresourceRange.baseArrayLayer = 0;
		to_storage_img_barrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_storage_img_barrier);

		vkCmdPushConstants(command_buffer, pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constant_data), &push_constant_data);

		vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_sets[swapchain_idx], 0, nullptr);

		vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

		uint32_t groups_x = (context.m_swapchain_extent.width + MAX_FRAMES_INFLIGHT - 1) / MAX_FRAMES_INFLIGHT;
		uint32_t groups_y = (context.m_swapchain_extent.height + MAX_FRAMES_INFLIGHT - 1) / MAX_FRAMES_INFLIGHT;

		vkCmdDispatch(command_buffer, groups_x, groups_y, 1);

		VkImageMemoryBarrier to_present_img_barrier{};
		to_present_img_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		to_present_img_barrier.pNext = nullptr;
		to_present_img_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		to_present_img_barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
		to_present_img_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
		to_present_img_barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
		to_present_img_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_present_img_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		to_present_img_barrier.image = context.m_swapchain_images[swapchain_idx];
		to_present_img_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		to_present_img_barrier.subresourceRange.baseMipLevel = 0;
		to_present_img_barrier.subresourceRange.levelCount = 1;
		to_present_img_barrier.subresourceRange.baseArrayLayer = 0;
		to_present_img_barrier.subresourceRange.layerCount = 1;

		vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_present_img_barrier);

		check(vkEndCommandBuffer(command_buffer));

		return {};
	}

	och::status recreate_swapchain() noexcept
	{
		check(context.recreate_swapchain());

		// Recreate Descriptor Sets
		{
			check(vkResetDescriptorPool(context.m_device, descriptor_pool, 0));

			VkDescriptorSetLayout set_layouts[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];
			for (auto& l : set_layouts) l = descriptor_set_layout;

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = descriptor_pool;
			descriptor_set_ai.descriptorSetCount = context.m_swapchain_image_cnt;
			descriptor_set_ai.pSetLayouts = set_layouts;

			check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, descriptor_sets));

			VkDescriptorImageInfo image_infos[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

			VkWriteDescriptorSet writes[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

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

		//Recreate Pipeline
		{
			vkDestroyPipeline(context.m_device, pipeline, nullptr);

			struct { uint32_t x, y, z; } group_size{ GROUP_SZ_X, GROUP_SZ_Y, GROUP_SZ_Z };

			VkSpecializationMapEntry specialization_map_entries[3]{};
			specialization_map_entries[0] = { 1, offsetof(decltype(group_size), x), sizeof(group_size.x) };
			specialization_map_entries[1] = { 2, offsetof(decltype(group_size), y), sizeof(group_size.y) };
			specialization_map_entries[2] = { 3, offsetof(decltype(group_size), z), sizeof(group_size.z) };

			VkSpecializationInfo specialization_info{};
			specialization_info.mapEntryCount = 3;
			specialization_info.pMapEntries = specialization_map_entries;
			specialization_info.dataSize = sizeof(group_size);
			specialization_info.pData = &group_size;

			VkPipelineShaderStageCreateInfo shader_stage_ci{};
			shader_stage_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shader_stage_ci.pNext = nullptr;
			shader_stage_ci.flags = 0;
			shader_stage_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shader_stage_ci.module = shader_module;
			shader_stage_ci.pName = "main";
			shader_stage_ci.pSpecializationInfo = &specialization_info;

			VkComputePipelineCreateInfo pipeline_ci{};
			pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipeline_ci.pNext = nullptr;
			pipeline_ci.flags = 0;
			pipeline_ci.stage = shader_stage_ci;
			pipeline_ci.layout = pipeline_layout;
			pipeline_ci.basePipelineHandle = nullptr;
			pipeline_ci.basePipelineIndex = 0;

			check(vkCreateComputePipelines(context.m_device, nullptr, 1, &pipeline_ci, nullptr, &pipeline));
		}

		return {};
	}
};

och::status run_compute_to_swapchain(bool use_simplex) noexcept
{
	compute_image_to_swapchain program;

	och::status err = program.create(use_simplex);

	if (!err)
		program.run();

	program.destroy();

	check(err);

	return {};
}
