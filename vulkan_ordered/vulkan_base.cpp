#include "vulkan_base.h"

#include "heap_buffer.h"
#include "och_fmt.h"
#include "och_fio.h"

#define NOMINMAX
#include <Windows.h>

#include "och_err.h"

#include <vulkan/vulkan_win32.h>



size_t find_aligned_size(size_t elem_size, size_t alignment) noexcept
{
	const size_t underalignment = elem_size % alignment;

	return underalignment == 0 ? elem_size : elem_size - underalignment + alignment;
}



VKAPI_ATTR VkBool32 VKAPI_CALL vulkan_debug_callback(VkDebugUtilsMessageSeverityFlagBitsEXT severity, VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data)
{
	user_data; type; severity;

	if (int32_t msg_id = callback_data->messageIdNumber; 
		msg_id == 0x0000'0000 ||	// Loader Message (loaderAddLayerProperties invalid layer manifest file version)
		msg_id == 0x7CD0'911D)		// vkCreateSwapchainKHR wrong image extent (not fixable due to race condition)
		return VK_FALSE;

	och::print("{:X}\n{}\n\n", callback_data->messageIdNumber, callback_data->pMessage);

	return VK_FALSE;
}

int64_t vulkan_context_window_fn(HWND hwnd, uint32_t msg, uint64_t wparam, int64_t lparam)
{
	vulkan_context* ctx = reinterpret_cast<vulkan_context*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));

	switch (msg)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		ctx->set_keycode(static_cast<och::vk>(wparam));
		return 0;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		ctx->unset_keycode(static_cast<och::vk>(wparam));
		return 0;

	case WM_LBUTTONDOWN:
	case WM_RBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_XBUTTONDOWN:
		SetCapture(hwnd);
		ctx->set_keycode(static_cast<och::vk>(msg - (msg >> 1) - (msg == WM_XBUTTONDOWN && (wparam & (1 << 16))))); //Figure out key from low four bits of message
		break;

	case WM_LBUTTONUP:
	case WM_RBUTTONUP:
	case WM_MBUTTONUP:
	case WM_XBUTTONUP:
		ReleaseCapture();
		ctx->unset_keycode(static_cast<och::vk>((msg >> 1) - (msg == WM_XBUTTONUP && (wparam & (1 << 16))))); //Figure out key from low four bits of message
		break;

	case WM_MOUSEWHEEL:
		ctx->m_mouse_vscroll += GET_WHEEL_DELTA_WPARAM(wparam);
		return 0;

	case WM_MOUSEHWHEEL:
		ctx->m_mouse_hscroll += GET_WHEEL_DELTA_WPARAM(wparam);
		return 0;

	case WM_MOUSEMOVE:
		ctx->set_mouse_pos(LOWORD(lparam), HIWORD(lparam));

		{
			wchar_t buf[64]{};

			uint32_t idx = 62;

			int16_t y = HIWORD(lparam);

			bool neg_y = y < 0;

			if (neg_y)
				y = -y;


			while (y >= 10)
			{
				buf[idx--] = L'0' + (y % 10);
				y /= 10;
			}

			buf[idx--] = static_cast<wchar_t>(L'0' + y);

			if (neg_y)
				buf[idx--] = L'-';

			buf[idx--] = L' ';
			buf[idx--] = L':';
			buf[idx--] = L'Y';
			buf[idx--] = L' ';
			buf[idx--] = L',';

			int16_t x = LOWORD(lparam);

			bool neg_x = x < 0;

			if (neg_x)
				x = -x;

			while (x >= 10)
			{
				buf[idx--] = L'0' + (x % 10);
				x /= 10;
			}

			buf[idx--] = static_cast<wchar_t>(L'0' + x);

			if (neg_x)
				buf[idx--] = L'-';

			buf[idx--] = L' ';
			buf[idx--] = L':';
			buf[idx--] = L'X';

			SetWindowTextW(static_cast<HWND>(ctx->m_hwnd), buf + idx + 1);
		}

		return 0;

	case WM_KILLFOCUS:
		ctx->reset_pressed_keys();
		break;

	case WM_CHAR:
		ctx->enqueue_input_char(static_cast<uint32_t>(wparam));
		return 0;

	case WM_SIZE:
		ctx->m_flags.framebuffer_resized.store(true, std::memory_order::release);
		return 0;

	case WM_CLOSE:
	case WM_QUIT:
		ctx->m_flags.is_window_closed.store(true, std::memory_order::release);
		return 0;

	default:
		break;
	}

	return DefWindowProcW(hwnd, msg, wparam, lparam);
}



