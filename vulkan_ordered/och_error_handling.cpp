#include "och_error_handling.h"

namespace och
{
	struct error_dump
	{
		static constexpr uint32_t max_call_stack_depth = 16;

		const error_context* call_stack[max_call_stack_depth];

		uint64_t errcode;

		error_type errtype;

		uint32_t call_stack_depth = 0;

		__declspec(noinline) void start(VkResult rst, const error_context& ctx) noexcept
		{
			call_stack[0] = &ctx;

			call_stack_depth = 1;

			errcode = rst;

			errtype = error_type::vkresult;
		}

		__declspec(noinline) void start(uint64_t rst, const error_context& ctx) noexcept
		{
			call_stack[0] = &ctx;

			call_stack_depth = 1;

			errcode = rst;

			errtype = error_type::custom;
		}

		__declspec(noinline) void add(const error_context& ctx) noexcept
		{
			if (call_stack_depth >= max_call_stack_depth)
			{
				call_stack_depth = max_call_stack_depth + 1;

				return;
			}

			call_stack[call_stack_depth++] = &ctx;
		}

		bool has_overflown() const noexcept
		{
			return call_stack_depth == max_call_stack_depth + 1;
		}
	};

	thread_local error_dump error_data;



	och::range<const error_context*> get_stacktrace() noexcept
	{
		return { error_data.call_stack, error_data.call_stack_depth };
	}

	uint64_t och::get_errcode() noexcept
	{
		return error_data.errcode;
	}

	och::error_type och::get_errtype() noexcept
	{
		return error_data.errtype;
	}

	err_info::err_info(VkResult rst, const error_context& ctx) noexcept
	{
		if (rst != VK_SUCCESS)
		{
			error_data.start(rst, ctx);

			is_bad = true;
		}
	}

	err_info::err_info(uint64_t rst, const error_context& ctx) noexcept
	{
		if (rst)
		{
			error_data.start(rst, ctx);

			is_bad = true;
		}
	}

	err_info::err_info(err_info rst, const error_context& ctx) noexcept
	{
		if (rst)
		{
			error_data.add(ctx);

			is_bad = true;
		}
	}

	err_info::operator bool() const noexcept { return is_bad; }
}
