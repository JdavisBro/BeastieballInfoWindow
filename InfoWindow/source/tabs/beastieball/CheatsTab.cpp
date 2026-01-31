#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../ModuleMain.h"
#include "../../Hooks.h"

#include "CheatsTab.h"

struct Vec2
{
  double x;
  double y;
};

RValue GetObjectInstance(const char *object_name)
{
  RValue asset = yytk->CallBuiltin("asset_get_index", {object_name});
  return yytk->CallBuiltin("instance_find", {asset, RValue(0)});
}

void TeleportToPosition(Vec2 position)
{
  RValue player = GetObjectInstance("objPlayer");
  double z = yytk->CallGameScript("gml_Script_collider_gravity_raycast", {position.x, position.y}).ToDouble();
  yytk->CallBuiltin("variable_instance_set", {player, "x", position.x});
  yytk->CallBuiltin("variable_instance_set", {player, "y", position.y});
  yytk->CallBuiltin("variable_instance_set", {player, "z", z});
  yytk->CallBuiltin("variable_instance_set", {player, "z_last", z}); // used for camera position, not updated until moved
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
bool view_collision = false;
bool camera_always_follow_player = false;

void ReloadLevel(RValue &game)
{
  yytk->CallGameScript("gml_Script_data_update_player_pos", {});
  yytk->CallGameScript("gml_Script_SceneClear", {});
  RValue level_id = yytk->CallBuiltin("variable_instance_get", {game, "level_id"});
  yytk->CallGameScript("gml_Script_level_goto", {level_id, RValue(true)});
}

void SetGroupRenders(RValue &group)
{
  RValue default_renders = yytk->CallBuiltin("variable_instance_get", {group, RValue("DEFAULT_RENDERS")});
  RValue collider = group["collider"];
  if (default_renders.IsUndefined())
  {
    RValue renders = group["renders"];
    yytk->CallBuiltin("variable_instance_set", {group, RValue("DEFAULT_RENDERS"), renders});
    default_renders = renders;
  }
  group["renders"] = view_collision ? collider : default_renders;
}

void ToggleCollision(RValue &game)
{
  RValue browser = yytk->CallBuiltin("variable_global_get", {RValue("__browser")});
  std::map<std::string, RValue> models = browser["content"]["models"].ToMap();
  for (auto model_pair : models)
  {
    auto [name, browser_model] = model_pair;
    if (!browser_model["loaded"].ToBoolean())
      continue;
    RValue model = browser_model["model"];
    std::vector<RValue> model_groups = model["groups_array"].ToVector();
    for (RValue group : model_groups)
    {
      SetGroupRenders(group);
    }
  }
  ReloadLevel(game);
}

PFUNC_YYGMLScript shapeTriangulateOriginal = nullptr;
RValue &ShapeTriangulate(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  if (view_collision)
  {
    RValue shape = Self->ToRValue();
    RValue solid = shape["solid"];
    RValue water = shape["water"];
    ;
    shape["visible"] = solid.ToBoolean() || water.ToBoolean();
  }
  shapeTriangulateOriginal(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

PFUNC_YYGMLScript groupAddToOriginal = nullptr;
RValue &GroupAddTo(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  if (view_collision)
  {
    RValue group = Self->ToRValue();
    SetGroupRenders(group);
  }
  groupAddToOriginal(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

PFUNC_YYGMLScript buildBakedModelsOriginal = nullptr;
RValue &BuildBakedModels(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  if (view_collision)
  {
    std::vector<RValue> models = yytk->CallBuiltin("variable_instance_get", {Self, RValue("models_array")}).ToVector();
    for (RValue model : models)
    {
      model["effect_layer"] = RValue(0);
    }
  }
  buildBakedModelsOriginal(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

bool do_pause_buffering = true;

PFUNC_YYGMLScript buttonlistPressedOriginal = nullptr;
RValue &ButtonlistPressed(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  if (do_pause_buffering && yytk->CallBuiltin("keyboard_check", {161}).ToBoolean())
  {
    RValue pause_buttons = yytk->CallBuiltin("variable_global_get", {"pause_buttons"});
    if (pause_buttons.m_Object == Args[0]->m_Object)
    {
      ReturnValue = 1.0;
      return ReturnValue;
    }
  }
  buttonlistPressedOriginal(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

void CheatsHooks()
{
  RequestHook("gml_Script_triangulate@", "class_shape", "IW shapeTriangulate", ShapeTriangulate, reinterpret_cast<PVOID *>(&shapeTriangulateOriginal));
  RequestHook("gml_Script_build_baked_models@", "class_level", "IW build_baked_models", BuildBakedModels, reinterpret_cast<PVOID *>(&buildBakedModelsOriginal));
  RequestHook("gml_Script_AddTo@", "DotobjClassGroup", "IW groupaddto", GroupAddTo, reinterpret_cast<PVOID *>(&groupAddToOriginal));
  RequestHook(NULL, "gml_Script_buttonlist_pressed", "IW buttonlistpressed", ButtonlistPressed, reinterpret_cast<PVOID *>(&buttonlistPressedOriginal));
}

const char *player_vars[] = {
    "x",
    "y",
    "z",
    "hSpeed",
    "vSpeed",
    "z_speed",
    "GROUNDED",
    "jump_holding",
};
const int player_vars_count = sizeof(player_vars) / sizeof(const char *);

void DisplayPlayerVars(RValue &player)
{
  ImGui::BeginTable("PlayerVarTable", player_vars_count, ImGuiTableFlags_Borders);
  for (int i = 0; i < player_vars_count; i++)
  {
    ImGui::TableSetupColumn(player_vars[i]);
  }
  ImGui::TableHeadersRow();
  ImGui::TableNextRow();
  for (int i = 0; i < player_vars_count; i++)
  {
    ImGui::TableNextColumn();
    RValue var = yytk->CallBuiltin("variable_instance_get", {player, player_vars[i]});
    bool is_bool = var.m_Kind == VALUE_BOOL;
    ImGui::Text(is_bool ? var.ToBoolean() ? "true" : "false" : var.ToString().c_str());
  }
  ImGui::EndTable();
}

void CheatsTab(bool *open)
{
  if (on_level_load_go)
  {
    TeleportToPosition(on_level_load_go_to);
    on_level_load_go = false;
  }
  RValue game = GetObjectInstance("objGame");
  if (teleport_on_middle_click && yytk->CallBuiltin("mouse_check_button_pressed", {3}))
    TeleportToMapWorldPosition(game, "mouse_world_x", "mouse_world_y", 380.0);

  bool debug_menu = yytk->CallBuiltin("variable_instance_get", {game, "debug_console"}).ToBoolean();
  if (yytk->CallBuiltin("keyboard_check_pressed", {192}).ToBoolean())
    yytk->CallBuiltin("variable_instance_set", {game, "debug_console", !debug_menu});

  RValue player = GetObjectInstance("objPlayer");
  if (infinite_jumps)
  {
    if (yytk->CallBuiltin("keyboard_check_pressed", {RValue(32.0)}).ToBoolean())
    {
      RValue jump_speed = yytk->CallBuiltin("variable_instance_get", {player, RValue("jump_speed")});
      yytk->CallBuiltin("variable_instance_set", {player, RValue("z_speed"), jump_speed});
    }
  }

  if (camera_always_follow_player)
  {
    RValue player_z = yytk->CallBuiltin("variable_instance_get", {player, RValue("z")});
    yytk->CallBuiltin("variable_instance_set", {player, RValue("z_last"), player_z});
  }

  if (!ImGui::Begin("Cheats", open, ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::End();
    return;
  }

  if (ImGui::Checkbox("Debug Menu", &debug_menu))
    yytk->CallBuiltin("variable_instance_set", {game, "debug_console", debug_menu});
  ImGui::Checkbox("Infinite Jumps", &infinite_jumps);
  ImGui::Checkbox("Camera Always Follow Player", &camera_always_follow_player);
  ImGui::Checkbox("Pause Buffer when RShift held.", &do_pause_buffering);

  if (ImGui::Checkbox("View Collision Only", &view_collision))
    ToggleCollision(game);

  if (ImGui::Button("Reload Level"))
    ReloadLevel(game);

  if (ImGui::Button("Teleport to Map Center"))
    TeleportToMapWorldPosition(game, "world_x", "world_y");
  ImGui::Checkbox("Teleport to mouse on Middle Click", &teleport_on_middle_click);

  ImGui::Text("Player Variables");
  if (player.ToBoolean())
  {
    DisplayPlayerVars(player);
  }

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