DWORD message_pump_thread_fn(void* data)
{
	vulkan_context* ctx = static_cast<vulkan_context*>(data);



	// Create window
	{
		constexpr uint32_t MAX_WINDOW_NAME_CUNITS = 512;

		wchar_t app_name_wide_buffer[MAX_WINDOW_NAME_CUNITS];

		const wchar_t* app_name_wide = app_name_wide_buffer;

		if (!MultiByteToWideChar(65001, 0, ctx->m_app_name, -1, app_name_wide_buffer, static_cast<int>(MAX_WINDOW_NAME_CUNITS)))
			if (int mb_to_wc_errorcode = GetLastError(); mb_to_wc_errorcode == ERROR_INSUFFICIENT_BUFFER)
				app_name_wide = L"???";
			else
				return 0x101;

		HINSTANCE instance = GetModuleHandleW(nullptr);

		if (!instance)
			return 0x100;

		WNDCLASSEXW window_class{};
		window_class.cbSize = sizeof(WNDCLASSEXW);
		window_class.style = 0;
		window_class.lpfnWndProc = vulkan_context_window_fn;
		window_class.cbClsExtra = 0;
		window_class.cbWndExtra = 0;
		window_class.hInstance = instance;
		window_class.hIcon = nullptr;
		window_class.hCursor = nullptr;
		window_class.hbrBackground = nullptr;
		window_class.lpszMenuName = nullptr;
		window_class.lpszClassName = vulkan_context::WINDOW_CLASS_NAME;
		window_class.hIconSm = nullptr;

		if (!RegisterClassExW(&window_class))
			return 0x102;

		const uint32_t window_style = WS_OVERLAPPEDWINDOW;
		const uint32_t window_ex_style = 0;

		RECT rect;
		rect.left = 0;
		rect.right = ctx->m_swapchain_extent.width;
		rect.top = 0;
		rect.bottom = ctx->m_swapchain_extent.height;

		if (!AdjustWindowRectEx(&rect, window_style, 0, window_ex_style))
			return 0x103;

		HWND hwnd = CreateWindowExW(window_ex_style, vulkan_context::WINDOW_CLASS_NAME, app_name_wide, window_style, CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, nullptr);

		if (!hwnd)
			return 0x104;
		else
			ctx->m_hwnd = static_cast<void*>(hwnd);

		SetLastError(0);

		SetWindowLongPtrW(static_cast<HWND>(hwnd), GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));

		if (GetLastError())
			return 0x105;

		if (!SetEvent(ctx->m_message_pump_initialization_wait_event))
			return 0x106;
	}

	// Wait for continuation signal from main thread
	
	if (WaitForSingleObject(ctx->m_message_pump_start_wait_event, INFINITE))
		return 0x107;

	ShowWindow(static_cast<HWND>(ctx->m_hwnd), SW_NORMAL);

	// Start pumping messages

	MSG msg;

	BOOL result;

	for (result = GetMessageW(&msg, static_cast<HWND>(ctx->m_hwnd), 0, 0); result != 0 && result != -1; result = GetMessageW(&msg, static_cast<HWND>(ctx->m_hwnd), 0, 0))
	{
		if (msg.message == vulkan_context::MESSAGE_PUMP_THREAD_TERMINATION_MESSAGE)
			return 1;

		TranslateMessage(&msg);

		DispatchMessageW(&msg);
	}

	return result;
}



