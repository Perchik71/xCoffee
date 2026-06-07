#pragma once

#include <string>
#include <string_view>
#include <vector>
#include <functional>

namespace xCoffee
{
	using namespace std::literals;

	struct TConvertUtil
	{
		static bool Utf8ToUtf16(const std::string& a_in, std::wstring& a_out) noexcept;
		static bool Utf16ToUtf8(const std::wstring& a_in, std::string& a_out) noexcept;
		static bool Utf8ToUtf16(const std::string_view& a_in, std::wstring& a_out) noexcept;
		static bool Utf16ToUtf8(const std::wstring_view& a_in, std::string& a_out) noexcept;
		static bool Utf8ToUtf16(const char* a_in, std::wstring& a_out) noexcept;
		static bool Utf16ToUtf8(const wchar_t* a_in, std::string& a_out) noexcept;

		static std::wstring ToDecode(const std::string& a_in) noexcept;
		static std::string ToEncode(const std::wstring& a_in) noexcept;
		static std::wstring ToDecode(const std::string_view& a_in) noexcept;
		static std::string ToEncode(const std::wstring_view& a_in) noexcept;
		static std::wstring ToDecode(const char* a_in) noexcept;
		static std::string ToEncode(const wchar_t* a_in) noexcept;
	};

	struct TFileUtil
	{
		static std::string GetFileVersion(const std::string& a_filename) noexcept;
		static std::string GetFileName(const std::string& a_filename) noexcept;
		static std::string GetFilePath(const std::string& a_filename) noexcept;
	};

	struct TProcessUtil
	{
		static std::string GetProcessFileName(const uintptr_t a_process) noexcept;
		static uintptr_t GetProcessBaseAddr(const uintptr_t a_process) noexcept;
		
		struct RegionInfo
		{
			uintptr_t addr{ 0 };
			uintptr_t size{ 0 };

			[[nodiscard]] inline uintptr_t GetAddressBegin() const noexcept { return addr; }
			[[nodiscard]] inline uintptr_t GetAddressEnd() const noexcept { return addr + size; }
			[[nodiscard]] inline uintptr_t GetOffsetBegin(uintptr_t a_base) const noexcept { return GetAddressBegin() - a_base; }
			[[nodiscard]] inline uintptr_t GetOffsetEnd(uintptr_t a_base) const noexcept { return GetAddressEnd() - a_base; }
		};

		struct SegmentInfo :
			public RegionInfo
		{
			std::string name{ 0 };
		};

		struct SegmentList
		{
			std::vector<SegmentInfo> list;

			[[nodiscard]] bool GetByName(const std::string_view& a_name, SegmentInfo& a_info) const noexcept;
		};
		
		[[nodiscard]] static bool GetProcessSegmentList(const uintptr_t a_process, SegmentList& a_list) noexcept;
	};

	struct TDialogUtil
	{
		static bool OpenSelectionDialog(const wchar_t* a_format, const wchar_t* a_title,
			std::function<bool(const wchar_t*)> a_handler, bool a_save = false, 
			const wchar_t* a_defaultName = L"") noexcept;
	};
}