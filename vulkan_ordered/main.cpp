#include "och_fmt.h"

#include "och_err.h"


#include "help.h"
#include "vulkan_tutorial.h"
#include "compute_buffer_copy.h"
#include "compute_to_swapchain.h"
#include "sdf_font.h"
#include "voxel_volume.h"
#include "gpu_info.h"

#include <Windows.h>


enum class sample_type
{
	none,
	vulkan_tutorial,
	compute_buffer_copy,
	compute_colour_to_swapchain,
	compute_simplex_to_swapchain,
	sdf_font,
	voxel_volume,
	gpu_info,
};

const char* sample_names[]
{
	"",
	"vulkan_tutorial",
	"compute_buffer_copy",
	"compute_colour_to_swapchain",
	"compute_simplex_to_swapchain",
	"sdf_font",
	"voxel_volume",
	"gpu_info",
};

int main(int argc, const char** argv)
{
	char curr_dir[1024];

	GetCurrentDirectoryA(sizeof(curr_dir), curr_dir);

	och::print("Current Directory: {}\n", curr_dir);

	sample_type to_run = sample_type::none;

	if (argc > 1)
	{
		och::utf8_view run_arg(argv[1]);

		for(uint32_t i = 0; i != sizeof(sample_names) / sizeof(*sample_names); ++i)
			if (run_arg == sample_names[i])
			{
				to_run = static_cast<sample_type>(i);

				break;
			}
	}

	och::status err{};

	switch (to_run)
	{
	case sample_type::none:
		err = run_help();
		break;

	case sample_type::vulkan_tutorial:
		err = run_vulkan_tutorial(argc, argv);
		break;

	case sample_type::compute_buffer_copy:
		err = run_compute_buffer_copy();
		break;

	case sample_type::compute_colour_to_swapchain:
		err = run_compute_to_swapchain(false);
		break;

	case sample_type::compute_simplex_to_swapchain:
		err = run_compute_to_swapchain(true);
		break;

	case sample_type::sdf_font:
		err = run_sdf_font(argc, argv);
		break;

	case sample_type::voxel_volume:
		err = run_voxel_volume(argc, argv);
		break;

	case sample_type::gpu_info:
		err = run_gpu_info(argc, argv);
		break;
	}

	if (err)
	{
		och::print("An Error occurred!\n\n0x{:X} (From \"{}\")\nDescription: {}\n\n", err.errcode(), err.errtype_name(), err.description());
		
		for (auto callstack = och::err::get_callstack(); auto& ctx : callstack)
			och::print("File: {:20}  Function: {:40}  Line {:5>}: \"{}\"\n\n", ctx.filename(), ctx.function(), ctx.line_number(), ctx.line_content());
	}
	else
		och::print("\nProcess terminated successfully\n\n");
}
