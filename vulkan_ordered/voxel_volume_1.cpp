#include "voxel_volume.h"

#include "vulkan_base.h"

#include "directory_constants.h"
#include "bitmap.h"

#include "och_fmt.h"
#include "och_matmath.h"
#include "och_timer.h"

#define TEMP_STATUS_MACRO to_status(och::status(1, och::error_type::och))

och::status parse_numeric_argument(uint32_t& out_number, const char* arg, bool must_be_pow2, bool must_be_nonzero) noexcept
{
	uint32_t n = 0;

	for (const char* curr = arg; *curr; ++curr)
		if (*curr > '9' || *curr < '0')
			return TEMP_STATUS_MACRO; // Argument must be numeric
		else
			n = n * 10 + *curr - '0';

	if (n == 0 && must_be_nonzero)
		return TEMP_STATUS_MACRO; // Argument must not be zero

	out_number = n;

	if (!must_be_pow2)
		return {};

	for (uint32_t i = 0; i != 32; ++i)
		if ((n & (1 << i)))
			if ((n & ~(1 << i)))
				return TEMP_STATUS_MACRO; // Argument must be a power of two
			else
				break;

	return {};
}

struct voxel_volume
{
	struct push_constant_data_t
	{
		och::vec4 origin;
		och::vec4 direction_delta;
		och::mat3 direction_rotation;
	};

	using base_elem_t = uint32_t;
	
	using brick_elem_t = uint32_t;

	using leaf_elem_t = uint32_t;



	static constexpr uint32_t LEVEL_CNT = 2;

	static constexpr uint32_t BASE_DIM_LOG2 = 6;

	static constexpr uint32_t BASE_DIM = 1 << BASE_DIM_LOG2;
	
	static constexpr uint32_t BASE_VOL = BASE_DIM * BASE_DIM * BASE_DIM;
	
	static constexpr uint32_t BRICK_DIM_LOG2 = 4;

	static constexpr uint32_t BRICK_DIM = 1 << BRICK_DIM_LOG2;
	
	static constexpr uint32_t BRICK_VOL = BRICK_DIM * BRICK_DIM * BRICK_DIM;
	
	static constexpr uint32_t LEAF_DIM = 2;
	
	static constexpr uint32_t LEAF_VOL = LEAF_DIM * LEAF_DIM * LEAF_DIM;

	static constexpr uint64_t BASE_BYTES_IN_TOTAL = BASE_VOL * sizeof(base_elem_t);

	static constexpr float BASE_OCCUPANCY = 0.25F;

	static constexpr uint32_t BRICK_CNT_PER_LEVEL = static_cast<uint32_t>(BASE_VOL * BASE_OCCUPANCY);

	static constexpr uint32_t BRICK_CNT_IN_TOTAL = BRICK_CNT_PER_LEVEL * LEVEL_CNT;

	static constexpr uint64_t BRICK_BYTES_IN_TOTAL = BRICK_CNT_IN_TOTAL * BRICK_VOL * sizeof(brick_elem_t);

	static constexpr float BRICK_OCCUPANCY = 0.5F;

	static constexpr uint32_t LEAF_CNT_PER_LEVEL = static_cast<uint32_t>(BRICK_CNT_PER_LEVEL * BRICK_OCCUPANCY);
	
	static constexpr uint32_t LEAF_CNT_IN_TOTAL = LEAF_CNT_PER_LEVEL * LEVEL_CNT;

	static constexpr uint64_t LEAF_BYTES_IN_TOTAL = LEAF_CNT_IN_TOTAL * LEAF_VOL * sizeof(leaf_elem_t);



	static constexpr uint32_t POPULATE_GROUP_SIZE_X = 8;

	static constexpr uint32_t POPULATE_GROUP_SIZE_Y = 8;
	
	static constexpr uint32_t POPULATE_GROUP_SIZE_Z = 8;

	static constexpr uint32_t TRACE_GROUP_SIZE_X = 8;

