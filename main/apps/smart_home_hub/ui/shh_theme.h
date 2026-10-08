#pragma once

// Shared look-and-feel constants for the Smart Home Hub overlay screens.
// One source of truth so every mini-app renders with the same palette and
// font sizes (see docs/ui-audit-core-s3.md, sections 3 and 5: the cyan/
// green/orange family used to be redefined per screen with drifting values).

#include <lvgl.h>

namespace shh_ui {

// Canvas / cards
constexpr uint32_t kBg = 0x070C10;
constexpr uint32_t kCardBg = 0x111B23;
constexpr uint32_t kTrackBg = 0x1E2A33;
constexpr uint32_t kLine = 0x2A3B47;

// Text
constexpr uint32_t kText = 0xFFFFFF;
constexpr uint32_t kDim = 0x7E93A3;

// Accents (info screens)
constexpr uint32_t kCyan = 0x29D3FF;
constexpr uint32_t kGreen = 0x22E06A;
constexpr uint32_t kBlue = 0x2E9BFF;
constexpr uint32_t kYellow = 0xFFC21A;
constexpr uint32_t kOrange = 0xFF6B35;
constexpr uint32_t kPurple = 0xC24BFF;
constexpr uint32_t kRed = 0xFF5252;
constexpr uint32_t kAmber = 0xFFB300;

// Emotion face neon palette (kept vivid on the pure-black idle screen)
constexpr uint32_t kNeonCyan = 0x00E5FF;
constexpr uint32_t kNeonGreen = 0x00FF88;
constexpr uint32_t kNeonMint = 0x00FFAA;
constexpr uint32_t kPink = 0xFF4081;
constexpr uint32_t kSlateBlue = 0x37474F;

}  // namespace shh_ui

// The large Montserrat faces are only compiled in on boards that ask for them
// (see boards/m5stack/core-s3/config.json). Undefined LV_FONT_*_N evaluates to
// 0 in #if, so this stays correct either way. LV_FONT_DECLARE gives a variable
// in LVGL 9, hence the &: LV_FONT_DEFAULT is already a pointer, so both
// branches end up as const lv_font_t*.
#if LV_FONT_MONTSERRAT_28
#define SHH_FONT_BIG (&lv_font_montserrat_28)
#else
#define SHH_FONT_BIG LV_FONT_DEFAULT
#endif
#if LV_FONT_MONTSERRAT_20
#define SHH_FONT_MID (&lv_font_montserrat_20)
#else
#define SHH_FONT_MID LV_FONT_DEFAULT
#endif
