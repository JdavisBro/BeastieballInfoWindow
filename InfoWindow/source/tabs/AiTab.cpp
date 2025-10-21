#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../ModuleMain.h"
#include "../Hooks.h"

#include "AiTab.h"

#include <map>

RValue InstanceGet(RValue instance, const char *name)
{
  return yytk->CallBuiltin("variable_instance_get", {instance, RValue(name)});
}

void InstanceSet(RValue instance, const char *name, RValue value)
{
  yytk->CallBuiltin("variable_instance_set", {instance, RValue(name), value});
}

void Undo()
{
  RValue game_active = yytk->CallBuiltin("variable_global_get", {RValue("GAME_ACTIVE")});
  RValue stack = InstanceGet(game_active, "board_snapshots");
  while (!yytk->CallBuiltin("ds_stack_empty", {stack}))
  {
    RValue board_snapshot = yytk->CallBuiltin("ds_stack_pop", {stack});
    yytk->CallGameScript("gml_Script_board_snapshot_load", {board_snapshot});
  }
}

void MakeAi()
{
  RValue game_active = yytk->CallBuiltin("variable_global_get", {RValue("GAME_ACTIVE")});
  RValue ai_choicegraph = InstanceGet(game_active, "ai_choicegraph");
  if (!game_active.ToBoolean() || ai_choicegraph.ToBoolean())
  {
    return;
  }
  int found_ai = -1;
  for (int i = 0; i < 2; i += 1)
  {
    int64_t team_control = InstanceGet(game_active, "teams_data")[i]["control"].ToInt64();
    if (team_control == 1 || team_control == 2)
    {
      // team is ai
      found_ai = i;
      break;
    }
  }
  if (found_ai == -1)
  {
    return;
  }
  Undo();
  yytk->CallGameScript("gml_Script_board_snapshot", {});
  InstanceSet(game_active, "ai_selecting", RValue(found_ai));
  RValue aitreeElephant = yytk->CallBuiltin("json_parse", {"{\"_\": \"class_aitree\"}"});
  RValue aitree = yytk->CallGameScript("gml_Script_ElephantFromJSON", {aitreeElephant});
  InstanceSet(game_active, "ai_choicegraph", aitree);
  RValue snapshot = yytk->CallGameScript("gml_Script_board_snapshot_save", {RValue(false)});
  aitree["root_snapshot"] = snapshot;
}

