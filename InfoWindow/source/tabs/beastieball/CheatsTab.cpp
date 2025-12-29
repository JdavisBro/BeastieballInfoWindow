#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../ModuleMain.h"

#include "CheatsTab.h"

struct Vec2
{
  double x;
  double y;
};

void TeleportToPosition(Vec2 position)
{
  RValue objPlayer_asset = yytk->CallBuiltin("asset_get_index", {"objPlayer"});
  RValue player = yytk->CallBuiltin("instance_find", {objPlayer_asset, RValue(0)});
  double z = yytk->CallGameScript("gml_Script_collider_gravity_raycast", {position.x, position.y}).ToDouble();
  yytk->CallBuiltin("variable_instance_set", {player, "x", position.x});
  yytk->CallBuiltin("variable_instance_set", {player, "y", position.y});
  yytk->CallBuiltin("variable_instance_set", {player, "z", z});
}

bool on_level_load_go = false;
Vec2 on_level_load_go_to;

RValue FindLevel(double x, double y)
{
  std::vector<RValue> stumps = yytk->CallBuiltin("variable_global_get", {"world_data"})["level_stumps_array"].ToVector();
  RValue fallback;
  for (RValue level : stumps)
  {
    if (
        level["world_layer"].ToDouble() == 0 && !level["map_hidden"].ToBoolean() &&
        level["world_x1"].ToDouble() <= x && level["world_x2"].ToDouble() >= x &&
        level["world_y1"].ToDouble() <= y && level["world_y2"].ToDouble() >= y)
    {
      if (level["name"].ToString() != "ocean")
        return level;
      fallback = level;
    }
  }
  return fallback;
}

void TeleportToMapWorldPosition(RValue &game, const char *x_key, const char *y_key, double x_offset = 0, double y_offset = 0)
{
  RValue map = yytk->CallBuiltin("variable_instance_get", {game, "mn_map"});
  bool menu_open = yytk->CallBuiltin("variable_global_get", {"menu_open"}).ToBoolean();
  RValue open_menu = yytk->CallBuiltin("variable_global_get", {"menu_tab_open"});
  if (!menu_open || map["name"].ToString() != open_menu["name"].ToString())
    return;
  double zoom = map["zoom"].ToDouble();
  double x = map[x_key].ToDouble() + (x_offset / zoom);
  double y = map[y_key].ToDouble() + (y_offset / zoom);
  RValue level = FindLevel(x, y);
  if (level.IsUndefined())
    return;
  std::string target_level = level["name"].ToString();
  on_level_load_go_to = {x, y};
  map["player_x"] = x;
  map["player_y"] = y;
  if (target_level == yytk->CallBuiltin("variable_instance_get", {game, "level_id"}).ToString())
  {
    TeleportToPosition(on_level_load_go_to);
    return;
  }
  on_level_load_go = true;
  yytk->CallGameScript("gml_Script_level_goto", {level["name"]});
}

void SaveTable(int &slot)
{
  ImGui::BeginTable("savetable", 8, ImGuiTableFlags_Borders);
  ImGui::TableSetupColumn("Index", ImGuiTableColumnFlags_WidthFixed);
  ImGui::TableSetupColumn("Player Name");
  ImGui::TableSetupColumn("Location");
  ImGui::TableSetupColumn("Beastie 1");
  ImGui::TableSetupColumn("Beastie 2");
  ImGui::TableSetupColumn("Beastie 3");
  ImGui::TableSetupColumn("Beastie 4");
  ImGui::TableSetupColumn("Beastie 5");
  ImGui::TableHeadersRow();
  RValue stumps = yytk->CallBuiltin("variable_global_get", {"savedata_stumps"});
  int stump_count = yytk->CallBuiltin("array_length", {stumps}).ToInt32();
  for (int i = 0; i <= stump_count; i++)
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    if (ImGui::Selectable(std::to_string(i).c_str(), i == slot, ImGuiSelectableFlags_SpanAllColumns))
      slot = i;
    if (i < stump_count && !stumps[i].IsUndefined())
    {
      RValue stump = stumps[i];
      ImGui::TableNextColumn();
      ImGui::Text(stump["name"].ToCString());
      ImGui::TableNextColumn();
      ImGui::Text(yytk->CallGameScript("gml_Script_level_location_name", {stump["level"]}).ToCString());
      std::vector<RValue> team = stump["team_party"].ToVector();
      for (RValue beastie : team)
      {
        ImGui::TableNextColumn();
        ImGui::Text(std::format("{}#{}", beastie["name"].ToString(), beastie["number"].ToString()).c_str());
      }
    }
  }
  ImGui::EndTable();
}

