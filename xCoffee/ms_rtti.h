// Author: Nukem9
// https://github.com/Nukem9/SkyrimSETest/blob/master/skyrim64_test/src/typeinfo/ms_rtti.h

#pragma once

#include <unordered_map>
#include <memory>
#include "singleton.h"
#include "util.h"

namespace xCoffee
{
	class TRTTIManager :
		public TSingleton<TRTTIManager>
	{
	public:
		template<typename T>
		struct RVA
		{
			uint32_t offset;

			[[nodiscard]] uintptr_t Address(uintptr_t base) const noexcept { return (uintptr_t)base + offset; }
			[[nodiscard]] T Get(uintptr_t base) const noexcept { return (T)Address(base); }
		};

		// Also known as `class type_info`
		struct TypeDescriptor
		{
			void* vftable;			// const type_info::`vftable'
			uintptr_t unknown;		// CRT internal
			char name[1];
		};

		struct PMD
		{
			int32_t Mdisp;	// Member displacement (vftable offset in the class itself)
			int32_t Pdisp;	// Vbtable displacement (vbtable offset, -1: vftable is at displacement PMD.mdisp inside the class)
			int32_t Vdisp;	// Displacement inside vbtable
		};

		struct BaseClassDescriptor
		{
			enum class Attribute : uint32_t
			{
				NotVisible				= 1 << 0,
				Ambiguous				= 1 << 1,
				Private					= 1 << 2,
				PrivOrProtBase			= 1 << 3,
				Virtual					= 1 << 4,
				Nonpolymorphic			= 1 << 5,
				HasHierarchyDescriptor	= 1 << 6,
			};

			RVA<TypeDescriptor*> TypeDescriptor;				// Type descriptor of the class
			uint32_t NumContainedBases;							// Number of nested classes following in the Base Class Array
			PMD Disp;											// Pointer-to-member displacement info
			Attribute Attributes;								// Flags (BaseClassDescriptorFlags)
		};

#pragma warning(push)
#pragma warning(disable: 4200)									// nonstandard extension used: zero-sized array in struct/union
		struct BaseClassArray
		{
			uint32_t ArrayOfBaseClassDescriptors[];				// BaseClassDescriptor *
		};
#pragma warning(pop)

		struct ClassHierarchyDescriptor
		{
			enum class Attribute : uint32_t
			{
				NoInheritance			= 0,
				MultipleInheritance		= 1 << 0,
				VirtualInheritance		= 1 << 1,
				AmbiguousInheritance	= 1 << 2,
			};

			uint32_t Signature;									// Always zero or one
			Attribute Attributes;								// Flags
			uint32_t NumBaseClasses;							// Number of classes in BaseClassArray
			RVA<BaseClassArray*> BaseClassArray;				// BaseClassArray
		};

		struct CompleteObjectLocator
		{
			enum class Signature : uint32_t
			{
				S_32 = 0,
				S_64 = 1,
			};

			Signature Signature;								// 32-bit zero, 64-bit one
			uint32_t Offset;									// Offset of this vtable in the complete class
			uint32_t CDOffset;									// Constructor displacement offset
			RVA<TypeDescriptor*> TypeDescriptor;				// TypeDescriptor of the complete class
			RVA<ClassHierarchyDescriptor*> ClassDescriptor;		// Describes inheritance hierarchy
		};

		struct Info
		{
			uintptr_t VTableAddress;							// Address in .rdata section
			uintptr_t VTableOffset;								// Offset of this vtable in complete class (from top)
			uint64_t VFunctionCount;							// Number of contiguous functions
			uintptr_t RvaDescriptor;							// Offset of raw type description
			std::string Name;									// Demangled
			std::string RawName;								// Mangled
			TypeDescriptor Descriptor;							//
			std::vector<std::string> HierarchyClasses;			//
		};

		using iterator = std::unordered_map<size_t, Info>::iterator;
		using const_iterator = std::unordered_map<size_t, Info>::const_iterator;

		constexpr TRTTIManager() = default;
		virtual ~TRTTIManager();

		virtual void Initialize() noexcept;
		virtual void Shutdown() noexcept;

		virtual void Dump(const char* a_filename) const noexcept;
		virtual void Dump(const std::string& a_filename) const noexcept;

		[[nodiscard]] virtual const Info* Find(const std::string_view& a_name) const noexcept;
		[[nodiscard]] virtual const Info* Find(const uintptr_t a_address) const noexcept;

		[[nodiscard]] inline iterator begin() noexcept { return srtti_data.begin(); }
		[[nodiscard]] inline iterator end() noexcept { return srtti_data.end(); }
		[[nodiscard]] inline const_iterator cbegin() const noexcept { return srtti_data.cbegin(); }
		[[nodiscard]] inline const_iterator cend() const noexcept { return srtti_data.cend(); }
		[[nodiscard]] inline size_t size() const noexcept { return srtti_data.size(); }
		[[nodiscard]] inline uintptr_t BaseAddress() const noexcept { return base; }
	private:
		uintptr_t base{ 0 };

		std::unique_ptr<uint8_t[]> psegtext{};
		std::unique_ptr<uint8_t[]> psegdata{};
		std::unique_ptr<uint8_t[]> psegrdata{};
		TProcessUtil::SegmentInfo segtext{};
		TProcessUtil::SegmentInfo segdata{};
		TProcessUtil::SegmentInfo segrdata{};
		std::unordered_map<size_t, Info> srtti_data{};

		[[nodiscard]] bool IsWithinDATA(uintptr_t addr) const noexcept;
		[[nodiscard]] bool IsWithinRDATA(uintptr_t addr) const noexcept;
		[[nodiscard]] bool IsWithinCODE(uintptr_t addr) const noexcept;
		[[nodiscard]] bool IsValidCOL(CompleteObjectLocator* Locator) const noexcept;
		[[nodiscard]] uint32_t GetCountVFunc(uintptr_t addr) const noexcept;
	};

	class TRTTIManagerScope
	{
		TRTTIManagerScope(TRTTIManagerScope&& a_rhs) = delete;
		TRTTIManagerScope(const TRTTIManagerScope& a_rhs) = delete;
		TRTTIManagerScope& operator=(TRTTIManagerScope&& a_rhs) = delete;
		TRTTIManagerScope& operator=(const TRTTIManagerScope& a_rhs) = delete;
	public:
		TRTTIManagerScope() { TRTTIManager::GetSingleton()->Initialize(); }
		~TRTTIManagerScope() { TRTTIManager::GetSingleton()->Shutdown(); }
	};


}