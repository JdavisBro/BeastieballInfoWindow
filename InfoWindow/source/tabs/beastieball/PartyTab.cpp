#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../ModuleMain.h"

#include "PartyTab.h"

struct RelationshipData
{
  std::string otherPid;
  std::string otherName;
  double pow;
  double prevPow;
  double friendly;
  double prevFriendly;
  bool hot;
  int myAttacks;
  int otherAttacks;
};

struct BeastieData
{
  std::string pid;
  std::string name;
  std::string number;
  double level;
  int ability;
  std::vector<int> attklist;
  std::vector<int> training;
  std::vector<double> coaching;
  std::map<std::string, RelationshipData> relationships;
  std::vector<RelationshipData *> relationships_sorted;
};

BeastieData selected_copy;

bool RelationshipSort(RelationshipData *a, RelationshipData *b)
{
  return a->pow != b->pow ? a->pow > b->pow : a->otherPid.compare(b->otherPid) > 0;
}

void GetRelationships(std::map<std::string, RelationshipData> &relationships, std::vector<RelationshipData *> &relationships_sorted, std::string pid)
{
  bool same_beastie = pid == selected_copy.pid;
  RValue all_relationships = yytk->CallBuiltin("variable_global_get", {"relationships"});
  std::vector<RValue> all_keys = yytk->CallBuiltin("variable_instance_get_names", {all_relationships}).ToVector();
  for (RValue key : all_keys)
  {
    RValue relationship = all_relationships[key.ToString()];
    std::string pidA = relationship["pidA"].ToString();
    std::string pidB = relationship["pidB"].ToString();
    int pidMatch = pidA == pid ? 1 : pidB == pid ? 2
                                                 : 0;
    if (pidMatch)
    {
      bool is_a = pidMatch == 1;
      std::string &otherPid = is_a ? pidB : pidA;
      RValue other = yytk->CallGameScript("gml_Script_char_find_by_pid", {RValue(otherPid)});

      RelationshipData copy;
      copy.otherPid = otherPid;
      copy.otherName = std::format("{}#{}", other["name"].ToString(), other["number"].ToString());
      copy.pow = relationship["pow"].ToDouble();
      copy.friendly = relationship["friendly"].ToDouble();
      copy.prevPow = copy.pow;
      copy.prevFriendly = copy.friendly;
      copy.hot = relationship["hot"].ToBoolean();
      copy.myAttacks = (is_a ? relationship["attacksA"] : relationship["attacksB"]).ToInt32();
      copy.otherAttacks = (is_a ? relationship["attacksB"] : relationship["attacksA"]).ToInt32();

      if (same_beastie && selected_copy.relationships.contains(otherPid))
      {
        RelationshipData old_relationship = selected_copy.relationships.at(otherPid);
        if (copy.pow != old_relationship.pow)
          copy.prevPow = old_relationship.pow;
        else
          copy.prevPow = old_relationship.prevPow;
        if (copy.friendly != old_relationship.friendly)
          copy.prevFriendly = old_relationship.friendly;
        else
          copy.prevFriendly = old_relationship.prevFriendly;
      }
      relationships[otherPid] = copy;
    }
  }
  for (auto r = relationships.begin(); r != relationships.end(); r++)
  {
    relationships_sorted.push_back(&r->second);
  }
  std::sort(relationships_sorted.begin(), relationships_sorted.end(), RelationshipSort);
}

const char *training_types[] = {
    "ba_t",
    "ha_t",
    "ma_t",
    "bd_t",
    "hd_t",
    "md_t",
};

const char *coaching_types[] = {
    "ba_r",
    "ha_r",
    "ma_r",
    "bd_r",
    "hd_r",
    "md_r",
};

BeastieData CopyBeastieData(RValue &beastie)
{
  RValue char_dic = yytk->CallBuiltin("variable_global_get", {"char_dic"});
  RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, beastie["specie"]});
  double growth = species["growth"].ToDouble();

  BeastieData copy;
  copy.pid = beastie["pid"].ToString();
  copy.name = beastie["name"].ToString();
  copy.number = beastie["number"].ToString();
  copy.level = cbrt(beastie["xp"].ToDouble() / growth);
  copy.ability = beastie["ability_index"].ToInt32();

  copy.attklist.resize(3, 0);
  std::vector<RValue> attklist = beastie["attklist"].ToVector();
  std::vector<RValue> species_attklist = species["attklist"].ToVector();
  for (int i = 0; i < species_attklist.size(); i++)
  {
    std::string id = species_attklist[i].ToString();
    for (int beastie_i = 0; beastie_i < attklist.size(); beastie_i++)
    {
      std::string attk = attklist[beastie_i].ToString();
      if (attk == id)
        copy.attklist[beastie_i] = i;
    }
  }
  for (const char *key : training_types)
  {
    copy.training.push_back(beastie[key].ToInt32());
  }
  for (const char *key : coaching_types)
  {
    copy.coaching.push_back(beastie[key].ToDouble());
  }
  GetRelationships(copy.relationships, copy.relationships_sorted, copy.pid);
  return copy;
}

