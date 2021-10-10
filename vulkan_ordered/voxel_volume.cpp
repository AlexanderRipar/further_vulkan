#include "voxel_volume.h"

#include "vulkan_base.h"

struct voxel_volume
{
	vulkan_context context;

	och::status create() noexcept
	{
		check(context.create("Voxel Volume", 1440, 810, 1, 0, 0, VK_IMAGE_USAGE_STORAGE_BIT));
		


		return {};
	}

	och::status run() noexcept
	{


		return {};
	}

	void destroy() noexcept
	{
		context.destroy();
	}
};

och::status run_voxel_volume() noexcept
{
	voxel_volume program{};

	och::status err = program.create();

	if (!err)
		err = program.run();

	program.destroy();

	check(err);

	return {};
}
