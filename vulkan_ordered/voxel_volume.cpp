#include "voxel_volume.h"

#include "vulkan_base.h"
#include "directory_constants.h"
#include "bitmap.h"

#include "och_matmath.h"

struct voxel_volume
{
	struct push_constant_data_t
	{
		och::vec4 origin;
		och::vec4 direction_delta;
		och::mat3 direction_rotation;
	};

	static constexpr uint32_t MAX_FRAMES_INFLIGHT = 2;



	using base_elem_t = uint16_t;

	using brick_elem_t = uint32_t;

	using leaf_elem_t = uint16_t;

	static constexpr uint32_t LEVEL_CNT = 3;

	static constexpr uint32_t BASE_DIM_LOG2 = 6;

	static constexpr uint32_t BRICK_DIM_LOG2 = 4;

	static constexpr uint32_t BASE_DIM = 1 << BASE_DIM_LOG2;

	static constexpr uint32_t BRICK_DIM = 1 << BRICK_DIM_LOG2;

	static constexpr uint32_t BASE_VOL = BASE_DIM * BASE_DIM * BASE_DIM;

	static constexpr uint32_t BRICK_VOL = BRICK_DIM * BRICK_DIM * BRICK_DIM;

	static constexpr float BASE_OCCUPANCY = 0.0625F;

	static constexpr float BRICK_OCCUPANCY = 0.5F;

	static constexpr uint32_t OCCUPIED_BRICKS = static_cast<uint32_t>(BASE_VOL * LEVEL_CNT * BASE_OCCUPANCY);

	static constexpr uint32_t BRICK_BYTES = OCCUPIED_BRICKS * BRICK_VOL * sizeof(brick_elem_t);

	static constexpr uint32_t OCCUPIED_LEAVES = static_cast<uint32_t>(OCCUPIED_BRICKS * BRICK_VOL * BRICK_OCCUPANCY);

	static constexpr uint32_t LEAF_BYTES = OCCUPIED_LEAVES * 8 * sizeof(leaf_elem_t);



	static constexpr uint32_t TRACE_GROUP_SIZE_X = 8;

	static constexpr uint32_t TRACE_GROUP_SIZE_Y = 8;





	vulkan_context ctx;



	VkImage base_image;

	VkImageView base_image_view;

	VkDeviceMemory base_image_memory;

	VkBuffer brick_buffer;

	VkDeviceMemory brick_memory;

	VkBuffer leaf_buffer;

	VkDeviceMemory leaf_memory;



	VkImage hit_index_images[MAX_FRAMES_INFLIGHT];

	VkImageView hit_index_image_views[MAX_FRAMES_INFLIGHT];

	VkDeviceMemory hit_index_memory;

	VkImage hit_times_images[MAX_FRAMES_INFLIGHT];

	VkImageView hit_times_image_views[MAX_FRAMES_INFLIGHT];

	VkDeviceMemory hit_times_memory;



	VkDescriptorPool descriptor_pool;

	VkDescriptorSet descriptor_sets[MAX_FRAMES_INFLIGHT];

	VkCommandPool command_pool;

	VkCommandBuffer command_buffers[MAX_FRAMES_INFLIGHT];



	VkShaderModule trace_shader_module;

	VkDescriptorSetLayout descriptor_set_layout;

	VkPipelineLayout pipeline_layout;

	VkPipeline pipeline;



	VkSemaphore image_available_semaphores[MAX_FRAMES_INFLIGHT]{};

	VkSemaphore render_complete_semaphores[MAX_FRAMES_INFLIGHT]{};

	VkFence frame_inflight_fences[MAX_FRAMES_INFLIGHT]{};

	VkFence image_inflight_fences[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};

	uint32_t frame_idx{};