void DrawRelationships()
{
  double window_width = ImGui::GetWindowWidth() - 16.0;
  double current_line = 0;
  size_t count = selected_copy.relationships_sorted.size();
  for (int i = 0; i < count; i++)
  {
    current_line += 308;
    if (current_line > window_width)
      current_line = 308;
    else if (i != 0)
      ImGui::SameLine();
    RelationshipData relationship = *selected_copy.relationships_sorted[i];
    ImGui::BeginChild(relationship.otherPid.c_str(), ImVec2(300, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeY);
    ImGui::Text("%s", relationship.otherName.c_str());
    double powChanged = relationship.pow - relationship.prevPow;
    ImGui::Text("Pow: %.2f • ± %.2f", relationship.pow, powChanged);
    double friendlyChanged = relationship.friendly - relationship.prevFriendly;
    ImGui::Text("Friendly: %.2f • ± %.2f", relationship.friendly, friendlyChanged);
    ImGui::Text(relationship.hot ? "Spicy" : "Not Spicy");
    ImGui::Text("Attacks: %i • %i", relationship.myAttacks, relationship.otherAttacks);
    ImGui::EndChild();
  }
}

const char *types[] = {
    "Body",
    "Spirit",
    "Mind",
};

void DoStatSection(bool training, RValue &beastie, bool &sport, RValue &sport_beastie, ImGuiWindowFlags window_flags)
{
  ImGui::BeginChild("statpow", ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, window_flags);
  ImGui::Text("POW");
  ImGui::PushItemWidth(150);
  for (int i = 0; i < 3; i++)
  {
    if (training)
      ImGui::InputInt(types[i], selected_copy.training.data() + i, 4, 4);
    else
      ImGui::InputDouble(types[i], selected_copy.coaching.data() + i);
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("statdef", ImVec2(300, 120), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, window_flags);
  ImGui::Text("DEF");
  ImGui::PushItemWidth(150);
  for (int i = 3; i < 6; i++)
  {
    if (training)
      ImGui::InputInt(std::format("##{}", i).c_str(), selected_copy.training.data() + i, 4, 4);
    else
      ImGui::InputDouble(std::format("##{}", i).c_str(), selected_copy.coaching.data() + i);
  }
  ImGui::EndChild();
  ImGui::BeginChild("setbutton", ImVec2(350, 0), ImGuiChildFlags_AutoResizeY, window_flags);
  if (ImGui::Button("Set##StatSet", ImVec2(350, 0)))
  {
    for (int i = 0; i < 6; i++)
    {
      beastie[training_types[i]] = RValue(selected_copy.training[i]);
      beastie[coaching_types[i]] = RValue(selected_copy.coaching[i]);
      if (sport)
      {
        sport_beastie[training_types[i]] = RValue(selected_copy.training[i]);
        sport_beastie[coaching_types[i]] = RValue(selected_copy.coaching[i]);
      }
    }
    if (sport)
      yytk->CallGameScript("gml_Script_gameplay_recalc_stats", {});
  }
  ImGui::EndChild();
  ImGui::EndTabItem();
}

bool editing = false;

void SelectedBeastie(RValue beastie)
{
  ImGui::Checkbox("Editing", &editing);
  if (!editing)
    selected_copy = CopyBeastieData(beastie);

  ImGui::BeginChild("beastie", ImVec2(0, 0), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar);
  ImGuiWindowFlags window_flags = editing ? 0 : ImGuiWindowFlags_NoInputs;
  ImGui::BeginChild("noclicksection", ImVec2(0, 0), ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_AutoResizeY, window_flags);

  RValue sport_beastie = yytk->CallGameScript("gml_Script_char_find_battler_by_pid", {beastie["pid"]});
  bool sport = sport_beastie.m_Kind == VALUE_OBJECT;

  RValue char_dic = yytk->CallBuiltin("variable_global_get", {"char_dic"});
  RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, beastie["specie"]});

  ImGui::PushItemWidth(150);
  ImGui::Text("Name: ");
  ImGui::SameLine();
  ImGui::InputText("##Name", &selected_copy.name);
  ImGui::SameLine();
  ImGui::InputText("##Number", &selected_copy.number);
  ImGui::SameLine();
  if (ImGui::Button("Set##NameSet"))
  {
    beastie["name"] = RValue(selected_copy.name);
    beastie["number"] = RValue(selected_copy.number);
    if (sport)
    {
      sport_beastie["name"] = RValue(selected_copy.name);
      sport_beastie["number"] = RValue(selected_copy.number);
    }
  }

  double set_level = -1;
  ImGui::Text("Level: ");
  ImGui::SameLine();
  ImGui::InputDouble("##Level", &selected_copy.level, 1.0);
  ImGui::SameLine();
  if (ImGui::Button("Set##LevelSet"))
  {
    selected_copy.level = selected_copy.level > 100 ? 100 : selected_copy.level < 1 ? 1
                                                                                    : selected_copy.level;
    double growth = species["growth"].ToDouble();
    beastie["level"] = (int)floor(selected_copy.level);
    beastie["xp"] = (int)floor(pow(selected_copy.level, 3) * growth);
    if (sport)
    {
      sport_beastie["level"] = beastie["level"];
      sport_beastie["xp"] = beastie["xp"];
    }
  }
  ImGui::PopItemWidth();
  ImGui::PushItemWidth(120);

  RValue move_dic = yytk->CallBuiltin("variable_global_get", {"move_dic"});
  std::string species_attklist;
  std::vector<RValue> species_attklist_rvalue = species["attklist"].ToVector();
  for (RValue id : species_attklist_rvalue)
  {
    RValue move = yytk->CallBuiltin("ds_map_find_value", {move_dic, id});
    species_attklist += move["name"].ToString() + '\0';
  }
  const char *items = species_attklist.data();

  for (int i = 0; i < 3; i++)
  {
    ImGui::Text("Play %i: ", i + 1);
    ImGui::SameLine();
    ImGui::Combo(std::format("##Play{}", i).c_str(), selected_copy.attklist.data() + i, items, (int)species_attklist_rvalue.size());
    ImGui::SameLine();
  }
  if (ImGui::Button("Set##PlaySet"))
  {
    RValue beastie_attklist = beastie["attklist"];
    for (int i = 0; i < 3; i++)
      beastie_attklist[i] = species_attklist_rvalue[selected_copy.attklist[i]];
    if (sport)
    {
      beastie_attklist = sport_beastie["attklist"];
      for (int i = 0; i < 3; i++)
        beastie_attklist[i] = species_attklist_rvalue[selected_copy.attklist[i]];
    }
  }
  ImGui::PopItemWidth();

  RValue abilities = species["ability"];
  RValue ability_dic = yytk->CallBuiltin("variable_global_get", {"ability_dic"});
  int ability_count = yytk->CallBuiltin("array_length", {abilities}).ToInt32();
  for (int i = 0; i < ability_count; i++)
  {
    RValue ability = yytk->CallBuiltin("ds_map_find_value", {ability_dic, abilities[i]});
    ImGui::RadioButton(ability["name"].ToCString(), &selected_copy.ability, i);
    ImGui::SameLine();
  }
  if (ImGui::Button("Set##SetAbility"))
  {
    beastie["ability_index"] = selected_copy.ability;
    beastie["ability"] = abilities[selected_copy.ability];
    if (sport)
    {
      sport_beastie["ability_index"] = selected_copy.ability;
      sport_beastie["ability"] = abilities[selected_copy.ability];
    }
  }

  ImGui::EndChild();
  ImGui::BeginTabBar("traincoach");
  if (ImGui::BeginTabItem("Training"))
    DoStatSection(true, beastie, sport, sport_beastie, window_flags);
  if (ImGui::BeginTabItem("Coaching"))
    DoStatSection(false, beastie, sport, sport_beastie, window_flags);
  ImGui::EndTabBar();
  DrawRelationships();
  ImGui::EndChild();
}

