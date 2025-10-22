#include <YYToolkit/YYTK_Shared.hpp>

#include <fstream>
#include <vector>

#include "imgui.h"

using namespace Aurie;
using namespace YYTK;

#include "Hooks.h"
#include "ImguiWindow.h"
#include "tabs/ObjectTab.h"
#include "tabs/beastieball/AiTab.h"
#include "tabs/beastieball/PartyTab.h";

extern YYTKInterface *yytk = nullptr;

bool is_beastieball = false;

void BeastieballCheck()
{
	is_beastieball = yytk->CallBuiltin("variable_global_exists", {RValue("sprite_beastie_ball_impact")}).ToBoolean();
	if (is_beastieball)
	{
		DbgPrint("Beastieball!");
	}
	else
	{
		DbgPrint("not beastieball..");
	}
}

void DoHooks()
{
	// hook requests
	if (is_beastieball)
	{
		AiHooks();
	}
	// make hooks
	CreateHooks();
}

inline void SetNextDock(ImGuiID dockspace)
{
	ImGui::SetNextWindowDockID(dockspace, ImGuiCond_FirstUseEver);
}

void FrameCallback(FWFrame &FrameContext)
{
	UNREFERENCED_PARAMETER(FrameContext);

	static bool window_exists = false;
	if (!window_exists)
	{
		ImguiCreateWindow();
		BeastieballCheck();
		DoHooks();
		window_exists = true;
	}

	ImGuiID dockspace;
	if (!ImguiFrameSetup(dockspace))
	{
		SetNextDock(dockspace);
		ObjectTab();
		if (is_beastieball)
		{
			SetNextDock(dockspace);
			AiTab();
			SetNextDock(dockspace);
			PartyTab();
		}

		ImguiFrameEnd();
	}

	// remove unfocus low fps.
	if (is_beastieball)
		yytk->CallBuiltin("game_set_speed", {RValue(30), RValue(0)});
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

	DbgPrintEx(LOG_SEVERITY_DEBUG, "[InfoWindow] - Hello from PluginEntry!");

	return AURIE_SUCCESS;
}