bool teleport_on_middle_click = true;
bool infinite_jumps = false;

void CheatsTab()
{
  if (on_level_load_go)
  {
    TeleportToPosition(on_level_load_go_to);
    on_level_load_go = false;
  }
  RValue objGame_asset = yytk->CallBuiltin("asset_get_index", {"objGame"});
  RValue game = yytk->CallBuiltin("instance_find", {objGame_asset, RValue(0)});
  if (teleport_on_middle_click && yytk->CallBuiltin("mouse_check_button_pressed", {3}))
    TeleportToMapWorldPosition(game, "mouse_world_x", "mouse_world_y", 380.0);

  bool debug_menu = yytk->CallBuiltin("variable_instance_get", {game, "debug_console"}).ToBoolean();
  if (yytk->CallBuiltin("keyboard_check_pressed", {192}).ToBoolean())
    yytk->CallBuiltin("variable_instance_set", {game, "debug_console", !debug_menu});

  if (infinite_jumps) {
    if (yytk->CallBuiltin("keyboard_check_pressed", {RValue(32.0)}).ToBoolean()) {
      RValue player_asset = yytk->CallBuiltin("asset_get_index", {RValue("objPlayer")});
      RValue player_instance = yytk->CallBuiltin("instance_find", {player_asset, RValue(0)});
      RValue jump_speed = yytk->CallBuiltin("variable_instance_get", {player_instance, RValue("jump_speed")});
      yytk->CallBuiltin("variable_instance_set", {player_instance, RValue("z_speed"), jump_speed});
    }
  }

  if (!ImGui::Begin("Cheats"))
  {
    ImGui::End();
    return;
  }

  bool debug_menu_new = debug_menu;
  ImGui::Checkbox("Debug Menu", &debug_menu_new);
  if (debug_menu != debug_menu_new)
    yytk->CallBuiltin("variable_instance_set", {game, "debug_console", debug_menu_new});

  ImGui::Checkbox("Infinite Jumps", &infinite_jumps);

  if (ImGui::Button("Teleport to Map Center"))
    TeleportToMapWorldPosition(game, "world_x", "world_y");
  ImGui::Checkbox("Teleport to mouse on Middle Click", &teleport_on_middle_click);
  ImGui::Text("Save Files:");
  ImGui::BeginChild("saves", ImVec2(0, 0), ImGuiChildFlags_Borders);
  int slot = yytk->CallBuiltin("variable_global_exists", {"SAVE_SLOT"}) ? yytk->CallBuiltin("variable_global_get", {"SAVE_SLOT"}).ToInt32() : 0;
  int prev_slot = slot;
  ImGui::SetNextItemWidth(150);
  ImGui::InputInt("Save Slot", &slot, 1, 1);
  ImGui::SameLine();
  if (ImGui::Button("Save"))
    yytk->CallGameScript("gml_Script_savedata_save", {});
  ImGui::SameLine();
  if (ImGui::Button("Load"))
  {
    yytk->CallGameScript("gml_Script_savedata_load", {});
    yytk->CallGameScript("gml_Script_data_load_level", {});
    yytk->CallGameScript("gml_Script_menu_level_out_all", {});
  }
  SaveTable(slot);
  if (prev_slot != slot)
    yytk->CallBuiltin("variable_global_set", {"SAVE_SLOT", slot});

  ImGui::EndChild();

  ImGui::End();
}