och::status vulkan_context::create(const char* app_name, uint32_t window_width, uint32_t window_height, uint32_t requested_general_queues, uint32_t requested_compute_queues, uint32_t requested_transfer_queues, VkImageUsageFlags swapchain_image_usage, const VkPhysicalDeviceFeatures* enabled_device_features, bool allow_compute_graphics_merge) noexcept
{
	if (requested_general_queues > queue_family_info::MAX_QUEUE_CNT || requested_compute_queues > queue_family_info::MAX_QUEUE_CNT || requested_transfer_queues > queue_family_info::MAX_QUEUE_CNT)
		return to_status(VK_ERROR_TOO_MANY_OBJECTS);

	m_app_name = app_name;

	// Create message pump thread
	{
		// Create an auto-reset event for the thread to indicate it has completed its window creation
		if (HANDLE wait_event = CreateEventW(nullptr, FALSE, FALSE, nullptr); !wait_event)
			return to_status(HRESULT_FROM_WIN32(GetLastError()));
		else
			m_message_pump_initialization_wait_event = wait_event;

		// Create an auto-reset event to signal the message pump thread once it should start pumping messages
		if (HANDLE continue_event = CreateEventW(nullptr, FALSE, FALSE, nullptr); !continue_event)
			return to_status(HRESULT_FROM_WIN32(GetLastError()));
		else
			m_message_pump_start_wait_event = continue_event;

		// Set requested window size in _init_... union member to let the creating thread know what to do

		m_swapchain_extent = { window_width, window_height };


		// Now create the thread

		DWORD thread_id;

		HANDLE thread_handle = CreateThread(nullptr, 4096 * 8, message_pump_thread_fn, static_cast<void*>(this), 0, &thread_id);

		if (!thread_handle)
		{
			m_message_pump_thread_handle = nullptr;

			m_message_pump_thread_id = 0;

			return to_status(HRESULT_FROM_WIN32(GetLastError()));
		}

		m_message_pump_thread_handle = thread_handle;

		m_message_pump_thread_id = thread_id;

		if (DWORD wait_result = WaitForSingleObject(m_message_pump_initialization_wait_event, MAX_MESSAGE_PUMP_INITIALIZATION_TIME_MS))
		{
			DWORD exit_code;

			BOOL exit_code_result = GetExitCodeThread(thread_handle, &exit_code);

			if (!exit_code_result)
				if (DWORD last_error = GetLastError(); last_error == STILL_ACTIVE)
					och::print("Message Pump Thread still active\n");
				else
					och::print("Failed to get Message Pump Thread exit code\n");
			else
				och::print("Message Pump Thread exited with 0x{:X}\n", static_cast<uint32_t>(exit_code));

			return to_status(HRESULT_FROM_WIN32(wait_result));
		}
	}

	// Fill debug messenger creation info
	VkDebugUtilsMessengerCreateInfoEXT messenger_ci{};
	messenger_ci.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	messenger_ci.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	messenger_ci.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
	messenger_ci.pfnUserCallback = vulkan_debug_callback;

	// Create instance
	{
		VkApplicationInfo app_info{};
		app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		app_info.pApplicationName = app_name;
		app_info.applicationVersion = VK_MAKE_VERSION(0, 0, 1);
		app_info.pEngineName = "No Engine";
		app_info.engineVersion = VK_MAKE_VERSION(0, 0, 0);
		app_info.apiVersion = VK_API_VERSION_1_0;

		bool supports_extensions;
		check(s_feats.check_instance_support(supports_extensions));

		if (!supports_extensions)
			return to_status(VK_ERROR_EXTENSION_NOT_PRESENT);

		VkInstanceCreateInfo instance_ci{};
		instance_ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		instance_ci.pNext = &messenger_ci;
		instance_ci.enabledLayerCount = s_feats.inst_layer_cnt();
		instance_ci.ppEnabledLayerNames = s_feats.inst_layers();
		instance_ci.pApplicationInfo = &app_info;
		instance_ci.enabledExtensionCount = s_feats.inst_extension_cnt();
		instance_ci.ppEnabledExtensionNames = s_feats.inst_extensions();

		check(vkCreateInstance(&instance_ci, nullptr, &m_instance));
	}

	// Create debug messenger
	{
#ifdef OCH_VALIDATE

		auto create_fn = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkCreateDebugUtilsMessengerEXT"));

		if (create_fn)
		{
			check(create_fn(m_instance, &messenger_ci, nullptr, &m_debug_messenger));
		}
		else
			return to_status(VK_ERROR_UNKNOWN);

#endif // OCH_VALIDATE
	}

	// Create surface
	{
		VkWin32SurfaceCreateInfoKHR surface_ci{};
		surface_ci.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		surface_ci.pNext = nullptr;
		surface_ci.flags = 0;
		surface_ci.hinstance = GetModuleHandleW(nullptr);
		surface_ci.hwnd = static_cast<HWND>(m_hwnd);
		
		check(vkCreateWin32SurfaceKHR(m_instance, &surface_ci, nullptr, &m_surface));
	}
	
	// Surface Capabilites for use throughout the create() Function
	VkSurfaceCapabilitiesKHR surface_capabilites;

	// Select physical device and save queue family info
	{
		uint32_t avl_dev_cnt;
		check(vkEnumeratePhysicalDevices(m_instance, &avl_dev_cnt, nullptr));
		heap_buffer<VkPhysicalDevice> avl_devs(avl_dev_cnt);
		check(vkEnumeratePhysicalDevices(m_instance, &avl_dev_cnt, avl_devs.data()));

		for (uint32_t i = 0; i != avl_dev_cnt; ++i)
		{
			VkPhysicalDevice dev = avl_devs[i];

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(dev, &properties);

			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(dev, &features);

			if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
				continue;

			// Check support for required extensions and layers

			bool supports_extensions;
			check(s_feats.check_device_support(dev, supports_extensions));

			if (!supports_extensions)
				continue;

			// Check queue families

			uint32_t queue_family_cnt;
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_cnt, nullptr);
			heap_buffer<VkQueueFamilyProperties> family_properties(queue_family_cnt);
			vkGetPhysicalDeviceQueueFamilyProperties(dev, &queue_family_cnt, family_properties.data());

			uint32_t general_queue_index = VK_QUEUE_FAMILY_IGNORED;
			uint32_t compute_queue_index = VK_QUEUE_FAMILY_IGNORED;
			uint32_t transfer_queue_index = VK_QUEUE_FAMILY_IGNORED;

			for (uint32_t f = 0; f != queue_family_cnt; ++f)
			{
				VkBool32 supports_present;
				check(vkGetPhysicalDeviceSurfaceSupportKHR(dev, f, m_surface, &supports_present));

				VkQueueFlags flags = family_properties[f].queueFlags;

				if (uint32_t general_mask = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT; (flags & general_mask) == general_mask && supports_present)
					general_queue_index = f;
				else if (uint32_t compute_mask = VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT; (flags & compute_mask) == compute_mask)
					compute_queue_index = f;
				else if ((flags & VK_QUEUE_TRANSFER_BIT) && !(flags & (VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT)))
					transfer_queue_index = f;
			}

			if (compute_queue_index == VK_QUEUE_FAMILY_IGNORED)
			{
				const uint32_t supported_queue_cnt = queue_family_info::MAX_QUEUE_CNT < family_properties[general_queue_index].queueCount ? queue_family_info::MAX_QUEUE_CNT : family_properties[general_queue_index].queueCount;

				if (allow_compute_graphics_merge && general_queue_index != VK_QUEUE_FAMILY_IGNORED && requested_general_queues + requested_compute_queues <= supported_queue_cnt)
				{
					compute_queue_index = general_queue_index;

					m_flags.separate_compute_and_general_queue = false;
				}
				else
					continue;
			}
			else
				m_flags.separate_compute_and_general_queue = true;

			if (((general_queue_index == VK_QUEUE_FAMILY_IGNORED && requested_general_queues) || (requested_general_queues && family_properties[general_queue_index].queueCount < requested_general_queues)) ||
				((compute_queue_index == VK_QUEUE_FAMILY_IGNORED && requested_compute_queues) || (requested_compute_queues && family_properties[compute_queue_index].queueCount < requested_compute_queues)) ||
				((transfer_queue_index == VK_QUEUE_FAMILY_IGNORED && requested_transfer_queues) || (requested_transfer_queues && family_properties[transfer_queue_index].queueCount < requested_transfer_queues)))
				continue;

			// Check support for window surface

			uint32_t surface_format_cnt;
			check(vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &surface_format_cnt, nullptr));

			uint32_t present_mode_cnt;
			check(vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surface, &present_mode_cnt, nullptr));

			if (!surface_format_cnt || !present_mode_cnt)
				continue;

			// Check if requested Image Usage is available for the Swapchain

			check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, m_surface, &surface_capabilites));

			if ((surface_capabilites.supportedUsageFlags & swapchain_image_usage) != swapchain_image_usage)
				continue;

			// Find a Format that supports the requested Image Usage, preferably VK_FORMAT_B8G8R8A8_SRGB

			heap_buffer<VkSurfaceFormatKHR> surface_formats(surface_format_cnt);
			check(vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surface, &surface_format_cnt, surface_formats.data()));

			bool format_found = false;

			for (uint32_t j = 0; j != surface_format_cnt; ++j)
			{
				VkImageFormatProperties format_props;

				if (VK_ERROR_FORMAT_NOT_SUPPORTED == vkGetPhysicalDeviceImageFormatProperties(dev, surface_formats[j].format, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, swapchain_image_usage, 0, &format_props))
					continue;
				
				if (!format_found || (surface_formats[j].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[j].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR))
				{
					format_found = true;

					m_swapchain_format = surface_formats[j].format;
					m_swapchain_colorspace = surface_formats[j].colorSpace;

					if (surface_formats[j].format == VK_FORMAT_B8G8R8A8_SRGB && surface_formats[j].colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR)
						break;
				}
			}

			if (!format_found)
				continue;

			// suitable Device found; Initialize member Variables

			if (requested_general_queues)
			{
				m_general_queues.family_index = general_queue_index;
				m_general_queues.cnt = requested_general_queues;
			}

			if (requested_compute_queues)
			{
				m_compute_queues.family_index = compute_queue_index;
				m_compute_queues.cnt = requested_compute_queues;
			}

			if (requested_transfer_queues)
			{
				m_transfer_queues.family_index = transfer_queue_index;
				m_transfer_queues.cnt = requested_transfer_queues;

				m_min_image_transfer_granularity = family_properties[transfer_queue_index].minImageTransferGranularity;
			}

			m_physical_device = dev;

			goto DEVICE_SELECTED;
		}

		return to_status(VK_ERROR_UNKNOWN);

	DEVICE_SELECTED:;
	}

	// Create logical device
	{
		float general_queue_priorities[queue_family_info::MAX_QUEUE_CNT];
		for (uint32_t i = 0; i != queue_family_info::MAX_QUEUE_CNT; ++i)
			general_queue_priorities[i] = 1.0F;

		float compute_queue_priorities[queue_family_info::MAX_QUEUE_CNT];

		for (uint32_t i = 0; i != queue_family_info::MAX_QUEUE_CNT; ++i)
			compute_queue_priorities[i] = 1.0F;

		float transfer_queue_priorities[queue_family_info::MAX_QUEUE_CNT];

		for (uint32_t i = 0; i != queue_family_info::MAX_QUEUE_CNT; ++i)
			transfer_queue_priorities[i] = 1.0F;

		VkDeviceQueueCreateInfo queue_cis[3]{};

		uint32_t ci_idx = 0;

		if (requested_general_queues)
		{
			queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_cis[ci_idx].queueFamilyIndex = m_general_queues.family_index;
			queue_cis[ci_idx].queueCount = requested_general_queues + (m_flags.separate_compute_and_general_queue ? 0 : requested_compute_queues);

			queue_cis[ci_idx].pQueuePriorities = general_queue_priorities;

			++ci_idx;
		}

		if (requested_compute_queues && m_flags.separate_compute_and_general_queue)
		{
			queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_cis[ci_idx].queueFamilyIndex = m_compute_queues.family_index;
			queue_cis[ci_idx].queueCount = requested_compute_queues;
			queue_cis[ci_idx].pQueuePriorities = compute_queue_priorities;

			++ci_idx;
		}

		if (requested_transfer_queues)
		{
			queue_cis[ci_idx].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
			queue_cis[ci_idx].queueFamilyIndex = m_transfer_queues.family_index;
			queue_cis[ci_idx].queueCount = requested_transfer_queues;
			queue_cis[ci_idx].pQueuePriorities = transfer_queue_priorities;

			++ci_idx;
		}

		VkPhysicalDeviceFeatures default_enabled_device_features{};

		VkDeviceCreateInfo device_ci{};
		device_ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
		device_ci.queueCreateInfoCount = ci_idx;
		device_ci.pQueueCreateInfos = queue_cis;
		device_ci.enabledLayerCount = s_feats.dev_layer_cnt();
		device_ci.ppEnabledLayerNames = s_feats.dev_layers();
		device_ci.enabledExtensionCount = s_feats.dev_extension_cnt();
		device_ci.ppEnabledExtensionNames = s_feats.dev_extensions();
		device_ci.pEnabledFeatures = enabled_device_features ? enabled_device_features : &default_enabled_device_features;

		check(vkCreateDevice(m_physical_device, &device_ci, nullptr, &m_device));

		for (uint32_t i = 0; i != requested_general_queues; ++i)
			vkGetDeviceQueue(m_device, m_general_queues.family_index, i, m_general_queues.queues + i);

		if (m_flags.separate_compute_and_general_queue)
			for (uint32_t i = 0; i != requested_compute_queues; ++i)
				vkGetDeviceQueue(m_device, m_compute_queues.family_index, i, m_compute_queues.queues + i);
		else
		{
			for (uint32_t i = 0; i != requested_compute_queues; ++i)
				vkGetDeviceQueue(m_device, m_compute_queues.family_index, requested_general_queues + i, m_compute_queues.queues + i);

			m_compute_queues.offset = requested_general_queues;
		}


		for (uint32_t i = 0; i != requested_transfer_queues; ++i)
			vkGetDeviceQueue(m_device, m_transfer_queues.family_index, i, m_transfer_queues.queues + i);
	}

	// Get supported swapchain settings
	{
		uint32_t present_mode_cnt;
		check(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_cnt, nullptr));
		heap_buffer<VkPresentModeKHR> present_modes(present_mode_cnt);
		check(vkGetPhysicalDeviceSurfacePresentModesKHR(m_physical_device, m_surface, &present_mode_cnt, present_modes.data()));

		m_swapchain_present_mode = VK_PRESENT_MODE_FIFO_KHR;
		for (uint32_t i = 0; i != present_mode_cnt; ++i)
			if (present_modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) // TODO: VK_PRESENT_MODE_IMMEDIATE_KHR
			{
				m_swapchain_present_mode = VK_PRESENT_MODE_MAILBOX_KHR;

				break;
			}
	}

	// Create swapchain
	{
		VkExtent2D surface_extent;

		if (surface_capabilites.currentExtent.width == ~0u)
		{
			RECT surface_rect;

			GetClientRect(static_cast<HWND>(m_hwnd), &surface_rect);

			surface_extent = { static_cast<uint32_t>(surface_rect.right - surface_rect.left), static_cast<uint32_t>(surface_rect.bottom - surface_rect.top) };
		}
		else
			surface_extent = surface_capabilites.currentExtent;

		if (!surface_extent.width || !surface_extent.height)
			surface_extent.width = surface_extent.height = 1;

		m_swapchain_extent = surface_extent;

		// Choose image count
		uint32_t requested_img_cnt = surface_capabilites.minImageCount + 1;

		if (surface_capabilites.minImageCount == MAX_SWAPCHAIN_IMAGE_CNT || surface_capabilites.maxImageCount == surface_capabilites.minImageCount)
			--requested_img_cnt;
		else if (surface_capabilites.minImageCount > MAX_SWAPCHAIN_IMAGE_CNT)
			return to_status(VK_ERROR_TOO_MANY_OBJECTS);

		VkSwapchainCreateInfoKHR swapchain_ci{};
		swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
		swapchain_ci.pNext = nullptr;
		swapchain_ci.flags = 0;
		swapchain_ci.surface = m_surface;
		swapchain_ci.minImageCount = requested_img_cnt;
		swapchain_ci.imageFormat = m_swapchain_format;
		swapchain_ci.imageColorSpace = m_swapchain_colorspace;
		swapchain_ci.imageExtent = { window_width, window_height };
		swapchain_ci.imageArrayLayers = 1;
		swapchain_ci.imageUsage = swapchain_image_usage;
		swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
		swapchain_ci.queueFamilyIndexCount = 0;
		swapchain_ci.pQueueFamilyIndices = nullptr;
		swapchain_ci.preTransform = surface_capabilites.currentTransform;
		swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
		swapchain_ci.presentMode = m_swapchain_present_mode;
		swapchain_ci.clipped = VK_TRUE; // TODO: VK_FALSE?
		swapchain_ci.oldSwapchain = nullptr;

		check(vkCreateSwapchainKHR(m_device, &swapchain_ci, nullptr, &m_swapchain));
	}

	// Get swapchain images (and the number thereof)
	{
		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, nullptr));

		if (m_swapchain_image_cnt > MAX_SWAPCHAIN_IMAGE_CNT)
			return to_status(VK_ERROR_TOO_MANY_OBJECTS);

		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, m_swapchain_images));
	}

	// Create swapchain image views
	{
		for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		{
			VkImageViewCreateInfo imview_ci{};
			imview_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imview_ci.pNext = nullptr;
			imview_ci.flags = 0;
			imview_ci.image = m_swapchain_images[i];
			imview_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imview_ci.format = m_swapchain_format;
			imview_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
			imview_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imview_ci.subresourceRange.baseMipLevel = 0;
			imview_ci.subresourceRange.levelCount = 1;
			imview_ci.subresourceRange.baseArrayLayer = 0;
			imview_ci.subresourceRange.layerCount = 1;

			check(vkCreateImageView(m_device, &imview_ci, nullptr, &m_swapchain_image_views[i]));
		}
	}

	// Set allowed swapchain usages
	{
		m_image_swapchain_usage = swapchain_image_usage;
	}

	// Get memory heap- and type-indices for device- and staging-memory
	{
		vkGetPhysicalDeviceMemoryProperties(m_physical_device, &m_memory_properties);
	}

	return {};
}