	och::status temp_populate() noexcept
	{
		static constexpr uint32_t POPULATE_GROUP_SIZE_X = 8;

		static constexpr uint32_t POPULATE_GROUP_SIZE_Y = 8;

		static constexpr uint32_t POPULATE_GROUP_SIZE_Z = 8;

		VkDescriptorPool pop_descriptor_pool;

		VkDescriptorSetLayout pop_descriptor_set_layout;

		VkDescriptorSet pop_descriptor_set;

		VkCommandPool pop_command_pool;

		VkCommandBuffer pop_command_buffer;

		VkPipelineLayout pop_pipeline_layout;

		VkShaderModule pop_shader_module;

		VkPipeline pop_pipeline;

		// Create Pipeline
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

			check(vkCreateDescriptorSetLayout(ctx.m_device, &descriptor_set_layout_ci, nullptr, &pop_descriptor_set_layout));

			VkPushConstantRange push_constant_range{};
			push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
			push_constant_range.offset = 0;
			push_constant_range.size = sizeof(och::vec4);

			VkPipelineLayoutCreateInfo pipeline_layout_ci{};
			pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipeline_layout_ci.pNext = nullptr;
			pipeline_layout_ci.flags = 0;
			pipeline_layout_ci.setLayoutCount = 1;
			pipeline_layout_ci.pSetLayouts = &pop_descriptor_set_layout;
			pipeline_layout_ci.pushConstantRangeCount = 1;
			pipeline_layout_ci.pPushConstantRanges = &push_constant_range;

			check(vkCreatePipelineLayout(ctx.m_device, &pipeline_layout_ci, nullptr, &pop_pipeline_layout));

			check(ctx.load_shader_module_file(pop_shader_module, OCH_DIR "shaders\\simplex3d.comp.spv"));

			struct { uint32_t x, y, z; } group_size{ POPULATE_GROUP_SIZE_X, POPULATE_GROUP_SIZE_Y, POPULATE_GROUP_SIZE_Z };

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
			shader_stage_ci.module = pop_shader_module;
			shader_stage_ci.pName = "main";
			shader_stage_ci.pSpecializationInfo = &specialization_info;

			VkComputePipelineCreateInfo pipeline_ci{};
			pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
			pipeline_ci.pNext = nullptr;
			pipeline_ci.flags = 0;
			pipeline_ci.stage = shader_stage_ci;
			pipeline_ci.layout = pop_pipeline_layout;
			pipeline_ci.basePipelineHandle = nullptr;
			pipeline_ci.basePipelineIndex = -1;

			check(vkCreateComputePipelines(ctx.m_device, nullptr, 1, &pipeline_ci, nullptr, &pop_pipeline));
		}

		// Create Descriptor Sets
		{
			VkDescriptorPoolSize pool_size;
			pool_size.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			pool_size.descriptorCount = 1;

			VkDescriptorPoolCreateInfo descriptor_pool_ci{};
			descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptor_pool_ci.pNext = nullptr;
			descriptor_pool_ci.flags = 0;
			descriptor_pool_ci.maxSets = 1;
			descriptor_pool_ci.poolSizeCount = 1;
			descriptor_pool_ci.pPoolSizes = &pool_size;

			check(vkCreateDescriptorPool(ctx.m_device, &descriptor_pool_ci, nullptr, &pop_descriptor_pool));

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = pop_descriptor_pool;
			descriptor_set_ai.descriptorSetCount = 1;
			descriptor_set_ai.pSetLayouts = &pop_descriptor_set_layout;

			check(vkAllocateDescriptorSets(ctx.m_device, &descriptor_set_ai, &pop_descriptor_set));

			VkDescriptorImageInfo descriptor_image_info{};
			descriptor_image_info.sampler = nullptr;
			descriptor_image_info.imageView = base_image_view;
			descriptor_image_info.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

			VkWriteDescriptorSet write_descriptor_set{};
			write_descriptor_set.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			write_descriptor_set.pNext = nullptr;
			write_descriptor_set.dstSet = pop_descriptor_set;
			write_descriptor_set.dstBinding = 0;
			write_descriptor_set.dstArrayElement = 0;
			write_descriptor_set.descriptorCount = 1;
			write_descriptor_set.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			write_descriptor_set.pImageInfo = &descriptor_image_info;
			write_descriptor_set.pBufferInfo = nullptr;
			write_descriptor_set.pTexelBufferView = nullptr;

			vkUpdateDescriptorSets(ctx.m_device, 1, &write_descriptor_set, 0, nullptr);
		}

