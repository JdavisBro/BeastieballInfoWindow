#include <YYToolkit/YYTK_Shared.hpp>

#include <fstream>
#include <vector>

#include "imgui.h"
#include "imgui_internal.h"

using namespace Aurie;
using namespace YYTK;

#include "Hooks.h"
#include "ImguiWindow.h"
#include "Utils.h"
#include "Storage.h"
#include "tabs/ObjectTab.h"
#include "tabs/beastieball/AiTab.h"
#include "tabs/beastieball/MatchTab.h"
#include "tabs/beastieball/PartyTab.h"
#include "tabs/beastieball/CheatsTab.h"

extern YYTKInterface *yytk = nullptr;

bool is_beastieball = false;

void BeastieballCheck()
{
	is_beastieball = Utils::GlobalExists("sprite_beastie_ball_impact");
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
		AiTab::AiHooks();
		CheatsTab::CheatsHooks();
	}
	// make hooks
	CreateHooks();
	hooks_done = true;
}

void DemoWindow(bool *open)
{
	ImGui::ShowDemoWindow(open);
}

struct TabInfo
{
	const char *name;
	bool open;
	bool beastieball;
	std::function<void(bool *)> draw;
	const char *storage_category = nullptr;
	std::function<void()> store = nullptr;
};

TabInfo tabs[] = {
	{"ImGui Demo", false, false, DemoWindow},
	{"Object Tab", true, false, ObjectTab::ObjectTab, "ObjectTab", ObjectTab::Store},
	{"AI Tab", true, true, AiTab::AiTab, "AiTab", AiTab::Store},
	{"Match Tab", true, true, MatchTab::MatchTab, "MatchTab", MatchTab::Store},
	{"Party Tab", true, true, PartyTab::PartyTab, "PartyTab", PartyTab::Store},
	{"Cheats Tab", true, true, CheatsTab::CheatsTab, "CheatsTab", CheatsTab::Store},
};
const int tab_count = sizeof(tabs) / sizeof(TabInfo);

void ReadStorage()
{
	Storage::LoadFile();
	for (int i = 0; i < tab_count; i++)
	{
		TabInfo &tab = tabs[i];
		if (tab.storage_category) Storage::category = tab.storage_category;
		if (tab.store) tab.store();
	}
	Storage::reading = false;
}

void PopupMenu(ImGuiID dockspace)
{
	bool showPopup = false;
	ImGuiDockNode *node = (ImGuiDockNode *)GImGui->DockContext.Nodes.GetVoidPtr(dockspace);
	if (ImGui::DockNodeBeginAmendTabBar(node))
	{
		showPopup = ImGui::TabItemButton("+", ImGuiTabItemFlags_Trailing);
		ImGui::DockNodeEndAmendTabBar();
	}
	else if (ImGui::BeginMainMenuBar())
	{
		showPopup = ImGui::MenuItem("Open Tabs", "Shift+A");
		ImGui::EndMainMenuBar();
	}
	if (showPopup)
		ImGui::OpenPopup("AddMenu");
	if (ImGui::BeginPopup("AddMenu"))
	{
		for (int i = 0; i < tab_count; i++)
		{
			TabInfo &tab = tabs[i];
			if (!is_beastieball && tab.beastieball)
				continue;
			ImGui::Checkbox(tab.name, &tab.open);
		}
		ImGui::Text("Settings");
		ImGui::Checkbox("Disable Save", &Storage::save_disbled);
		if (ImGui::Button("Reset Saved Data to Default")) {
			Storage::ResetToDefault();
			Storage::reading = true;
			ReadStorage();
		}
		ImGui::EndPopup();
	}
}

bool has_drawn = false;

std::filesystem::path save_dir = "mod_data/";

void CodeCallback(FWCodeEvent &Event)
{
	auto [Self, Other, Code, ArgCount, Arg] = Event.Arguments();
	Event.Call(Self, Other, Code, ArgCount, Arg);

	std::string name = Code->GetName();

	if (is_beastieball)
	{
		if (name != "gml_Object_objGame_Draw_0")
			return;
	}
	else
	{
		if (name.ends_with("Draw_0"))
		{ // draws first event after every Draw_0
			has_drawn = false;
			return;
		}
		else if (has_drawn || name.find("_Other_") != -1)
		{
			return;
		}
	}

	has_drawn = true;
	static bool window_exists = false;
	if (!window_exists)
	{
		if (!std::filesystem::is_directory(save_dir))
			std::filesystem::create_directory(save_dir);
		ReadStorage();
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
		Utils::Setup();
		BeastieballCheck();
		DoHooks();
	}

	ImGuiID dockspace;
	if (!ImguiFrameSetup(dockspace))
	{
		PopupMenu(dockspace);
		for (int i = 0; i < tab_count; i++)
		{
			TabInfo &tab = tabs[i];
			if (!tab.open || (!is_beastieball && tab.beastieball))
				continue;
			ImGui::SetNextWindowDockID(dockspace, ImGuiCond_FirstUseEver);
			tab.draw(&tab.open);
			if (tab.storage_category) Storage::category = tab.storage_category;
			if (tab.store) tab.store();
		}
		ImguiFrameEnd();
		Storage::SaveFile();
	}

	// remove unfocus low fps.
	if (is_beastieball)
	{
		RValue settings = Utils::GlobalGet("SETTINGS");
		if (settings.m_Kind != VALUE_UNDEFINED)
			yytk->CallBuiltin("game_set_speed", {settings["framerate"], 0});
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
