#pragma once

#include "och_vulkan_base.h"

#include "och_error_handling.h"
#include "och_timer.h"
#include "och_fmt.h"
#include "och_fio.h"

namespace och
{
	// Just renders a simple Color Gradient to the Swapchain
	struct simple_compute
	{
		vulkan_context context;

		VkPipeline compute_pipeline;

		VkPipelineLayout compute_pipeline_layout;

		VkCommandBuffer compute_command_buffer;

		err_info create()
		{
			check(context.create("Simple Compute", 1440, 810, 1, 1, 0, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT));

			return {};
		}

		void destroy()
		{

		}
	};
}
