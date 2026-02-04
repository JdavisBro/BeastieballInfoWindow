#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../ModuleMain.h"
#include "../../Hooks.h"
#include "../../Utils.h"

#include "MatchTab.h"

#include "AiTab.h"

namespace MatchTab {

struct GameplayState
{
  std::string name = "";
  std::map<std::string, RValue> normal_values = {};
  std::map<std::string, RValue> elephant_values = {};
};

std::map<int, GameplayState> states = {};

const char *excluded_names[] = {
    "command_scribble", "game_triggers",
    "level", "crowd",
    "dread_particles", "emotes_frame",
    "net_data_queue", "net_model",
    "network_log", "parent_struct",
    "phase_titles", "quake_models",
    "reverse_fx", "rhythm_fx",
    "shockwave_fx", "trap_fx",
    "ai_choicegraph", "ai_selecting"};

bool IsExcludedName(std::string &name)
{
  for (const char *check_name : excluded_names)
  {
    if (name == check_name)
      return true;
  }
  return false;
}

GameplayState SaveState(RValue &game)
{
  AiTab::Undo(game);
  GameplayState state;
  state.name = std::format("Turn {:03d}", Utils::InstanceGet(game, "round_count").ToInt32());
  std::vector<RValue> names = yytk->CallBuiltin("variable_instance_get_names", {game}).ToVector();
  for (RValue key_rv : names)
  {
    std::string key = key_rv.ToString();
    if (IsExcludedName(key))
      continue;
    RValue value = Utils::InstanceGet(game, key_rv);
    switch (value.m_Kind)
    {
    case VALUE_INT64:
    case VALUE_INT32:
    case VALUE_REAL:
    case VALUE_STRING:
    case VALUE_BOOL:
      state.normal_values[key] = value;
      break;
    case VALUE_OBJECT:
      if (yytk->CallBuiltin("is_method", {value}))
        continue;
    case VALUE_ARRAY:
      state.elephant_values[key] = yytk->CallGameScript("gml_Script_ElephantExportString", {value});
      break;
    default:
      continue;
    }
  }
  return state;
}

bool auto_create_ai_after_load = true;

void LoadState(RValue &game, GameplayState &state)
{
  AiTab::Undo(game);
  for (auto data : state.normal_values)
  {
    Utils::InstanceSet(game, data.first, data.second);
  }
  for (auto data : state.elephant_values)
  {
    RValue value = yytk->CallGameScript("gml_Script_ElephantImportString", {data.second});
    Utils::InstanceSet(game, data.first, value);
  }
  Utils::InstanceSet(game, "ai_choicegraph", RValue());
  Utils::InstanceSet(game, "ai_selecting", -1);
  if (auto_create_ai_after_load)
    AiTab::MakeAi(game); // from AiTab
}

int selected = 0;
bool auto_save_enabled = true;

void OtherSettings()
{
  ImGui::Checkbox("Auto Save on New Rounds", &auto_save_enabled);
  ImGui::SameLine();
  ImGui::Checkbox("Create AI after load", &auto_create_ai_after_load);
}

void DoGame(RValue &game, bool can_save)
{
  if (ImGui::Button("Load") && states.contains(selected))
    LoadState(game, states[selected]);
  ImGui::SameLine();
  if (ImGui::Button("Save") && can_save)
  {
    int i = 1000000;
    while (states.contains(i))
      i++;
    states[i] = SaveState(game);
    selected = i;
  }
  ImGui::SameLine();
  if (ImGui::Button("Delete"))
    states.erase(selected);
  OtherSettings();
  if (states.contains(selected))
    ImGui::InputText("Name", states.contains(selected) ? &states[selected].name : nullptr);
  ImGui::BeginChild("states", ImVec2(0, 0), ImGuiChildFlags_Borders);
  std::vector<int> sorted_states = {};
  for (auto pair : states)
    sorted_states.push_back(pair.first);
  std::sort(sorted_states.begin(), sorted_states.end(), [](int a, int b)
    { return states[a].name < states[b].name; });
  for (int state_i : sorted_states)
  {
    if (ImGui::Selectable(std::format("{}###Index{}", states[state_i].name, state_i).c_str(), selected == state_i))
      selected = state_i;
  }
  ImGui::EndChild();
}

bool DoAutoSave(RValue &game)
{
  if (!game.ToBoolean())
    return false;
  int round = Utils::InstanceGet(game, "round_count").ToInt32();
  bool scene_playing = Utils::GlobalGet("SCENE_PLAYING").ToBoolean();
  int menu_input_freeze = Utils::InstanceGet(game, "menu_input_freeze").ToInt32();
  int selection_mode = Utils::InstanceGet(game, "selection_mode").ToInt32();
  bool can_save = !scene_playing && menu_input_freeze == 0 && selection_mode == 0;
  if (can_save && auto_save_enabled && !states.contains(round))
    states[round] = SaveState(game);
  return can_save;
}

void MatchTab(bool *open)
{
  RValue game = Utils::GlobalGet("GAME_ACTIVE");
  bool can_save = DoAutoSave(game);
  if (!ImGui::Begin("Match", open))
  {
    ImGui::End();
    return;
  }
  if (!game.ToBoolean())
  {
    ImGui::Text("No Active Match");
    OtherSettings();
    states.clear();
  }
  else
    DoGame(game, can_save);

  ImGui::End();
}

}