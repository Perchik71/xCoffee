#include <windows.h>
#include <Psapi.h>

#include "util.h"
#include "pluginmain.h"

#include <vector>
#include <format>

bool xCoffee::TConvertUtil::Utf8ToUtf16(const std::string& a_in, std::wstring& a_out) noexcept
{
	const auto cvt = [&](wchar_t* a_dst, std::size_t a_length) {
		return MultiByteToWideChar(CP_UTF8, 0, a_in.data(), static_cast<int>(a_in.length()),
			a_dst, static_cast<int>(a_length));
		};

	const auto len = cvt(nullptr, 0);
	if (len == 0)
		return false;

	std::wstring out(len, '\0');
	if (cvt(out.data(), out.length()) == 0)
		return false;

	a_out = out;
	return true;
}

bool xCoffee::TConvertUtil::Utf16ToUtf8(const std::wstring& a_in, std::string& a_out) noexcept
{
	const auto cvt = [&](char* a_dst, std::size_t a_length) {
		return WideCharToMultiByte(CP_UTF8, 0, a_in.data(), static_cast<int>(a_in.length()),
			a_dst, static_cast<int>(a_length), nullptr, nullptr);
		};

	const auto len = cvt(nullptr, 0);
	if (len == 0)
		return false;

	std::string out(len, '\0');
	if (cvt(out.data(), out.length()) == 0)
		return false;

	a_out = out;
	return true;
}

bool xCoffee::TConvertUtil::Utf8ToUtf16(const std::string_view& a_in, std::wstring& a_out) noexcept
{
	const auto cvt = [&](wchar_t* a_dst, std::size_t a_length) {
		return MultiByteToWideChar(CP_UTF8, 0, a_in.data(), static_cast<int>(a_in.length()),
			a_dst, static_cast<int>(a_length));
		};

	const auto len = cvt(nullptr, 0);
	if (len == 0)
		return false;

	std::wstring out(len, '\0');
	if (cvt(out.data(), out.length()) == 0)
		return false;

	a_out = out;
	return true;
}

bool xCoffee::TConvertUtil::Utf16ToUtf8(const std::wstring_view& a_in, std::string& a_out) noexcept
{
	const auto cvt = [&](char* a_dst, std::size_t a_length) {
		return WideCharToMultiByte(CP_UTF8, 0, a_in.data(), static_cast<int>(a_in.length()),
			a_dst, static_cast<int>(a_length), nullptr, nullptr);
		};

	const auto len = cvt(nullptr, 0);
	if (len == 0)
		return false;

	std::string out(len, '\0');
	if (cvt(out.data(), out.length()) == 0)
		return false;

	a_out = out;
	return true;
}

bool xCoffee::TConvertUtil::Utf8ToUtf16(const char* a_in, std::wstring& a_out) noexcept
{
	return Utf8ToUtf16(std::string(a_in), a_out);
}

bool xCoffee::TConvertUtil::Utf16ToUtf8(const wchar_t* a_in, std::string& a_out) noexcept
{
	return Utf16ToUtf8(std::wstring(a_in), a_out);
}

std::wstring xCoffee::TConvertUtil::ToDecode(const std::string& a_in) noexcept
{
	std::wstring r{};
	return Utf8ToUtf16(a_in, r) ? r : L"";
}

std::string xCoffee::TConvertUtil::ToEncode(const std::wstring& a_in) noexcept
{
	std::string r{};
	return Utf16ToUtf8(a_in, r) ? r : "";
}

std::wstring xCoffee::TConvertUtil::ToDecode(const std::string_view& a_in) noexcept
{
	std::wstring r{};
	return Utf8ToUtf16(a_in, r) ? r : L"";
}

std::string xCoffee::TConvertUtil::ToEncode(const std::wstring_view& a_in) noexcept
{
	std::string r{};
	return Utf16ToUtf8(a_in, r) ? r : "";
}

std::wstring xCoffee::TConvertUtil::ToDecode(const char* a_in) noexcept
{
	std::wstring r{};
	return Utf8ToUtf16(a_in, r) ? r : L"";
}

std::string xCoffee::TConvertUtil::ToEncode(const wchar_t* a_in) noexcept
{
	std::string r{};
	return Utf16ToUtf8(a_in, r) ? r : "";
}

