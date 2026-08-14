// common_types.h
// A thin conversion of some common_* items from Citra/Azahar code
// for the purpose of not having to refactor large parts of arm_dyncom

#pragma once

#include <array>
#include <cstdint>


// technically not a type but whatever
#ifdef _MSC_VER
extern "C" {
__declspec(dllimport) void __stdcall DebugBreak(void);
}
#define Crash() DebugBreak()
#else
#define Crash() __builtin_trap()
#endif

// signed types
typedef std::int8_t s8;
typedef std::int16_t s16;
typedef std::int32_t s32;
typedef std::int64_t s64;

//unsigned types
typedef std::uint8_t u8;
typedef std::uint16_t u16;
typedef std::uint32_t u32;
typedef std::uint64_t u64;

// float types
typedef float f32;
typedef double f64;

// unsigned 128 (idk if we use this)
using u128 = std::array<std::uint64_t, 2>;
static_assert(sizeof(u128) == 16, "u128 must be 128 bits wide");