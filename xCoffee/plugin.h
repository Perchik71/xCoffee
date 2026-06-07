#pragma once

#include "pluginmain.h"
#include "singleton.h"

namespace xCoffee
{
	class TPlugin :
		public TSingleton<TPlugin>
	{
		int handle{ -1 };
		HWND hwndDlg{ nullptr };
		int hMenu{ -1 };
		int hMenuDisasm{ -1 };
		int hMenuDump{ -1 };
		int hMenuStack{ -1 };
		int hMenuGraph{ -1 };
		int hMenuMemmap{ -1 };
		int hMenuSymmod{ -1 };

		int hMenuPlugin_ImportExportNames{ -1 };
		int hMenuPlugin_RTTI{ -1 };

		static void MenuEntry_Handler(CBTYPE cbType, void* callbackInfo) noexcept;

		void UnsafeCreateFakePDB();

		static constexpr auto HASH_MAGIC = 0x59120345ul;
		static constexpr auto VERSION = 0x1ul;

		struct FileHeaderBin
		{
			uint32_t magic{ HASH_MAGIC };
			uint32_t version{ VERSION };
			uint64_t count{ 0 };
			uint64_t filesize{ 0 };
		};

	public:
		constexpr static auto PLUGIN_NAME = __PLUGIN_NAME;
		constexpr static auto PLUGIN_VERSION = 1;

		TPlugin() = default;
		virtual ~TPlugin() = default;

		virtual bool Init(PLUG_INITSTRUCT* a_initStruct);
		virtual void InitGUI(PLUG_SETUPSTRUCT* a_setupStruct) noexcept;
		virtual void Shutdown() noexcept;

		virtual void ImportNames();
		virtual void ExportNames();
		virtual void CreateSignature();
		virtual void CreateFakePDB();
		virtual void RTTIDump();
		virtual void RTTIApply();
		virtual void RTTIExportName();
		virtual void RTTIImportName();
	};
}