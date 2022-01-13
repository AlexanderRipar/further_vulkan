#include "help.h"

#include <Windows.h>

#include "och_fmt.h"

och::status run_help() noexcept
{
	char program_name[MAX_PATH];

	if (int32_t chars = static_cast<int32_t>(GetModuleFileNameA(nullptr, program_name, sizeof(program_name))); chars == sizeof(program_name) || chars == 0)
	{
		program_name[0] = '?';
		program_name[1] = '?';
		program_name[2] = '?';
		program_name[3] = '\0';
	}
	else
	{
		for(int32_t i = chars; i != -1; --i)
			if (program_name[i] == '\\')
			{
				for (int32_t j = 0; j != chars - i; ++j)
					program_name[j] = program_name[i + 1 + j];

				break;
			}
	}

	och::print("\nUsage: {} sample [...]\n\n\t-----Samples-----\n\n", program_name);

	och::print("\tvulkan_tutorial [model path] [texture path]\n");
	och::print("\tcompute_buffer_copy\n");
	och::print("\tcompute_colour_to_swapchain\n");
	och::print("\tcompute_simplex_to_swapchain\n");
	och::print("\tsdf_font [ttf file] [cache file] [output image]\n");
	och::print("\tvoxel_volume [brick size] [layer size] [layer count]\n\n");

	return {};
}
