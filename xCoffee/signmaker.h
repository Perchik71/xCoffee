#pragma once

#include "singleton.h"

#include <memory>
#include <string>

namespace xCoffee
{
	class TSignMaker :
		public TSingleton<TSignMaker>
	{
		void* handle{ nullptr };
		std::unique_ptr<uint8_t[]> mem{};
	public:
		constexpr static auto SIZE_SIG = 64ull;

		TSignMaker();
		virtual ~TSignMaker();

		virtual std::string CreateSign(size_t a_addr, size_t a_size) noexcept;

		static std::string Hex2Str(const uint8_t* raw, size_t len) noexcept;
	};
}