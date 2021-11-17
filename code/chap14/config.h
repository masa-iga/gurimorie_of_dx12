#pragma once
#include <cstdint>

#define HIGH_RESOLUTION (1)

namespace Config {
#if HIGH_RESOLUTION
	constexpr int32_t kWindowWidth = 1920;
	constexpr int32_t kWindowHeight = 1080;
#else
	constexpr int32_t kWindowWidth = 640;
	constexpr int32_t kWindowHeight = 480;
#endif // HIGH_RESOLUTION

	constexpr int32_t kShadowBufferWidth = 1024;
	constexpr int32_t kShadowBufferHeight = 1024;
} // namespace Config
