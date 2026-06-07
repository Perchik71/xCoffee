//llvm
#include <llvm/DebugInfo/CodeView/TypeRecord.h>
#include <llvm/DebugInfo/CodeView/ContinuationRecordBuilder.h>
#include <llvm/DebugInfo/PDB/Native/GSIStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/PDBFileBuilder.h>
#include <llvm/DebugInfo/PDB/Native/TpiStreamBuilder.h>
#include <llvm/DebugInfo/CodeView/SymbolSerializer.h>
#include <llvm/DebugInfo/MSF/MSFBuilder.h>
#include <llvm/DebugInfo/PDB/Native/DbiModuleDescriptorBuilder.h>
#include <llvm/DebugInfo/PDB/Native/DbiStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/InfoStreamBuilder.h>
#include <llvm/DebugInfo/PDB/Native/RawTypes.h>
#include <llvm/Object/COFF.h>
#include <llvm/Support/Allocator.h>
#include <llvm/Support/ErrorOr.h>
#include <llvm/Support/Error.h>
#include <llvm/Support/MemoryBuffer.h>
#include <llvm/Support/Parallel.h>

#include "plugin.h"
#include "util.h"
#include "ms_rtti.h"
#include "signmaker.h"
#include "..\resource.h"

#include <Psapi.h>
#include <tlhelp32.h>
#include <filesystem>
#include <fstream>
#include <array>
#include <unordered_map>
#if 0
#include <regex>
#endif

namespace xCoffee
{
	enum MenuItems : int
	{
		MENU_IMPORT = 0,
		MENU_EXPORT,
		MENU_CREATESIG,
		MENU_CREATEFAKEPDB,
		MENU_RTTI_DUMP,
		MENU_RTTI_APPLY,
		MENU_MAX
	};

	constexpr static auto START_ADDRESS = 0x140000000ull;
	constexpr static auto FILENAME = L"XDBGImportNames.txt";
	constexpr static auto FILENAME_RTTI = L"XDBG_RTTI.txt";
	constexpr static auto FILENAME_PDB = L"FakeDBG.pdb";

	static duint DbgGetCurrentModule() noexcept
	{
		if (!DbgIsDebugging())
		{
			_plugin_logprintf("The debugger is not running!\n");
			return 0;
		}

		// First get the current code location
		SELECTIONDATA selection;

		if (!GuiSelectionGet(GUI_DISASSEMBLY, &selection))
		{
			_plugin_logprintf("GuiSelectionGet(GUI_DISASSEMBLY) failed\n");
			return 0;
		}

		// Convert the selected address to a module base
		duint moduleBase = DbgFunctions()->ModBaseFromAddr(selection.start);

		if (moduleBase <= 0)
		{
			_plugin_logprintf("Failed to resolve module base at address '0x%llX'\n", (ULONGLONG)selection.start);
			return 0;
		}

		return moduleBase;
	}

	template<typename R, class FuncTy>
	void parallelSort(R&& Range, FuncTy Fn) noexcept {
		llvm::parallelSort(std::begin(Range), std::end(Range), Fn);
	}
}

void xCoffee::TPlugin::MenuEntry_Handler(CBTYPE cbType, void* callbackInfo) noexcept
{
	if (!callbackInfo) return;

	auto info = (PLUG_CB_MENUENTRY*)callbackInfo;
	switch (info->hEntry)
	{
	case MENU_IMPORT:
		TPlugin::GetSingleton()->ImportNames();
		break;
	case MENU_EXPORT:
		TPlugin::GetSingleton()->ExportNames();
		break;
	case MENU_CREATESIG:
		TPlugin::GetSingleton()->CreateSignature();
		break;
	case MENU_CREATEFAKEPDB:
		TPlugin::GetSingleton()->CreateFakePDB();
		break;
	case MENU_RTTI_DUMP:
		TPlugin::GetSingleton()->RTTIDump();
		break;
	case MENU_RTTI_APPLY:
		TPlugin::GetSingleton()->RTTIApply();
		break;
	}
}