void vulkan_context::destroy() const noexcept
{
	for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		if(m_swapchain_images[i])
			vkDestroyImageView(m_device, m_swapchain_image_views[i], nullptr);

	if(m_swapchain)
		vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

	if(m_device)
		vkDestroyDevice(m_device, nullptr);

	if(m_surface)
		vkDestroySurfaceKHR(m_instance, m_surface, nullptr);

#ifdef OCH_VALIDATE

	// If m_instance is null at this point, this cleanup must be the result of a failed initialization, 
	// and m_debug_messenger cannot have been created yet either. Hence, we skip this to avoid loader issues.
	if (m_instance && m_debug_messenger)
	{
		auto destroy_fn = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));

		if (destroy_fn)
			destroy_fn(m_instance, m_debug_messenger, nullptr);
		else if (m_instance)
			och::print("\nERROR DURING CLEANUP: Could not load vkDestroyDebugUtilsMessengerEXT\n");
	}

#endif // OCH_VALIDATE

	if(m_instance)
		vkDestroyInstance(m_instance, nullptr);

	DestroyWindow(static_cast<HWND>(m_hwnd));

	UnregisterClassW(WINDOW_CLASS_NAME, GetModuleHandleW(nullptr));
}

och::status vulkan_context::recreate_swapchain() noexcept
{
	check(vkDeviceWaitIdle(m_device));

	// Get surface capabilities for current pre-transform

	VkSurfaceCapabilitiesKHR surface_capabilities;
	check(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physical_device, m_surface, &surface_capabilities));

	if (!surface_capabilities.currentExtent.width || !surface_capabilities.currentExtent.height)
		return {};

	m_swapchain_extent = surface_capabilities.currentExtent;

	// Destroy old swapchain's image views

	for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		vkDestroyImageView(m_device, m_swapchain_image_views[i], nullptr);

	// Temporary swapchain handle
	VkSwapchainKHR tmp_swapchain;

	// Actually recreate swapchain

	VkSwapchainCreateInfoKHR swapchain_ci{};
	swapchain_ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchain_ci.pNext = nullptr;
	swapchain_ci.flags = 0;
	swapchain_ci.surface = m_surface;
	swapchain_ci.minImageCount = m_swapchain_image_cnt;
	swapchain_ci.imageFormat = m_swapchain_format;
	swapchain_ci.imageColorSpace = m_swapchain_colorspace;
	swapchain_ci.imageExtent = surface_capabilities.currentExtent;
	swapchain_ci.imageArrayLayers = 1;
	swapchain_ci.imageUsage = m_image_swapchain_usage;
	swapchain_ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchain_ci.queueFamilyIndexCount = 0;
	swapchain_ci.pQueueFamilyIndices = nullptr;
	swapchain_ci.preTransform = surface_capabilities.currentTransform;
	swapchain_ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchain_ci.presentMode = m_swapchain_present_mode;
	swapchain_ci.clipped = VK_TRUE;
	swapchain_ci.oldSwapchain = m_swapchain;

	check(vkCreateSwapchainKHR(m_device, &swapchain_ci, nullptr, &tmp_swapchain));

	// Now that the new swapchain has been created, the old one can be destroyed and the new one assigned to the struct member

	vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);

	m_swapchain = tmp_swapchain;

	// Get swapchain images (and the number thereof) for new swapchain
	{
		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, nullptr));

		if (m_swapchain_image_cnt > MAX_SWAPCHAIN_IMAGE_CNT)
			return to_status(VK_ERROR_TOO_MANY_OBJECTS);

		check(vkGetSwapchainImagesKHR(m_device, m_swapchain, &m_swapchain_image_cnt, m_swapchain_images));
	}

	// Create swapchain image views for new swapchain
	{
		for (uint32_t i = 0; i != m_swapchain_image_cnt; ++i)
		{
			VkImageViewCreateInfo imview_ci{};
			imview_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
			imview_ci.pNext = nullptr;
			imview_ci.flags = 0;
			imview_ci.image = m_swapchain_images[i];
			imview_ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
			imview_ci.format = m_swapchain_format;
			imview_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
			imview_ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			imview_ci.subresourceRange.baseMipLevel = 0;
			imview_ci.subresourceRange.levelCount = 1;
			imview_ci.subresourceRange.baseArrayLayer = 0;
			imview_ci.subresourceRange.layerCount = 1;

			check(vkCreateImageView(m_device, &imview_ci, nullptr, &m_swapchain_image_views[i]));
		}
	}

	return {};
}

