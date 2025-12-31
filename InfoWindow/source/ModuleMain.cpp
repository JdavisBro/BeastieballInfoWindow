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
#include "tabs/beastieball/PartyTab.h"
#include "tabs/beastieball/CheatsTab.h"

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

bool hooks_done = false;

void DoHooks()
{
	// hook requests
	if (is_beastieball)
	{
		AiHooks();
		CheatsHooks();
	}
	// make hooks
	CreateHooks();
	hooks_done = true;
}

inline void SetNextDock(ImGuiID dockspace)
{
	ImGui::SetNextWindowDockID(dockspace, ImGuiCond_FirstUseEver);
}


void CodeCallback(FWCodeEvent &Event)
{
	auto [Self, Other, Code, ArgCount, Arg] = Event.Arguments();
	Event.Call(Self, Other, Code, ArgCount, Arg);
	
	std::string name = Code->GetName();

	if (name != "gml_Object_objGame_Draw_0")
		return;

	static bool window_exists = false;
	if (!window_exists)
	{
		if (!ImguiCreateWindow())
		{
			window_exists = true;
			DbgPrint("[InfoWindow] Window Created");
		}
		else
		{
			DbgPrint("[InfoWindow] Window Creation Failed.");
		}
	}
	if (!hooks_done)
	{
	DbgPrint("[InfoWindow] Hooks Setup");	
		BeastieballCheck();
		DoHooks();
	}

	ImGuiID dockspace;
	DbgPrint("[InfoWindow] Frame Setup");
	if (!ImguiFrameSetup(dockspace))
	{
		SetNextDock(dockspace);
		DbgPrint("[InfoWindow] Object Tab");
		ObjectTab();
		if (is_beastieball)
		{
			SetNextDock(dockspace);
			DbgPrint("[InfoWindow] Ai Tab");
			AiTab();
			SetNextDock(dockspace);
			DbgPrint("[InfoWindow] Party Tab");
			PartyTab();
			SetNextDock(dockspace);
			CheatsTab();
		}
		
		DbgPrint("[InfoWindow] Frame End");
		ImguiFrameEnd();
	}

	// remove unfocus low fps.
	if (is_beastieball)
	{
		DbgPrint("[InfoWindow] Set Speed");
		RValue settings = yytk->CallBuiltin("variable_global_get", {RValue("SETTINGS")});
		yytk->CallBuiltin("game_set_speed", {settings["framerate"], RValue(0)});
	}
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
			EVENT_OBJECT_CALL,
			CodeCallback,
			0);

	if (!AurieSuccess(last_status))
	{
		DbgPrintEx(LOG_SEVERITY_ERROR, "[InfoWindow] - Failed to register code callback!");
	}

	DbgPrintEx(LOG_SEVERITY_DEBUG, "[InfoWindow] - Hello from PluginEntry!");

	return AURIE_SUCCESS;
}