void DeleteAi()
{
  RValue game_active = yytk->CallBuiltin("variable_global_get", {RValue("GAME_ACTIVE")});
  InstanceSet(game_active, "ai_choicegraph", RValue());
  InstanceSet(game_active, "ai_selecting", RValue(-1));
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

AiBranch AddBranch(RValue branch)
{
  AiBranch branch_rep;
  branch_rep.text = branch["_debug_str"].ToString();
  branch_rep.erratic = yytk->CallBuiltin("variable_instance_exists", {branch, "erratic_result"}) ? branch["erratic_result"].ToDouble() : 0;
  branch_rep.eval = branch["eval"].ToDouble() - branch_rep.erratic;
  branch_rep.sim_count = 0;
  if (branch_rep.eval > max_eval)
  {
    max_eval = branch_rep.eval;
  }
  RValue children = branch["children"];
  if (children.IsArray())
  {
    int32_t length = yytk->CallBuiltin("array_length", {children}).ToInt32();
    for (int32_t i = 0; i < length; i++)
    {
      branch_rep.children.push_back(AddBranch(children[i]));
    }
  }
  return branch_rep;
}

void CreateAiTree()
{
  RValue game_active = yytk->CallBuiltin("variable_global_get", {RValue("GAME_ACTIVE")});
  if (!game_active.ToBoolean())
    return;
  RValue ai_choicegraph = InstanceGet(game_active, "ai_choicegraph");
  if (!ai_choicegraph.ToBoolean())
    return;
  if (!ai_choicegraph["children"].IsArray() || !yytk->CallBuiltin("array_length", {ai_choicegraph["children"]}).ToInt32())
    return;
  while (!ai_choicegraph["parent"].IsUndefined())
    ai_choicegraph = ai_choicegraph["parent"];
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
    CreateAiTree();
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
  RValue game_active = yytk->CallBuiltin("variable_global_get", {RValue("GAME_ACTIVE")});
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
  if (tree["children"].IsArray() && numArgs > 1)
  {
    double new_eval = (*Args[0]).ToDouble();
    double best_eval = tree["eval"].ToDouble();
    RValue favorite_child = tree["children"][(new_eval >= best_eval ? (*Args[1]) : tree["_favorite"]).ToInt32()];
    if (yytk->CallBuiltin("variable_instance_exists", {favorite_child, "erratic_result"}).ToBoolean())
    {
      InstanceSet(tree, "erratic_result", favorite_child["erratic_result"]);
    }
    else
    {
      DbgPrint("favorite child does not have erratic");
    }
  }
  else if (numArgs == 0 || (*Args[0]).IsUndefined()) // is from board eval
  {
    RValue game_active = yytk->CallBuiltin("variable_global_get", {RValue("GAME_ACTIVE")});
    int32_t ai_selecting = InstanceGet(game_active, "ai_selecting").ToInt32();
    RValue teams_data = InstanceGet(game_active, "teams_data");
    RValue team_ai = teams_data[ai_selecting]["ai"];

    erraticness = team_ai["erratic"].ToDouble();
    erratic_result = yytk->CallBuiltin("random", {RValue(120)}).ToDouble() * erraticness;

    if (!rigged_for_path.empty())
    {
      RValue checking = tree;
      bool path_correct = true;
      size_t path_index = rigged_for_path.size();
      while (!checking["parent"].IsUndefined())
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
    InstanceSet(tree, "erratic_result", RValue(erratic_result));
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

bool shouldBranchShow(std::string text)
{
  return text.find("clicks on") == -1 && !text.starts_with("select");
}

void DrawBranch(AiBranch &branch, int index, std::vector<int32_t> path)
{
  if (!draw_all && branch.text == "" && branch.eval == -1)
  {
    return;
  }
  if (!draw_all && !draw_notpossible && branch.eval <= (max_eval - (erraticness * 120)))
  {
    return;
  }

  size_t child_count = branch.children.size();
  ImGuiTreeNodeFlags flags = child_count ? ImGuiTreeNodeFlags_DefaultOpen : (ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet);
  if ((branch.eval + branch.erratic) == winning_eval)
  {
    flags |= ImGuiTreeNodeFlags_Selected;
  }
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
      text = std::format("{}: {:.3f} +{:.3f} = {:.3f} ({:.3f}%)", branch.text, branch.eval, branch.erratic, branch.eval + branch.erratic, chance);
    }
    node_open = ImGui::TreeNodeEx(text.c_str(), flags);
  }
  if (node_open)
  {
    for (int i = 0; i < child_count; i++)
    {
      path.push_back(i);
      DrawBranch(branch.children[i], i, path);
      path.pop_back();
    }
    if (show_node)
    {
      ImGui::TreePop();
    }
  }
  if (!child_count && show_node && ImGui::IsItemClicked())
  {
    rigged_for_path = path;
    MakeAi();
  }

  if (show_node)
  {
    ImGui::PopID();
  }
}

void DrawAiTree()
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
  for (int i = 0; i < root_children; i++)
  {
    std::vector<int32_t> path = {i};
    DrawBranch(aitree.children[i], i, path);
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
  RValue game_active = yytk->CallBuiltin("variable_global_get", {RValue("GAME_ACTIVE")});
  RValue ai_choicegraph = InstanceGet(game_active, "ai_choicegraph");
  if (!game_active.ToBoolean() || aitree.children.empty())
  {
    return;
  }
  std::vector<AiBranch *> ends;
  SimCollectEnds(ends, &aitree);
  size_t end_count = ends.size();
  for (int i = 0; i < SIM_COUNT; i++)
  {
    double sim_max_eval = 0;
    size_t favorite = 0;
    for (int end_i = 0; end_i < end_count; end_i++)
    {
      AiBranch branch = *ends[end_i];
      double eval = branch.eval + yytk->CallBuiltin("random", {RValue(120)}).ToDouble() * erraticness;
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

// MARK: Tab Stuff
void AiTab()
{
  if (!ImGui::Begin("AI Info"))
  {
    ImGui::End();
    return;
  }
  replay_saved_tree = false;
  if (ImGui::Button("Make AI"))
  {
    MakeAi();
  }
  ImGui::SameLine();
  if (ImGui::Button("Simulate Chances"))
  {
    SimulateTree();
  }
  CreateAiTree();
  DrawAiTree();

  ImGui::End();
}