void xCoffee::TPlugin::UnsafeCreateFakePDB()
{
	if (!DbgIsDebugging())
	{
		dputs("The debugger is not running!");
		return;
	}

	auto process = reinterpret_cast<uintptr_t>(DbgGetProcessHandle());
	auto nameProcess = TProcessUtil::GetProcessFileName(process);
	if (nameProcess.empty())
	{
		dputs("Failed get filename process!");
		return;
	}

	llvm::BumpPtrAllocator _allocator;
	llvm::pdb::PDBFileBuilder _pdbBuilder(_allocator);

	// Initialize builder
	if (_pdbBuilder.initialize(4096))
	{
		dputs("Failed initialize PDBBuilder object llvm");
		return;
	}

	// Create streams in MSF for predefined streams, namely PDB, TPI, DBI and IPI.
	for (int I = 0; I < (int)llvm::pdb::kSpecialStreamCount; ++I)
		if (_pdbBuilder.getMsfBuilder().addStream(0).takeError())
		{
			dputs("Failed create special msf streams pdb");
			return;
		}

	auto modBase = TProcessUtil::GetProcessBaseAddr(process);

	struct SegmentInfo
	{
		duint rva;
		duint size;
		char name[MAX_SECTION_SIZE * 5];
	};

	std::vector<SegmentInfo> segments;
	{
		BridgeList<Script::Module::ModuleSectionInfo> sectionList;
		if (!Script::Module::SectionListFromAddr(modBase, &sectionList))
		{
			dputs("Failed get filename process!");
			return;
		}
		else
		{
			for (size_t i = 0; i < sectionList.Count(); i++)
			{
				SegmentInfo seg{};

				auto& sec = sectionList[i];
				memcpy(std::addressof(seg), std::addressof(sec), sizeof(seg));
				seg.rva -= modBase;

				segments.emplace_back(seg);
			}
		}
	}

	static auto getSectionIndexByRva = [&segments](uint32_t rva) -> uint16_t {
		uint16_t id = 0;
		for (auto& sec : segments)
		{
			if ((sec.rva <= rva) && ((sec.size + sec.rva) > rva))
				return id;
			id++;
		}
		return (uint16_t)-1;
		};

	IMAGE_DOS_HEADER dosHeader{};
	if (!DbgMemRead(modBase, std::addressof(dosHeader), sizeof(dosHeader)))
	{
		dputs("Failed read IMAGE_DOS_HEADER!");
		return;
	}

	IMAGE_NT_HEADERS ntHeader{};
	if (!DbgMemRead(modBase + dosHeader.e_lfanew, std::addressof(ntHeader), sizeof(ntHeader)))
	{
		dputs("Failed read IMAGE_NT_HEADERS!");
		return;
	}

	auto debugDirectoryEntry = ntHeader.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
	if (!debugDirectoryEntry.VirtualAddress || !debugDirectoryEntry.Size)
	{
		dputs("No debug info to .exe");
		return;
	}
	else
	{
		auto count = debugDirectoryEntry.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
		auto debugDirectories = std::make_unique<IMAGE_DEBUG_DIRECTORY[]>(count);

		if (!DbgMemRead(modBase + debugDirectoryEntry.VirtualAddress,
			debugDirectories.get(), debugDirectoryEntry.Size))
		{
			dputs("Failed read IMAGE_DEBUG_DIRECTORY entries!");
			return;
		}

		struct CV_INFO_PDB70
		{
			uint32_t CvSignature;
			llvm::codeview::GUID Signature;
			uint32_t Age;
			char PdbFileName[ANYSIZE_ARRAY];
		};

		bool addedInfo = false;

		// Lookup CodeView record.
		for (duint i = 0; i < count; i++)
		{
			auto dir = debugDirectories.get()[i];
			if (dir.Type != IMAGE_DEBUG_TYPE_CODEVIEW)
				continue;

			auto CvInfo = (CV_INFO_PDB70*)malloc(dir.SizeOfData);
			if (!CvInfo)
			{
				dputs("Out of memory!");
				return;
			}

			if (!DbgMemRead(modBase + dir.AddressOfRawData, CvInfo, dir.SizeOfData))
			{
				dputs("Failed read CV_INFO_PDB70");
				free(CvInfo);
				return;
			}

			if (CvInfo->CvSignature != 'SDSR')
			{
				dputs("Weird, old PDB format maybe");
				free(CvInfo);
				return;
			}

			// Add an Info stream.
			auto& infoBuilder = _pdbBuilder.getInfoBuilder();
			infoBuilder.setVersion(llvm::pdb::PdbRaw_ImplVer::PdbImplVC70);
			infoBuilder.setHashPDBContentsToGUID(false);
			//infoBuilder.addFeature(llvm::pdb::PdbRaw_FeatureSig::VC110);
			infoBuilder.setAge(CvInfo->Age);
			infoBuilder.setGuid(CvInfo->Signature);

			// Add an empty DBI stream.
			auto& DbiBuilder = _pdbBuilder.getDbiBuilder();
			DbiBuilder.setVersionHeader(llvm::pdb::PdbDbiV70);
			DbiBuilder.setMachineType(static_cast<llvm::pdb::PDB_Machine>(ntHeader.FileHeader.Machine));
			DbiBuilder.setFlags(llvm::pdb::DbiFlags::FlagHasCTypesMask);
			DbiBuilder.setAge(infoBuilder.getAge());
			// Add version compiler llvm
			DbiBuilder.setBuildNumber(19, 1);
			addedInfo = true;

			free(CvInfo);
			break;
		}

		if (!addedInfo)
		{
			dputs("Can't find debug info");
			return;
		}

		// Start the TPI or IPI stream header.
		_pdbBuilder.getTpiBuilder().setVersionHeader(llvm::pdb::PdbTpiV80);
		_pdbBuilder.getIpiBuilder().setVersionHeader(llvm::pdb::PdbTpiV80);

		// Add COFF section header stream.
		auto temp = std::make_unique<uint8_t[]>(0x1000);
		if (!temp)
		{
			dputs("Out of memory!");
			return;
		}

		if (!DbgMemRead(modBase, temp.get(), 0x1000))
		{
			dputs("Failed read data 0x1000!");
			return;
		}

		auto pntHeader = (PIMAGE_NT_HEADERS)(temp.get() + dosHeader.e_lfanew);
		auto sectionStart = IMAGE_FIRST_SECTION(pntHeader);
		auto& DbiBuilder = _pdbBuilder.getDbiBuilder();

		// Add Section Map stream
		std::vector<llvm::object::coff_section> _sections{};
		_sections.resize(pntHeader->FileHeader.NumberOfSections);
		memcpy(_sections.data(), sectionStart,
			static_cast<size_t>(pntHeader->FileHeader.NumberOfSections) * sizeof(llvm::object::coff_section));
		DbiBuilder.createSectionMap(_sections);

		auto sectionsTable = llvm::ArrayRef<uint8_t>(
			reinterpret_cast<const uint8_t*>(sectionStart),
			static_cast<size_t>(pntHeader->FileHeader.NumberOfSections) * sizeof(llvm::object::coff_section)
		);

		if (DbiBuilder.addDbgStream(llvm::pdb::DbgHeaderType::SectionHdr, sectionsTable))
		{
			dputs("addDbgStream() return failed");
			return;
		}

		std::vector<char*> cacheNames{};
		
		BridgeList<Script::Symbol::SymbolInfo> symbolList{};
		if (!Script::Symbol::GetList(&symbolList) && !symbolList.Count())
		{
			dputs("Function list empty");
			return;
		}
		else
		{
			std::unordered_map<std::string, Script::Symbol::SymbolInfo> symbols{};
			auto total = symbolList.Count();
			dprintf("Debug symbols: %llu", total);

			nameProcess = TFileUtil::GetFileName(nameProcess);

			for (int ids = 0; ids < total; ids++)
			{
				// ignoring import/exports and default naming functions
				if ((symbolList[ids].type != Script::Symbol::Function) || !strnicmp(symbolList[ids].name, "sub_", 4) ||
					!strnicmp(symbolList[ids].name, "fun_", 4) || stricmp(symbolList[ids].mod, nameProcess.c_str()))
					continue;

				auto tmp = strdup(symbolList[ids].name);
				if (tmp)
				{
					// insert label with unique hash (name+rva)
					symbols.insert({ std::string(strlwr(tmp)) + std::to_string(symbolList[ids].rva), symbolList[ids]});
					free(tmp);
				}
			}

			dprintf("Num symbols: %llu", symbols.size());
			cacheNames.reserve(symbols.size());

			if (symbols.size())
			{
				std::vector<llvm::pdb::BulkPublic> publics{};

				for (auto& symbol : symbols)
				{
					llvm::pdb::BulkPublic public_sym{};
					std::fill_n(reinterpret_cast<uint8_t*>(&public_sym), sizeof(llvm::pdb::BulkPublic), 0);

					auto segid = getSectionIndexByRva(symbol.second.rva);
					// need reallocate string.... without this CTD
					auto copyName = strdup(symbol.second.name);
					cacheNames.push_back(copyName);

					public_sym.Name = copyName;
					public_sym.NameLen = strlen(symbol.second.name);
					// maybe need starts with 1
					public_sym.Segment = segid + 1;
					// offset should relative seg off.
					public_sym.Offset = symbol.second.rva - segments[segid].rva;
					// manual added flags
					public_sym.Flags = std::to_underlying(llvm::codeview::PublicSymFlags::Code) |
						std::to_underlying(llvm::codeview::PublicSymFlags::Function);
					publics.push_back(public_sym);
					
					//dprintf("symbols: %s %u %u 0x%X", public_sym.Name, public_sym.NameLen, public_sym.Segment, public_sym.Offset);
				}

				if (!publics.empty())
					// Sort the public symbols and add them to the stream.
					parallelSort(publics, [](const llvm::pdb::BulkPublic& L, const llvm::pdb::BulkPublic& R) {
					return strcmp(L.Name, R.Name) < 0;
						});

				_pdbBuilder.getGsiBuilder().addPublicSymbols(std::move(publics));
			}
		}

		if (!TDialogUtil::OpenSelectionDialog(L"Program Databases (*.pdb)\0*.pdb\0\0", L"Create fake pdb...",
			[&_pdbBuilder](const wchar_t* a_filename) -> bool {
				std::filesystem::path filename(a_filename);
				filename.replace_extension(L".pdb");
				auto guid = _pdbBuilder.getInfoBuilder().getGuid();
				if (_pdbBuilder.commit(filename.string(), std::addressof(guid)))
					return false;
				return true;
			}, true, FILENAME_PDB))
		{
			dputs("Abort! or Failed!");
			return;
		}

		for (auto& nam : cacheNames)
			free(nam);
	}
}

