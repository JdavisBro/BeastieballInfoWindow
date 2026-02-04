#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../ModuleMain.h"
#include "../Utils.h"

#include <format>

#include "ObjectTab.h"

namespace ObjectTab {


struct BuiltinVarList {
  const char *name;
  std::vector<const char *> vars;
  bool visible = false;
};

BuiltinVarList all_builtin_types[] = {
  {"Common", {"visible", "x", "y"}, true},
  {"General", {"id", "solid", "persistent", "depth", "layer", "alarm", "object_index"}},
  {"Movement", {"direction", "friction", "gravity", "gravity_direction", "hspeed", "vspeed", "xstart", "ystart", "xprevious", "yprevious", "speed"}},
  {"Sprite", {"sprite_index", "sprite_width", "sprite_height", "sprite_xoffset", "sprite_yoffset"}},
  {"Image", {"image_alpha", "image_angle", "image_blend", "image_index", "image_number", "image_speed", "image_xscale", "image_yscale"}},
  {"Mask", {"mask_index", "bbox_bottom", "bbox_left", "bbox_right", "bbox_top"}},
  {"Path", {"path_index", "path_position", "path_positionprevious", "path_speed", "path_scale", "path_orientation", "path_endaction"}},
  {"Timeline & Sequence", {"timeline_index", "timeline_running", "timeline_speed", "timeline_position", "timeline_loop", "in_sequence", "sequence_instance"}},
  {"Physics", {"phy_active", "phy_angular_velocity", "phy_angular_damping", "phy_linear_velocity_x", "phy_linear_velocity_y", "phy_linear_damping", "phy_speed_x", "phy_speed_y", "phy_position_x", "phy_position_y", "phy_position_xprevious", "phy_position_yprevious", "phy_rotation", "phy_fixed_rotation", "phy_bullet"}}
};
const int all_builtin_count = 9;

enum StorageType : int32_t
{
  STORAGE_UNKNOWN = -1,
  STORAGE_STRUCT = 0,
  STORAGE_INSTANCE = 1,
  STORAGE_ARRAY = 2,
  STORAGE_DS_MAP = 3,
  STORAGE_DS_LIST = 4,
};

StorageType GetStorageType(RValue &object)
{
  switch (object.m_Kind)
  {
  case VALUE_OBJECT:
    if (!yytk->CallBuiltin("is_method", {object}).ToBoolean())
      return STORAGE_STRUCT;
  case VALUE_REF:
  {
    if (yytk->CallBuiltin("instance_exists", {object}))
      return STORAGE_INSTANCE;
    std::string ref_string = object.ToString();
    if (ref_string.find("ds_map") != -1)
      return STORAGE_DS_MAP;
    if (ref_string.find("ds_list") != -1)
      return STORAGE_DS_LIST;
    break;
  }
  case VALUE_ARRAY:
    return STORAGE_ARRAY;
  }
  return STORAGE_UNKNOWN;
}

RValue GetIndex(int i, RValue &parent, RValue &name, StorageType type)
{
  if (parent.m_Kind == VALUE_UNDEFINED)
  {
    if (i == -1)
    {
      CInstance *global = nullptr;
      yytk->GetGlobalInstance(&global);
      return global->ToRValue();
    }
    return yytk->CallBuiltin("instance_find", {-3, i});
  }
  switch (type)
  {
  case STORAGE_ARRAY:
    return parent[i];
  case STORAGE_STRUCT:
  case STORAGE_INSTANCE:
    return Utils::InstanceGet(parent, name);
  case STORAGE_DS_MAP:
    return yytk->CallBuiltin("ds_map_find_value", {parent, name});
  case STORAGE_DS_LIST:
    return yytk->CallBuiltin("ds_list_find_value", {parent, i});
  }
  return RValue();
}

std::string RValueToString(RValue &value)
{
  switch (value.m_Kind)
  {
  case VALUE_ARRAY:
    return std::format("Array[{}]", yytk->CallBuiltin("array_length", {value}).ToString());
  case VALUE_OBJECT:
  {
    if (yytk->CallBuiltin("is_method", {value}).ToBoolean())
      break;
    std::string constructor = yytk->CallBuiltin("instanceof", {value}).ToString();
    return std::format("Struct ({})", constructor);
  }
  case VALUE_REF:
  {
    if (!yytk->CallBuiltin("instance_exists", {value}))
      break;
    RValue object = Utils::InstanceGet(value, "object_index");
    return std::format("Instance ({})", yytk->CallBuiltin("object_get_name", {object}).ToString());
  }
  case VALUE_STRING:
    return std::format("\"{}\"", value.ToString());
  case VALUE_BOOL:
    return value.ToBoolean() ? "true" : "false";
  case VALUE_UNSET:
    return "Unset";
  }
  return value.ToString();
}

bool setter_bool = false;
double setter_double = 0;
std::string setter_string = "";

RValue ValueSetter(RValue &name, RValue &value, bool just_changed)
{
  ImGui::Text(std::format("{} = {}", name.ToString(), RValueToString(value)).c_str());
  RValue return_value;
  if (ImGui::Button("To Bool"))
    return_value = RValue(value.ToBoolean());
  ImGui::SameLine();
  if (ImGui::Button("To Number"))
    return_value = RValue(value.ToDouble());
  ImGui::SameLine();
  if (ImGui::Button("To String"))
    return_value = RValue(value.ToString());
  if (return_value.m_Kind != VALUE_UNDEFINED) {
    just_changed = true;
    value = return_value;
  }
  switch (value.m_Kind)
  {
  case VALUE_BOOL:
  {
    if (just_changed)
      setter_bool = value.ToBoolean();
    ImGui::Text("type: bool");
    ImGui::Checkbox("Value", &setter_bool);
    if (setter_bool != value.ToBoolean())
      return RValue(setter_bool);
    break;
  }
  case VALUE_INT32:
  case VALUE_INT64:
  case VALUE_REAL:
    if (just_changed)
      setter_double = value.ToDouble();
    ImGui::Text("type: number (double / int32 / int64)");
    ImGui::InputDouble("##DoubleInput", &setter_double, 0.0, 0.0, "%f");
    ImGui::SameLine();
    if (ImGui::Button("Set"))
      return RValue(setter_double);
    break;
  case VALUE_STRING:
    if (just_changed)
      setter_string = value.ToString();
    ImGui::Text("type: string");
    ImGui::InputTextMultiline("##TextInput", &setter_string);
    ImGui::SameLine();
    if (ImGui::Button("Set"))
      return RValue(setter_string);
    break;
  }
  return return_value;
}

#define NONE_SELECTED -2

struct Pane {
  int selection = NONE_SELECTED;
  std::string name_selection = "";
  std::string search = "";
};

std::vector<Pane> panes = {};

bool options_drawn = false;

bool hide_functions = true;
bool hide_dunder = true;
bool sort_names = true;

bool just_changed = false;

void BeginEndPane()
{
  ImGui::BeginChild("END", ImVec2(350, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
  ImGui::BeginGroup();
  ImGui::BeginChild("EDIT", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
}

void EndEndPane()
{
  options_drawn = true;
  ImGui::EndChild();
  if (ImGui::Button("Options"))
    ImGui::OpenPopup("options");
  if (ImGui::BeginPopup("options")) {
    ImGui::Checkbox("Hide Functions", &hide_functions);
    ImGui::SameLine();
    ImGui::Checkbox("Hide __ vars", &hide_dunder);
    ImGui::SameLine();
    ImGui::Checkbox("Sort Keys", &sort_names);
    for (int i = 0; i < all_builtin_count; i++) {
      BuiltinVarList *list = &all_builtin_types[i];
      ImGui::Checkbox(list->name, &list->visible);
      if (i % 3 != 2)
        ImGui::SameLine();
    }
    ImGui::EndPopup();
  }
  ImGui::EndGroup();
  ImGui::EndChild();
  if (just_changed)
    ImGui::SetScrollHereX(1.0);
}

std::string new_obj_name = "";
double new_obj_x = 0;
double new_obj_y = 0;
double new_obj_z = 0;
bool new_obj_set_z = false;
double new_obj_depth = 0;

void CreateObject() {
  if (ImGui::Button("Create"))
    ImGui::OpenPopup("createobj");
  if (ImGui::BeginPopup("createobj")) {
    ImGui::InputText("Object Name", &new_obj_name);
    ImGui::InputDouble("X", &new_obj_x);
    ImGui::SameLine();
    ImGui::InputDouble("Y", &new_obj_y);
    ImGui::SameLine();
    ImGui::InputDouble("Z", &new_obj_z);
    ImGui::SameLine();
    ImGui::Checkbox("Set Z", &new_obj_set_z);
    if (ImGui::Button("Copy Position from 1st Selected Instance")) {
      int selection = panes[0].selection;
      if (selection >= 0) {
        RValue old_inst = yytk->CallBuiltin("instance_find", {-3, selection});
        if (old_inst.ToBoolean()) {
          new_obj_x = Utils::InstanceGet(old_inst, "x").ToDouble();
          new_obj_y = Utils::InstanceGet(old_inst, "y").ToDouble();
          new_obj_set_z = Utils::InstanceExists(old_inst, "z");
          if (new_obj_set_z)
            new_obj_z = Utils::InstanceGet(old_inst, "z").ToDouble();
        }
      }
    }
    ImGui::InputDouble("Depth", &new_obj_depth);
    if (ImGui::Button("Create")) {
      RValue obj = yytk->CallBuiltin("asset_get_index", {RValue(new_obj_name)});
      if (obj.ToBoolean() && yytk->CallBuiltin("object_exists", {obj}).ToBoolean()) {
        RValue inst = yytk->CallBuiltin("instance_create_depth", {new_obj_x, new_obj_y, new_obj_depth, obj});
        if (new_obj_set_z)
          Utils::InstanceSet(inst, "z", new_obj_z);
      }
    }
    ImGui::EndPopup();
  }
}

void SetValue(StorageType type, const RValue &object, const RValue &key, int index, const RValue &value)
{
  switch (type)
  {
  case STORAGE_STRUCT:
  case STORAGE_INSTANCE:
    Utils::InstanceSet(object, key, value);
    break;
  case STORAGE_ARRAY:
    if (index == -1)
      yytk->CallBuiltin("array_push", {object, value});
    else
      yytk->CallBuiltin("array_set", {object, index, value});
    break;
  case STORAGE_DS_MAP:
    yytk->CallBuiltin("ds_map_replace", {object, key, value});
    break;
  case STORAGE_DS_LIST:
    if (index == -1)
      yytk->CallBuiltin("ds_list_add", {object, value});
    else
      yytk->CallBuiltin("ds_list_set", {object, index, value});
    break;
  }
}

std::string new_var_name = "";

void NewValue(StorageType type, RValue &object)
{
  if (ImGui::Button("New"))
    ImGui::OpenPopup("newvar");
  if (ImGui::BeginPopup("newvar")) {
    bool name_check = false;
    switch (type) {
    case STORAGE_STRUCT:
    case STORAGE_INSTANCE:
    case STORAGE_DS_MAP:
      ImGui::InputText("Name", &new_var_name);
      name_check = true;
    }
    if (!name_check || (!new_var_name.empty() && !new_var_name.starts_with("@@"))) {
      if (ImGui::Button("Create Bool"))
        SetValue(type, object, RValue(new_var_name), -1, false);
      ImGui::SameLine();
      if (ImGui::Button("Create Number"))
        SetValue(type, object, RValue(new_var_name), -1, 0.0);
      ImGui::SameLine();
      if (ImGui::Button("Create String"))
        SetValue(type, object, RValue(new_var_name), -1, "");
    }
    ImGui::EndPopup();
  }
}

const char *storage_type_size_functions[] = {
    "variable_instance_names_count", // STORAGE_STRUCT
    "variable_instance_names_count", // STORAGE_INSTANCE
    "array_length",                  // STORAGE_ARRAY
    "ds_map_size",                   // STORAGE_DS_MAP
    "ds_list_size",                  // STORAGE_DS_LIST
};

void MakePane(int pane_id, RValue &object, std::function<std::string(int, RValue &)> name_func, int count, int start_index, StorageType type, RValue &builtins_array)
{
  while (panes.size() <= pane_id)
    panes.push_back(Pane());
  Pane pane = panes[pane_id];

  if (pane.selection >= count || (pane.selection < start_index))
    pane.selection = NONE_SELECTED;

  ImGui::BeginChild(std::format("pane {}", pane_id).c_str(), ImVec2(350, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX | ImGuiWindowFlags_NoSavedSettings);

  bool use_names = true;
  RValue names;
  int builtins_length = yytk->CallBuiltin("array_length", {builtins_array}).ToInt32();
  switch (type)
  {
  case STORAGE_STRUCT:
    names = yytk->CallBuiltin("variable_instance_get_names", {object});
    break;
  case STORAGE_INSTANCE: {
    names = yytk->CallBuiltin("variable_instance_get_names", {object});
    RValue names_length = yytk->CallBuiltin("array_length", {names});
    yytk->CallBuiltin("array_copy", {names, names_length, builtins_array, 0, builtins_length});
    break;
  }
  case STORAGE_DS_MAP:
    names = yytk->CallBuiltin("ds_map_keys_to_array", {object});
    break;
  case STORAGE_ARRAY:
  case STORAGE_DS_LIST:
  case STORAGE_UNKNOWN:
    use_names = false;
    break;
  }
  if (use_names && sort_names)
    yytk->CallBuiltin("array_sort", {names, true});
  RValue selected_key;
  RValue selected_value;
  ImGui::InputText("Search", &pane.search);
  ImGui::SameLine();
  if (object.m_Kind == VALUE_UNDEFINED)
    CreateObject();
  else
    NewValue(type, object);
  ImGui::BeginChild("vars");
  for (int i = start_index; i < count; i++)
  {
    std::string name = use_names ? names[i].ToString() : name_func ? name_func(i, object)
      : std::to_string(i);
    if (!pane.search.empty() && name.find(pane.search) == -1)
      continue;
    RValue key = use_names ? names[i] : RValue(name);
    if (hide_dunder && name.starts_with("__"))
      continue;
    RValue value = GetIndex(i, object, key, type);
    if (hide_functions && value.m_Kind == VALUE_OBJECT && yytk->CallBuiltin("is_method", {value}))
      continue;
    bool is_selected = use_names ? pane.name_selection == name : pane.selection == i;
    if (ImGui::Selectable(std::format("{}: {}###{}", name, RValueToString(value), i).c_str(), is_selected))
    {
      if (panes.size() > pane_id + 1)
      {
        panes.resize(pane_id + 1);
        panes.shrink_to_fit();
      }
      pane.selection = i;
      pane.name_selection = name;
      just_changed = true;
    }
    if ((!just_changed && is_selected) || pane.selection == i)
    {
      pane.selection = i;
      selected_key = key;
      selected_value = value;
    }
  }
  if (count == 0)
    ImGui::Text("empty...");
  ImGui::EndChild();
  ImGui::EndChild();
  if (pane.selection >= start_index && selected_key.m_Kind != VALUE_UNDEFINED)
  {
    ImGui::SameLine();
    StorageType new_type = GetStorageType(selected_value);
    if (new_type != STORAGE_UNKNOWN)
    {
      int count = yytk->CallBuiltin(storage_type_size_functions[new_type], {selected_value}).ToInt32();
      if (new_type == STORAGE_INSTANCE)
        count += builtins_length;
      MakePane(pane_id + 1, selected_value, nullptr, count, 0, new_type, builtins_array);
    }
    else
    {
      BeginEndPane();
      RValue new_value = ValueSetter(selected_key, selected_value, just_changed);
      if (new_value.m_Kind != VALUE_UNDEFINED)
        SetValue(type, object, selected_key, pane.selection, new_value);
      EndEndPane();
    }
  }
  panes[pane_id] = pane;
}

std::string GetObjectName(int i, RValue &parent)
{
  if (i == -1)
    return "-: Global";
  RValue instance = yytk->CallBuiltin("instance_find", {-3, i});
  RValue object = Utils::InstanceGet(instance, "object_index");
  return std::format("{}: {}", i, yytk->CallBuiltin("object_get_name", {object}).ToString());
}

RValue MakeBuiltinList()
{
  RValue arr = yytk->CallBuiltin("array_create", {});
  size_t count = 0;
  for (int i = 0; i < all_builtin_count; i++) {
    BuiltinVarList *list = &all_builtin_types[i];
    if (list->visible) {
      std::vector<RValue> args = {arr, count};
      size_t var_count = list->vars.size();
      for (size_t var_i = 0; var_i < var_count; var_i++)
        args.push_back(list->vars[var_i]);
      yytk->CallBuiltin("array_insert", args);
      count += var_count;
    }
  }
  return arr;
}

void ObjectTab(bool *open)
{
  if (!ImGui::Begin("Object Viewer", open, ImGuiWindowFlags_HorizontalScrollbar | ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::End();
    return;
  }
  options_drawn = false;
  just_changed = false;

  RValue instance_count_rvalue;
  yytk->GetBuiltin("instance_count", nullptr, NULL_INDEX, instance_count_rvalue);
  int instance_count = instance_count_rvalue.ToInt32();

  RValue builtins_array = MakeBuiltinList();
  RValue undef;
  MakePane(0, undef, GetObjectName, instance_count, -1, STORAGE_UNKNOWN, builtins_array);

  if (!options_drawn)
  {
    ImGui::SameLine();
    BeginEndPane();
    ImGui::Text("No editable variable selected.");
    EndEndPane();
  }

  ImGui::End();
}

}