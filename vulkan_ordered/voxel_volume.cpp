#include "voxel_volume.h"

#include "vulkan_base.h"

#include "och_fmt.h"

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
	using brick_element_t = uint16_t;

	using layer_element_t = uint32_t;

	static constexpr uint32_t MAX_LAYER_COUNT = 8;

	static constexpr uint32_t MAX_BRICK_BYTES = 1u << 31;

	vulkan_context context;

	VkBuffer layer_buffer;

	VkDeviceMemory layer_memory;

	VkBuffer brick_buffer;

	VkDeviceMemory brick_memory;

	och::status create(int argc, const char** argv) noexcept
	{
		check(context.create("Voxel Volume", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));
		
		uint32_t brick_size = 32;

		uint32_t layer_size = 256;

		uint32_t layer_count = 4;

		uint32_t max_brick_count;

		if (argc >= 3)
			check(parse_numeric_argument(brick_size, argv[2], true, true));

		if (argc >= 4)
			check(parse_numeric_argument(layer_size, argv[3], true, true));

		if (argc >= 5)
			check(parse_numeric_argument(layer_count, argv[4], false, true));

		if (argc >= 6)
		{
			check(parse_numeric_argument(max_brick_count, argv[6], false, true));
		}
		else
		{
			max_brick_count = MAX_BRICK_BYTES / (brick_size * brick_size * brick_size * sizeof(brick_element_t));
		}

		if (brick_size > layer_size)
			return TEMP_STATUS_MACRO; // Brick size cannot be greater than layer size

		if (layer_count > MAX_LAYER_COUNT)
			return TEMP_STATUS_MACRO; // Maximal layer count exceeded

		uint32_t bricks_per_layer = layer_size / brick_size;

		VkDeviceSize layer_bytes = static_cast<VkDeviceSize>(layer_count) * layer_size * layer_size * layer_size * sizeof(layer_element_t);

		VkDeviceSize brick_bytes = static_cast<VkDeviceSize>(max_brick_count) * brick_size * brick_size * brick_size * sizeof(brick_element_t);

		och::print("Layers: {}\nLayer Dimension: {}\nBrick Dimension: {}\nMax Brick count: {}\nBrick Dim per Layer Dim: {}\nMemory for Layers: {} ({} MB)\nMemory for Bricks: {} ({} MB)\n",
			layer_count, layer_size, brick_size, max_brick_count, bricks_per_layer, layer_bytes, layer_bytes / (1024 * 1024), brick_bytes, brick_bytes / (1024 * 1024));

		check(context.create_buffer(layer_buffer, layer_memory, layer_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		check(context.create_buffer(brick_buffer, brick_memory, brick_bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT));

		return {};
	}

	och::status run() noexcept
	{
		

		return {};
	}

	void destroy() noexcept
	{
		vkDestroyBuffer(context.m_device, layer_buffer, nullptr);

		vkFreeMemory(context.m_device, layer_memory, nullptr);

		vkDestroyBuffer(context.m_device, brick_buffer, nullptr);

		vkFreeMemory(context.m_device, brick_memory, nullptr);

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