bool xCoffee::TPlugin::Init(PLUG_INITSTRUCT* a_initStruct)
{
	__try
	{
		a_initStruct->pluginVersion = PLUGIN_VERSION;
		a_initStruct->sdkVersion = PLUG_SDKVERSION;
		strncpy_s(a_initStruct->pluginName, PLUGIN_NAME, _TRUNCATE);
		handle = a_initStruct->pluginHandle;

		_plugin_registercallback(handle, CB_MENUENTRY, MenuEntry_Handler);

		return true;
	}
	__except (1)
	{
		return false;
	}
}

void xCoffee::TPlugin::InitGUI(PLUG_SETUPSTRUCT* a_setupStruct) noexcept
{
	hwndDlg = a_setupStruct->hwndDlg;
	hMenu = a_setupStruct->hMenu;
	hMenuDisasm = a_setupStruct->hMenuDisasm;
	hMenuDump = a_setupStruct->hMenuDump;
	hMenuStack = a_setupStruct->hMenuStack;
	hMenuGraph = a_setupStruct->hMenuGraph;
	hMenuMemmap = a_setupStruct->hMenuMemmap;
	hMenuSymmod = a_setupStruct->hMenuSymmod;

	hMenuPlugin_ImportExportNames = _plugin_menuadd(hMenu, "Import/Export names");
	if (hMenuPlugin_ImportExportNames == -1)
		dputs("Failed create menu");
	else
	{
		_plugin_menuaddentry(hMenuPlugin_ImportExportNames, MENU_IMPORT, "Import");
		_plugin_menuaddentry(hMenuPlugin_ImportExportNames, MENU_EXPORT, "Export");
	}

	// main menu
	_plugin_menuaddseparator(hMenu);
	_plugin_menuaddentry(hMenu, MENU_CREATESIG, "Create signature");
	_plugin_menuaddentry(hMenu, MENU_CREATEFAKEPDB, "Create fake PDB");
	_plugin_menuaddseparator(hMenu);
	
	hMenuPlugin_RTTI = _plugin_menuadd(hMenu, "RTTI");
	if (hMenuPlugin_RTTI == -1)
		dputs("Failed create menu");
	{
		_plugin_menuaddentry(hMenuPlugin_RTTI, MENU_RTTI_DUMP, "Dump");
		_plugin_menuaddentry(hMenuPlugin_RTTI, MENU_RTTI_APPLY, "Apply");
	}

	// disasm
	_plugin_menuaddentry(hMenuDisasm, MENU_CREATESIG, "Create signature");

	// hotkeys
	_plugin_menuentrysethotkey(handle, MENU_CREATESIG, "Shift+S");
}