och::status vulkan_context::suitable_memory_type_idx(uint32_t& out_memory_type_idx, uint32_t memory_type_mask, VkMemoryPropertyFlags property_flags) const noexcept
{
	for (uint32_t i = 0; i != m_memory_properties.memoryTypeCount; ++i)
		if ((memory_type_mask & (1 << i)) && (m_memory_properties.memoryTypes[i].propertyFlags & property_flags) == property_flags)
		{
			out_memory_type_idx = i;

			return {};
		}

	out_memory_type_idx = 0;

	return to_status(VK_ERROR_FORMAT_NOT_SUPPORTED);
}

och::status vulkan_context::load_shader_module_file(VkShaderModule& out_shader_module, const char* filename) const noexcept
{
	och::mapped_file<uint32_t> shader_file;
	
	check(shader_file.create(filename, och::fio::access::read, och::fio::open::normal, och::fio::open::fail));

	VkShaderModuleCreateInfo module_ci{};
	module_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	module_ci.pNext = nullptr;
	module_ci.flags = 0;
	module_ci.codeSize = shader_file.bytes();
	module_ci.pCode = shader_file.data();

	check(vkCreateShaderModule(m_device, &module_ci, nullptr, &out_shader_module));

	shader_file.close();

	return {};
}

och::status vulkan_context::begin_onetime_command(VkCommandBuffer& out_command_buffer, VkCommandPool command_pool) const noexcept
{
	VkCommandBufferAllocateInfo alloc_info{};
	alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	alloc_info.commandBufferCount = 1;
	alloc_info.commandPool = command_pool;

	check(vkAllocateCommandBuffers(m_device, &alloc_info, &out_command_buffer));

	VkCommandBufferBeginInfo beg_info{};
	beg_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	beg_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

	check(vkBeginCommandBuffer(out_command_buffer, &beg_info));

	return {};
}

