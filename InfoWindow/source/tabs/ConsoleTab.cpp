#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../ModuleMain.h"
#include "../Utils.h"
#include "../Hooks.h"
#include "../Storage.h"

#include "ObjectTab.h"

#include <fstream>

namespace ConsoleTab {

std::vector<std::string> game_output;
std::vector<std::string> aurie_output;

TRoutine show_debug_message;
void ShowDebugMessage(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  if (numArgs > 1) {
    std::vector<std::string> format_args(numArgs - 1);
    for (int i = 1; i < numArgs; i++) {
      format_args[i - 1] = Args[i].ToString();
    }
    game_output.push_back(std::vformat(Args[0].ToString(), std::make_format_args(format_args)));
  }
  else {
    game_output.push_back(Args->ToString());
  }
  show_debug_message(Result, Self, Other, numArgs, Args);
}

std::fstream aurie_log;

void ConsoleHooks()
{
  aurie_log.open("aurie.log", std::fstream::in, _SH_DENYNO);
  BuiltinHook("IW show_debug_message", "show_debug_message", ShowDebugMessage, reinterpret_cast<PVOID *>(&show_debug_message));
}

size_t selected = -1;
bool go_to_bottom = true;
ImGuiTextFilter filter;

void DrawConsoleOutput(std::vector<std::string> &output, bool do_colors = false)
{
  // if (ImGui::Button("print")) {
  //   DbgPrint("something");
  //   DbgPrintEx(LOG_SEVERITY_CRITICAL, "a");
  //   DbgPrintEx(LOG_SEVERITY_DEBUG, "a");
  //   DbgPrintEx(LOG_SEVERITY_ERROR, "a");
  //   DbgPrintEx(LOG_SEVERITY_INFO, "a");
  //   DbgPrintEx(LOG_SEVERITY_TRACE, "a");
  //   DbgPrintEx(LOG_SEVERITY_WARNING, "a");
  // }
  // ImGui::SameLine();
  if (ImGui::Button("Copy"))
    ImGui::SetClipboardText(output[selected].c_str());
  ImGui::SameLine();
  if (ImGui::Button("To Bottom"))
    go_to_bottom = true;
  ImGui::SameLine();
  filter.Draw("Filter (\"incl,-excl\") (\"error\")", 180);
  ImGui::BeginChild("console!!!", {0, 0}, ImGuiChildFlags_Borders);
  size_t count = output.size();
  for (size_t i = 0; i < count; i++) {
    std::string &str = output[i];
    if (!filter.PassFilter(str.c_str()))
      continue;
    ImGui::PushID((int)i);
    bool color_set = false;
    if (do_colors && str[0] == '[') {
      size_t end = str.find("]", 12);
      if (end != std::string::npos) {
        std::string level = str.substr(12, end - 12);
        ImVec4 color;
        if (level == "trace")
          color = {0.95f, 0.95f, 0.95f, 1.0f};
        else if (level == "debug")
          color = {0.0f, 0.75f, 0.75f, 1.0f};
        else if (level == "info")
          color = {0.0f, 0.75f, 0.0f, 1.0f};
        else if (level == "warning")
          color = {1.0f, 1.0f, 0.0f, 1.0f};
        else if (level == "error")
          color = {1.0f, 0.0f, 0.0f, 1.0f};
        else if (level == "critical")
          color = {1.0f, 0.0f, 1.0f, 1.0f};
        if (color.w) {
          ImGui::PushStyleColor(ImGuiCol_Text, color);
          color_set = true;
        }
      }
    }
    std::string new_str = str;
    size_t newline_pos = new_str.find('\n');
    while (newline_pos != std::string::npos) {
      new_str.replace(newline_pos, (size_t)1, " ");
      newline_pos = new_str.find('\n', newline_pos + 1);
    }
    if (ImGui::Selectable(new_str.c_str(), i == selected))
      selected = i;
    if (color_set)
      ImGui::PopStyleColor();
    ImGui::PopID();
  }
  if (go_to_bottom || ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
    ImGui::SetScrollHereY(1.0f);
  go_to_bottom = false;
  ImGui::EndChild();
}

void ReadAurieOutput()
{
  std::streampos cur_pos = aurie_log.tellg();
  aurie_log.seekg(0, aurie_log.end);
  std::streampos end_pos = aurie_log.tellg();
  if (cur_pos < end_pos) {
    aurie_log.seekg(cur_pos);
    while (aurie_log.tellg() < end_pos) {
      std::string line;
      std::getline(aurie_log, line);
      aurie_output.push_back(line);
    }
  }
}

void ConsoleTab(bool *open)
{
  if (!ImGui::Begin("Console", open, ImGuiWindowFlags_NoFocusOnAppearing)) {
    ImGui::End();
    return;
  }
  ReadAurieOutput();
  if (ImGui::BeginTabBar("consolebar")) {
    if (ImGui::BeginTabItem("Game")) {
      DrawConsoleOutput(game_output);
      ImGui::EndTabItem();
    }
    if (ImGui::BeginTabItem("Mods")) {
      DrawConsoleOutput(aurie_output, true);
      ImGui::EndTabItem();
    }
    ImGui::EndTabBar();
  }
  ImGui::End();
}

void Store()
{

}

}