std::string xCoffee::TFileUtil::GetFileVersion(const std::string& a_filename) noexcept
{
	auto filename = TConvertUtil::ToDecode(a_filename);

	DWORD dwDummy;
	DWORD dwSize = GetFileVersionInfoSizeW(filename.c_str(), &dwDummy);
	if (!dwSize)
		return "";

	std::vector<BYTE> data(dwSize);
	if (!GetFileVersionInfoW(filename.c_str(), 0, dwSize, data.data()))
	{
		dprintf("Failed to get file version info \"%s\".", a_filename.c_str());
		return "";
	}

	UINT uLen = 0;
	VS_FIXEDFILEINFO* pFileInfo = nullptr;
	if (VerQueryValueW(data.data(), L"\\", (LPVOID*)&pFileInfo, &uLen))
		return std::format("{}.{}.{}.{}",
			(int)HIWORD(pFileInfo->dwFileVersionMS), (int)LOWORD(pFileInfo->dwFileVersionMS),
			(int)HIWORD(pFileInfo->dwFileVersionLS), (int)LOWORD(pFileInfo->dwFileVersionLS));

	return "";
}

std::string xCoffee::TFileUtil::GetFileName(const std::string& a_filename) noexcept
{
	auto begin_it = a_filename.find_last_of("\\/");
	if (begin_it != std::string::npos)
		return a_filename.substr(begin_it + 1);
	return a_filename;
}

std::string xCoffee::TFileUtil::GetFilePath(const std::string& a_filename) noexcept
{
	auto begin_it = a_filename.find_last_of("\\/");
	if (begin_it != std::string::npos)
		return a_filename.substr(0, begin_it + 1);
	return "";
}

std::string xCoffee::TProcessUtil::GetProcessFileName(const uintptr_t a_process) noexcept
{
	std::wstring nameProcess;
	nameProcess.resize(1024);
	nameProcess.resize(GetModuleFileNameExW(reinterpret_cast<HANDLE>(a_process), nullptr, 
		nameProcess.data(), (DWORD)nameProcess.length()));
	return TConvertUtil::ToEncode(nameProcess);
}

uintptr_t xCoffee::TProcessUtil::GetProcessBaseAddr(const uintptr_t a_process) noexcept
{
	auto filename = TFileUtil::GetFileName(GetProcessFileName(a_process));
	return DbgFunctions()->ModBaseFromName(filename.c_str());
}

bool xCoffee::TProcessUtil::GetProcessSegmentList(const uintptr_t a_process, SegmentList& a_list) noexcept
{
	a_list.list.clear();

	auto modBase = TProcessUtil::GetProcessBaseAddr(a_process);
	if (!modBase) return false;

	BridgeList<Script::Module::ModuleSectionInfo> sectionList;
	if (!Script::Module::SectionListFromAddr(modBase, &sectionList))
		return false;
	else
	{
		for (size_t i = 0; i < sectionList.Count(); i++)
		{
			auto& sec = sectionList[i];
			a_list.list.push_back({ sec.addr, sec.size, sec.name });
		}
	}

	return a_list.list.size() != 0;
}

bool xCoffee::TProcessUtil::SegmentList::GetByName(const std::string_view& a_name, SegmentInfo& a_info) const noexcept
{
	for (auto& item : list)
		if (!stricmp(item.name.c_str(), a_name.data()))
		{
			a_info = item;
			return true;
		}

	return false;
}

bool xCoffee::TDialogUtil::OpenSelectionDialog(const wchar_t* a_format, const wchar_t* a_title,
	std::function<bool(const wchar_t*)> a_handler, bool a_save, const wchar_t* a_defaultName) noexcept
{
	wchar_t buffer[MAX_PATH]{};
	if (!a_defaultName)
		return buffer[0] = L'\0';
	else
		wcscpy_s(buffer, a_defaultName);

	OPENFILENAMEW ofn{};
	std::fill_n((uint8_t*)std::addressof(ofn), sizeof(OPENFILENAMEW), 0);

	ofn.lStructSize = sizeof(OPENFILENAMEW);
	ofn.hwndOwner = GuiGetWindowHandle();
	ofn.lpstrFilter = a_format;
	ofn.lpstrFile = buffer;
	ofn.nMaxFile = ARRAYSIZE(buffer);
	ofn.lpstrInitialDir = L".\\";
	ofn.lpstrTitle = a_title;
	ofn.Flags = OFN_FILEMUSTEXIST;
	ofn.lpstrDefExt = wcschr(a_format, '\0') + 3;

	if (a_save)
	{
		ofn.Flags = OFN_OVERWRITEPROMPT;

		if (!GetSaveFileNameW(&ofn))
		{
			dputs("Aborting!");
			return false;
		}
	}
	else
	{
		if (!GetOpenFileNameW(&ofn))
		{
			dputs("Aborting!");
			return false;
		}
	}

	return (a_handler) ? a_handler(buffer) : false;
}