och::status vulkan_context::submit_onetime_command(VkCommandBuffer command_buffer, VkCommandPool command_pool, VkQueue submit_queue, bool wait_and_free) const noexcept
{
	check(vkEndCommandBuffer(command_buffer));

	VkSubmitInfo submit_info{};
	submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submit_info.commandBufferCount = 1;
	submit_info.pCommandBuffers = &command_buffer;

	check(vkQueueSubmit(submit_queue, 1, &submit_info, nullptr));

	if (wait_and_free)
	{
		check(vkDeviceWaitIdle(m_device));

		vkFreeCommandBuffers(m_device, command_pool, 1, &command_buffer);
	}

	return {};
}

och::status vulkan_context::create_buffer(VkBuffer& out_buffer, VkDeviceMemory& out_memory, VkDeviceSize bytes, VkBufferUsageFlags buffer_usage, VkMemoryPropertyFlags memory_properties, VkSharingMode sharing_mode, uint32_t queue_family_idx_cnt, const uint32_t* queue_family_indices) const noexcept
{
	VkBufferCreateInfo buffer_ci{};
	buffer_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	buffer_ci.pNext = nullptr;
	buffer_ci.flags = 0;
	buffer_ci.size = bytes;
	buffer_ci.usage = buffer_usage;
	buffer_ci.sharingMode = sharing_mode;
	buffer_ci.queueFamilyIndexCount = queue_family_idx_cnt;
	buffer_ci.pQueueFamilyIndices = queue_family_indices;

	check(vkCreateBuffer(m_device, &buffer_ci, nullptr, &out_buffer));

	VkMemoryRequirements mem_reqs{};
	vkGetBufferMemoryRequirements(m_device, out_buffer, &mem_reqs);

	uint32_t buffer_memory_type_idx;
	check(suitable_memory_type_idx(buffer_memory_type_idx, mem_reqs.memoryTypeBits, memory_properties));

	VkMemoryAllocateInfo memory_ai{};
	memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_ai.pNext = nullptr;
	memory_ai.allocationSize = mem_reqs.size;
	memory_ai.memoryTypeIndex = buffer_memory_type_idx;

	check(vkAllocateMemory(m_device, &memory_ai, nullptr, &out_memory));

	check(vkBindBufferMemory(m_device, out_buffer, out_memory, 0));

	return {};
}