void xCoffee::TPlugin::Shutdown() noexcept
{
	_plugin_unregistercallback(handle, CB_MENUENTRY);

	_plugin_menuclear(hMenuPlugin_ImportExportNames);
	_plugin_menuclear(hMenuPlugin_RTTI);
	_plugin_menuclear(hMenu);
}

void xCoffee::TPlugin::ImportNames()
{
	MessageBoxA(0, "No support", "Warning", MB_OK | MB_ICONWARNING);
}

void xCoffee::TPlugin::ExportNames()
{
	if (!DbgIsDebugging())
	{
		dputs("The debugger is not running!");
		return;
	}

	std::string nameProcess, version;
	auto process = reinterpret_cast<uintptr_t>(DbgGetProcessHandle());
	nameProcess = TProcessUtil::GetProcessFileName(process);
	if (nameProcess.length())
	{
		FILE* f = nullptr;

		if (!TDialogUtil::OpenSelectionDialog(L"Text files (*.txt)\0*.txt\0\0", L"Export names...",
			[&f](const wchar_t* a_filename) -> bool {
				std::filesystem::path filename(a_filename);
				filename.replace_extension(L".txt");
				f = _wfopen(filename.wstring().c_str(), L"w+");
				if (!f)
				{
					dprintf("File \"%s\" no create (%s)", TConvertUtil::ToEncode(a_filename).c_str(), strerror(errno));
					return false;
				}
				return true;
			}, true, FILENAME))
			return;

		auto modBase = TProcessUtil::GetProcessBaseAddr(process);
		version = TFileUtil::GetFileVersion(nameProcess);
		if (!version.length()) version = "0";

		dprintf("Process \"%s\" v%s 0x%llX", nameProcess.c_str(), version.c_str(), modBase);
		nameProcess = TFileUtil::GetFileName(nameProcess);

		fprintf(f, "Name: %s\nVersion: %s\nBase: 0x%llX\n", nameProcess.c_str(), version.c_str(), modBase);
		
		std::vector<Script::Symbol::SymbolInfo> symbols;
		{
			BridgeList<Script::Symbol::SymbolInfo> symbolList;
			if (!Script::Symbol::GetList(&symbolList) && !symbolList.Count())
			{
				dputs("Function list empty");
				return;
			}

			auto total = symbolList.Count();
			dprintf("Symbol list (total: %d)", total);
	
			for (int ids = 0; ids < total; ids++)
			{
				if ((symbolList[ids].type != Script::Symbol::Function) || !strnicmp(symbolList[ids].name, "sub_", 4) ||
					!strnicmp(symbolList[ids].name, "fun_", 4) || stricmp(symbolList[ids].mod, nameProcess.c_str()))
					continue;

				symbols.push_back(symbolList[ids]);
			}
		}

		auto total = symbols.size();
		dprintf("Symbol list for export (total: %llu)", total);
		fprintf(f, "Total: %llu\n\n\n", total);
		
		std::string sign{};
		for (auto& symbol : symbols)
		{
			auto addr = (size_t)symbol.rva + modBase;
			
			duint start, end;
			if (!DbgFunctionGet(addr, &start, &end))
				start = end = 0;

			auto sizeFunc = end - start;
			if (sizeFunc > 5)
				sign = TSignMaker::GetSingleton()->CreateSign(addr, sizeFunc);
			else sign.clear();
			
			fprintf(f, "0x%llX (size 0x%llX) %s | sign: %s\n", (size_t)symbol.rva + START_ADDRESS, sizeFunc, symbol.name,
				sign.empty() ? "<none>" : sign.c_str());
		}

		fflush(f);
		fclose(f);
	}
}

