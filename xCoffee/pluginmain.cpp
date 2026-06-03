#include "pluginmain.h"
#include "plugin.h"

HINSTANCE g_LocalDllHandle{ nullptr };

PLUG_EXPORT bool pluginit(PLUG_INITSTRUCT* initStruct)
{
    return xCoffee::TPlugin::GetSingleton()->Init(initStruct);
}

PLUG_EXPORT bool plugstop()
{
    xCoffee::TPlugin::GetSingleton()->Shutdown();
    return true;
}

PLUG_EXPORT void plugsetup(PLUG_SETUPSTRUCT* setupStruct)
{
    xCoffee::TPlugin::GetSingleton()->InitGUI(setupStruct);
}

BOOL APIENTRY DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        g_LocalDllHandle = hinstDLL;

    return TRUE;
}