och::status vulkan_context::create_image_with_view(VkImageView& out_view, VkImage& out_image, VkDeviceMemory& out_memory, VkExtent3D extent, VkImageAspectFlags aspect, VkImageUsageFlags image_usage, VkImageType image_type, VkImageViewType view_type, VkFormat image_format, VkFormat view_format, VkMemoryPropertyFlags memory_properties, VkImageTiling image_tiling, VkSharingMode sharing_mode, uint32_t queue_family_idx_cnt, const uint32_t* queue_family_indices) noexcept
{
	VkImageCreateInfo image_ci{};
	image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_ci.pNext = nullptr;
	image_ci.flags = 0;
	image_ci.imageType = image_type;
	image_ci.format = image_format;
	image_ci.extent = extent;
	image_ci.mipLevels = 1;
	image_ci.arrayLayers = 1;
	image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
	image_ci.tiling = image_tiling;
	image_ci.usage = image_usage;
	image_ci.sharingMode = sharing_mode;
	image_ci.queueFamilyIndexCount = queue_family_idx_cnt;
	image_ci.pQueueFamilyIndices = queue_family_indices;
	image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	check(vkCreateImage(m_device, &image_ci, nullptr, &out_image));

	VkMemoryRequirements mem_reqs;

	vkGetImageMemoryRequirements(m_device, out_image, &mem_reqs);

	uint32_t mem_type_idx;

	check(suitable_memory_type_idx(mem_type_idx, mem_reqs.memoryTypeBits, memory_properties));

	VkMemoryAllocateInfo memory_ai{};
	memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_ai.pNext = nullptr;
	memory_ai.allocationSize = mem_reqs.size;
	memory_ai.memoryTypeIndex = mem_type_idx;

	check(vkAllocateMemory(m_device, &memory_ai, nullptr, &out_memory));

	check(vkBindImageMemory(m_device, out_image, out_memory, 0));

	VkImageViewCreateInfo image_view_ci{};
	image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_ci.pNext = nullptr;
	image_view_ci.flags = 0;
	image_view_ci.image = out_image;
	image_view_ci.viewType = view_type;
	image_view_ci.format = view_format;
	image_view_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
	image_view_ci.subresourceRange.aspectMask = aspect;
	image_view_ci.subresourceRange.baseMipLevel = 0;
	image_view_ci.subresourceRange.levelCount = 1;
	image_view_ci.subresourceRange.baseArrayLayer = 0;
	image_view_ci.subresourceRange.layerCount = 1;

	check(vkCreateImageView(m_device, &image_view_ci, nullptr, &out_view));

	return {};
}

och::status vulkan_context::create_images_with_views(uint32_t image_cnt, VkImageView* out_views, VkImage* out_images, VkDeviceMemory& out_memory, VkExtent3D extent, VkImageAspectFlags aspect, VkImageUsageFlags image_usage, VkImageType image_type, VkImageViewType view_type, VkFormat image_format, VkFormat view_format, VkMemoryPropertyFlags memory_properties, VkImageTiling image_tiling, VkSharingMode sharing_mode, uint32_t queue_family_idx_cnt, const uint32_t* queue_family_indices) noexcept
{
	VkImageCreateInfo image_ci{};
	image_ci.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	image_ci.pNext = nullptr;
	image_ci.flags = 0;
	image_ci.imageType = image_type;
	image_ci.format = image_format;
	image_ci.extent = extent;
	image_ci.mipLevels = 1;
	image_ci.arrayLayers = 1;
	image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
	image_ci.tiling = image_tiling;
	image_ci.usage = image_usage;
	image_ci.sharingMode = sharing_mode;
	image_ci.queueFamilyIndexCount = queue_family_idx_cnt;
	image_ci.pQueueFamilyIndices = queue_family_indices;
	image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	for (uint32_t i = 0; i != image_cnt; ++i)
		check(vkCreateImage(m_device, &image_ci, nullptr, &out_images[i]));

	VkMemoryRequirements mem_reqs;

	vkGetImageMemoryRequirements(m_device, out_images[0], &mem_reqs);

	uint32_t mem_type_idx;

	check(suitable_memory_type_idx(mem_type_idx, mem_reqs.memoryTypeBits, memory_properties));

	size_t aligned_size = find_aligned_size(mem_reqs.size, mem_reqs.alignment);

	VkMemoryAllocateInfo memory_ai{};
	memory_ai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memory_ai.pNext = nullptr;
	memory_ai.allocationSize = aligned_size * (image_cnt - 1) + mem_reqs.size;
	memory_ai.memoryTypeIndex = mem_type_idx;

	check(vkAllocateMemory(m_device, &memory_ai, nullptr, &out_memory));

	for (uint32_t i = 0; i != image_cnt; ++i)
		check(vkBindImageMemory(m_device, out_images[i], out_memory, aligned_size * i));

	VkImageViewCreateInfo image_view_ci{};
	image_view_ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	image_view_ci.pNext = nullptr;
	image_view_ci.flags = 0;
	image_view_ci.image = nullptr;
	image_view_ci.viewType = view_type;
	image_view_ci.format = view_format;
	image_view_ci.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY,VK_COMPONENT_SWIZZLE_IDENTITY };
	image_view_ci.subresourceRange.aspectMask = aspect;
	image_view_ci.subresourceRange.baseMipLevel = 0;
	image_view_ci.subresourceRange.levelCount = 1;
	image_view_ci.subresourceRange.baseArrayLayer = 0;
	image_view_ci.subresourceRange.layerCount = 1;

	for (uint32_t i = 0; i != image_cnt; ++i)
	{
		image_view_ci.image = out_images[i];

		check(vkCreateImageView(m_device, &image_view_ci, nullptr, &out_views[i]));
	}

	return {};
}




och::status vulkan_context::begin_message_processing() noexcept
{
	if (!SetEvent(m_message_pump_start_wait_event))
		return to_status(HRESULT_FROM_WIN32(GetLastError()));

	return {};
}

void vulkan_context::end_message_processing() noexcept
{
	PostMessageW(static_cast<HWND>(m_hwnd), MESSAGE_PUMP_THREAD_TERMINATION_MESSAGE, 0, 0);
}

bool vulkan_context::is_window_closed() const noexcept
{
	return m_flags.is_window_closed.load(std::memory_order::acquire);
}

bool vulkan_context::is_framebuffer_resized() const noexcept
{
	return m_flags.framebuffer_resized.load(std::memory_order::acquire);
}