	static constexpr uint32_t TRACE_GROUP_SIZE_Y = 8;



	static constexpr uint32_t MAX_FRAMES_INFLIGHT = 2;



	vulkan_context context;

	VkImageView base_image_view{};

	VkImage base_image_array{};

	VkDeviceMemory base_memory{};

	VkBuffer brick_buffer{};

	VkDeviceMemory brick_memory{};

	VkBuffer leaf_buffer{};

	VkDeviceMemory leaf_memory{};



	VkImage hit_index_images[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkImageView hit_index_image_views[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkDeviceMemory hit_index_memory;

	VkImage hit_times_images[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkImageView hit_times_image_views[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

	VkDeviceMemory hit_times_memory;



	VkDescriptorSetLayout descriptor_set_layout{};

	VkPipelineLayout pipeline_layout{};

	VkShaderModule trace_shader_module{};

	VkPipeline pipeline{};



	VkDescriptorPool descriptor_pool{};

	VkDescriptorSet descriptor_sets[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};

	VkCommandPool command_pool{};

	VkCommandBuffer command_buffers[MAX_FRAMES_INFLIGHT]{};



	VkSemaphore image_available_semaphores[MAX_FRAMES_INFLIGHT]{};

	VkSemaphore render_complete_semaphores[MAX_FRAMES_INFLIGHT]{};

	VkFence frame_inflight_fences[MAX_FRAMES_INFLIGHT]{};

	VkFence image_inflight_fences[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT]{};

	uint32_t frame_idx{};
	

	och::status allocate_base() noexcept
	{
		VkImageCreateInfo image_ci{};
		image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
		image_ci.pNext = nullptr;
		image_ci.flags = 0;
		image_ci.imageType = VK_IMAGE_TYPE_3D;
		image_ci.format = VK_FORMAT_R32_UINT;
		image_ci.extent = { BASE_DIM * LEVEL_CNT, BASE_DIM, BASE_DIM };
		image_ci.mipLevels = 1;
		image_ci.arrayLayers = 1;
		image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
		image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
		image_ci.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
		image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
		image_ci.queueFamilyIndexCount = 0;
		image_ci.pQueueFamilyIndices = nullptr;
		image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

		check(vkCreateImage(context.m_device, &image_ci, nullptr, &base_image_array));

		VkMemoryRequirements mem_reqs;

		vkGetImageMemoryRequirements(context.m_device, base_image_array, &mem_reqs);

		uint32_t mem_idx;

		check(context.suitable_memory_type_idx(mem_idx, mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		VkMemoryAllocateInfo mem_ai{};
		mem_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		mem_ai.pNext = nullptr;
		mem_ai.allocationSize = mem_reqs.size;
		mem_ai.memoryTypeIndex = mem_idx;

		check(vkAllocateMemory(context.m_device, &mem_ai, nullptr, &base_memory));

		check(vkBindImageMemory(context.m_device, base_image_array, base_memory, 0));

		VkImageSubresourceRange subresource_range{};
		subresource_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		subresource_range.baseMipLevel = 0;
		subresource_range.levelCount = 1;
		subresource_range.baseArrayLayer = 0;
		subresource_range.layerCount = 1;

		VkImageViewCreateInfo image_view_ci{};
		image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		image_view_ci.pNext = nullptr;
		image_view_ci.flags = 0;
		image_view_ci.image = base_image_array;
		image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_3D;
		image_view_ci.format = VK_FORMAT_R32_UINT;
		image_view_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
		image_view_ci.subresourceRange = subresource_range;

		check(vkCreateImageView(context.m_device, &image_view_ci, nullptr, &base_image_view));

		return {};
	}

	och::status allocate_bricks() noexcept
	{
		check(context.create_buffer(brick_buffer, brick_memory, BRICK_BYTES_IN_TOTAL, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		return {};
	}

	och::status allocate_leaves() noexcept
	{
		check(context.create_buffer(leaf_buffer, leaf_memory, LEAF_BYTES_IN_TOTAL, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		return {};
	}

	och::status populate() noexcept
	{
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

			check(vkCreateDescriptorSetLayout(context.m_device, &descriptor_set_layout_ci, nullptr, &pop_descriptor_set_layout));

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

			check(vkCreatePipelineLayout(context.m_device, &pipeline_layout_ci, nullptr, &pop_pipeline_layout));

			check(context.load_shader_module_file(pop_shader_module, OCH_DIR "shaders\\simplex3d.comp.spv"));

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

			check(vkCreateComputePipelines(context.m_device, nullptr, 1, &pipeline_ci, nullptr, &pop_pipeline));
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

			check(vkCreateDescriptorPool(context.m_device, &descriptor_pool_ci, nullptr, &pop_descriptor_pool));

			VkDescriptorSetAllocateInfo descriptor_set_ai{};
			descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
			descriptor_set_ai.pNext = nullptr;
			descriptor_set_ai.descriptorPool = pop_descriptor_pool;
			descriptor_set_ai.descriptorSetCount = 1;
			descriptor_set_ai.pSetLayouts = &pop_descriptor_set_layout;

			check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, &pop_descriptor_set));

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

			vkUpdateDescriptorSets(context.m_device, 1, &write_descriptor_set, 0, nullptr);
		}

		// Create Command Buffer
		{
			VkCommandPoolCreateInfo command_pool_ci{};
			command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			command_pool_ci.pNext = nullptr;
			command_pool_ci.flags = 0;
			command_pool_ci.queueFamilyIndex = context.m_general_queues.family_index;

			check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &pop_command_pool));

			VkCommandBufferAllocateInfo command_buffer_ai{};
			command_buffer_ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			command_buffer_ai.pNext = nullptr;
			command_buffer_ai.commandPool = pop_command_pool;
			command_buffer_ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			command_buffer_ai.commandBufferCount = 1;

			check(vkAllocateCommandBuffers(context.m_device, &command_buffer_ai, &pop_command_buffer));
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
			to_storage_barrier.image = base_image_array;
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

			check(vkQueueSubmit(context.m_general_queues[0], 1, &submit_info, nullptr));
		}

		check(vkQueueWaitIdle(context.m_general_queues[0]));

		vkDestroyCommandPool(context.m_device, pop_command_pool, nullptr);

		vkDestroyDescriptorPool(context.m_device, pop_descriptor_pool, nullptr);

		vkDestroyPipeline(context.m_device, pop_pipeline, nullptr);

		vkDestroyShaderModule(context.m_device, pop_shader_module, nullptr);

		vkDestroyPipelineLayout(context.m_device, pop_pipeline_layout, nullptr);

		vkDestroyDescriptorSetLayout(context.m_device, pop_descriptor_set_layout, nullptr);

		return {};
	}

	och::status create_output_images() noexcept
	{
		// Create images
		{
			VkImageCreateInfo hit_idx_image_ci{};
			hit_idx_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			hit_idx_image_ci.pNext = nullptr;
			hit_idx_image_ci.flags = 0;
			hit_idx_image_ci.imageType = VK_IMAGE_TYPE_2D;
			hit_idx_image_ci.format = VK_FORMAT_R16_UINT;
			hit_idx_image_ci.extent = { context.m_swapchain_extent.width, context.m_swapchain_extent.height, 1 };
			hit_idx_image_ci.mipLevels = 1;
			hit_idx_image_ci.arrayLayers = 1;
			hit_idx_image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
			hit_idx_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
			hit_idx_image_ci.usage = VK_IMAGE_USAGE_STORAGE_BIT;
			hit_idx_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			hit_idx_image_ci.queueFamilyIndexCount = 0;
			hit_idx_image_ci.pQueueFamilyIndices = nullptr;
			hit_idx_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			VkImageCreateInfo hit_times_image_ci{};
			hit_times_image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
			hit_times_image_ci.pNext = nullptr;
			hit_times_image_ci.flags = 0;
			hit_times_image_ci.imageType = VK_IMAGE_TYPE_2D;
			hit_times_image_ci.format = VK_FORMAT_R32_SFLOAT;
			hit_times_image_ci.extent = { context.m_swapchain_extent.width, context.m_swapchain_extent.height, 1 };
			hit_times_image_ci.mipLevels = 1;
			hit_times_image_ci.arrayLayers = 1;
			hit_times_image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
			hit_times_image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
			hit_times_image_ci.usage = VK_IMAGE_USAGE_STORAGE_BIT;
			hit_times_image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
			hit_times_image_ci.queueFamilyIndexCount = 0;
			hit_times_image_ci.pQueueFamilyIndices = nullptr;
			hit_times_image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

			for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			{
				check(vkCreateImage(context.m_device, &hit_idx_image_ci, nullptr, &hit_index_images[i]));

				check(vkCreateImage(context.m_device, &hit_times_image_ci, nullptr, &hit_times_images[i]));
			}
		}

		// Bind images to memory
		{
			VkMemoryRequirements hit_index_mem_reqs;

			VkMemoryRequirements hit_times_mem_reqs;

			vkGetImageMemoryRequirements(context.m_device, hit_index_images[0], &hit_index_mem_reqs);

			vkGetImageMemoryRequirements(context.m_device, hit_times_images[0], &hit_times_mem_reqs);

			uint32_t hit_index_memory_type_idx;

			uint32_t hit_times_memory_type_idx;

			check(context.suitable_memory_type_idx(hit_index_memory_type_idx, hit_index_mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

			check(context.suitable_memory_type_idx(hit_times_memory_type_idx, hit_times_mem_reqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

			size_t hit_index_aligned_size = find_aligned_size(hit_index_mem_reqs.size, hit_index_mem_reqs.alignment);

			size_t hit_times_aligned_size = find_aligned_size(hit_times_mem_reqs.size, hit_times_mem_reqs.alignment);

			VkMemoryAllocateInfo hit_index_memory_ai{};
			hit_index_memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			hit_index_memory_ai.pNext = nullptr;
			hit_index_memory_ai.allocationSize = hit_index_aligned_size * (context.m_swapchain_image_cnt - 1) + hit_index_mem_reqs.size;
			hit_index_memory_ai.memoryTypeIndex = hit_index_memory_type_idx;

			VkMemoryAllocateInfo hit_times_memory_ai{};
			hit_times_memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			hit_times_memory_ai.pNext = nullptr;
			hit_times_memory_ai.allocationSize = hit_times_aligned_size * (context.m_swapchain_image_cnt - 1) + hit_times_mem_reqs.size;
			hit_times_memory_ai.memoryTypeIndex = hit_times_memory_type_idx;

			check(vkAllocateMemory(context.m_device, &hit_index_memory_ai, nullptr, &hit_index_memory));

			check(vkAllocateMemory(context.m_device, &hit_times_memory_ai, nullptr, &hit_times_memory));

			for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			{
				check(vkBindImageMemory(context.m_device, hit_index_images[i], hit_index_memory, hit_index_aligned_size * i));

				check(vkBindImageMemory(context.m_device, hit_times_images[i], hit_times_memory, hit_times_aligned_size * i));
			}
		}

		// Create image views
		{
			VkImageViewCreateInfo hit_index_image_view_ci{};
			hit_index_image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			hit_index_image_view_ci.pNext = nullptr;
			hit_index_image_view_ci.flags = 0;
			hit_index_image_view_ci.image = nullptr;
			hit_index_image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			hit_index_image_view_ci.format = VK_FORMAT_R16_UINT;
			hit_index_image_view_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
			hit_index_image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			hit_index_image_view_ci.subresourceRange.baseMipLevel = 0;
			hit_index_image_view_ci.subresourceRange.levelCount = 1;
			hit_index_image_view_ci.subresourceRange.baseArrayLayer = 0;
			hit_index_image_view_ci.subresourceRange.layerCount = 1;

			VkImageViewCreateInfo hit_times_image_view_ci{};
			hit_times_image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			hit_times_image_view_ci.pNext = nullptr;
			hit_times_image_view_ci.flags = 0;
			hit_times_image_view_ci.image = nullptr;
			hit_times_image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			hit_times_image_view_ci.format = VK_FORMAT_R32_SFLOAT;
			hit_times_image_view_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
			hit_times_image_view_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			hit_times_image_view_ci.subresourceRange.baseMipLevel = 0;
			hit_times_image_view_ci.subresourceRange.levelCount = 1;
			hit_times_image_view_ci.subresourceRange.baseArrayLayer = 0;
			hit_times_image_view_ci.subresourceRange.layerCount = 1;

			for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			{
				hit_index_image_view_ci.image = hit_index_images[i];

				check(vkCreateImageView(context.m_device, &hit_index_image_view_ci, nullptr, &hit_index_image_views[i]));

				hit_times_image_view_ci.image = hit_times_images[i];

				check(vkCreateImageView(context.m_device, &hit_times_image_view_ci, nullptr, &hit_times_image_views[i]));
			}
		}

		return {};
	}

	och::status create_pipeline() noexcept
	{
		check(context.load_shader_module_file(trace_shader_module, OCH_DIR "shaders\\voxel_volume_trace.comp.spv"));

		struct
		{
			uint32_t group_size_x = TRACE_GROUP_SIZE_X;
			uint32_t group_size_y = TRACE_GROUP_SIZE_Y;
			uint32_t base_dim_log2 = BASE_DIM_LOG2;
			uint32_t brick_dim_log2 = BRICK_DIM_LOG2;
			uint32_t level_cnt = LEVEL_CNT;
		} specialization_data;

		VkSpecializationMapEntry specialization_entries[]{
			{ 1, offsetof(decltype(specialization_data), group_size_x  ), sizeof(uint32_t) },
			{ 2, offsetof(decltype(specialization_data), group_size_y  ), sizeof(uint32_t) },
			{ 3, offsetof(decltype(specialization_data), base_dim_log2 ), sizeof(uint32_t) },
			{ 4, offsetof(decltype(specialization_data), brick_dim_log2), sizeof(uint32_t) },
			{ 5, offsetof(decltype(specialization_data), level_cnt     ), sizeof(uint32_t) },
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

		check(vkCreateDescriptorSetLayout(context.m_device, &descriptor_set_layout_ci, nullptr, &descriptor_set_layout));

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

		check(vkCreatePipelineLayout(context.m_device, &pipeline_layout_ci, nullptr, &pipeline_layout));

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

		check(vkCreateComputePipelines(context.m_device, nullptr, 1, &pipeline_ci, nullptr, &pipeline));

		return {};
	}

	och::status copy_back() noexcept
	{
		VkBuffer cb_buffer;

		VkDeviceMemory cb_memory;

		VkCommandPool cb_command_pool;

		check(context.create_buffer(cb_buffer, cb_memory, BASE_DIM * BASE_DIM * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

		VkCommandPoolCreateInfo command_pool_ci{};
		command_pool_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
		command_pool_ci.pNext = nullptr;
		command_pool_ci.flags = 0;
		command_pool_ci.queueFamilyIndex = context.m_general_queues.family_index;

		check(vkCreateCommandPool(context.m_device, &command_pool_ci, nullptr, &cb_command_pool));

		VkCommandBuffer cb_command_buffer;

		check(context.begin_onetime_command(cb_command_buffer, cb_command_pool));

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
		
		vkCmdCopyImageToBuffer(cb_command_buffer, base_image_array, VK_IMAGE_LAYOUT_GENERAL, cb_buffer, 1, &copy_region);

		check(context.submit_onetime_command(cb_command_buffer, cb_command_pool, context.m_general_queues.queues[0]));

		uint32_t* cb_ptr;

		check(vkMapMemory(context.m_device, cb_memory, 0, BASE_DIM * BASE_DIM * 4, 0, reinterpret_cast<void**>(&cb_ptr)));

		bitmap_file bmp;

		check(bmp.create("base_slice.bmp", och::fio::open::truncate, BASE_DIM, BASE_DIM));

		for (uint32_t y = 0; y != BASE_DIM; ++y)
			for (uint32_t x = 0; x != BASE_DIM; ++x)
				bmp(x, y) = texel_b8g8r8(static_cast<uint8_t>(cb_ptr[x + y * BASE_DIM]));

		bmp.destroy();

		vkUnmapMemory(context.m_device, cb_memory);

		vkDestroyBuffer(context.m_device, cb_buffer, nullptr);
		
		vkFreeMemory(context.m_device, cb_memory, nullptr);

		vkDestroyCommandPool(context.m_device, cb_command_pool, nullptr);

		return {};
	}

	och::status create_descriptor_resources() noexcept
	{
		VkDescriptorPoolSize descriptor_pool_sizes[2]{};
		descriptor_pool_sizes[0].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		descriptor_pool_sizes[0].descriptorCount = 3 * context.m_swapchain_image_cnt;
		descriptor_pool_sizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		descriptor_pool_sizes[1].descriptorCount = 2 * context.m_swapchain_image_cnt;

		VkDescriptorPoolCreateInfo descriptor_pool_ci{};
		descriptor_pool_ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		descriptor_pool_ci.pNext = nullptr;
		descriptor_pool_ci.flags = 0;
		descriptor_pool_ci.maxSets = context.m_swapchain_image_cnt;
		descriptor_pool_ci.poolSizeCount = 2;
		descriptor_pool_ci.pPoolSizes = descriptor_pool_sizes;

		check(vkCreateDescriptorPool(context.m_device, &descriptor_pool_ci, nullptr, &descriptor_pool));

		VkDescriptorSetLayout descriptor_set_layouts[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT];

		for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
			descriptor_set_layouts[i] = descriptor_set_layout;

		VkDescriptorSetAllocateInfo descriptor_set_ai{};
		descriptor_set_ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		descriptor_set_ai.pNext = nullptr;
		descriptor_set_ai.descriptorPool = descriptor_pool;
		descriptor_set_ai.descriptorSetCount = context.m_swapchain_image_cnt;
		descriptor_set_ai.pSetLayouts = descriptor_set_layouts;

		check(vkAllocateDescriptorSets(context.m_device, &descriptor_set_ai, descriptor_sets));

		VkDescriptorImageInfo image_infos[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT * 3];
		
		VkDescriptorBufferInfo buffer_infos[2]
		{
			{ brick_buffer, 0, VK_WHOLE_SIZE },
			{ leaf_buffer , 0, VK_WHOLE_SIZE },
		};
		
		VkWriteDescriptorSet writes[vulkan_context::MAX_SWAPCHAIN_IMAGE_CNT * 2];
		
		for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
		{
			image_infos[3 * i + 0].sampler = nullptr;
			image_infos[3 * i + 0].imageView = hit_index_image_views[i];
			image_infos[3 * i + 0].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		
			image_infos[3 * i + 1].sampler = nullptr;
			image_infos[3 * i + 1].imageView = hit_times_image_views[i];
			image_infos[3 * i + 1].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		
			image_infos[3 * i + 2].sampler = nullptr;
			image_infos[3 * i + 2].imageView = base_image_view;
			image_infos[3 * i + 2].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
		
			writes[2 * i + 0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2 * i + 0].pNext = nullptr;
			writes[2 * i + 0].dstSet = descriptor_sets[i];
			writes[2 * i + 0].dstBinding = 0;
			writes[2 * i + 0].dstArrayElement = 0;
			writes[2 * i + 0].descriptorCount = 3;
			writes[2 * i + 0].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
			writes[2 * i + 0].pImageInfo = image_infos;
			writes[2 * i + 0].pBufferInfo = nullptr;
			writes[2 * i + 0].pTexelBufferView = nullptr;
		
			writes[2 * i + 1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
			writes[2 * i + 1].pNext = nullptr;
			writes[2 * i + 1].dstSet = descriptor_sets[i];
			writes[2 * i + 1].dstBinding = 3;
			writes[2 * i + 1].dstArrayElement = 0;
			writes[2 * i + 1].descriptorCount = 1;
			writes[2 * i + 1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
			writes[2 * i + 1].pImageInfo = nullptr;
			writes[2 * i + 1].pBufferInfo = buffer_infos;
			writes[2 * i + 1].pTexelBufferView = nullptr;
		}
		
		vkUpdateDescriptorSets(context.m_device, context.m_swapchain_image_cnt * 2, writes, 0, nullptr);

		return {};
	}

	och::status create_command_resources() noexcept
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
		command_buffer_ai.commandBufferCount = MAX_FRAMES_INFLIGHT;

		check(vkAllocateCommandBuffers(context.m_device, &command_buffer_ai, command_buffers));

		return {};
	}

	och::status create_sync_objects() noexcept
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

		return {};
	}

	och::status create(int argc, const char** argv) noexcept
	{
		argc; argv;

		och::timer running_time;

		och::print("BEG\n");

		check(context.create("Voxel Volume", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));
		
		check(allocate_base());

		check(allocate_bricks());

		check(allocate_leaves());

		check(populate());

		// check(copy_back());

		check(create_output_images());

		check(create_pipeline());

		check(create_descriptor_resources());

		check(create_command_resources());

		check(create_sync_objects());

		och::print("END\n{} elapsed\n", running_time.read());

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
			}
			else if (acquire_rst != VK_SUBOPTIMAL_KHR)
				return to_status(acquire_rst);
		}

		return {};
	}

	void destroy() noexcept
	{
		for (uint32_t i = 0; i != context.m_swapchain_image_cnt; ++i)
		{
			vkDestroyImageView(context.m_device, hit_index_image_views[i], nullptr);

			vkDestroyImage(context.m_device, hit_index_images[i], nullptr);

			vkDestroyImageView(context.m_device, hit_times_image_views[i], nullptr);

			vkDestroyImage(context.m_device, hit_times_images[i], nullptr);
		}

		vkFreeMemory(context.m_device, hit_index_memory, nullptr);

		vkFreeMemory(context.m_device, hit_times_memory, nullptr);



		vkDestroyDescriptorPool(context.m_device, descriptor_pool, nullptr);

		vkDestroyCommandPool(context.m_device, command_pool, nullptr);



		for (uint32_t i = 0; i != MAX_FRAMES_INFLIGHT; ++i)
		{
			vkDestroySemaphore(context.m_device, image_available_semaphores[i], nullptr);

			vkDestroySemaphore(context.m_device, render_complete_semaphores[i], nullptr);

			vkDestroyFence(context.m_device, frame_inflight_fences[i], nullptr);
		}



		vkDestroyPipeline(context.m_device, pipeline, nullptr);

		vkDestroyDescriptorSetLayout(context.m_device, descriptor_set_layout, nullptr);

		vkDestroyPipelineLayout(context.m_device, pipeline_layout, nullptr);

		vkDestroyShaderModule(context.m_device, trace_shader_module, nullptr);



		vkDestroyImageView(context.m_device, base_image_view, nullptr);

		vkDestroyImage(context.m_device, base_image_array, nullptr);

		vkFreeMemory(context.m_device, base_memory, nullptr);

		vkDestroyBuffer(context.m_device, brick_buffer, nullptr);

		vkFreeMemory(context.m_device, brick_memory, nullptr);

		vkDestroyBuffer(context.m_device, leaf_buffer, nullptr);

		vkFreeMemory(context.m_device, leaf_memory, nullptr);

		context.destroy();
	}
};

och::status run_voxel_volume(int argc, const char** argv) noexcept
{
	voxel_volume program{};

	och::status err = program.create(argc, argv);

	if (!err)
		err = program.run();

	program.destroy();

	check(err);

	return {};
}
