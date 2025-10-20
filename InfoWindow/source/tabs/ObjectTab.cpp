#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../ModuleMain.h"

#include <format>
#include "ObjectTab.h"

RValue GetIndex(int i, RValue &parent, std::string &name)
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
  switch (parent.m_Kind)
  {
  case VALUE_ARRAY:
    return parent[i];
  case VALUE_OBJECT:
  {
    if (yytk->CallBuiltin("is_method", {parent}).ToBoolean())
      break;
    return yytk->CallBuiltin("variable_instance_get", {parent, RValue(name)});
  }
  case VALUE_REF:
  {
    if (!yytk->CallBuiltin("instance_exists", {parent}))
      break;
    return yytk->CallBuiltin("variable_instance_get", {parent, RValue(name)});
  }
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

RValue ValueSetter(std::string name, RValue value, bool just_changed)
{
  ImGui::Text(std::format("{} = {}", name, RValueToString(value)).c_str());
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

bool ShouldMakePane(RValue &instance)
{
  return instance.m_Kind == VALUE_ARRAY ||
         (instance.m_Kind == VALUE_OBJECT && !yytk->CallBuiltin("is_method", {instance}).ToBoolean()) ||
         (instance.m_Kind == VALUE_REF && yytk->CallBuiltin("instance_exists", {instance}).ToBoolean());
}

#define NONE_SELECTED -2
std::vector<int> pane_selections;

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

void MakePane(int pane_id, RValue &object, std::function<std::string(int, RValue &)> name_func, int count, int start_index)
{
  while (pane_selections.size() <= pane_id)
    pane_selections.push_back(NONE_SELECTED);
  int selected = pane_selections[pane_id];
  if (selected >= count || (selected < start_index))
  {
    pane_selections[pane_id] = NONE_SELECTED;
    selected = NONE_SELECTED;
  }

  ImGui::BeginChild(std::format("pane {}", pane_id).c_str(), ImVec2(150, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);

  bool use_names = false;
  RValue names;
  if (!name_func && object.m_Kind == VALUE_OBJECT || object.m_Kind == VALUE_REF)
  {
    names = yytk->CallBuiltin("variable_instance_get_names", {object});
    use_names = true;
  }
  bool just_changed = false;
  std::string selected_name;
  RValue selected_value;
  for (int i = start_index; i < count; i++)
  {
    std::string name = use_names ? names[i].ToString() : name_func ? name_func(i, object)
                                                                   : std::to_string(i);
    if (hide_dunder && name.starts_with("__"))
      continue;
    RValue value = GetIndex(i, object, name);
    if (hide_functions && value.m_Kind == VALUE_OBJECT && yytk->CallBuiltin("is_method", {value}))
      continue;
    if (ImGui::Selectable(std::format("{}: {}###{}", name, RValueToString(value), i).c_str(), selected == i))
    {
      if (pane_selections.size() > pane_id + 1)
      {
        pane_selections.resize(pane_id + 1);
        pane_selections.shrink_to_fit();
      }
      pane_selections[pane_id] = i;
      selected = i;
      just_changed = true;
    }
    if (selected == i)
    {
      selected_name = name;
      selected_value = value;
    }
  }
  ImGui::EndChild();
  if (selected >= start_index)
  {
    ImGui::SameLine();
    if (ShouldMakePane(selected_value))
    {
      int count = yytk->CallBuiltin(selected_value.m_Kind == VALUE_ARRAY ? "array_length" : "variable_instance_names_count", {selected_value}).ToInt32();
      MakePane(pane_id + 1, selected_value, nullptr, count, 0);
    }
    else
    {
      ImGui::BeginChild("END", ImVec2(150, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
      ImGui::BeginGroup();
      ImGui::BeginChild("EDIT", ImVec2(0, -ImGui::GetFrameHeightWithSpacing()));
      RValue new_value = ValueSetter(selected_name, selected_value, just_changed);
      if (!new_value.IsUndefined())
      {
        if (object.IsArray())
          object[selected] = new_value;
        else
          yytk->CallBuiltin("variable_instance_set", {object, RValue(selected_name), new_value});
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
  if (!ImGui::Begin("Object Viewer"))
  {
    ImGui::End();
    return;
  }
  options_drawn = false;

  RValue instance_count_rvalue;
  yytk->GetBuiltin("instance_count", nullptr, NULL_INDEX, instance_count_rvalue);
  int instance_count = instance_count_rvalue.ToInt32();

  RValue undef;
  MakePane(0, undef, GetObjectName, instance_count, -1);

  if (!options_drawn)
  {
    ImGui::SameLine();
    ImGui::BeginChild("END", ImVec2(150, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
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