void vulkan_context::enqueue_input_char(uint32_t utf16_cp) noexcept
{
	const char16_t hi = utf16_cp & 0xFFFF;

	const char16_t lo = (utf16_cp >> 16) & 0xFFFF;

	const char32_t codept = (hi >= 0xD800 && hi <= 0xDBFF && lo >= 0xDC00 && lo <= 0xDFFF) ? ((static_cast<char32_t>(hi - 0xD800) << 10) | (lo - 0xDC00)) + 0x10000 : static_cast<char32_t>(hi);

	uint8_t loaded_head = m_input_char_head.load(std::memory_order::acquire);

	uint8_t loaded_tail = m_input_char_tail.load(std::memory_order::acquire);

	if (((loaded_head + 1) & 63) == loaded_tail)
		m_input_char_tail.store((loaded_tail + 1) & 63);

	m_input_char_queue[loaded_head].store(codept, std::memory_order::release);

	m_input_char_head.store((loaded_head + 1) & 63, std::memory_order::release);
}

char32_t vulkan_context::get_input_char() noexcept
{
	uint8_t loaded_tail = m_input_char_tail.load(std::memory_order::acquire);

	uint8_t loaded_head = m_input_char_head.load(std::memory_order::acquire);

	if (loaded_head == loaded_tail)
		return L'\0';

	char32_t next_char = m_input_char_queue[loaded_tail];
	
	m_input_char_tail.fetch_add(1);

	m_input_char_tail.fetch_and(63);

	return next_char;
}

char32_t vulkan_context::peek_input_char() const noexcept
{
	uint8_t loaded_tail = m_input_char_tail.load(std::memory_order::acquire);

	uint8_t loaded_head = m_input_char_head.load(std::memory_order::acquire);

	if (loaded_head == loaded_tail)
		return U'\0';

	return m_input_char_queue[loaded_tail].load(std::memory_order::acquire);
}



void vulkan_context::enqueue_input_event(input_event e) noexcept
{
	uint8_t loaded_head = m_input_event_head.load(std::memory_order::acquire);

	uint8_t loaded_tail = m_input_event_tail.load(std::memory_order::acquire);

	if (((loaded_head + 1) & 63) == loaded_tail)
		m_input_event_tail.store((loaded_tail + 1) & 63);

	m_input_event_queue[loaded_head].store(e, std::memory_order::release);

	m_input_event_head.store((loaded_head + 1) & 63, std::memory_order::release);
}

input_event vulkan_context::get_input_event() noexcept
{
	uint8_t loaded_tail = m_input_event_tail.load(std::memory_order::acquire);

	uint8_t loaded_head = m_input_event_head.load(std::memory_order::acquire);

	if (loaded_head == loaded_tail)
		return { 0 };

	input_event next_event = m_input_event_queue[loaded_tail];

	m_input_event_tail.fetch_add(1);

	m_input_event_tail.fetch_and(63);

	return next_event;
}

input_event vulkan_context::peek_input_event() const noexcept
{
	uint8_t loaded_tail = m_input_event_tail.load(std::memory_order::acquire);

	uint8_t loaded_head = m_input_event_head.load(std::memory_order::acquire);

	if (loaded_head == loaded_tail)
		return { 0 };

	input_event next_event = m_input_event_queue[loaded_tail];

	return next_event;
}

uint16_t vulkan_context::register_input_event(const input_event_desc& event_desc) noexcept
{
	uint32_t event_cnt = m_input_events.size();

	uint16_t free_id = event_cnt ? m_input_events[event_cnt - 1].m_event_id + 1 : static_cast<uint16_t>(1);

	m_input_events.add(event_desc);

	m_input_events[event_cnt].m_event_id = free_id;

	return free_id;
}

void vulkan_context::unregister_input_event(uint16_t event_id) noexcept
{
		for (uint32_t i = 0; i != m_input_events.size(); ++i)
			if (m_input_events[i].m_event_id == event_id)
			{
				m_input_events.remove(i);

				break;
			}
}

void vulkan_context::check_input_events(och::vk changing_keycode, key_event change_type) noexcept
{
	for (const auto& e : m_input_events)
		if (e.m_change_type == key_event::held)
		{
			for (uint32_t i = 0; i != 5; ++i)
				if (e.m_held_keys[i] == och::vk::NONE)
					goto ENQUEUE_HELD;
				else if (!(m_pressed_keycodes[static_cast<uint8_t>(e.m_held_keys[i]) >> 6] & (1ull << static_cast<uint8_t>(e.m_held_keys[i]) & 63)))
					goto SKIP_HELD;

		ENQUEUE_HELD:

			enqueue_input_event({ e.m_event_id });

		SKIP_HELD:;
		}
		else if(e.m_change_type == change_type && e.m_held_keys[0] == changing_keycode)
		{
			for (uint32_t i = 0; i != 5; ++i)
				if (e.m_held_keys[i] == och::vk::NONE)
					goto ENQUEUE;
				else if (!(m_pressed_keycodes[static_cast<uint8_t>(e.m_held_keys[i]) >> 6] & (1ull << static_cast<uint8_t>(e.m_held_keys[i]) & 63)))
					goto SKIP;

		ENQUEUE:

			enqueue_input_event({ e.m_event_id });

		SKIP:;
		}
}

void vulkan_context::set_keycode(och::vk keycode) noexcept
{
	m_pressed_keycodes[static_cast<uint8_t>(keycode) >> 6] |= 1ull << (static_cast<uint8_t>(keycode) & 63);
}

void vulkan_context::unset_keycode(och::vk keycode) noexcept
{
	m_pressed_keycodes[static_cast<uint8_t>(keycode) >> 6] &= ~(1ull << (static_cast<uint8_t>(keycode) & 63));
}

bool vulkan_context::get_keycode(och::vk keycode) noexcept
{
	return m_pressed_keycodes[static_cast<uint8_t>(keycode) >> 6] & (1ull << (static_cast<uint8_t>(keycode) & 63));
}

void vulkan_context::reset_pressed_keys() noexcept
{
	for (auto& i : m_pressed_keycodes)
		i = 0;
}

void vulkan_context::set_mouse_pos(uint16_t x, uint16_t y) noexcept
{
	m_mouse_x = x;

	m_mouse_y = y;
}
