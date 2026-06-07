// Author: Nukem9
// https://github.com/Nukem9/SkyrimSETest/blob/master/skyrim64_test/src/typeinfo/ms_rtti.cpp

#include <windows.h>
#include <rttidata.h>
#include <format>

#include "ms_rtti.h"
#include "pluginmain.h"
#include "stream.h"

extern "C"
{
	typedef void* (*malloc_func_t)(size_t);
	typedef void(*free_func_t)(void*);
	char* __unDNameEx(char* outputString, const char* name, int maxStringLength, malloc_func_t pAlloc, free_func_t pFree, 
		char* (__fastcall* pGetParameter)(int), unsigned int disableFlags);
}

namespace xCoffee
{
	TRTTIManager::~TRTTIManager()
	{
		Shutdown();
	}

	void TRTTIManager::Initialize() noexcept
	{
		srtti_data.clear();

		if (!DbgIsDebugging())
		{
			dputs("The debugger is not running!");
			return;
		}

		auto process = reinterpret_cast<uintptr_t>(DbgGetProcessHandle());
		TProcessUtil::SegmentList segs;
		if (!TProcessUtil::GetProcessSegmentList(process, segs))
		{
			dputs("No sigments!");
			return;
		}

		(void)segs.GetByName(".text", segtext);
		(void)segs.GetByName(".data", segdata);
		(void)segs.GetByName(".rdata", segrdata);
		base = TProcessUtil::GetProcessBaseAddr(process);

		psegtext = std::make_unique<uint8_t[]>(segtext.size);
		psegdata = std::make_unique<uint8_t[]>(segdata.size);
		psegrdata = std::make_unique<uint8_t[]>(segrdata.size);

		if (!DbgMemRead(segtext.GetAddressBegin(), psegtext.get(), segtext.size) ||
			!DbgMemRead(segdata.GetAddressBegin(), psegdata.get(), segdata.size) ||
			!DbgMemRead(segrdata.GetAddressBegin(), psegrdata.get(), segrdata.size))
		{
			dputs("<RTTI> Failed get segments data");
			return;
		}

		for (uintptr_t i = 0; i < (segrdata.size - (sizeof(uintptr_t) << 1)); i++)
		{
			// Skip all non-2-aligned addresses. Not sure if this is OK or it skips tables.
			if (i % 2 != 0)
				continue;

			// This might be a valid RTTI entry, so check if:
			// - The COL points to somewhere in .rdata
			// - The COL has a valid signature
			// - The first virtual function points to .text
			//
			uintptr_t addr = *(uintptr_t*)(psegrdata.get() + i);
			uintptr_t vfuncAddr = *(uintptr_t*)((psegrdata.get() + i) + sizeof(uintptr_t));

			if (!(IsWithinDATA(addr) || IsWithinRDATA(addr)) || !IsWithinCODE(vfuncAddr))
				continue;

			CompleteObjectLocator* locator = nullptr;
			if (IsWithinDATA(addr))
				locator = reinterpret_cast<CompleteObjectLocator*>(psegdata.get() + (addr - segdata.addr));
			else if (IsWithinRDATA(addr))
				locator = reinterpret_cast<CompleteObjectLocator*>(psegrdata.get() + (addr - segrdata.addr));
			else
				locator = reinterpret_cast<CompleteObjectLocator*>(psegtext.get() + (addr - segtext.addr));

			if (!IsValidCOL(locator))
				continue;

			// skip childs
			if (locator->Offset != 0)
				continue;

			TypeDescriptor* descriptor = nullptr;
			auto rawDescriptor = base + locator->TypeDescriptor.offset;
			if (IsWithinDATA(rawDescriptor))
				descriptor = reinterpret_cast<TypeDescriptor*>(psegdata.get() + (rawDescriptor - segdata.addr));
			else if (IsWithinRDATA(rawDescriptor))
				descriptor = reinterpret_cast<TypeDescriptor*>(psegrdata.get() + (rawDescriptor - segrdata.addr));
			else
			{
				dprintf("<RTTI> Failed get descriptor (0x%llX)", rawDescriptor);
				continue;
			}

			Info info{
				.VTableAddress = segrdata.addr + i + sizeof(uintptr_t),
				.VTableOffset = locator->Offset,
				.VFunctionCount = 0,
				.RvaDescriptor = locator->TypeDescriptor.offset,
				.RawName = descriptor->name,
				.Descriptor = *descriptor,
			};

			info.Name = __unDNameEx(nullptr, descriptor->name + 1, 0, malloc, free, nullptr, 0x2800);
			info.VFunctionCount = GetCountVFunc(info.VTableAddress);

			if (locator->ClassDescriptor.offset)
			{
				ClassHierarchyDescriptor* hierarchy = nullptr;
				auto rawHierarchy = base + locator->ClassDescriptor.offset;
				if (IsWithinDATA(rawHierarchy))
					hierarchy = reinterpret_cast<ClassHierarchyDescriptor*>(psegdata.get() + (rawHierarchy - segdata.addr));
				else if (IsWithinRDATA(rawHierarchy))
					hierarchy = reinterpret_cast<ClassHierarchyDescriptor*>(psegrdata.get() + (rawHierarchy - segrdata.addr));
				else
				{
					dprintf("<RTTI> Failed get hierarchy (0x%llX)", rawHierarchy);
					continue;
				}

				if (hierarchy->NumBaseClasses)
				{
					BaseClassArray* baseClasses = nullptr;
					auto rawBaseClassArray = base + hierarchy->BaseClassArray.offset;
					if (IsWithinDATA(rawHierarchy))
						baseClasses = reinterpret_cast<BaseClassArray*>(psegdata.get() + (rawBaseClassArray - segdata.addr));
					else if (IsWithinRDATA(rawHierarchy))
						baseClasses = reinterpret_cast<BaseClassArray*>(psegrdata.get() + (rawBaseClassArray - segrdata.addr));
					else
					{
						dprintf("<RTTI> Failed get base classes array (0x%llX)", rawBaseClassArray);
						continue;
					}

					for (uint32_t ids = 0; ids < hierarchy->NumBaseClasses; ids++)
					{
						auto offset = baseClasses->ArrayOfBaseClassDescriptors[ids];
						BaseClassDescriptor* baseClassDescriptor = nullptr;
						auto rawBaseClassDescriptor = base + offset;

						if (IsWithinDATA(rawHierarchy))
							baseClassDescriptor = reinterpret_cast<BaseClassDescriptor*>(psegdata.get() + (rawBaseClassDescriptor - segdata.addr));
						else if (IsWithinRDATA(rawHierarchy))
							baseClassDescriptor = reinterpret_cast<BaseClassDescriptor*>(psegrdata.get() + (rawBaseClassDescriptor - segrdata.addr));
						else
						{
							dprintf("<RTTI> Failed get base class description (0x%llX)", rawBaseClassDescriptor);
							continue;
						}

						TypeDescriptor* descriptorBase = nullptr;
						auto rawDescriptorBase = base + baseClassDescriptor->TypeDescriptor.offset;
						if (IsWithinDATA(rawDescriptorBase))
							descriptorBase = reinterpret_cast<TypeDescriptor*>(psegdata.get() + (rawDescriptorBase - segdata.addr));
						else if (IsWithinRDATA(rawDescriptorBase))
							descriptorBase = reinterpret_cast<TypeDescriptor*>(psegrdata.get() + (rawDescriptorBase - segrdata.addr));
						else
						{
							dprintf("<RTTI> Failed get descriptor base (0x%llX)", rawDescriptorBase);
							continue;
						}

						info.HierarchyClasses.push_back(__unDNameEx(nullptr, descriptorBase->name + 1, 0, malloc, free, nullptr, 0x2800));
					}
				}
			}

			srtti_data.insert({ std::hash<std::string>{}(info.Name), info });
		}

		dprintf("<RTTI> Num records %llu", srtti_data.size());
	}

