#pragma once

#include <string>
#include <string_view>
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
		static std::string GetProcessFileName(const HANDLE a_process) noexcept;
		static uintptr_t GetProcessBaseAddr(const HANDLE a_process) noexcept;
	};

	struct TDialogUtil
	{
		static bool OpenSelectionDialog(const std::string_view& a_format, const std::string_view& a_title,
			std::function<bool(const wchar_t*)> a_handler, bool a_save = false, 
			const std::string_view& a_defaultName = "") noexcept;
	};
}