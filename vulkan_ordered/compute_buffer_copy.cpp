#include "compute_buffer_copy.h"

#include "vulkan_base.h"

#include "och_timer.h"
#include "och_fmt.h"
#include "och_fio.h"

struct simple_compute_buffer_copy
{
	static constexpr VkDeviceSize BUFFER_ELEMS = 1024;

	static constexpr VkDeviceSize BUFFER_BYTES = sizeof(uint32_t) * BUFFER_ELEMS;

	static constexpr uint32_t COMPUTE_GROUP_SZ = 256;

	vulkan_context context;



	VkBuffer src_buffer;

	VkBuffer dst_buffer;

	VkDeviceSize src_offset;

	VkDeviceSize dst_offset;

	VkDeviceMemory buffer_mem;



	VkDescriptorSetLayout descriptor_set_layout;

	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;



	VkShaderModule shader_module;



	VkDescriptorPool descriptor_pool;

	VkDescriptorSet descriptor_set;

	VkCommandPool command_pool;

	VkCommandBuffer command_buffer;



	och::status create() noexcept
	{
		vulkan_context_create_info context_ci{};
		context_ci.app_name = "Compute Buffer Copy";
		context_ci.swapchain_image_usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;
		context_ci.requested_compute_queues = 1;

		check(context.create(&context_ci));

		// Allocate Buffers
		{
			VkBufferCreateInfo buffer_ci{};
			buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			buffer_ci.pNext = nullptr;
			buffer_ci.flags = 0;
			buffer_ci.size = BUFFER_BYTES;
			buffer_ci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
			buffer_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			buffer_ci.queueFamilyIndexCount = 0;
			buffer_ci.pQueueFamilyIndices = nullptr;

			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &src_buffer));

			check(vkCreateBuffer(context.m_device, &buffer_ci, nullptr, &dst_buffer));

			VkMemoryRequirements mem_reqs;
			vkGetBufferMemoryRequirements(context.m_device, src_buffer, &mem_reqs);

			uint32_t memory_type_idx;

			check(context.suitable_memory_type_idx(memory_type_idx, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));

			src_offset = 0;

			dst_offset = BUFFER_BYTES > mem_reqs.alignment ? BUFFER_BYTES : mem_reqs.alignment;

			VkMemoryAllocateInfo alloc_info{};
			alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			alloc_info.pNext = nullptr;
			alloc_info.allocationSize = dst_offset + BUFFER_BYTES;
			alloc_info.memoryTypeIndex = memory_type_idx;

			check(vkAllocateMemory(context.m_device, &alloc_info, nullptr, &buffer_mem));

			check(vkBindBufferMemory(context.m_device, src_buffer, buffer_mem, src_offset));

