#include <YYToolkit/YYTK_Shared.hpp>

#include <fstream>
#include <vector>

#include "imgui.h"

using namespace Aurie;
using namespace YYTK;

#include "Hooks.h"
#include "ImguiWindow.h"
#include "tabs/ObjectTab.h"
#include "tabs/AiTab.h"

extern YYTKInterface *yytk = nullptr;

void FrameCallback(FWFrame &FrameContext)
{
	UNREFERENCED_PARAMETER(FrameContext);

	static bool window_exists = false;
	if (!window_exists)
	{
		ImguiCreateWindow();
		window_exists = true;
	}

	if (!ImguiFrameSetup())
	{
		ObjectTab();
		AiTab();

		ImguiFrameEnd();
	}

	// remove unfocus low fps.
	yytk->CallBuiltin("game_set_speed", {RValue(30), RValue(0)});
}

void DoHooks()
{
	// hook requests
	AiHooks();
	// make hooks
	CreateHooks();
}

EXPORTED AurieStatus ModuleInitialize(
		IN AurieModule *Module,
		IN const fs::path &ModulePath)
{
	UNREFERENCED_PARAMETER(ModulePath);

	AurieStatus last_status = AURIE_SUCCESS;

	yytk = YYTK::GetInterface();

	if (!yytk)
		return AURIE_MODULE_DEPENDENCY_NOT_RESOLVED;

	last_status = yytk->CreateCallback(
			Module,
			EVENT_FRAME,
			FrameCallback,
			0);

	if (!AurieSuccess(last_status))
	{
		DbgPrintEx(LOG_SEVERITY_ERROR, "[InfoWindow] - Failed to register frame callback!");
	}

	DoHooks();

	DbgPrintEx(LOG_SEVERITY_DEBUG, "[InfoWindow] - Hello from PluginEntry!");

	return AURIE_SUCCESS;
}
