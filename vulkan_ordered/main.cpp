#include "och_fmt.h"

#include "och_error_handling.h"



#include "vulkan_tutorial.h"

#include "compute_buffer_copy.h"

#include "compute_to_swapchain.h"

enum class sample_type
{
	vulkan_tutorial,
	compute_buffer_copy,
	compute_to_swapchain,
};

int main()
{
	constexpr sample_type to_run = sample_type::compute_to_swapchain;

	och::err_info err{};

	switch (to_run)
	{
	case sample_type::vulkan_tutorial:
		err = run_vulkan_tutorial();
		break;

	case sample_type::compute_buffer_copy:
		err = run_compute_buffer_copy();
		break;

	case sample_type::compute_to_swapchain:
		err = run_compute_to_swapchain();
		break;
	}


	
	if (err)
	{
		och::print("An Error occurred!\n");
	
		auto stack = och::get_stacktrace();
	
		for (auto& e : stack)
			och::print("Function {} on Line {}: \"{}\"\n\n", e.function, e.line_num, e.call);
	}
	else
		och::print("\nProcess terminated successfully\n");
}