			check(vkBindBufferMemory(context.m_device, dst_buffer, buffer_mem, dst_offset));
		}

		// Create Compute Pipeline
		{
			VkDescriptorSetLayoutBinding bindings[2]{};
			// Dst
			bindings[0].binding = 0;
			bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[0].descriptorCount = 1;
			bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			bindings[0].pImmutableSamplers = nullptr;
			// Src
			bindings[1].binding = 1;
			bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			bindings[1].descriptorCount = 1;
			bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			bindings[1].pImmutableSamplers = nullptr;

			VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci{};
			descriptor_set_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
			descriptor_set_layout_ci.pNext = nullptr;
			descriptor_set_layout_ci.flags = 0;
			descriptor_set_layout_ci.bindingCount = 2;
			descriptor_set_layout_ci.pBindings = bindings;

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

			check(context.load_shader_module_file(shader_module, "shaders/buffer_copy.comp.spv"));

			struct { uint32_t x, y, z; } group_size{ COMPUTE_GROUP_SZ, 1, 1 };

			VkSpecializationMapEntry specialization_entries[3]{};
			specialization_entries[0].constantID = 1;
			specialization_entries[0].offset = offsetof(decltype(group_size), x);
			specialization_entries[0].size = sizeof(group_size.x);
			specialization_entries[1].constantID = 2;
			specialization_entries[1].offset = offsetof(decltype(group_size), y);
			specialization_entries[1].size = sizeof(group_size.y);
			specialization_entries[2].constantID = 3;
			specialization_entries[2].offset = offsetof(decltype(group_size), z);
			specialization_entries[2].size = sizeof(group_size.z);

			VkSpecializationInfo specialization_ci{};
			specialization_ci.mapEntryCount = 3;
			specialization_ci.pMapEntries = specialization_entries;
			specialization_ci.dataSize = sizeof(group_size);
			specialization_ci.pData = &group_size;

			VkPipelineShaderStageCreateInfo shader_ci{};
			shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			shader_ci.pNext = nullptr;
			shader_ci.flags = 0;
			shader_ci.stage = VK_SHADER_STAGE_COMPUTE_BIT;
			shader_ci.module = shader_module;
			shader_ci.pName = "main";
			shader_ci.pSpecializationInfo = &specialization_ci;

			VkComputePipelineCreateInfo pipeline_ci{};
			pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipeline_ci.pNext = nullptr;
			pipeline_ci.flags = 0;
			pipeline_ci.stage = shader_ci;
			pipeline_ci.layout = pipeline_layout;
			pipeline_ci.basePipelineHandle = nullptr;
			pipeline_ci.basePipelineIndex = 0;

			check(vkCreateComputePipelines(context.m_device, nullptr, 1, &pipeline_ci, nullptr, &pipeline));
		}

		// Create Descriptor Set
		{
			VkDescriptorPoolSize pool_sizes[2]{};
			// Dst
			pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			pool_sizes[0].descriptorCount = 1;
			// Src
			pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			pool_sizes[1].descriptorCount = 1;

			VkDescriptorPoolCreateInfo descriptor_pool_ci{};
			descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptor_pool_ci.pNext = nullptr;
			descriptor_pool_ci.flags = 0;
			descriptor_pool_ci.maxSets = 1;
			descriptor_pool_ci.poolSizeCount = 2;
			descriptor_pool_ci.pPoolSizes = pool_sizes;

			check(vkCreateDescriptorPool(context.m_device, &descriptor_pool_ci, nullptr, &descriptor_pool));

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = descriptor_pool;
			descriptor_set_ai.descriptorSetCount = 1;
			descriptor_set_ai.pSetLayouts = &descriptor_set_layout;

			check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, &descriptor_set));

			VkDescriptorBufferInfo dst_buffer_info{};
			dst_buffer_info.buffer = dst_buffer;
			dst_buffer_info.offset = 0;
			dst_buffer_info.range = VK_WHOLE_SIZE;

			VkDescriptorBufferInfo src_buffer_info{};
			src_buffer_info.buffer = src_buffer;
			src_buffer_info.offset = 0;
			src_buffer_info.range = VK_WHOLE_SIZE;

			VkWriteDescriptorSet writes[2]{};
			// Dst
			writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[0].pNext = nullptr;
			writes[0].dstSet = descriptor_set;
			writes[0].dstBinding = 0;
			writes[0].dstArrayElement = 0;
			writes[0].descriptorCount = 1;
			writes[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[0].pImageInfo = nullptr;
			writes[0].pBufferInfo = &dst_buffer_info;
			writes[0].pTexelBufferView = nullptr;
			// Src
			writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[1].pNext = nullptr;
			writes[1].dstSet = descriptor_set;
			writes[1].dstBinding = 1;
			writes[1].dstArrayElement = 0;
			writes[1].descriptorCount = 1;
			writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[1].pImageInfo = nullptr;
			writes[1].pBufferInfo = &src_buffer_info;
			writes[1].pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(context.m_device, 2, writes, 0, nullptr);
		}

		// Create Command Buffer
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = 0;
			command_pool_ci.queueFamilyIndex = context.m_compute_queues.family_index;

			check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &command_pool));

			VkCommandBufferAllocateInfo command_buffer_ai{};
			command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_ai.pNext = nullptr;
			command_buffer_ai.commandPool = command_pool;
			command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_ai.commandBufferCount = 1;

			check(vkAllocateCommandBuffers(context.m_device, &command_buffer_ai, &command_buffer));
		}

		return {};
	}

	och::status run() noexcept
	{
		// Populate Source-Buffer
		{
			void* src_data;
			check(vkMapMemory(context.m_device, buffer_mem, src_offset, BUFFER_BYTES, 0, &src_data));

			for (VkDeviceSize i = 0; i != BUFFER_ELEMS; ++i)
				reinterpret_cast<uint32_t*>(src_data)[i] = static_cast<uint32_t>(i + 1);

			vkUnmapMemory(context.m_device, buffer_mem);
		}

		// Reset Dst-Buffer
		{
			void* dst_data;
			check(vkMapMemory(context.m_device, buffer_mem, dst_offset, BUFFER_BYTES, 0, &dst_data));

			for (uint32_t i = 0; i != BUFFER_ELEMS; ++i)
				reinterpret_cast<uint32_t*>(dst_data)[i] = 0;

			vkUnmapMemory(context.m_device, buffer_mem);
		}

		// Run Shader
		{
			VkCommandBufferBeginInfo begin_info{};
			begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			begin_info.pNext = nullptr;
			begin_info.flags = 0;
			begin_info.pInheritanceInfo = nullptr;

			check(vkBeginCommandBuffer(command_buffer, &begin_info));

			vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);

			vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);

			vkCmdDispatch(command_buffer, BUFFER_ELEMS / COMPUTE_GROUP_SZ, 1, 1);

			check(vkEndCommandBuffer(command_buffer));

			VkSubmitInfo submit_info{};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info.pNext = nullptr;
			submit_info.waitSemaphoreCount = 0;
			submit_info.pWaitSemaphores = nullptr;
			submit_info.pWaitDstStageMask = nullptr;
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &command_buffer;
			submit_info.signalSemaphoreCount = 0;
			submit_info.pSignalSemaphores = nullptr;

			check(vkQueueSubmit(context.m_general_queues[0], 1, &submit_info, nullptr));

			check(vkQueueWaitIdle(context.m_general_queues[0]));
		}

		// Check Destination-Buffer
		{
			void* dst_data;

			check(vkMapMemory(context.m_device, buffer_mem, dst_offset, BUFFER_BYTES, 0, &dst_data));

			uint32_t wrong_cnt = 0;

			for (VkDeviceSize i = 0; i != BUFFER_ELEMS; ++i)
				if (reinterpret_cast<uint32_t*>(dst_data)[i] != static_cast<uint32_t>(i + 1))
					++wrong_cnt;

			vkUnmapMemory(context.m_device, buffer_mem);

			if (wrong_cnt)
				och::print("FAILURE: Buffers are not equal! {} out of {} wrong.\n", wrong_cnt, BUFFER_ELEMS);
			else
				och::print("SUCCESS: Buffers are equal!\n");
		}

		return {};
	}

	void destroy() const noexcept
	{
		if (!context.m_device) return;

		vkDestroyShaderModule(context.m_device, shader_module, nullptr);

		vkDestroyPipeline(context.m_device, pipeline, nullptr);

		vkDestroyPipelineLayout(context.m_device, pipeline_layout, nullptr);

		vkDestroyCommandPool(context.m_device, command_pool, nullptr);

		vkDestroyDescriptorPool(context.m_device, descriptor_pool, nullptr);

		vkDestroyDescriptorSetLayout(context.m_device, descriptor_set_layout, nullptr);

		vkDestroyBuffer(context.m_device, src_buffer, nullptr);

		vkDestroyBuffer(context.m_device, dst_buffer, nullptr);

		vkFreeMemory(context.m_device, buffer_mem, nullptr);

		context.destroy();
	}
};





och::status run_compute_buffer_copy() noexcept
{
	simple_compute_buffer_copy program;

	och::status err = program.create();

	if (!err)
		err = program.run();

	program.destroy();

	check(err);

	return {};
}
