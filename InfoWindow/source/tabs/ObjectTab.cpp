#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../ModuleMain.h"

#include <format>
#include "ObjectTab.h"

void MakeSubtree(RValue &object)
{
  bool is_array = object.m_Kind == VALUE_ARRAY;
  std::vector<RValue> array = {};
  if (!is_array)
  {
    array = yytk->CallBuiltin("variable_instance_get_names", {object}).ToVector();
  }
  int count = is_array ? yytk->CallBuiltin("array_length", {object}).ToInt32() : array.size();
  for (int i = 0; i < count; i++)
  {
    RValue var_val;
    const char *var_name_str;
    if (is_array)
    {
      var_val = object[i];
      var_name_str = std::format("{}", i).c_str();
    }
    else
    {
      RValue var_name = array[i];
      var_name_str = var_name.ToCString();
      var_val = yytk->CallBuiltin("variable_instance_get", {object, var_name_str});
    }
    switch (var_val.m_Kind)
    {
    case VALUE_ARRAY:
    {
      int array_length = yytk->CallBuiltin("array_length", {var_val}).ToInt32();
      ImGui::PushID(i);
      if (ImGui::TreeNodeEx(std::format("{}: Array[{}]", var_name_str, array_length).c_str()))
      {
        MakeSubtree(var_val);
      }
      ImGui::PopID();
      break;
    }
    case VALUE_OBJECT:
    {
      if (yytk->CallBuiltin("is_method", {var_val}).ToBoolean())
      {
        break;
      }
      ImGui::PushID(i);
      if (ImGui::TreeNodeEx(std::format("{}: Object", var_name_str).c_str()))
      {
        MakeSubtree(var_val);
      }
      ImGui::PopID();
      break;
    }
    case VALUE_BOOL:
    {
      static bool change_to = false;
      ImGui::TreeNodeEx(std::format("{}: {}", var_name_str, var_val.ToBoolean() ? "true" : "false").c_str(), ImGuiTreeNodeFlags_Leaf);
      if (ImGui::IsItemClicked())
      {
        ImGui::OpenPopup("changenumberpopup");
        change_to = var_val.ToBoolean();
      }
      if (ImGui::BeginPopup("changenumberpopup"))
      {
        if (
            ImGui::Button(std::format("Change {} to {}", var_name_str, change_to ? "true" : "false").c_str()))
        {
          change_to = !change_to;
        }
        if (ImGui::Button("Done"))
        {
          if (is_array)
          {
            object[i] = RValue(change_to);
          }
          else
          {
            yytk->CallBuiltin("variable_instance_set", {object, var_name_str, RValue(change_to)});
          }
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
      ImGui::TreePop();
      break;
    }
    case VALUE_STRING:
    {
      static std::string change_to;
      ImGui::TreeNodeEx(std::format("{}: \"{}\"", var_name_str, var_val.ToCString()).c_str(), ImGuiTreeNodeFlags_Leaf);
      if (ImGui::IsItemClicked())
      {
        ImGui::OpenPopup("changestringpopup");
        change_to = var_val.ToString();
      }
      if (ImGui::BeginPopup("changestringpopup"))
      {
        ImGui::InputText(std::format("Change {}", var_name_str).c_str(), &change_to);
        if (ImGui::Button("Done"))
        {
          if (is_array)
          {
            object[i] = RValue(change_to);
          }
          else
          {
            yytk->CallBuiltin("variable_instance_set", {object, var_name_str, RValue(change_to)});
          }
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
      ImGui::TreePop();
      break;
    }
    case VALUE_INT32:
    case VALUE_INT64:
    case VALUE_REAL:
    {
      static double change_to = 0;
      ImGui::TreeNodeEx(std::format("{}: {}", var_name_str, var_val.ToCString()).c_str(), ImGuiTreeNodeFlags_Leaf);
      if (ImGui::IsItemClicked())
      {
        ImGui::OpenPopup("changenumberpopup");
        change_to = var_val.ToDouble();
      }
      if (ImGui::BeginPopup("changenumberpopup"))
      {
        ImGui::InputDouble(std::format("Change {}", var_name_str).c_str(), &change_to);
        if (ImGui::Button("Done"))
        {
          if (is_array)
          {
            object[i] = RValue(change_to);
          }
          else
          {
            yytk->CallBuiltin("variable_instance_set", {object, var_name_str, RValue(change_to)});
          }
          ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
      }
      ImGui::TreePop();
      break;
    }
    case VALUE_REF:
      if (yytk->CallBuiltin("instance_exists", {var_val}))
      {
        if (ImGui::TreeNodeEx(std::format("{}: Instance", var_name_str).c_str()))
        {
          MakeSubtree(var_val);
        }
        break;
      }
    default:
      ImGui::TreeNodeEx(std::format("{}: {}", var_name_str, var_val.ToCString()).c_str(), ImGuiTreeNodeFlags_Leaf);
      ImGui::TreePop();
    }
  }
  ImGui::TreePop();
}

void AddInstanceNode(int i, YYTK::RValue &object)
{
  if (object.IsUndefined())
  {
    return;
  }
  ImGui::PushID(i);
  std::string name = yytk->CallBuiltin("object_get_name", {object}).ToString();
  if (ImGui::TreeNodeEx(name.c_str()))
  {
    MakeSubtree(object);
  }
  ImGui::PopID();
}

void ObjectTab()
{
  static bool open = true;
  ImGui::Begin("Object Viewer");
  RValue instance_count_rvalue;
  yytk->GetBuiltin("instance_count", nullptr, NULL_INDEX, instance_count_rvalue);
  int instance_count = instance_count_rvalue.ToInt32();

  CInstance *global = nullptr;
  yytk->GetGlobalInstance(&global);
  RValue globalR = global->ToRValue();
  ImGui::PushID(-1);
  if (ImGui::TreeNodeEx("Global"))
  {
    MakeSubtree(globalR);
  }
  ImGui::PopID();

  for (int i = 0; i < instance_count; i++)
  {
    RValue instance = yytk->CallBuiltin("instance_find", {RValue(-3),
                                                          RValue(i)});
    RValue object = yytk->CallBuiltin("variable_instance_get", {instance, RValue("object_index")});
    AddInstanceNode(i, object);
  }
  ImGui::End();
}
