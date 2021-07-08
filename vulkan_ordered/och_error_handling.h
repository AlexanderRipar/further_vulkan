#pragma once

#include <cstdint>

#include "och_range.h"
#include "och_utf8.h"

#include <vulkan/vulkan.h>

namespace och
{
	enum class error_type : uint32_t
	{
		custom,
		hresult,
		win32,
		vkresult,
	};

	struct error_context
	{
		const char* file;
		const char* function;
		const char* call;
		uint32_t line_num;
	};

	struct err_info
	{
		bool is_bad = false;

		err_info(VkResult rst, const error_context& ctx) noexcept;

		err_info(uint64_t rst, const error_context& ctx) noexcept;

		err_info(err_info rst, const error_context& ctx) noexcept;

		err_info() noexcept = default;

		operator bool() const noexcept;
	};

	och::range<const error_context*> get_stacktrace() noexcept;

	uint64_t get_errcode() noexcept;

	error_type get_errtype() noexcept;

#define check(macro_error_cause) {static constexpr och::error_context macro_defined_ctx{__FILE__, __FUNCTION__, #macro_error_cause, __LINE__}; if(och::err_info macro_result = och::err_info(macro_error_cause, macro_defined_ctx)) return macro_result; }

#define MAKE_ERROR(macro_num) och::err_info(static_cast<uint64_t>(macro_num), och::error_context{__FILE__, __FUNCTION__, #macro_num, __LINE__})

#define MSG_ERROR(macro_msg) och::err_info(1ull, och::error_context(__FILE__, __FUNCTION__, macro_msg, __LINE__));
}