		// Create Command Buffer
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = 0;
			command_pool_ci.queueFamilyIndex = ctx.m_general_queues.family_index;

			check(vkCreateCommandPool(ctx.m_device, &command_pool_ci, nullptr, &pop_command_pool));

			VkCommandBufferAllocateInfo command_buffer_ai{};
			command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_ai.pNext = nullptr;
			command_buffer_ai.commandPool = pop_command_pool;
			command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_ai.commandBufferCount = 1;

			check(vkAllocateCommandBuffers(ctx.m_device, &command_buffer_ai, &pop_command_buffer));
		}

		// Submit Command Buffer
		{
			VkCommandBufferBeginInfo command_buffer_bi{};
			command_buffer_bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
			command_buffer_bi.pNext = nullptr;
			command_buffer_bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
			command_buffer_bi.pInheritanceInfo = nullptr;

			check(vkBeginCommandBuffer(pop_command_buffer, &command_buffer_bi));

			VkImageMemoryBarrier to_storage_barrier;
			to_storage_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
			to_storage_barrier.pNext = nullptr;
			to_storage_barrier.srcAccessMask = VK_ACCESS_NONE_KHR;
			to_storage_barrier.dstAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
			to_storage_barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
			to_storage_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
			to_storage_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			to_storage_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
			to_storage_barrier.image = base_image;
			to_storage_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			to_storage_barrier.subresourceRange.baseMipLevel = 0;
			to_storage_barrier.subresourceRange.levelCount = 1;
			to_storage_barrier.subresourceRange.baseArrayLayer = 0;
			to_storage_barrier.subresourceRange.layerCount = 1;

			vkCmdPipelineBarrier(pop_command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0, nullptr, 0, nullptr, 1, &to_storage_barrier);

			och::vec4 push_constant_data{ 0.0F, 0.0F, 0.0F, 0.1F };

			vkCmdPushConstants(pop_command_buffer, pop_pipeline_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(push_constant_data), &push_constant_data);

			vkCmdBindDescriptorSets(pop_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pop_pipeline_layout, 0, 1, &pop_descriptor_set, 0, nullptr);

			vkCmdBindPipeline(pop_command_buffer, VK_PIPELINE_BIND_POINT_COMPUTE, pop_pipeline);

			vkCmdDispatch(pop_command_buffer, BASE_DIM / POPULATE_GROUP_SIZE_X, BASE_DIM / POPULATE_GROUP_SIZE_Y, BASE_DIM / POPULATE_GROUP_SIZE_Z);

			check(vkEndCommandBuffer(pop_command_buffer));

			VkSubmitInfo submit_info{};
			submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
			submit_info.pNext = nullptr;
			submit_info.waitSemaphoreCount = 0;
			submit_info.pWaitSemaphores = nullptr;
			submit_info.pWaitDstStageMask = nullptr;
			submit_info.commandBufferCount = 1;
			submit_info.pCommandBuffers = &pop_command_buffer;
			submit_info.signalSemaphoreCount = 0;
			submit_info.pSignalSemaphores = nullptr;

			check(vkQueueSubmit(ctx.m_general_queues[0], 1, &submit_info, nullptr));
		}

		check(vkQueueWaitIdle(ctx.m_general_queues[0]));

		vkDestroyCommandPool(ctx.m_device, pop_command_pool, nullptr);

		vkDestroyDescriptorPool(ctx.m_device, pop_descriptor_pool, nullptr);

		vkDestroyPipeline(ctx.m_device, pop_pipeline, nullptr);

		vkDestroyShaderModule(ctx.m_device, pop_shader_module, nullptr);

		vkDestroyPipelineLayout(ctx.m_device, pop_pipeline_layout, nullptr);

		vkDestroyDescriptorSetLayout(ctx.m_device, pop_descriptor_set_layout, nullptr);

		return {};
	}

	och::status temp_copyback() noexcept
	{
		VkBuffer cb_buffer;

		VkDeviceMemory cb_memory;

		VkCommandPool cb_command_pool;

		check(ctx.create_buffer(cb_buffer, cb_memory, BASE_DIM * BASE_DIM * 2, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

		VkCommandPoolCreateInfo command_pool_ci{};
		command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_ci.pNext = nullptr;
		command_pool_ci.flags = 0;
		command_pool_ci.queueFamilyIndex = ctx.m_general_queues.family_index;

		check(vkCreateCommandPool(ctx.m_device, &command_pool_ci, nullptr, &cb_command_pool));

		VkCommandBuffer cb_command_buffer;

		check(ctx.begin_onetime_command(cb_command_buffer, cb_command_pool));

		VkBufferImageCopy copy_region;
		copy_region.bufferOffset = 0;
		copy_region.bufferRowLength = 0;
		copy_region.bufferImageHeight = 0;
		copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		copy_region.imageSubresource.mipLevel = 0;
		copy_region.imageSubresource.baseArrayLayer = 0;
		copy_region.imageSubresource.layerCount = 1;
		copy_region.imageOffset = { 0, 0, 0 };
		copy_region.imageExtent = { BASE_DIM, BASE_DIM, 1 };

		vkCmdCopyImageToBuffer(cb_command_buffer, base_image, VK_IMAGE_LAYOUT_GENERAL, cb_buffer, 1, &copy_region);

		check(ctx.submit_onetime_command(cb_command_buffer, cb_command_pool, ctx.m_general_queues.queues[0]));

		uint16_t* cb_ptr;

		check(vkMapMemory(ctx.m_device, cb_memory, 0, BASE_DIM * BASE_DIM * 2, 0, reinterpret_cast<void**>(&cb_ptr)));

		bitmap_file bmp;

		check(bmp.create("base_slice.bmp", och::fio::open::truncate, BASE_DIM, BASE_DIM));

		for (uint32_t y = 0; y != BASE_DIM; ++y)
			for (uint32_t x = 0; x != BASE_DIM; ++x)
				bmp(x, y) = texel_b8g8r8(static_cast<uint8_t>(cb_ptr[x + y * BASE_DIM]));

		bmp.destroy();

		vkUnmapMemory(ctx.m_device, cb_memory);

		vkDestroyBuffer(ctx.m_device, cb_buffer, nullptr);

		vkFreeMemory(ctx.m_device, cb_memory, nullptr);

		vkDestroyCommandPool(ctx.m_device, cb_command_pool, nullptr);

		return {};
	}

	och::status create() noexcept
	{
		check(ctx.create("Voxel Volume", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));

		// Create Base Image
		check(ctx.create_image_with_view(base_image_view, base_image, base_image_memory, 
			{ BASE_DIM * LEVEL_CNT, BASE_DIM, BASE_DIM },
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
			VK_IMAGE_TYPE_3D, 
			VK_IMAGE_VIEW_TYPE_3D, 
			VK_FORMAT_R16_UINT, 
			VK_FORMAT_R16_UINT, 
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		// Allocate Brick buffer
		check(ctx.create_buffer(brick_buffer, brick_memory, BRICK_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		// Allocate Leaf buffer
		check(ctx.create_buffer(leaf_buffer, leaf_memory, LEAF_BYTES, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		// Allocate hit index images
		check(ctx.create_images_with_views(
			MAX_FRAMES_INFLIGHT,
			hit_index_image_views, hit_index_images, hit_index_memory,
			{ ctx.m_swapchain_extent.width, ctx.m_swapchain_extent.height, 1 },
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_R16_UINT,
			VK_FORMAT_R16_UINT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		// Allocate hit times images
		check(ctx.create_images_with_views(
			MAX_FRAMES_INFLIGHT,
			hit_times_image_views, hit_times_images, hit_times_memory,
			{ ctx.m_swapchain_extent.width, ctx.m_swapchain_extent.height, 1 },
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_R32_SFLOAT,
			VK_FORMAT_R32_SFLOAT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		// Create Pipeline
		{
			check(ctx.load_shader_module_file(trace_shader_module, OCH_DIR "shaders\\voxel_volume_trace.comp.spv"));

struct
{
	uint32_t group_size_x = TRACE_GROUP_SIZE_X;
	uint32_t group_size_y = TRACE_GROUP_SIZE_Y;
	uint32_t base_dim_log2 = BASE_DIM_LOG2;
	uint32_t brick_dim_log2 = BRICK_DIM_LOG2;
	uint32_t level_cnt = LEVEL_CNT;
} specialization_data;

VkSpecializationMapEntry specialization_entries[]{
	{ 1, offsetof(decltype(specialization_data), group_size_x), sizeof(uint32_t) },
	{ 2, offsetof(decltype(specialization_data), group_size_y), sizeof(uint32_t) },
	{ 3, offsetof(decltype(specialization_data), base_dim_log2), sizeof(uint32_t) },
	{ 4, offsetof(decltype(specialization_data), brick_dim_log2), sizeof(uint32_t) },
	{ 5, offsetof(decltype(specialization_data), level_cnt), sizeof(uint32_t) },
};

VkSpecializationInfo specialization_info{};
specialization_info.mapEntryCount = _countof(specialization_entries);
specialization_info.pMapEntries = specialization_entries;
specialization_info.dataSize = sizeof(specialization_data);
specialization_info.pData = &specialization_data;

VkDescriptorSetLayoutBinding descriptor_set_layout_bindings[5]{};
// Base image array
descriptor_set_layout_bindings[0].binding = 0;
descriptor_set_layout_bindings[0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
descriptor_set_layout_bindings[0].descriptorCount = 1;
descriptor_set_layout_bindings[0].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
descriptor_set_layout_bindings[0].pImmutableSamplers = nullptr;
// Brick buffer
descriptor_set_layout_bindings[1].binding = 1;
descriptor_set_layout_bindings[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
descriptor_set_layout_bindings[1].descriptorCount = 1;
descriptor_set_layout_bindings[1].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
descriptor_set_layout_bindings[1].pImmutableSamplers = nullptr;
// Leaf buffer
descriptor_set_layout_bindings[2].binding = 2;
descriptor_set_layout_bindings[2].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
descriptor_set_layout_bindings[2].descriptorCount = 1;
descriptor_set_layout_bindings[2].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
descriptor_set_layout_bindings[2].pImmutableSamplers = nullptr;
// Hit ids
descriptor_set_layout_bindings[3].binding = 3;
descriptor_set_layout_bindings[3].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
descriptor_set_layout_bindings[3].descriptorCount = 1;
descriptor_set_layout_bindings[3].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
descriptor_set_layout_bindings[3].pImmutableSamplers = nullptr;
// Hit times
descriptor_set_layout_bindings[4].binding = 4;
descriptor_set_layout_bindings[4].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
descriptor_set_layout_bindings[4].descriptorCount = 1;
descriptor_set_layout_bindings[4].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
descriptor_set_layout_bindings[4].pImmutableSamplers = nullptr;

VkDescriptorSetLayoutCreateInfo descriptor_set_layout_ci{};
descriptor_set_layout_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
descriptor_set_layout_ci.pNext = nullptr;
descriptor_set_layout_ci.flags = 0;
descriptor_set_layout_ci.bindingCount = 5;
descriptor_set_layout_ci.pBindings = descriptor_set_layout_bindings;

check(vkCreateDescriptorSetLayout(ctx.m_device, &descriptor_set_layout_ci, nullptr, &descriptor_set_layout));

VkPushConstantRange push_constant_range;
push_constant_range.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
push_constant_range.offset = 0;
push_constant_range.size = sizeof(push_constant_data_t);

VkPipelineLayoutCreateInfo pipeline_layout_ci{};
pipeline_layout_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
pipeline_layout_ci.pNext = nullptr;
pipeline_layout_ci.flags = 0;
pipeline_layout_ci.setLayoutCount = 1;
pipeline_layout_ci.pSetLayouts = &descriptor_set_layout;
pipeline_layout_ci.pushConstantRangeCount = 1;
pipeline_layout_ci.pPushConstantRanges = &push_constant_range;

check(vkCreatePipelineLayout(ctx.m_device, &pipeline_layout_ci, nullptr, &pipeline_layout));

VkComputePipelineCreateInfo pipeline_ci{};
pipeline_ci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
pipeline_ci.pNext = nullptr;
pipeline_ci.flags = 0;
pipeline_ci.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
pipeline_ci.stage.pNext = nullptr;
pipeline_ci.stage.flags = 0;
pipeline_ci.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
pipeline_ci.stage.module = trace_shader_module;
pipeline_ci.stage.pName = "main";
pipeline_ci.stage.pSpecializationInfo = &specialization_info;
pipeline_ci.layout = pipeline_layout;
pipeline_ci.basePipelineHandle = nullptr;
pipeline_ci.basePipelineIndex = -1;

check(vkCreateComputePipelines(ctx.m_device, nullptr, 1, &pipeline_ci, nullptr, &pipeline));
		}

		// Create Descriptors
		{
			VkDescriptorPoolSize descriptor_pool_sizes[2]{};
			descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			descriptor_pool_sizes[0].descriptorCount = 3 * MAX_FRAMES_INFLIGHT;
			descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			descriptor_pool_sizes[1].descriptorCount = 2 * MAX_FRAMES_INFLIGHT;

			VkDescriptorPoolCreateInfo descriptor_pool_ci{};
			descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
			descriptor_pool_ci.pNext = nullptr;
			descriptor_pool_ci.flags = 0;
			descriptor_pool_ci.maxSets = MAX_FRAMES_INFLIGHT;
			descriptor_pool_ci.poolSizeCount = 2;
			descriptor_pool_ci.pPoolSizes = descriptor_pool_sizes;

			check(vkCreateDescriptorPool(ctx.m_device, &descriptor_pool_ci, nullptr, &descriptor_pool));

			VkDescriptorSetLayout descriptor_set_layouts[MAX_FRAMES_INFLIGHT];

			for (uint32_t i = 0; i != MAX_FRAMES_INFLIGHT; ++i)
				descriptor_set_layouts[i] = descriptor_set_layout;

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = descriptor_pool;
			descriptor_set_ai.descriptorSetCount = MAX_FRAMES_INFLIGHT;
			descriptor_set_ai.pSetLayouts = descriptor_set_layouts;

			check(vkAllocateDescriptorSets(ctx.m_device, &descriptor_set_ai, descriptor_sets));

			// VkDescriptorImageInfo image_infos[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT * 3];
			// 
			// VkDescriptorBufferInfo buffer_infos[2]
			// {
			// 	{ brick_buffer, 0, VK_WHOLE_SIZE },
			// 	{ leaf_buffer , 0, VK_WHOLE_SIZE },
			// };
			// 
			// VkWriteDescriptorSet writes[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT * 2];
			// 
			// for (uint32_t i = 0; i != ctx.m_swapchain_image_cnt; ++i)
			// {
			// 	image_infos[3 * i + 0].sampler = nullptr;
			// 	image_infos[3 * i + 0].imageView = hit_index_image_views[i];
			// 	image_infos[3 * i + 0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			// 
			// 	image_infos[3 * i + 1].sampler = nullptr;
			// 	image_infos[3 * i + 1].imageView = hit_times_image_views[i];
			// 	image_infos[3 * i + 1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			// 
			// 	image_infos[3 * i + 2].sampler = nullptr;
			// 	image_infos[3 * i + 2].imageView = base_image_view;
			// 	image_infos[3 * i + 2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
			// 
			// 	writes[2 * i + 0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			// 	writes[2 * i + 0].pNext = nullptr;
			// 	writes[2 * i + 0].dstSet = descriptor_sets[i];
			// 	writes[2 * i + 0].dstBinding = 0;
			// 	writes[2 * i + 0].dstArrayElement = 0;
			// 	writes[2 * i + 0].descriptorCount = 3;
			// 	writes[2 * i + 0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			// 	writes[2 * i + 0].pImageInfo = image_infos;
			// 	writes[2 * i + 0].pBufferInfo = nullptr;
			// 	writes[2 * i + 0].pTexelBufferView = nullptr;
			// 
			// 	writes[2 * i + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			// 	writes[2 * i + 1].pNext = nullptr;
			// 	writes[2 * i + 1].dstSet = descriptor_sets[i];
			// 	writes[2 * i + 1].dstBinding = 3;
			// 	writes[2 * i + 1].dstArrayElement = 0;
			// 	writes[2 * i + 1].descriptorCount = 1;
			// 	writes[2 * i + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			// 	writes[2 * i + 1].pImageInfo = nullptr;
			// 	writes[2 * i + 1].pBufferInfo = buffer_infos;
			// 	writes[2 * i + 1].pTexelBufferView = nullptr;
			// }
			// 
			// vkUpdateDescriptorSets(ctx.m_device, ctx.m_swapchain_image_cnt * 2, writes, 0, nullptr);
		}

		// Create Command Buffers
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = 0;
			command_pool_ci.queueFamilyIndex = ctx.m_general_queues.family_index;

			check(vkCreateCommandPool(ctx.m_device, &command_pool_ci, nullptr, &command_pool));

			VkCommandBufferAllocateInfo command_buffer_ai{};
			command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_ai.pNext = nullptr;
			command_buffer_ai.commandPool = command_pool;
			command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_ai.commandBufferCount = MAX_FRAMES_INFLIGHT;

			check(vkAllocateCommandBuffers(ctx.m_device, &command_buffer_ai, command_buffers));
		}

		// Create sync resources
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
				check(vkCreateSemaphore(ctx.m_device, &semaphore_ci, nullptr, &image_available_semaphores[i]));

				check(vkCreateSemaphore(ctx.m_device, &semaphore_ci, nullptr, &render_complete_semaphores[i]));

				check(vkCreateFence(ctx.m_device, &fence_ci, nullptr, &frame_inflight_fences[i]));
			}
		}

		check(temp_populate());

		check(temp_copyback());

		return {};
	}

	void destroy() noexcept
	{
		if (vkDeviceWaitIdle(ctx.m_device) != VK_SUCCESS)
			return;

		for (uint32_t i = 0; i != MAX_FRAMES_INFLIGHT; ++i)
		{
			vkDestroySemaphore(ctx.m_device, image_available_semaphores[i], nullptr);

			vkDestroySemaphore(ctx.m_device, render_complete_semaphores[i], nullptr);

			vkDestroyFence(ctx.m_device, frame_inflight_fences[i], nullptr);
		}

		vkDestroyPipeline(ctx.m_device, pipeline, nullptr);

		vkDestroyPipelineLayout(ctx.m_device, pipeline_layout, nullptr);

		vkDestroyDescriptorSetLayout(ctx.m_device, descriptor_set_layout, nullptr);

		vkDestroyShaderModule(ctx.m_device, trace_shader_module, nullptr);



		vkDestroyDescriptorPool(ctx.m_device, descriptor_pool, nullptr);

		vkDestroyCommandPool(ctx.m_device, command_pool, nullptr);



		for (uint32_t i = 0; i != MAX_FRAMES_INFLIGHT; ++i)
		{
			vkDestroyImageView(ctx.m_device, hit_times_image_views[i], nullptr);
		
			vkDestroyImage(ctx.m_device, hit_times_images[i], nullptr);

			vkDestroyImageView(ctx.m_device, hit_index_image_views[i], nullptr);

			vkDestroyImage(ctx.m_device, hit_index_images[i], nullptr);
		}

		vkFreeMemory(ctx.m_device, hit_times_memory, nullptr);

		vkFreeMemory(ctx.m_device, hit_index_memory, nullptr);



		vkDestroyImageView(ctx.m_device, base_image_view, nullptr);

		vkDestroyImage(ctx.m_device, base_image, nullptr);

		vkFreeMemory(ctx.m_device, base_image_memory, nullptr);

		vkDestroyBuffer(ctx.m_device, brick_buffer, nullptr);

		vkFreeMemory(ctx.m_device, brick_memory, nullptr);

		vkDestroyBuffer(ctx.m_device, leaf_buffer, nullptr);

		vkFreeMemory(ctx.m_device, leaf_memory, nullptr);



		ctx.destroy();
	}

	och::status run() noexcept
	{


		return {};
	}
};

och::status run_voxel_volume(int argc, const char** argv) noexcept
{
	argc; argv;

	voxel_volume program;

	och::status err = program.create();

	if (!err)
		err = program.run();

	program.destroy();

	check(err);

	return {};
}