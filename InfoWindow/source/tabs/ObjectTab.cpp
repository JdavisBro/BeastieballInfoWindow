#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../ModuleMain.h"

#include <format>
#include "ObjectTab.h"

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
  if (parent.IsUndefined())
  {
    if (i == -1)
    {
      CInstance *global = nullptr;
      yytk->GetGlobalInstance(&global);
      return global->ToRValue();
    }
    return yytk->CallBuiltin("instance_find", {RValue(-3), RValue(i)});
  }
  switch (type)
  {
  case STORAGE_ARRAY:
    return parent[i];
  case STORAGE_STRUCT:
  case STORAGE_INSTANCE:
    return yytk->CallBuiltin("variable_instance_get", {parent, name});
  case STORAGE_DS_MAP:
    return yytk->CallBuiltin("ds_map_find_value", {parent, name});
  case STORAGE_DS_LIST:
    return yytk->CallBuiltin("ds_list_find_value", {parent, RValue(i)});
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
    if (yytk->CallBuiltin("is_method", {value}).ToBoolean())
      break;
    return "Struct";
  case VALUE_REF:
  {
    if (!yytk->CallBuiltin("instance_exists", {value}))
      break;
    RValue object = yytk->CallBuiltin("variable_instance_get", {value, RValue("object_index")});
    return std::format("{} Instance", yytk->CallBuiltin("object_get_name", {object}).ToString());
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

RValue ValueSetter(RValue name, RValue value, bool just_changed)
{
  ImGui::Text(std::format("{} = {}", name.ToString(), RValueToString(value)).c_str());
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
    ImGui::InputText("##TextInput", &setter_string);
    ImGui::SameLine();
    if (ImGui::Button("Set"))
      return RValue(setter_string);
    break;
  }
  return RValue();
}

#define NONE_SELECTED -2
std::vector<int> pane_selections;
std::vector<std::string> pane_searches;

bool options_drawn = false;

bool hide_functions = true;
bool hide_dunder = true;

void DrawOptions()
{
  options_drawn = true;
  ImGui::Checkbox("Hide Functions", &hide_functions);
  ImGui::SameLine();
  ImGui::Checkbox("Hide __ vars", &hide_dunder);
}

const char *storage_type_size_functions[] = {
    "variable_instance_names_count", // STORAGE_STRUCT
    "variable_instance_names_count", // STORAGE_INSTANCE
    "array_length",                  // STORAGE_ARRAY
    "ds_map_size",                   // STORAGE_DS_MAP
    "ds_list_size",                  // STORAGE_DS_LIST
};

void MakePane(int pane_id, RValue &object, std::function<std::string(int, RValue &)> name_func, int count, int start_index, StorageType type)
{
  while (pane_selections.size() <= pane_id)
    pane_selections.push_back(NONE_SELECTED);
  while (pane_searches.size() <= pane_id)
    pane_searches.push_back("");

  int selected = pane_selections[pane_id];
  if (selected >= count || (selected < start_index))
  {
    pane_selections[pane_id] = NONE_SELECTED;
    selected = NONE_SELECTED;
  }

  ImGui::BeginChild(std::format("pane {}", pane_id).c_str(), ImVec2(150, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

  bool use_names = true;
  RValue names;
  switch (type)
  {
  case STORAGE_STRUCT:
  case STORAGE_INSTANCE:
    names = yytk->CallBuiltin("variable_instance_get_names", {object});
    break;
  case STORAGE_DS_MAP:
    names = yytk->CallBuiltin("ds_map_keys_to_array", {object});
    break;
  case STORAGE_ARRAY:
  case STORAGE_DS_LIST:
  case STORAGE_UNKNOWN:
    use_names = false;
    break;
  }
  bool just_changed = false;
  RValue selected_key;
  RValue selected_value;
  std::string *search_ptr = pane_searches.data() + pane_id;
  ImGui::InputText("Search", search_ptr);
  std::string search = *search_ptr;
  ImGui::BeginChild("vars");
  for (int i = start_index; i < count; i++)
  {
    std::string name = use_names ? names[i].ToString() : name_func ? name_func(i, object)
                                                                   : std::to_string(i);
    if (!search.empty() && name.find(search) == -1)
      continue;
    RValue key = use_names ? names[i] : RValue(name);
    if (hide_dunder && name.starts_with("__"))
      continue;
    RValue value = GetIndex(i, object, key, type);
    if (hide_functions && value.m_Kind == VALUE_OBJECT && yytk->CallBuiltin("is_method", {value}))
      continue;
    if (ImGui::Selectable(std::format("{}: {}###{}", name, RValueToString(value), i).c_str(), selected == i))
    {
      if (pane_selections.size() > pane_id + 1)
      {
        pane_selections.resize(pane_id + 1);
        pane_selections.shrink_to_fit();
      }
      if (pane_searches.size() > pane_id + 1)
      {
        pane_searches.resize(pane_id + 1);
        pane_searches.shrink_to_fit();
      }
      pane_selections[pane_id] = i;
      selected = i;
      just_changed = true;
    }
    if (selected == i)
    {
      selected_key = key;
      selected_value = value;
    }
  }
  if (count == 0)
  {
    ImGui::Text("empty...");
  }
  ImGui::EndChild();
  ImGui::EndChild();
  if (selected >= start_index && !selected_key.IsUndefined())
  {
    ImGui::SameLine();
    StorageType new_type = GetStorageType(selected_value);
    if (new_type != STORAGE_UNKNOWN)
    {
      int count = yytk->CallBuiltin(storage_type_size_functions[new_type], {selected_value}).ToInt32();
      MakePane(pane_id + 1, selected_value, nullptr, count, 0, new_type);
    }
    else
    {
      ImGui::BeginChild("END", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
      ImGui::BeginGroup();
      ImGui::BeginChild("EDIT", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
      RValue new_value = ValueSetter(selected_key, selected_value, just_changed);
      if (!new_value.IsUndefined())
      {
        switch (type)
        {
        case STORAGE_STRUCT:
        case STORAGE_INSTANCE:
          yytk->CallBuiltin("variable_instance_set", {object, selected_key, new_value});
          break;
        case STORAGE_ARRAY:
          object[selected] = new_value;
          break;
        case STORAGE_DS_MAP:
          yytk->CallBuiltin("ds_map_replace", {object, selected_key, new_value});
          break;
        case STORAGE_DS_LIST:
          yytk->CallBuiltin("ds_list_set", {object, RValue(selected), new_value});
          break;
        }
      }
      ImGui::EndChild();
      DrawOptions();
      ImGui::EndGroup();
      ImGui::EndChild();
    }
  }
}

std::string GetObjectName(int i, RValue &parent)
{
  if (i == -1)
  {
    return "-: Global";
  }
  RValue instance = yytk->CallBuiltin("instance_find", {RValue(-3),
                                                        RValue(i)});
  RValue object = yytk->CallBuiltin("variable_instance_get", {instance, RValue("object_index")});
  return std::format("{}: {}", i, yytk->CallBuiltin("object_get_name", {object}).ToString());
}

void ObjectTab()
{
  if (!ImGui::Begin("Object Viewer", NULL, ImGuiWindowFlags_HorizontalScrollbar))
  {
    ImGui::End();
    return;
  }
  options_drawn = false;

  RValue instance_count_rvalue;
  yytk->GetBuiltin("instance_count", nullptr, NULL_INDEX, instance_count_rvalue);
  int instance_count = instance_count_rvalue.ToInt32();

  RValue undef;
  MakePane(0, undef, GetObjectName, instance_count, -1, STORAGE_UNKNOWN);

  if (!options_drawn)
  {
    ImGui::SameLine();
    ImGui::BeginChild("END", ImVec2(0, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    ImGui::BeginGroup();
    ImGui::BeginChild("EDIT", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
    ImGui::Text("No editable variable selected.");
    ImGui::EndChild();
    DrawOptions();
    ImGui::EndGroup();
    ImGui::EndChild();
  }

  ImGui::End();
}
