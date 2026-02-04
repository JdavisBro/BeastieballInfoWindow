#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../ModuleMain.h"
#include "../../Hooks.h"
#include "../../Utils.h"

#include <numbers>

#include "CheatsTab.h"

namespace CheatsTab {

struct Vec2
{
  double x;
  double y;
};

void TeleportToPosition(RValue &player, Vec2 position)
{
  double z = yytk->CallGameScript("gml_Script_collider_gravity_raycast", {position.x, position.y}).ToDouble();
  Utils::InstanceSet(player, "x", position.x);
  Utils::InstanceSet(player, "y", position.y);
  Utils::InstanceSet(player, "z", z);
  Utils::InstanceSet(player, "z_last", z); // used for camera position, not updated until moved
}

bool on_level_load_go = false;
Vec2 on_level_load_go_to;

RValue FindLevel(double x, double y)
{
  std::vector<RValue> stumps = Utils::GlobalGet("world_data")["level_stumps_array"].ToVector();
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

void TeleportToMapWorldPosition(RValue &game, RValue &player, const char *x_key, const char *y_key, double x_offset = 0, double y_offset = 0)
{
  RValue map = Utils::InstanceGet(game, "mn_map");
  bool menu_open = Utils::GlobalGet("menu_open").ToBoolean();
  RValue open_menu = Utils::GlobalGet("menu_tab_open");
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
  if (target_level == Utils::InstanceGet(game, "level_id").ToString())
  {
    TeleportToPosition(player, on_level_load_go_to);
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
  RValue stumps = Utils::GlobalGet("savedata_stumps");
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
  RValue level_id = Utils::InstanceGet(game, "level_id");
  yytk->CallGameScript("gml_Script_level_goto", {level_id, true});
}

void SetGroupRenders(RValue &group)
{
  RValue default_renders = Utils::InstanceGet(group, "DEFAULT_RENDERS");
  RValue collider = group["collider"];
  if (default_renders.IsUndefined())
  {
    RValue renders = group["renders"];
    Utils::InstanceSet(group, "DEFAULT_RENDERS", renders);
    default_renders = renders;
  }
  group["renders"] = view_collision ? collider : default_renders;
}

void ToggleCollision(RValue &game)
{
  RValue browser = Utils::GlobalGet("__browser");
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
    std::vector<RValue> models = Utils::InstanceGet(Self, "models_array").ToVector();
    for (RValue model : models)
    {
      model["effect_layer"] = 0;
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
    RValue pause_buttons = Utils::GlobalGet("pause_buttons");
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
    RValue var = Utils::InstanceGet(player, player_vars[i]);
    bool is_bool = var.m_Kind == VALUE_BOOL;
    ImGui::Text(is_bool ? var.ToBoolean() ? "true" : "false" : var.ToString().c_str());
  }
  ImGui::EndTable();
}

bool debug_shortcuts = true;

void DoDebugChildren(RValue &thing)
{
  if (!thing.IsStruct())
    return; // sometimes they are numbers
  if (Utils::InstanceExists(thing, "action"))
  {
    // is button
    bool pressed = false;
    std::vector<RValue> keys = thing["keys"].ToVector();
    for (RValue &key : keys)
    {
      if (yytk->CallBuiltin("keyboard_check_pressed", {key}).ToBoolean())
      {
        pressed = true;
      }
      else if (!yytk->CallBuiltin("keyboard_check", {key}).ToBoolean())
      {
        pressed = false;
        break;
      }
    }
    if (pressed)
      yytk->CallBuiltin("method_call", {thing["action"]});
  }
  else
  {
    RValue rv_type = thing["type"];
    int type = rv_type.IsStruct() ? 0 : rv_type.ToInt32();
    if (type != 1 && type != 2)
    {
      // is category
      std::vector<RValue> children = thing["buttons"].ToVector();
      for (RValue &child : children)
        DoDebugChildren(child);
    }
  }
}

void DoDebugShortcuts(RValue &game)
{
  if (debug_shortcuts && !Utils::ObjectInstanceExists("objInit"))
  {
    std::vector<RValue> stuff = Utils::InstanceGet(game, "debug_console_stuff").ToVector();
    for (RValue &thing : stuff)
      DoDebugChildren(thing);
  }
}

const int CYLINDER_SEGMENTS = 12;
const double CYLINDER_DIST = 2 / (double)CYLINDER_SEGMENTS;

const double CYLINDER_COLOR = 0xFFFFFF;
const double CYLINDER_ALPHA = 0.9;

bool draw_player_collision = false;

void DrawPlayerCollision(RValue &player)
{
  yytk->CallBuiltin("draw_set_color", {CYLINDER_COLOR});
  yytk->CallBuiltin("draw_set_alpha", {CYLINDER_ALPHA});
  double x = Utils::InstanceGet(player, "x").ToDouble();
  double y = Utils::InstanceGet(player, "y").ToDouble();
  double z = Utils::InstanceGet(player, "z").ToDouble();
  double height = Utils::InstanceGet(player, "height").ToDouble();
  double radius = Utils::InstanceGet(player, "collider_radius").ToDouble();
  bool grounded = Utils::InstanceGet(player, "GROUNDED").ToBoolean();
  double offset = Utils::InstanceGet(player, grounded ? "collider_z_offset" : "collider_z_offset_jump").ToDouble();
  z = z + offset;
  height = height - offset;
  for (double step = 0.0; step < 2.0; step += CYLINDER_DIST)
  {
    double cx = x + sin(step * std::numbers::pi) * radius;
    double cy = y + cos(step * std::numbers::pi) * radius;
    double cx2 = x + sin((step + CYLINDER_DIST) * std::numbers::pi) * radius;
    double cy2 = y + cos((step + CYLINDER_DIST) * std::numbers::pi) * radius;
    yytk->CallGameScript("gml_Script_draw_plane_3d", {cx, cy, z, cx2, cy2, z, cx, cy, z + height, cx2, cy2, z + height});
  }
  yytk->CallBuiltin("draw_set_alpha", {1.0});
}

bool keep_freecam = false;
int old_proj_mode = 0;

void KeepFreecam(const RValue &player)
{
  RValue scenemanager = Utils::GetObjectInstance("objSceneManager");
  bool freecam = Utils::GlobalGet("FREE_CAM").ToBoolean();
  int proj_mode = Utils::InstanceGet(scenemanager, "camera_projection_mode").ToInt32();
  if (proj_mode != 3) {
    old_proj_mode = proj_mode;
  }
  if (keep_freecam && !freecam) {
    RValue shot = Utils::InstanceGet(scenemanager, "shot_overworld");
    shot["look_x"] = Utils::InstanceGet(player, "x");
    shot["look_y"] = Utils::InstanceGet(player, "y");
    shot["look_z"] = Utils::InstanceGet(player, "z");
  }
  Utils::InstanceSet(scenemanager, "camera_projection_mode", (keep_freecam && !freecam) ? 3 : old_proj_mode);
}

void CheatsTab(bool *open)
{
  RValue player = Utils::GetObjectInstance("objPlayer");
  if (on_level_load_go)
  {
    TeleportToPosition(player, on_level_load_go_to);
    on_level_load_go = false;
  }
  RValue game = Utils::GetObjectInstance("objGame");
  if (teleport_on_middle_click && yytk->CallBuiltin("mouse_check_button_pressed", {3}))
    TeleportToMapWorldPosition(game, player, "mouse_world_x", "mouse_world_y", 380.0);
  DoDebugShortcuts(game);

  bool debug_menu = Utils::InstanceGet(game, "debug_console").ToBoolean();
  if (yytk->CallBuiltin("keyboard_check_pressed", {192}).ToBoolean())
    Utils::InstanceSet(game, "debug_console", !debug_menu);

  if (draw_player_collision && player.ToBoolean())
    DrawPlayerCollision(player);
  if (infinite_jumps)
  {
    if (yytk->CallBuiltin("keyboard_check_pressed", {32.0}).ToBoolean())
    {
      RValue jump_speed = Utils::InstanceGet(player, "jump_speed");
      Utils::InstanceSet(player, "z_speed", jump_speed);
    }
  }

  if (keep_freecam)
    KeepFreecam(player);

  if (camera_always_follow_player)
  {
    RValue player_z = Utils::InstanceGet(player, "z");
    Utils::InstanceSet(player, "z_last", player_z);
  }

  if (!ImGui::Begin("Cheats", open, ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::End();
    return;
  }

  if (ImGui::Checkbox("Debug Menu", &debug_menu))
    Utils::InstanceSet(game, "debug_console", debug_menu);
  ImGui::SameLine();
  ImGui::Checkbox("Do Debug Menu Shortcuts", &debug_shortcuts);
  ImGui::Checkbox("Infinite Jumps", &infinite_jumps);
  ImGui::Checkbox("Camera Always Follow Player", &camera_always_follow_player);
  ImGui::SameLine();
  if (ImGui::Checkbox("Keep Freecam Shot", &keep_freecam) && !keep_freecam)
    KeepFreecam(player);
  ImGui::Checkbox("Pause Buffer when RShift held.", &do_pause_buffering);

  if (ImGui::Checkbox("View Collision Only", &view_collision))
    ToggleCollision(game);
  ImGui::SameLine();
  ImGui::Checkbox("Draw Player Collision", &draw_player_collision);

  if (ImGui::Button("Reload Level"))
    ReloadLevel(game);

  if (ImGui::Button("Teleport to Map Center"))
    TeleportToMapWorldPosition(game, player, "world_x", "world_y");
  ImGui::Checkbox("Teleport to mouse on Middle Click", &teleport_on_middle_click);

  ImGui::Text("Player Variables");
  if (player.ToBoolean())
  {
    DisplayPlayerVars(player);
  }

  ImGui::Text("Save Files:");
  ImGui::BeginChild("saves", ImVec2(0, 0), ImGuiChildFlags_Borders);
  int slot = Utils::GlobalExists("SAVE_SLOT") ? Utils::GlobalGet("SAVE_SLOT").ToInt32() : 0;
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
    Utils::GlobalSet("SAVE_SLOT", slot);

  ImGui::EndChild();

  ImGui::End();
}

}