void xCoffee::TPlugin::CreateSignature()
{
	if (!DbgIsDebugging())
	{
		dputs("The debugger is not running!");
		return;
	}

	SELECTIONDATA selection{};
	if (!GuiSelectionGet(GUI_DISASSEMBLY, std::addressof(selection)) || (selection.start == selection.end))
		return;

	auto sign = TSignMaker::GetSingleton()->CreateSign(selection.start, selection.end - selection.start);
	auto DialogProc = [](HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) -> INT_PTR
		{
			switch (uMsg) {
			case WM_INITDIALOG:
				SetDlgItemTextA(hwndDlg, IDC_SIGN_EDIT, (LPCSTR)lParam);
				return TRUE;

			case WM_COMMAND:
				if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
				{
					EndDialog(hwndDlg, LOWORD(wParam));
					return TRUE;
				}
				break;
			}
			return FALSE;
		};

	if (!sign.empty())
		DialogBoxParamA(g_LocalDllHandle, MAKEINTRESOURCEA(IDD_CREATESIG_DIALOG), hwndDlg, DialogProc, (LPARAM)sign.c_str());
	else
		dputs("Sign empty");
}

void xCoffee::TPlugin::CreateFakePDB()
{
	__try
	{
		UnsafeCreateFakePDB();
	}
	__except (1)
	{
		dputs("FATAL!!!");
	}
}

