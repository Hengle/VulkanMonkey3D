#pragma once
#include "../../include/Vulkan.h"

namespace vm {
	struct Surface
	{
		vk::SurfaceKHR surface;
		vk::Extent2D actualExtent;
		vk::SurfaceCapabilitiesKHR capabilities;
		vk::SurfaceFormatKHR formatKHR;
		vk::PresentModeKHR presentModeKHR;
	};
}