#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../ModuleMain.h"
#include "../../Hooks.h"

#include "AiTab.h"

namespace AiTab {

RValue InstanceGet(const RValue &instance, const char *name)
{
  return yytk->CallBuiltin("variable_instance_get", {instance, name});
}

void InstanceSet(const RValue &instance, const char *name, const RValue &value)
{
  yytk->CallBuiltin("variable_instance_set", {instance, name, value});
}

void Undo(const RValue &game_active)
{
  RValue stack = InstanceGet(game_active, "board_snapshots");
  while (!yytk->CallBuiltin("ds_stack_empty", {stack}))
  {
    RValue board_snapshot = yytk->CallBuiltin("ds_stack_pop", {stack});
    yytk->CallGameScript("gml_Script_board_snapshot_load", {board_snapshot});
  }
}

int FindAi(const RValue &game_active)
{
  if (!game_active.ToBoolean())
    return -2;
  RValue ai_choicegraph = InstanceGet(game_active, "ai_choicegraph");
  RValue selection_mode = InstanceGet(game_active, "selection_mode");
  bool scene_playing = yytk->CallBuiltin("variable_global_get", {"SCENE_PLAYING"}).ToBoolean();
  if (ai_choicegraph.ToBoolean() || selection_mode.ToInt32() != 0 || scene_playing)
    return -1;
  int found_ai = -1;
  for (int i = 0; i < 2; i += 1)
  {
    int64_t team_control = InstanceGet(game_active, "teams_data")[i]["control"].ToInt64();
    // team is ai
    if (team_control == 1 || team_control == 2)
    {
      if (found_ai >= i)
      {
        // multiple AIs, do nothing.
        found_ai = -1;
        break;
      }
      found_ai = i;
    }
  }
  // ai not found or player turned.
  if (found_ai == -1 || InstanceGet(game_active, "teams")[!found_ai]["turned"].ToBoolean())
  {
    return -1;
  }
  return found_ai;
}

int player_team = -1;

void ActuallyMakeAi(const RValue &game_active, const int &found_ai)
{
  Undo(game_active);
  yytk->CallGameScript("gml_Script_board_snapshot", {});
  InstanceSet(game_active, "ai_selecting", found_ai);
  RValue aitreeElephant = yytk->CallBuiltin("json_parse", {"{\"_\": \"class_aitree\"}"});
  RValue aitree = yytk->CallGameScript("gml_Script_ElephantFromJSON", {aitreeElephant});
  InstanceSet(game_active, "ai_choicegraph", aitree);
  RValue snapshot = yytk->CallGameScript("gml_Script_board_snapshot_save", {false});
  aitree["root_snapshot"] = snapshot;
}

void AutoMakeAi(const RValue &game_active)
{
  int found_ai = FindAi(game_active);
  if (found_ai == -2)
  {
    player_team = -1;
    return;
  }
  if (player_team > -1 && InstanceGet(game_active, "teams")[player_team]["turned"].ToBoolean())
    player_team = -1;
  if (player_team >= 0)
    return;
  if (found_ai < 0)
    return;
  player_team = !found_ai;
  ActuallyMakeAi(game_active, found_ai);
}

void MakeAi(const RValue &game_active)
{
  int found_ai = FindAi(game_active);
  if (found_ai < 0)
    return;
  ActuallyMakeAi(game_active, found_ai);
}

void DeleteAi(const RValue &game_active)
{
  InstanceSet(game_active, "ai_choicegraph", RValue());
  InstanceSet(game_active, "ai_selecting", -1);
}

// MARK: Aitree Storing
struct AiBranch
{
  std::string text;
  double eval;
  double erratic;
  std::vector<AiBranch> children;
  int sim_count;
};

AiBranch aitree = {};
double max_eval = 0;
int sim_total = 0;

AiBranch AddBranch(const RValue &branch)
{
  AiBranch branch_rep;
  branch_rep.text = branch["_debug_str"].ToString();
  branch_rep.erratic = yytk->CallBuiltin("variable_instance_exists", {branch, "erratic_result"}) ? branch["erratic_result"].ToDouble() : 0;
  branch_rep.eval = branch["eval"].ToDouble() - branch_rep.erratic;
  branch_rep.sim_count = 0;
  if (branch_rep.eval > max_eval)
    max_eval = branch_rep.eval;
  RValue children = branch["children"];
  if (children.m_Kind == VALUE_ARRAY)
  {
    int32_t length = yytk->CallBuiltin("array_length", {children}).ToInt32();
    for (int32_t i = 0; i < length; i++)
    {
      branch_rep.children.push_back(AddBranch(children[i]));
    }
  }
  return branch_rep;
}

void CreateAiTree(const RValue &game_active)
{
  if (!game_active.ToBoolean())
    return;
  RValue ai_choicegraph = InstanceGet(game_active, "ai_choicegraph");
  if (!ai_choicegraph.ToBoolean())
    return;
  while (ai_choicegraph["parent"].m_Kind != VALUE_UNDEFINED)
    ai_choicegraph = ai_choicegraph["parent"];
  if (ai_choicegraph["children"].m_Kind != VALUE_ARRAY || !yytk->CallBuiltin("array_length", {ai_choicegraph["children"]}).ToInt32())
    return;
  sim_total = 0;
  max_eval = 0;
  aitree = AddBranch(ai_choicegraph);
}

// MARK: Hooks
bool replay_saved_tree = false;

std::vector<int32_t> rigged_for_path = {};

PFUNC_YYGMLScript aitreeReplayOriginal = nullptr;
RValue &AitreeReplay(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  // Done when the AI is decided. Save the tree here since it's deleted immediately after.
  if (numArgs > 0 && (*Args[0]).ToBoolean() && !replay_saved_tree)
  {
    replay_saved_tree = true;
    CreateAiTree(yytk->CallBuiltin("variable_global_get", {"GAME_ACTIVE"}));
    rigged_for_path = {};
  }
  aitreeReplayOriginal(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

double erraticness = 0;
double erratic_result = 0;

PFUNC_YYGMLScript aiBoardEvalOriginal = nullptr;
RValue &AiBoardEval(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue game_active = yytk->CallBuiltin("variable_global_get", {"GAME_ACTIVE"});
  int32_t ai_selecting = InstanceGet(game_active, "ai_selecting").ToInt32();
  RValue teams_data = InstanceGet(game_active, "teams_data");
  RValue team_ai = teams_data[ai_selecting]["ai"];

  erraticness = team_ai["erratic"].ToDouble();
  team_ai["erratic"] = RValue(0.0);
  aiBoardEvalOriginal(Self, Other, ReturnValue, numArgs, Args);
  team_ai["erratic"] = RValue(erraticness);
  ReturnValue = RValue(ReturnValue.ToDouble() + erratic_result);
  return ReturnValue;
}

PFUNC_YYGMLScript aitreeSelectionSubmitOriginal = nullptr;
RValue &AitreeSelectionSubmit(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue tree = Self->ToRValue();
  if (tree["children"].m_Kind == VALUE_ARRAY && numArgs > 1)
  {
    double new_eval = (*Args[0]).ToDouble();
    double best_eval = tree["eval"].ToDouble();
    RValue favorite_child = tree["children"][(new_eval >= best_eval ? (*Args[1]) : tree["_favorite"]).ToInt32()];
    if (yytk->CallBuiltin("variable_instance_exists", {favorite_child, "erratic_result"}).ToBoolean())
      InstanceSet(tree, "erratic_result", favorite_child["erratic_result"]);
    else
      DbgPrint("favorite child does not have erratic");
  }
  else if (numArgs == 0 || (*Args[0]).m_Kind == VALUE_UNDEFINED) // is from board eval
  {
    RValue game_active = yytk->CallBuiltin("variable_global_get", {"GAME_ACTIVE"});
    int32_t ai_selecting = InstanceGet(game_active, "ai_selecting").ToInt32();
    RValue teams_data = InstanceGet(game_active, "teams_data");
    RValue team_ai = teams_data[ai_selecting]["ai"];

    erraticness = team_ai["erratic"].ToDouble();
    erratic_result = yytk->CallBuiltin("random", {120}).ToDouble() * erraticness;

    if (!rigged_for_path.empty())
    {
      RValue checking = tree;
      bool path_correct = true;
      size_t path_index = rigged_for_path.size();
      while (checking["parent"].m_Kind != VALUE_UNDEFINED)
      {
        if (path_index == 0)
        {
          path_correct = false;
          break;
        }
        path_index -= 1;
        if (checking["selection"].ToInt32() != rigged_for_path[path_index])
        {
          path_correct = false;
          break;
        }
        checking = checking["parent"];
      }
      if (path_index > 0)
      {
        path_correct = false;
      }
      erratic_result = erraticness * 120 * path_correct;
    }
    InstanceSet(tree, "erratic_result", erratic_result);
  }
  aitreeSelectionSubmitOriginal(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

void AiHooks()
{
  RequestHook("gml_Script_Replay@", "class_aitree", "IW AiTreeReplay", AitreeReplay, reinterpret_cast<PVOID *>(&aitreeReplayOriginal));
  RequestHook(NULL, "gml_Script_ai_board_eval", "IW AiBoardEval", AiBoardEval, reinterpret_cast<PVOID *>(&aiBoardEvalOriginal));
  RequestHook("gml_Script_SelectionSubmit@", "class_aitree", "IW AiTreeSelectionSubmit", AitreeSelectionSubmit, reinterpret_cast<PVOID *>(&aitreeSelectionSubmitOriginal));
}

// MARK: Aitree Drawing
bool draw_all = false;
bool draw_notpossible = false;
bool draw_intermediary = false;
double winning_eval = 0;

bool shouldBranchShow(const std::string &text)
{
  return text.find("clicks on") == -1 && !text.starts_with("select");
}

void DrawBranch(AiBranch &branch, int index, std::vector<int32_t> &path, const RValue game_active)
{
  if (!draw_all && branch.text == "" && branch.eval == -1)
    return;
  if (!draw_all && !draw_notpossible && branch.eval <= (max_eval - (erraticness * 120)))
    return;

  size_t child_count = branch.children.size();
  ImGuiTreeNodeFlags flags = child_count ? ImGuiTreeNodeFlags_DefaultOpen : (ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet);
  if ((branch.eval + branch.erratic) == winning_eval)
    flags |= ImGuiTreeNodeFlags_Selected;
  bool node_open = true;
  bool show_node = draw_intermediary || shouldBranchShow(branch.text);
  if (show_node)
  {
    ImGui::PushID(index);
    double chance = sim_total > 0 ? (double)branch.sim_count / (double)sim_total * 100 : -1;
    std::string text;
    if (child_count)
    {
      text = branch.text;
    }
    else
    {
      if (chance != -1)
        text = std::format("{}: {:.3f} +{:.3f} = {:.3f} ({:.3f}%)", branch.text, branch.eval, branch.erratic, branch.eval + branch.erratic, chance);
      else
        text = std::format("{}: {:.3f} +{:.3f} = {:.3f}", branch.text, branch.eval, branch.erratic, branch.eval + branch.erratic);
    }
    node_open = ImGui::TreeNodeEx(text.c_str(), flags);
  }
  if (node_open)
  {
    for (int i = 0; i < child_count; i++)
    {
      path.push_back(i);
      DrawBranch(branch.children[i], i, path, game_active);
      path.pop_back();
    }
    if (show_node)
      ImGui::TreePop();
  }
  if (!child_count && show_node && ImGui::IsItemClicked())
  {
    rigged_for_path = path;
    MakeAi(game_active);
  }

  if (show_node)
  {
    ImGui::PopID();
  }
}

void DrawAiTree(const RValue &game_active)
{
  ImGui::Checkbox("Show All", &draw_all);
  ImGui::SameLine();
  ImGui::Checkbox("Show Not Possible", &draw_notpossible);
  ImGui::SameLine();
  ImGui::Checkbox("Show Intermediary", &draw_intermediary);
  ImGui::Text(std::format("Min Possible: {:.3f} - Max Erratic: +{:.3f} - Sim Runs: {}", max_eval - (erraticness * 120), erraticness * 120, sim_total).c_str());
  size_t root_children = aitree.children.size();
  if (!root_children)
  {
    return;
  }
  winning_eval = aitree.eval + aitree.erratic;
  ImGui::BeginChild("aitree", ImVec2(0, 0), ImGuiChildFlags_Borders);
  std::vector<int32_t> path = {0};
  for (int i = 0; i < root_children; i++)
  {
    path[0] = i;
    DrawBranch(aitree.children[i], i, path, game_active);
  }
  ImGui::EndChild();
}

// MARK: Sim
#define SIM_COUNT 100'000

void SimCollectEnds(std::vector<AiBranch *> &ends, AiBranch *branch)
{
  if (branch->eval <= (max_eval - (erraticness * 120)))
  {
    return;
  }
  size_t children = branch->children.size();
  if (children)
  {
    for (size_t i = 0; i < children; i++)
    {
      SimCollectEnds(ends, (branch->children.data() + i));
    }
  }
  else
  {
    ends.push_back(branch);
  }
}

void SimulateTree()
{
  if (aitree.children.empty())
    return;
  std::vector<AiBranch *> ends;
  SimCollectEnds(ends, &aitree);
  size_t end_count = ends.size();
  for (int i = 0; i < SIM_COUNT; i++)
  {
    double sim_max_eval = 0;
    size_t favorite = 0;
    for (int end_i = 0; end_i < end_count; end_i++)
    {
      double eval = ends[end_i]->eval + yytk->CallBuiltin("random", {120}).ToDouble() * erraticness;
      if (eval > sim_max_eval)
      {
        sim_max_eval = eval;
        favorite = end_i;
      }
    }
    ends[favorite]->sim_count += 1;
  }
  sim_total += SIM_COUNT;
}

bool auto_create_ai = true;

// MARK: Tab Stuff
void AiTab(bool *open)
{
  RValue game_active = yytk->CallBuiltin("variable_global_get", {"GAME_ACTIVE"});
  replay_saved_tree = false;
  if (auto_create_ai)
    AutoMakeAi(game_active);
  if (!ImGui::Begin("AI Info", open, ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::End();
    return;
  }
  if (ImGui::Button("Make AI"))
    MakeAi(game_active);
  ImGui::SameLine();
  if (ImGui::Button("Simulate Chances"))
    SimulateTree();
  ImGui::SameLine();
  ImGui::Checkbox("Auto Create AI", &auto_create_ai);
  CreateAiTree(game_active);
  DrawAiTree(game_active);

  ImGui::End();
}

}