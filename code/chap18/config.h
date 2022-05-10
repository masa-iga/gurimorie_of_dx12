#pragma once
#include <cstdint>

#define HIGH_RESOLUTION (1)
#define USE_AGILITY_SDK (0)

#if USE_AGILITY_SDK
#define AGILITY_SDK_VERSION (600)
#endif // USE_AGILITY_SDK

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
	constexpr float kDefaultHighLuminanceThreshold = 0.85f;
} // namespace Config
