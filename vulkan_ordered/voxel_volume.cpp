#include "voxel_volume.h"

#include "vulkan_base.h"

struct voxel_volume
{
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

	och::status create() noexcept
	{
		check(ctx.create("Voxel Volume", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));

		// Create Base Image
		check(ctx.create_image_with_view(base_image_view, base_image, base_image_memory, 
			{ BASE_DIM * LEVEL_CNT, BASE_DIM, BASE_DIM },
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
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
			hit_index_image_views, hit_index_images, hit_index_memory,
			{ ctx.m_swapchain_extent.width, ctx.m_swapchain_extent.height, 1 },
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_USAGE_STORAGE_BIT,
			VK_IMAGE_TYPE_2D,
			VK_IMAGE_VIEW_TYPE_2D,
			VK_FORMAT_R32_SFLOAT,
			VK_FORMAT_R32_SFLOAT,
			VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		return {};
	}

	och::status destroy() noexcept
	{


		return {};
	}

	och::status run() noexcept
	{


		return {};
	}
};

och::status run_voxel_volume(int argc, const char* argv) noexcept
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