	void TRTTIManager::Shutdown() noexcept
	{
		srtti_data.clear();
		base = 0;

		segtext.addr = segtext.size = 0;
		segtext.name.clear();
		segdata.addr = segdata.size = 0;
		segdata.name.clear();
		segrdata.addr = segrdata.size = 0;
		segrdata.name.clear();
	}

	void TRTTIManager::Dump(const char* a_filename) const noexcept
	{
		Dump(std::string(a_filename));
	}

	void TRTTIManager::Dump(const std::string& a_filename) const noexcept
	{
		TFileStream stream(a_filename, TTextFileStream::EMode::CREATE);
		for (const auto& it : srtti_data)
		{
			auto& info = it.second;	
			auto str = std::format("`{}`: VTable [0x{:X}, 0x{:X} offset, {} functions] `{}`\n", 
				info.Name, info.VTableAddress - base, info.VTableOffset, info.VFunctionCount, info.RawName);
			if (!stream.WriteBuffer(str.c_str(), str.length()))
				return;

			if (info.HierarchyClasses.size())
			{
				uint32_t num = 0;
				str = "\t\tHierarchy:\n";
				if (!stream.WriteBuffer(str.c_str(), str.length()))
					return;
				
				for (const auto& ib : info.HierarchyClasses)
				{
					str = std::format("\t\t\t#{}: `{}`\n", num, ib);
					if (!stream.WriteBuffer(str.c_str(), str.length()))
						return;
					num++;
				}
			}
		}
	}

	const TRTTIManager::Info* TRTTIManager::Find(const std::string_view& a_name) const noexcept
	{
		auto it = srtti_data.find(std::hash<std::string>{}(a_name.data()));
		return it == srtti_data.end() ? nullptr : &(it->second);
	}

	const TRTTIManager::Info* TRTTIManager::Find(const uintptr_t a_address) const noexcept
	{
		for (const auto& info : srtti_data)
			if (info.second.VTableAddress == a_address)
				return &info.second;
		return nullptr;
	}

	bool TRTTIManager::IsWithinDATA(uintptr_t addr) const noexcept
	{
		return (addr >= segdata.GetAddressBegin()) && (addr <= segdata.GetAddressEnd());
	}

	bool TRTTIManager::IsWithinRDATA(uintptr_t addr) const noexcept
	{
		return (addr >= segrdata.GetAddressBegin()) && (addr <= segrdata.GetAddressEnd());
	}

	bool TRTTIManager::IsWithinCODE(uintptr_t addr) const noexcept
	{
		return (addr >= segtext.GetAddressBegin()) && (addr <= segtext.GetAddressEnd());
	}

	bool TRTTIManager::IsValidCOL(CompleteObjectLocator* Locator) const noexcept
	{
		return (Locator->Signature == CompleteObjectLocator::Signature::S_64) &&
			IsWithinDATA(Locator->TypeDescriptor.Address(base));
	}

	uint32_t TRTTIManager::GetCountVFunc(uintptr_t addr) const noexcept
	{
		uint32_t r = 0;
		// Determine number of virtual functions
		for (uintptr_t j = addr; j < (segrdata.GetAddressEnd() - sizeof(uintptr_t)); j += sizeof(uintptr_t))
		{
			if (!IsWithinCODE(*(uintptr_t*)(psegrdata.get() + (j - segrdata.addr)))) break;
			r++;
		}
		return r;
	}
}