void xCoffee::TPlugin::RTTIDump()
{
	TRTTIManager::GetSingleton()->Initialize();

	if (!TDialogUtil::OpenSelectionDialog(L"Text files (*.txt)\0*.txt\0\0", L"Dump RTTI...",
		[&](const wchar_t* a_filename) -> bool {
			TRTTIManager::GetSingleton()->Dump(TConvertUtil::ToEncode(a_filename));
			return true;
		}, true, FILENAME_RTTI))
		return;

	TRTTIManager::GetSingleton()->Shutdown();
}

void xCoffee::TPlugin::RTTIApply()
{
	auto& rtti = *TRTTIManager::GetSingleton();
	rtti.Initialize();
	auto base = rtti.BaseAddress();
	char szLabelAPIFunction[MAX_COMMENT_SIZE] = "";

	uint64_t num = 0;

	for (auto& it : rtti)
	{
		auto& typeInfo = it.second;

		// get label if any as function name
		if (!DbgGetLabelAt(base + typeInfo.RvaDescriptor, SEG_DEFAULT, szLabelAPIFunction))
		{
			std::string temp = typeInfo.Name;
			auto itStr = temp.find_first_of(' ');
			if (itStr != std::string::npos)
				temp.erase(0, itStr + 1);

			temp.insert(0, "RTTI_");

			if (!DbgSetLabelAt(base + typeInfo.RvaDescriptor, temp.c_str()))
				dprintf("<RTTI> Failed set label for address (0X%llX)", base + typeInfo.RvaDescriptor);
			else
				num++;
		}
	}

	rtti.Shutdown();

	dprintf("<RTTI> Added new labels (%llu)", num);
}