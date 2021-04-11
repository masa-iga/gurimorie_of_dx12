#pragma once
#include <cstdint>

#define HIGH_RESOLUTION (1)

#if HIGH_RESOLUTION
static constexpr int32_t kWindowWidth = 1920;
static constexpr int32_t kWindowHeight = 1080;
#else
static constexpr int32_t kWindowWidth = 640;
static constexpr int32_t kWindowHeight = 480;
#endif // HIGH_RESOLUTION