bool DrawParty()
{
  RValue data = yytk->CallBuiltin("variable_global_get", {"data"});
  if (!data.ToBoolean())
    return false;
  RValue party = data["team_party"];
  if (!party.IsArray())
    return false;
  int party_count = yytk->CallBuiltin("array_length", {party}).ToInt32();
  if (!party_count)
    return false;

  ImGui::BeginChild("select", ImVec2(0, 140), ImGuiChildFlags_Borders);
  static std::string selected = "";
  RValue selected_beastie;
  for (int i = 0; i < party_count; i++)
  {
    RValue beastie = party[i];
    bool is_selected = beastie["pid"].ToString() == selected;
    RValue char_dic = yytk->CallBuiltin("variable_global_get", {"char_dic"});
    RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, beastie["specie"]});
    std::string name = std::format("{}#{} - {} - Lvl {}", beastie["name"].ToString(), beastie["number"].ToString(), species["name"].ToString(), beastie["level"].ToString());
    if (ImGui::Selectable(name.c_str(), is_selected))
    {
      selected = beastie["pid"].ToString();
      is_selected = true;
      selected_copy = CopyBeastieData(beastie);
    }
    if (is_selected)
      selected_beastie = beastie;
  }
  ImGui::EndChild();

  if (selected_beastie.ToBoolean())
  {
    SelectedBeastie(selected_beastie);
  }
  return true;
}

void PartyTab()
{
  if (!ImGui::Begin("Party"))
  {
    ImGui::End();
    return;
  }
  if (!DrawParty())
    ImGui::Text("No Party.");

  ImGui::End();
}
