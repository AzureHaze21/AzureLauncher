#pragma once

#include <array>

namespace globals
{
	inline std::array<char, 256> login{ 0 };
	inline std::array<char, 256> password{ 0 };
	inline bool bAuthenticated{ false };
}