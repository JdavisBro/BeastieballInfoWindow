#include <YYToolkit/YYTK_Shared.hpp>
using namespace Aurie;
using namespace YYTK;

#include "imgui.h"
#include "imgui/misc/cpp/imgui_stdlib.h"
#include "../../ModuleMain.h"
#include "../../Hooks.h"
#include "../../Utils.h"
#include "../../Storage.h"

#include <numbers>

#include "GachaTab.h"

#include "CheatsTab.h"

namespace GachaTab {

bool is_backup = false;

// MARK: File Hooks

std::vector<RValue> val;

std::string GetFilename(std::string name)
{
  if (name.starts_with("save") || name.starts_with("backup_save")) {
    return "gacha/" + name;
  }
  return name;
}

void SetFilename(RValue **name_rvalue)
{
  std::string name = (*name_rvalue)->ToString();
  if (name.starts_with("save") || name.starts_with("backup_save")) {
    RValue new_value = RValue("gacha/" + name);
    if (val.empty()) val.push_back(new_value);
    else val[0] = new_value;
    *name_rvalue = val.data();
  }
}

PFUNC_YYGMLScript fileReadString = nullptr;
RValue &FileReadString(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  SetFilename(Args);
  fileReadString(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}
PFUNC_YYGMLScript fileWriteString = nullptr;
RValue &FileWriteString(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  SetFilename(Args);
  fileWriteString(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

TRoutine file_exists = nullptr;
void FileExists(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  std::vector<RValue> args = {Args[0]};
  RValue *args_ptr = args.data();
  SetFilename(&Args);
  file_exists(Result, Self, Other, numArgs, Args);
}

TRoutine file_delete = nullptr;
void FileDelete(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  SetFilename(&Args);
  file_delete(Result, Self, Other, numArgs, Args);
}
TRoutine file_copy = nullptr;
void FileCopy(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  std::vector<RValue> args = {RValue(GetFilename(Args[0].ToString())), RValue(GetFilename(Args[1].ToString()))};
  file_copy(Result, Self, Other, numArgs, args.data());
}

// MARK: Other Hooks

TRoutine keyboard_check_pressed = nullptr;
void KeyboardCheckPressed(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  if (Args[0].ToDouble() == 119) { // f8 key
    Result = RValue(false);
  }
  else {
    keyboard_check_pressed(Result, Self, Other, numArgs, Args);
  }
}

PFUNC_YYGMLScript feedbackSubmit = nullptr;
RValue &FeedbackSubmit(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  // no feedback for you greg 😈
  return ReturnValue;
}

PFUNC_YYGMLScript analyticsSubmit = nullptr;
RValue &AnalyticsSubmit(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  return ReturnValue;
}

PFUNC_YYGMLScript tameAdjust = nullptr;
RValue &TameAdjust(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  // tameAdjust(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}
PFUNC_YYGMLScript evAdjust = nullptr;
RValue &EvAdjust(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  int bad_index = yytk->CallBuiltin("array_get_index", {*Args[0], "train"}).ToInt32();
  if (bad_index >= 0)
    yytk->CallBuiltin("array_delete", {*Args[0], bad_index, 2});
  evAdjust(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

void BuiltinHook(const char *HookId, const char *FnName, PVOID HookFunction, PVOID *Trampoline)
{
  PVOID function = nullptr;
  yytk->GetNamedRoutinePointer(FnName, &function);
  AurieStatus last_status = MmCreateHook(
    g_ArSelfModule,
    HookId,
    function,
    HookFunction,
    Trampoline);
  if (!AurieSuccess(last_status))
    DbgPrintEx(LOG_SEVERITY_WARNING, "Failed to create hook for %s", FnName);
  else
    DbgPrintEx(LOG_SEVERITY_INFO, "Hook created for %s!", FnName);
}

struct TextPos {
  const char *text;
  double x;
  double y;
  double scale = 1.0;
  int color = 0xFFFFFF;
  bool front_of_beasties = false;
  bool scribble = false;
};

struct BeastiePos {
  const char *species;
  const char *animation;
  double x;
  double y;
  double x_scale = 1.0;
  double y_scale = 1.0;
  bool front_of_text = false;
  double rotation = 0.0;
  double alpha = 1.0;
};

struct SpritePos {
  const char *sprite;
  double index;
  double x;
  double y;
  double x_scale = 1.0;
  double y_scale = 1.0;
  double rotation = 0.0;
  double rotating_speed = 0.0;
  bool front_of_beastie = true;
};

struct PullButtonPos {
  double x;
  double y;
  int selection_x;
  int selection_y;
};

struct BeastieDrop {
  const char *family;
  double weight = 1;
};

struct ItemDrop {
  const char *item;
  int rarity = 3;
  double weight = 3 / (double)rarity * 2;
};

std::vector<ItemDrop> default_item_drops = {
  {"heal_b", 1}, {"heal_c", 2},
  {"status_b", 2}, {"heal_max", 3},
  {"status_a", 1},
  {"sell1", 2}, {"sell2", 3},
  {"train_ba", 1}, {"train_ha", 1}, {"train_ma", 1},
  {"train_bd", 1}, {"train_hd", 1}, {"train_md", 1},
  {"train_level", 2}, {"train_reset", 3}, {"train_trait", 3},
  {"trainspray_a", 1}, {"trainspray_b", 2}, {"trainspray_c", 3},
};

struct DropRates {
  std::vector<BeastieDrop> drops_5;
  std::vector<BeastieDrop> drops_4;
  std::vector<ItemDrop> drops_other;
  double weight_5 = 0.02;
  double weight_4 = 0.20;
  double weight_other = 0.78;
};

struct GachaType {
  const char *name;
  std::vector<TextPos> text;
  std::vector<BeastiePos> beastie_layout;
  std::vector<SpritePos> sprites;
  PullButtonPos button_pos[3];
  int down_x_pos;
  DropRates rates;
  const char *level_name;
};

#define scentered "[fa_center][fa_middle]"

std::map<std::string, GachaType> gachas = {
  {"amberstone", {
    "Amberstone",
    {
      {"Beasts of", 0.2, 0.38, 1.5},
      {"AMBERSTONE", 0.26, 0.48, 1.5, 0x57a2ff},
      {"Servace", 0.76, 0.29, 1.2, 0xFFFFFF, true},
      {scentered"[scale,0.125][sprBall,2][sprBall,2][sprBall,2][sprBall,2][sprBall,2]", 0.756, 0.34, 1, 0xFFFFFF, true, true},
      {"#1 Spiker", 0.757, 0.375, 0.5, 0xFFFFFF, true},
      {"Drop Rate Up!", 0.828, 0.24, 0.4, 0xFFFFFF, true},
    },
    {
      {"serval", "volley", 0.46, 0.76, 1.2, 1.2, true},
      {"possum", "good", 0.85, 0.78, -1, 1},
      {"moth", "good", 0.66, 0.84, 1, 1, true},
      {"platypus2", "spike", 0.3, 0.9, 1, 1},
      {"lyrebird", "spike", 0.15, 0.995, 1, 1, true},
      {"dog", "ready", 0.225, 1.0, 1, 1, true},
      {"cheerleader", "good", 0.45, 0.92, 1.3, 1.3, true},
      {"daredevil", "spike", 0.7, 1.02, -1.2, 1.2, true},
      {"kangaroo", "menu", 0.9, 0.93, -1, 1, true},
      {"bestie", "good", 0.55, 0.98, -1.1, 1.1, true},
      {"dragonfly", "volley", 0.89, 0.3, 1, 1, true},
    },
    {
      {"sprBall", 2, 0.39, 0.2, 0.7, 0.7, 0, 1.5},
      {"sprBall", 1, 0.39, 0.2, 0.7, 0.7},
    },
    { { 0.45, 0.825, 1, 1 }, {0.45, 0.925, 1, 2}, {0.14, 0.26, 0, 1} },
    1,
    {
      { {"serval1", 3}, {"cheerleader1"}, {"daredevil1"}, {"bestie"} },
      { {"platypus1"}, {"moth1"}, {"possum1"}, {"lyrebird1"}, {"dog1"}, {"kangaroo1"}, {"dragonfly"}, {"disruptor"} },
      default_item_drops,
    },
    "gacha_amberstone",
  }},
  {"starter", {
    "Starters",
    {
      {"Beginning to end,", 0.3, 0.83, 1.25, 0xFFFFFF, true},
      {scentered"[scale,1.25][rainbow]with you", 0.35, 0.925, 1, 0xFFFFFF, true, true},
      {"Sprecko", 0.08, 0.73, 0.5, 0xFFFFFF, true},
      {"Bildit", 0.31, 0.73, 0.5, 0xFFFFFF, true},
      {"Kichik", 0.57, 0.73, 0.5, 0xFFFFFF, true},
      {"Axolati", 0.775, 0.73, 0.5, 0xFFFFFF, true},
    },
    {
      {"frog", "good", 0.775, 0.73, 1.2, 1.2, false, 0, 0.1},
      {"frog2", "good", 0.775, 0.73, 1.2, 1.2, false, 0, 0.25},
      
      {"cassowary", "spike", 0.57, 0.73, 1.2, 1.2, false, 0, 0.1},
      {"cassowary2", "spike", 0.57, 0.73, 1.2, 1.2, false, 0, 0.25},
      
      {"bilby", "ready", 0.31, 0.73, -1.2, 1.2, false, 0, 0.1},
      {"bilby2", "ready", 0.31, 0.73, -1.2, 1.2, false, 0, 0.25},

      {"shroom_b", "good", -0.0395, 0.73, -1.2, 1.2, false, 0, 0.15},
      {"shroom_s", "good", 0.05, 0.73, -1.2, 1.2, false, 0, 0.15},
      {"shroom_m", "good", 0.18, 0.73, -1.2, 1.2, false, 0, 0.15},
            
      {"frog1", "good", 0.775, 0.73, 1.2, 1.2},
      {"cassowary1", "spike", 0.57, 0.73, 1.2, 1.2},
      {"shroom1", "good", 0.05, 0.73, -1.2, 1.2},
      {"bilby1", "ready", 0.31, 0.73, -1.2, 1.2},
    },
    {
      {"sprBall", 2, 0.46, 0.37, 0.7, 0.7, 0, 1.5},
      {"sprBall", 1, 0.46, 0.37, 0.7, 0.7},
    },
    { { 0.8, 0.8, 1, 1 }, { 0.8, 0.9, 1, 2 }, { 0.15, 0.2, 0, 1 } },
    1,
    {
      { {"shroom1"} },
      { {"cassowary1"}, {"frog1"}, {"bilby1"} },
      default_item_drops,
      0.02, 0.10, 0.88,
    },
    "gacha_amberstone",
  }},
};

GachaType *active_gacha = &gachas["amberstone"];

size_t gacha_count = 2;

enum GachaResultType {
  GACHA_BEASTIE,
  GACHA_ITEM,
};


struct GachaResultBeastie {
  std::string pid;
  bool raremorph;
  double duplicates = 0;
  int metamorph = 0; // 0: no, positive: metamorph to this index then display, negative: can metamorph, do not display.
};

struct GachaResultItem {
  std::string id;
};

struct GachaResult {
  GachaResultType type;
  int rarity;
  GachaResultBeastie beastie;
  GachaResultItem item;
};

double GetBeastieDuplicates(const RValue &beastie)
{
  double duplicates = floor((beastie["ba_r"].ToDouble() + 0.000001) * 6);
  return min(6, duplicates);
}

const char *coaching_types[] = {
    "ba_r", "ha_r", "ma_r",
    "bd_r", "hd_r", "md_r",
};

bool IsBeastieMatch(const char *family, const RValue &beastie, const RValue &char_dic)
{
  if (!beastie.ToBoolean())
    return false;
  RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, beastie["specie"]});
  double duplicates = GetBeastieDuplicates(beastie);
  bool hidden = species["hidden"].ToBoolean();
  if (((!hidden && species["family"].ToString() == family) || (hidden && species["id"].ToString() == family)) && duplicates < 6)
    return true;
  return false;
}

std::map<std::string, std::vector<std::string>> metamorph_lines = {
  {"shroom1", {"shroom1", "shroom_b", "shroom_s", "shroom_m"}},
  {"cassowary1", {"cassowary1", "cassowary2", "cassowary"}},
  {"frog1", {"frog1", "frog2", "frog"}},
  {"bilby1", {"bilby1", "bilby2", "bilby"}},
  // AMBERSTONE
  {"moth1", {"moth1", "moth2", "moth"}},
  {"possum1", {"possum1", "possum2", "possum"}},
  {"lyrebird1", {"lyrebird1", "lyrebird"}},
  {"dog1", {"dog1", "dog"}},
  {"kangaroo1", {"kangaroo1", "kangaroo"}},
  {"dragonfly", {"dragonfly"}},
  {"platypus1", {"platypus1", "platypus2"}},
  {"serval1", {"serval1", "serval"}},
  {"daredevil1", {"daredevil1", "daredevil"}},
  {"disruptor", {"disruptor"}},
  {"bestie", {"bestie"}},
  {"cheerleader1", {"cheerleader1", "cheerleader"}},
  // WOODS
  {"alien1", {"alien1", "alien"}},
  {"football1", {"football1", "football2", "football"}},
  {"clown1", {"clown1", "clown"}},
  {"ghost1", {"ghost1", "ghost2", "ghost"}},
  {"shy", {"shy"}},
  {"tricky1", {"tricky1", "tricky"}},
  {"okapi", {"okapi"}},
  {"mantis", {"mantis"}},
  {"monkey", {"monkey"}},
  {"fox1", {"fox1", "fox"}},
  // OCEAN
  {"turtle1", {"turtle1", "turtle2", "turtle"}},
  {"shark1", {"shark1", "shark"}},
  {"seabird1", {"seabird1", "seabird"}},
  {"jellyfish1", {"jellyfish1", "jellyfish"}},
  {"rainbow", {"rainbow"}},
  {"crab", {"crab"}},
  {"seal1", {"seal1", "seal"}},
  {"mudskipper", {"mudskipper"}},
  {"croc", {"croc"}},
  {"clam", {"clam"}},
  {"horseshoe", {"horseshoe"}},
  {"psychic", {"psychic"}},
  // MINES (probably add to woods)
  {"wizard", {"wizard"}},
  {"rocklizard1", {"rocklizard1", "rockllizard2", "rocklizard"}},
  {"millipede1", {"millipede1", "millipede"}},
  // CITY
  {"rat", {"rat"}},
  {"magpie1", {"magpie1", "magpie"}},
  {"nerd1", {"nerd1", "nerd"}},
  {"snake", {"snake"}},
  {"bat1", {"bat1", "bat"}},
  {"ibis", {"ibis"}},
  {"gremlin", {"gremlin"}},
  {"opossum", {"opossum"}},
  // Eburneaen Cavern (probably add to ocean)
  {"swift", {"swift"}},
  {"olm1", {"olm1", "olm"}},
  // Mountain (add with extincts?)
  {"spirit1", {"spirit1", "spirit", "snowspirit"}},
  {"beluga", {"beluga"}},
  {"yeti", {"yeti"}},
  // extincts don't need to be here.
};

int BeastieCanMetamorph(const RValue &beastie, const RValue &species, const char *family_id)
{
  if (species["hidden"].ToBoolean())
    return 0; // extincts have no metamorph line.
  double duplicates = GetBeastieDuplicates(beastie);
  if (strcmp(family_id, "shroom1") > -1 || strcmp(family_id, "tricky1") > -1)
    return -(duplicates >= 3); // location morphs are done differently
  if (!metamorph_lines.contains(family_id))
    return 0;
  std::string my_id = beastie["specie"].ToString();
  std::vector<std::string> line = metamorph_lines[family_id];
  double line_pos = 0;
  double line_max = double(line.size() - 1);
  bool is_humflit = strcmp(family_id, "spirit1") > -1;
  for (int i = 0; i <= line_max; i++) {
    std::string &id = line[i];
    if (id == my_id || (is_humflit && i == 1)) {
      line_pos = i;
      break;
    }
  }
  int should_pos = (int)(min(line_max, floor(duplicates / 6.0 * (double)(line_max + 1))));
  if (line_pos < should_pos) {
    if (is_humflit) {
      double variant = beastie["variant"].ToDouble();
      if (variant == 1) {
        RValue level = yytk->CallGameScript("gml_Script_level_get_data", {});
        return 1 + (level["palette_name"].ToString() == "mountain" || level["name"].ToString().starts_with("meadows"));
      }
      return 1 + (variant == 2);
    }
    return 1;
  }
  return 0;
}

PFUNC_YYGMLScript charDeposit = nullptr;
RValue &CharDeposit(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue beastie = *Args[0]; // prevent bat from evolving, it shouldn't get the yearning but just in case
  if (beastie["yearning"].ToString() == "reserve")
    beastie["yearning"] = "";
  charDeposit(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

PFUNC_YYGMLScript classBeastieTameRatingString = nullptr;
RValue &ClassBeastieTameRatingString(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  ReturnValue = RValue(std::format("{:.0f}/6", GetBeastieDuplicates(Self->ToRValue())));
  return ReturnValue;
}

bool CheckMetamorph(const RValue &beastie)
{
  RValue char_dic = Utils::GlobalGet("char_dic");
  RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, beastie["specie"]});
  RValue family_species = yytk->CallBuiltin("ds_map_find_value", {char_dic, species["family"]});
  return BeastieCanMetamorph(beastie, family_species, family_species["id"].ToCString()) > 0;
}

extern bool specie_in_party_must_metamorph = false;

PFUNC_YYGMLScript beastieSpecieInPartyArray = nullptr;
RValue &BeastieSpecieInPartyArray(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  bool is_tricky = Args[0]->ToString() == "tricky1";
  int array_length = yytk->CallBuiltin("array_length", {ReturnValue}).ToInt32();
  // other uses are for spreckomorph -> shroom so disable that too.
  if (is_tricky && array_length) {
    beastieSpecieInPartyArray(Self, Other, ReturnValue, numArgs, Args);
    for (int i = 0; i < array_length; i++) {
      RValue beastie = ReturnValue[i];
      RValue char_dic = Utils::GlobalGet("char_dic");
      RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, beastie["specie"]});
      RValue family_species = yytk->CallBuiltin("ds_map_find_value", {char_dic, species["family"]});
      if (BeastieCanMetamorph(beastie, family_species, family_species["id"].ToCString()) == 0) {
        yytk->CallBuiltin("array_delete", {ReturnValue, i, 1});
        i--;
        array_length--;
      }
    }
  }
  return ReturnValue;
}

PFUNC_YYGMLScript beastieSpecieInParty = nullptr;
RValue &BeastieSpecieInParty(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  beastieSpecieInParty(Self, Other, ReturnValue, numArgs, Args);
  if (specie_in_party_must_metamorph && ReturnValue.ToBoolean()) {
    RValue char_dic = Utils::GlobalGet("char_dic");
    RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, ReturnValue["specie"]});
    RValue family_species = yytk->CallBuiltin("ds_map_find_value", {char_dic, species["family"]});
    if (BeastieCanMetamorph(ReturnValue, family_species, family_species["id"].ToCString()) == 0)
      ReturnValue = RValue();
  }
  return ReturnValue;
}

PFUNC_YYGMLScript trickiesCheck = nullptr;
RValue &TrickiesCheck(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  specie_in_party_must_metamorph = true;
  trickiesCheck(Self, Other, ReturnValue, numArgs, Args);
  specie_in_party_must_metamorph = false;
  return ReturnValue;
}

PFUNC_YYGMLScript dataUpdatePlayerPos = nullptr;
RValue &DataUpdatePlayerPos(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  RValue name = numArgs > 0 && Args[0]->m_Kind != VALUE_UNDEFINED ? *Args[0] : Utils::InstanceGet(Utils::GetObjectInstance("objLevel"), "level_data")["name"];
  if (name.ToString().starts_with("gacha"))
    return ReturnValue;
  dataUpdatePlayerPos(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

bool is_encounter = false;

PFUNC_YYGMLScript beastieEncounterGenerate = nullptr;
RValue &BeastieEncounterGenerate(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  is_encounter = true;
  beastieEncounterGenerate(Self, Other, ReturnValue, numArgs, Args);
  is_encounter = false;
  return ReturnValue;
}

PFUNC_YYGMLScript classBeastieCanEvolve = nullptr;
RValue &ClassBeastieCanEvolve(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  if (is_encounter || Utils::GlobalGet("ROGUELIKE").ToBoolean())
    classBeastieCanEvolve(Self, Other, ReturnValue, numArgs, Args);
  else
    ReturnValue = RValue(CheckMetamorph(Self->ToRValue()) ? 0 : -1);
  return ReturnValue;
}

PFUNC_YYGMLScript classBeastieWaitingToMorph = nullptr;
RValue &ClassBeastieWaitingToMorph(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{

  if (is_encounter || Utils::GlobalGet("ROGUELIKE").ToBoolean())
    classBeastieWaitingToMorph(Self, Other, ReturnValue, numArgs, Args);
  else
    ReturnValue = RValue(CheckMetamorph(Self->ToRValue()));
  return ReturnValue;
}

RValue EnsureBeastieStats(const RValue &beastie, const char *family_id, const RValue &species, GachaResultBeastie &result, double duplicates = -1)
{
  if (duplicates == -1)
    duplicates = GetBeastieDuplicates(beastie) + 1;
  for (const char *type : coaching_types)
    Utils::InstanceSet(beastie, type, duplicates / 6);
  result.pid = beastie["pid"].ToString();
  result.raremorph = floor(beastie["color"][0].ToDouble()) == 1;
  result.duplicates = GetBeastieDuplicates(beastie);
  result.metamorph = BeastieCanMetamorph(beastie, species, family_id);
  return beastie;
}

RValue CreateBeastie(const char *family, GachaResultBeastie &result)
{
  RValue char_dic = Utils::GlobalGet("char_dic");
  RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, family});
  // check if species already exists without max coached
  std::vector<RValue> party = Utils::GlobalGet("team_party").ToVector();
  for (RValue &beastie : party)
    if (IsBeastieMatch(family, beastie, char_dic))
      return EnsureBeastieStats(beastie, family, species, result);
  RValue registry = Utils::GlobalGet("team_registry");
  std::vector<RValue> registry_keys = yytk->CallBuiltin("variable_instance_get_names", {registry}).ToVector();
  for (RValue &pid : registry_keys)
    if (IsBeastieMatch(family, registry[pid.ToString()], char_dic))
      return EnsureBeastieStats(registry[pid.ToString()], family, species, result);
  RValue beastie = Utils::CallStructMethod(species, "generate", {5, 1});
  yytk->CallGameScript("gml_Script_char_new_register", {beastie, 1});
  return EnsureBeastieStats(beastie, family, char_dic, result, 0);
}

std::vector<GachaResult> gacha_pull;

const int gacha_scene_progress_start = -1;
enum GachaSceneProgress {
  GACHA_SCENE_BEGIN = -1,
  GACHA_SCENE_POST = 10,
  GACHA_SCENE_DESTROY = 11,
};
int gacha_scene_progress = gacha_scene_progress_start;
int gacha_scene_drawing = gacha_scene_progress_start;

struct CameraLocation {
  double x;
  double y;
  double z;
  double pan_angle = 0;
  double height_angle = 80;
  double dist = -200;
};

const int pan_angle_default_turn = -20;

struct Vec3 {
  double x;
  double y;
  double z;
};

struct ItemDrawer {
  Vec3 pos;
  Vec3 middle_pos;
  Vec3 end_pos;
  int index;
  double dir = 1;
  bool draw_index = false;
  double rotation_x = 0;
  double rotation_y = 0;
  double rotation_z = 0;
};

struct BeastieDrawer {
  Vec3 pos;
  Vec3 middle_pos;
  Vec3 end_pos;
  double x;
  double y;
  double dir = 1;
  double angle = 0;
  std::string anim = "spike";
  double color = 0xFFFFFF;
  double color2 = 0xFFFFFF;
};

struct ImpactPos {
  Vec3 pos;
  bool drawing = false;
  double color = 0x8DDDF4;
  double color2 = 0xFFFFFF;
};

struct PullTextDisplay {
  double name = 0;
  std::string name_string = "";
  double rarity = 0;
  double coached = 0;
  bool press_to_continue = false;
  double continue_time = 0;
};

PullTextDisplay text_display;

bool do_scene_render = false;

std::vector<CameraLocation> camera_locations;
const double tween_speed = 1; // per second
double tween_progress = 0;

std::vector<ItemDrawer> item_drawers;
std::vector<BeastieDrawer> beastie_drawers;
ImpactPos impact;

double EaseInSin(double from, double to, double x)
{
  return from + (to - from) * (1 - cos(x * std::numbers::pi / 2));
}

double EaseOutSin(double from, double to, double x)
{
  return from + (to - from) * sin(x * std::numbers::pi / 2);
}

double Linear(double from, double to, double x)
{
  return from + (to - from) * x;
}

inline double Interp(double from, double to, double x)
{
  return EaseOutSin(from, to, x);
}

CameraLocation CameraInterp(CameraLocation &from, CameraLocation &to, double x)
{
  return {Interp(from.x, to.x, x), Interp(from.y, to.y, x), Interp(from.z, to.z, x), Interp(from.pan_angle, to.pan_angle, x), Interp(from.height_angle, to.height_angle, x), Interp(from.dist, to.dist, x)};
}

CameraLocation scene_camera;

void SetCameraLocation(CameraLocation camera, const RValue &scene_manager)
{
  scene_camera = camera;
  RValue shot = Utils::InstanceGet(scene_manager, "scene_camshot");
  shot["look_x"] = camera.x;
  shot["look_y"] = camera.y;
  shot["look_z"] = camera.z - 30;
  shot["pan_angle"] = camera.pan_angle;
  shot["height_angle"] = camera.height_angle;
  shot["dist"] = camera.dist;
}

Vec3 Vec3EaseInSin(Vec3 from, Vec3 to, double x)
{
  return {EaseInSin(from.x, to.x, x), EaseInSin(from.y, to.y, x), EaseInSin(from.z, to.z, x)};
}

Vec3 Vec3Interp(Vec3 from, Vec3 to, double x)
{
  return {Interp(from.x, to.x, x), Interp(from.y, to.y, x), Interp(from.z, to.z, x)};
}

void SceneDestroy()
{
  RValue renderers = Utils::GlobalGet("GACHA_SCENE_RENDERERS");
  size_t gacha_pull_size = gacha_pull.size();
  for (size_t i = 0; i < gacha_pull_size; i++)
    if (gacha_pull[i].type == GACHA_BEASTIE)
      Utils::CallStructMethod(renderers[i], "cleanup", {});
  yytk->CallBuiltin("array_resize", {renderers, 0});
  gacha_scene_progress = gacha_scene_progress_start;
  gacha_scene_drawing = gacha_scene_progress_start;
  camera_locations.clear();
  gacha_pull.clear();
  beastie_drawers.clear();
  tween_progress = 0;
  impact = {};
  text_display = {};
  do_scene_render = false;
}

bool MySceneFrame()
{
  RValue delta_rv;
  yytk->GetBuiltin("delta_time", nullptr, NULL_INDEX, delta_rv);
  RValue scene_manager = Utils::GetObjectInstance("objSceneManager");
  double delta = delta_rv.ToDouble() / 1'000'000;
  size_t gacha_pull_size = gacha_pull.size();
  if (!Utils::GlobalExists("GACHA_SCENE_RENDERERS") || !yytk->CallBuiltin("array_length", {Utils::GlobalGet("GACHA_SCENE_RENDERERS")}).ToBoolean()) {
    Utils::InstanceSet(Utils::GetObjectInstance("objPlayer"), "visible", false);
    RValue renderers = yytk->CallBuiltin("array_create", {gacha_pull_size});
    camera_locations.clear();
    camera_locations.resize(gacha_pull_size + 2);
    item_drawers.resize(gacha_pull_size);
    beastie_drawers.resize(gacha_pull_size);
    RValue item_dic = Utils::GlobalGet("item_dic");
    for (int i = 0; i < 2; i++)
    {
      RValue node = yytk->CallGameScript("gml_Script_levelnode_find", {RValue("gacha_start" + std::to_string(i))});
      camera_locations[i] = {node["x"].ToDouble(), node["y"].ToDouble(), node["z"].ToDouble()};
    }
    Utils::InstanceSet(scene_manager, "scene_camshot", yytk->CallGameScript("gml_Script_ElephantFromJSON", {yytk->CallBuiltin("json_parse", {R"({"_": "class_camerashot"})"})}));
    Utils::InstanceSet(scene_manager, "scene_camshot_speed", 1);
    SetCameraLocation(camera_locations[0], scene_manager);
    Vec3 start_pos = {camera_locations[1].x, camera_locations[1].y + 60, camera_locations[1].z - 400};
    RValue global_renderers = Utils::GlobalGet("char_renderers");
    for (size_t i = 0; i < gacha_pull_size; i++) {
      GachaResult &result = gacha_pull[i];
      RValue node = yytk->CallGameScript("gml_Script_levelnode_find", {RValue("gacha_location" + std::to_string(i))});
      CameraLocation prev_location = camera_locations[i + 1];
      CameraLocation camera_location = {node["x"].ToDouble(), node["y"].ToDouble(), node["z"].ToDouble() - 50};
      if (prev_location.pan_angle == 0) {
        camera_location.pan_angle = prev_location.pan_angle > 0 ? pan_angle_default_turn : -pan_angle_default_turn;
      }
      else {
        camera_location.pan_angle = camera_location.x > prev_location.x ? pan_angle_default_turn : -pan_angle_default_turn;
      }
      Vec3 pos = {camera_location.x, camera_location.y, camera_location.z};
      double dir = camera_location.x > prev_location.x ? -1 : 1;
      camera_location.x += 100 * dir;
      camera_location.y -= 10;
      camera_locations[i + 2] = camera_location;
      item_drawers[i] = {{0, 0, -1000}, {start_pos.x + (pos.x - start_pos.x) * 0.75, start_pos.y + (pos.y - start_pos.y) * 0.5, pos.z + 600}, pos, 6, dir};
      if (result.type == GACHA_BEASTIE) {
        RValue renderer = yytk->CallGameScript("gml_Script_ElephantFromJSON", {yytk->CallBuiltin("json_parse", {R"({"_": "class_beastie_renderer"})"})});
        RValue beastie = yytk->CallGameScript("gml_Script_char_find_by_pid", {RValue(result.beastie.pid)});
        beastie = yytk->CallGameScript("gml_Script_ElephantDuplicate", {beastie});
        beastie["ba_r"] = result.beastie.duplicates / 6;
        if (result.beastie.metamorph > 0) {
          while (Utils::CallStructMethod(beastie, "can_evolve", {}).ToDouble() > -1) {
            Utils::CallStructMethod(beastie, "evolve", {result.beastie.metamorph - 1});
          }
        }
        RValue anim_data = Utils::CallStructMethod(beastie, "anim_data", {4});
        renderer["x"] = 0;
        renderer["y"] = 0;
        renderer["z"] = -1000;
        Utils::CallStructMethod(renderer, "set_char", {beastie});
        BeastieDrawer beastie_pos;
        double scale = renderer["scale"].ToDouble();
        dir = -dir;
        renderer["image_xscale"] = dir * scale;
        beastie_pos.x = anim_data["x"].ToDouble() * scale;
        beastie_pos.y = anim_data["y"].ToDouble() * scale;
        beastie_pos.dir = dir;
        double ang = (camera_location.pan_angle + 90 + (dir < 0 ? 180 : 0)) / 180 * std::numbers::pi;
        beastie_pos.middle_pos = {pos.x - 2000 * sin(ang), pos.y - 2000 * cos(ang) , 0};
        beastie_pos.end_pos = pos;
        double beastie_angle = anim_data["angle"].ToDouble();
        beastie_pos.angle = (beastie_angle > 180 ? beastie_angle - 360 : beastie_angle) * dir;
        beastie_pos.anim = anim_data["anim"].ToString();
        RValue type = Utils::CallStructMethod(beastie, "specie_data", {})["type_focus"];
        beastie_pos.color = yytk->CallGameScript("gml_Script_type_color", {type}).ToDouble();
        beastie_pos.color2 = yytk->CallGameScript("gml_Script_type_colord", {type}).ToDouble();
        renderers[i] = renderer;
        yytk->CallBuiltin("array_push", {global_renderers, renderer});
        beastie_drawers[i] = beastie_pos;
      }
      else {
        RValue item = yytk->CallBuiltin("ds_map_find_value", {item_dic, RValue(result.item.id)});
        item_drawers[i].index = item["img"].ToInt32();
      }
    }
    Utils::GlobalSet("GACHA_SCENE_RENDERERS", renderers);
  }
  RValue renderers = Utils::GlobalGet("GACHA_SCENE_RENDERERS");
  tween_progress += delta;
  bool scene_skipping = Utils::InstanceGet(scene_manager, "scene_skipping").ToBoolean();
  do_scene_render = true;
  switch (gacha_scene_progress) {
  case GACHA_SCENE_BEGIN: {
    SetCameraLocation(CameraInterp(camera_locations[0], camera_locations[1], min(2, tween_progress) / 2), scene_manager);
    CameraLocation cam_pos = CameraInterp(camera_locations[0], camera_locations[1], 0.5);
    Vec3 start_pos = {cam_pos.x, cam_pos.y, cam_pos.z - 300};
    if (scene_skipping) tween_progress = 4;
    if (tween_progress >= 2 && tween_progress < 3) {
      for (ItemDrawer &item : item_drawers) {
        item.pos = Vec3Interp(start_pos, item.middle_pos, tween_progress - 2);
      }
    }
    if (tween_progress >= 2 && tween_progress - delta < 2) {
      yytk->CallGameScript("gml_Script_container_play", {"ui_transition_swipe_in"});
      Utils::InstanceSet(scene_manager, "screen_shake_amt", 4);
      Utils::InstanceSet(scene_manager, "screen_shake_time", 0.2);
    }
    if (tween_progress >= 3) {
      for (ItemDrawer &item : item_drawers) {
        item.pos = Vec3Interp(item.middle_pos, item.end_pos, min(4, tween_progress) - 3);
      }
      if (tween_progress >= 3.5 && tween_progress - delta < 3.5) {
        yytk->CallGameScript("gml_Script_container_play", {"ui_ball_move_slowDown"});
      }
    }
    if (tween_progress >= 4) {
      tween_progress = 0;
      gacha_scene_progress += 1;
      return true;
    }
    break;
  }
  case GACHA_SCENE_POST: {
    impact.drawing = false;
    text_display = {};
    gacha_scene_drawing = gacha_scene_progress_start;
    if (scene_skipping) {
      tween_progress = 2.1;
      Utils::InstanceSet(scene_manager, "scene_skipping", false);
    };
    SetCameraLocation(CameraInterp(camera_locations[gacha_pull_size + 2 - 1], camera_locations[1], min(1, tween_progress / 2)), scene_manager);
    if (tween_progress >= 2) {
      gacha_scene_progress += 1;
      return true;
    }
    break;
  }
  case GACHA_SCENE_DESTROY: {
    SceneDestroy();
    return true;
  }
  default: {
    // gacha index
    if (tween_progress <= delta) {
      text_display = {};
      yytk->CallGameScript("gml_Script_buttonlist_affirmative_reset_release", {});
    }
    GachaResult &result = gacha_pull[gacha_scene_progress];
    double end_progress = 2.85 + result.rarity;
    if (scene_skipping) {
      if (result.rarity < 4 && tween_progress < end_progress)
        tween_progress = end_progress;
      else if (tween_progress < 1)
        tween_progress = 1.01;
    }
    bool is_scene_skipped = result.rarity < 4 && scene_skipping;
    CameraLocation camera_location = camera_locations[gacha_scene_progress + 2];
    SetCameraLocation(CameraInterp(camera_locations[gacha_scene_progress + 1], camera_location, min(1, tween_progress)), scene_manager);
    ItemDrawer &drawer = item_drawers[gacha_scene_progress];
    if (tween_progress > 1)
      gacha_scene_drawing = gacha_scene_progress;
    else
      gacha_scene_drawing = gacha_scene_progress_start;
    if (result.type == GACHA_BEASTIE) {
      RValue renderer = renderers[gacha_scene_progress];
      BeastieDrawer &beastie_pos = beastie_drawers[gacha_scene_progress];
      double prog = max(0, min(1, tween_progress - 1));
      beastie_pos.pos = Vec3EaseInSin(beastie_pos.middle_pos, beastie_pos.end_pos, prog);
      renderer["image_angle"] = EaseInSin(0, beastie_pos.angle, prog);
      if (tween_progress > 1.8 && tween_progress - delta <= 1.8 && renderer["animation_state"][0].ToString() != beastie_pos.anim) {
        Utils::CallStructMethod(renderer["char"], "play_vo_hello", {});
      }
      if (tween_progress > 1.8)
        yytk->CallGameScript("gml_Script_char_animation", {renderer, RValue(beastie_pos.anim), 0, RValue(), false, true});
      if (tween_progress < 2)
        impact.drawing = false;
      if (tween_progress > 1.5 && tween_progress - delta <= 1.5 && !is_scene_skipped) {
        yytk->CallGameScript("gml_Script_audio_param_tween", {"slomo", 100, 0.5});
      }
      if (tween_progress >= 2) {
        if (!impact.drawing && !is_scene_skipped) {
          yytk->CallGameScript("gml_Script_container_play", {"sfx_beastie_high_five"});
          yytk->CallGameScript("gml_Script_eff_raylines", {0.6, 0.5});
          Utils::InstanceSet(scene_manager, "screen_shake_amt", 5);
          Utils::InstanceSet(scene_manager, "screen_shake_time", 0.3);
        }
        impact.pos = drawer.end_pos;
        impact.color = beastie_pos.color;
        impact.color2 = beastie_pos.color2;
        impact.drawing = true;
      }
      if (tween_progress > 2.5 && tween_progress - delta <= 2.5) {
        yytk->CallGameScript("gml_Script_audio_param_tween", {"slomo", 0, 0.25});
      }
    }
    else {
      double new_rot_x = 0;
      if (tween_progress < 1.75) {
        new_rot_x = (tween_progress - 1.5) * 4;
      }
      else {
        drawer.draw_index = true;
        new_rot_x = 3 + (tween_progress - 1.75) * 4;
      }
      if (tween_progress > 1.2 && tween_progress - delta <= 1.2 && !is_scene_skipped) {
        yytk->CallGameScript("gml_Script_audio_param_tween", {"slomo", 100, 0.5});
      }
      if (tween_progress > 1.9) {
        if (!impact.drawing) {
          if (!is_scene_skipped) {
            yytk->CallGameScript("gml_Script_container_play", {"ui_ball_receive_chance"});
            Utils::InstanceSet(scene_manager, "screen_shake_amt", 5);
            Utils::InstanceSet(scene_manager, "screen_shake_time", 0.3);
          }
          yytk->CallGameScript("gml_Script_audio_param_tween", {"slomo", 0, 0.2});
        }
        else if (!impact.drawing) {
        }
        impact.pos = drawer.end_pos;
        impact.color = -1;
        impact.color2 = -1;
        impact.drawing = true;
      }
      else {
        impact.drawing = false;
      }
      drawer.rotation_x = min(4, max(0, new_rot_x)) / 4;
    }
    if (tween_progress >= 2.5) {
      double name_prog = (tween_progress - 2.5) * 4;
      if (name_prog >= 1 && text_display.name < 1 && !is_scene_skipped) {
        yytk->CallGameScript("gml_Script_container_play", {"ui_ball_bounce_gameWinner"});
        Utils::InstanceSet(scene_manager, "screen_shake_amt", 4);
        Utils::InstanceSet(scene_manager, "screen_shake_time", 0.2);
      }
      text_display.name = min(1, name_prog);
      if (result.type == GACHA_BEASTIE)
        text_display.name_string = Utils::CallStructMethod(renderers[gacha_scene_progress]["char"], "displayname", {}).ToString();
      else
        text_display.name_string = yytk->CallBuiltin("ds_map_find_value", {Utils::GlobalGet("item_dic"), RValue(result.item.id)})["name"].ToString();
      if (tween_progress > 2.85)
        tween_progress += delta;
      double rarity_prog = max(0, tween_progress - 2.85);
      if (floor(text_display.rarity) < floor(rarity_prog) && result.rarity >= floor(rarity_prog) && !is_scene_skipped) {
        if (result.rarity > floor(rarity_prog) || result.rarity <= 2)
          yytk->CallGameScript("gml_Script_container_play", {"ui_ball_attack_0"});
        if (result.rarity > 2 && floor(rarity_prog + 1) == result.rarity) {
          yytk->CallGameScript("gml_Script_container_play", {
            result.rarity == 3 ? "ui_ball_attack_33" :
            result.rarity == 4 ? "ui_ball_attack_66" :
                                 "ui_ball_attack_99"
            });
        }
        if (result.rarity > 2 && floor(rarity_prog) == result.rarity) {
          yytk->CallGameScript("gml_Script_container_play", {
            result.rarity == 1 ? "ui_ball_receive_16" :
            result.rarity == 2 ? "ui_ball_receive_33" :
            result.rarity == 3 ? "ui_ball_receive_50" :
            result.rarity == 4 ? "ui_ball_receive_66" :
                                 "ui_ball_receive_83"
            });
        }
        Utils::InstanceSet(scene_manager, "screen_shake_amt", 4);
        Utils::InstanceSet(scene_manager, "screen_shake_time", 0.1 * floor(rarity_prog));
      }
      text_display.rarity = min((double)result.rarity, rarity_prog);
    }
    if (tween_progress >= end_progress) {
      text_display.press_to_continue = true;
      tween_progress = 0;
      gacha_scene_progress += 1;
      if (gacha_scene_progress >= gacha_pull_size)
        gacha_scene_progress = 10;
      return true;
    }
    if (yytk->CallGameScript("gml_Script_buttonlist_released_affirmative", {}).ToBoolean())
      tween_progress = end_progress;
    break;
  }
  }
  return false;
}

void DrawStarburst(double x, double y, double z, double color, double power, double mid_power, double rng = -1)
{
  yytk->CallGameScript("gml_Script_mstack_rotate_ext", {0, 0, scene_camera.pan_angle});
  yytk->CallGameScript("gml_Script_mstack_translate", {x, y, z});
  yytk->CallGameScript("gml_Script_mstack_set_and_clear", {});
  yytk->CallBuiltin("draw_set_color", {color});
  yytk->CallGameScript("gml_Script_draw_starburst_3d", {0, 0, 0, power, mid_power, 1, 10, rng});
}

void DrawPullIndexInWorld(size_t i, RValue &renderers, RValue &shader3dflatsprite, RValue &sprItems)
{
  GachaResult &result = gacha_pull[i];

  ItemDrawer &drawer = item_drawers[i];
  yytk->CallGameScript("gml_Script_mstack_rotate_ext", {scene_camera.height_angle + drawer.rotation_x * 360, drawer.rotation_y * 360, scene_camera.pan_angle + drawer.rotation_z * 360});
  yytk->CallGameScript("gml_Script_mstack_translate", {drawer.pos.x, drawer.pos.y - 10, drawer.pos.z});
  yytk->CallGameScript("gml_Script_mstack_set_and_clear", {});
  yytk->CallBuiltin("shader_set", {shader3dflatsprite});
  yytk->CallBuiltin("draw_sprite_ext", {sprItems, drawer.draw_index ? drawer.index : 6, -32, -32, 1, 1, 0, 0xFFFFFF, 1});
  yytk->CallBuiltin("matrix_set", {2, Utils::GlobalGet("__identity_matrix")});
  if (result.type == GACHA_BEASTIE) {
    RValue renderer = renderers[i];
    BeastieDrawer beastie_pos = beastie_drawers[i];
    double ang = (scene_camera.pan_angle + 90 + (beastie_pos.dir < 0 ? 180 : 0)) / 180 * std::numbers::pi;
    renderer["x"] = beastie_pos.pos.x - beastie_pos.x * sin(ang);
    renderer["y"] = beastie_pos.pos.y - beastie_pos.x * cos(ang);
    renderer["z"] = beastie_pos.pos.z + beastie_pos.y;
    Utils::CallStructMethod(renderer, "draw", {});
  }
}

RValue Scribble(const RValue &text, const RValue &id = RValue())
{
  return yytk->CallGameScript("gml_Script_scribble", {text, id});
}

RValue ScribbleCentered(const std::string &text)
{
  return yytk->CallGameScript("gml_Script_scribble", {RValue(scentered + text)});
}

void DrawTextPgramScreen(double x, double y, const RValue &text_str, double scale, double color, double offset_x = 0)
{
  yytk->CallGameScript("gml_Script_draw_text_pgram", {yytk->CallBuiltin("display_get_width", {}).ToDouble() * x + offset_x, yytk->CallBuiltin("display_get_height", {}).ToDouble() * y, text_str, scale, 0.25,  0, 0, color});
}

void DrawTextScreen(double x, double y, const char *text_str, double scale, double color)
{
  yytk->CallBuiltin("draw_set_color", {color});
  yytk->CallBuiltin("draw_set_halign", {1});
  yytk->CallBuiltin("draw_set_valign", {1});
  yytk->CallBuiltin("draw_text_transformed", {yytk->CallBuiltin("display_get_width", {}).ToDouble() * x, yytk->CallBuiltin("display_get_height", {}).ToDouble() * y, text_str, scale, 1, 0});
}

const double result_text_start = 0.5 - 0.2;
const double result_rarity_start = result_text_start + 0.06;
const double ball_size = 32;

void DrawResultText()
{
  yytk->CallBuiltin("shader_reset", {});
  GachaResult &result = gacha_pull[gacha_scene_drawing];
  ItemDrawer &drawer = item_drawers[gacha_scene_drawing];
  if (text_display.press_to_continue) {
    double x = text_display.continue_time <= 1 ? text_display.continue_time : sin((text_display.continue_time - 0.5) * std::numbers::pi) / 4 + 0.75;
    yytk->CallBuiltin("draw_set_alpha", {x});
    text_display.continue_time += (1. / 60.);
    std::string button = yytk->CallGameScript("gml_Script_buttonlist_to_string", {Utils::GlobalGet("confirm_buttons")}).ToString();
    DrawTextScreen(0.5, 0.9, ("< Press " + button + " to Continue >").c_str(), 1, 0xFFFFFF);
    yytk->CallBuiltin("draw_set_alpha", {1.0});
  }
  double x_begin = (0.5 + 0.25 * drawer.dir);
  if (text_display.name > 0) {
    RValue name = ScribbleCentered(text_display.name_string);
    double prog = (text_display.name - 0.5) * 2;
    Utils::CallStructMethod(name, "scale", {EaseInSin(1.5, 1, max(0, prog))});
    DrawTextPgramScreen(x_begin - (0.05 * EaseInSin(1, 0, max(0, prog))), result_text_start, name, 1, 0xFFFFFF);
  }
  if (text_display.rarity > 0.75) {
    RValue ball = yytk->CallBuiltin("asset_get_index", {"sprBall"});
    double pgram_width = ball_size * (result.rarity + 1);
    double pgram_height = ball_size * 1.25;
    double x = yytk->CallBuiltin("display_get_width", {}).ToDouble() * x_begin - pgram_width / 2;
    double y = yytk->CallBuiltin("display_get_height", {}).ToDouble() * result_rarity_start;
    {
      double prog = max(0, min(1, (text_display.rarity - 0.75) * 4));
      yytk->CallBuiltin("draw_set_color", {0});
      yytk->CallGameScript("gml_Script_draw_pgram", {x, y - pgram_height / 2, EaseInSin(x + pgram_height * 0.25, x + pgram_width, prog), y + pgram_height / 2});
    }
    for (int i = 0; i < text_display.rarity && i < result.rarity; i++) {
      double prog = min(1, (text_display.rarity - i - 0.75) * 4);
      double ball_x = (drawer.dir > 0 ? x + pgram_width : x) + (ball_size * (i + 1) * -drawer.dir);
      double rot = (prog < 1 ? EaseInSin(280, 360, prog) : text_display.rarity * 360 / 2) * drawer.dir;
      if (prog > 0) {
        double x_pos = Linear(ball_x + 70 * drawer.dir, ball_x, prog);
        double y_pos = EaseInSin(y - 80, y, prog);
        if (i >= 2 && i == result.rarity - 1 && (text_display.rarity > i + 1 && text_display.rarity < i + 3)) {
          double color2 = 0xF0F0F0;
          double color = 0xFFFFFF;
          if (result.type == GACHA_BEASTIE) {
            BeastieDrawer &beastie_pos = beastie_drawers[gacha_scene_drawing];
            color2 = beastie_pos.color2;
            color = beastie_pos.color;
          }
          yytk->CallBuiltin("draw_set_color", {0});
          yytk->CallGameScript("gml_Script_draw_starburst", {x_pos, y_pos, ball_size * 0.9});
          yytk->CallBuiltin("draw_set_color", {color2});
          yytk->CallGameScript("gml_Script_draw_starburst", {x_pos, y_pos, ball_size * 1});
          yytk->CallBuiltin("draw_set_color", {color});
          yytk->CallGameScript("gml_Script_draw_starburst", {x_pos, y_pos, ball_size * 0.8});
        }
        yytk->CallBuiltin("draw_sprite_ext", {ball, 2, x_pos, y_pos, ball_size / 256, ball_size / 256, rot, 0xFFFFFF, 1});
      }
    }
    if (text_display.rarity >= result.rarity) {
      RValue delta_rv;
      yytk->GetBuiltin("delta_time", nullptr, NULL_INDEX, delta_rv);
      double delta = delta_rv.ToDouble() / 1'000'000;
      double new_rarity = text_display.rarity + delta * 2;
      if (result.type == GACHA_BEASTIE && new_rarity - 0.65 >= result.rarity && text_display.rarity - 0.65 < result.rarity) {
        RValue renderers = Utils::GlobalGet("GACHA_SCENE_RENDERERS");
        Utils::CallStructMethod(renderers[gacha_scene_drawing]["char"], "play_vo_cheer", {});
      }
      text_display.rarity = new_rarity;
    }
  }
}

void RenderScene()
{
  if (!do_scene_render)
    return;
  RValue renderers = Utils::GlobalGet("GACHA_SCENE_RENDERERS");
  RValue sprItems = yytk->CallBuiltin("asset_get_index", {"sprItems"});
  RValue shader3dflatsprite = yytk->CallBuiltin("asset_get_index", {"shader3DFlatSprite"});
  RValue z_test = yytk->CallBuiltin("gpu_get_ztestenable", {});
  yytk->CallBuiltin("gpu_set_ztestenable", {z_test});
  for (size_t i = 0; i < gacha_pull.size(); i++) {
    if (i == gacha_scene_drawing)
      continue;
    DrawPullIndexInWorld(i, renderers, shader3dflatsprite, sprItems);
  }
  if (impact.drawing) {
    yytk->CallBuiltin("gpu_set_ztestenable", {false});
    yytk->CallBuiltin("shader_set", {shader3dflatsprite});
    double bg_pow = impact.color2 > -1 ? 180 : 70;
    DrawStarburst(impact.pos.x, impact.pos.y, impact.pos.z, 0, bg_pow, bg_pow / 2, -2);
    if (impact.color2 > -1)
      DrawStarburst(impact.pos.x, impact.pos.y, impact.pos.z, impact.color2, 200, 100);
    if (impact.color > -1)
      DrawStarburst(impact.pos.x, impact.pos.y, impact.pos.z, impact.color, 140, 70);
    DrawStarburst(impact.pos.x, impact.pos.y, impact.pos.z, 0xFFFFFF, 90, 45);
    DrawStarburst(impact.pos.x, impact.pos.y, impact.pos.z, 0xF0F0F0, 40, 20);
  }
  if (gacha_scene_drawing >= 0 && gacha_scene_drawing < 10) {
    yytk->CallBuiltin("gpu_set_ztestenable", {false});
    DrawPullIndexInWorld(gacha_scene_drawing, renderers, shader3dflatsprite, sprItems);
  }
  yytk->CallBuiltin("gpu_set_ztestenable", {z_test});
}

bool AmScene(CInstance *Self)
{
  RValue am_scene;
  yytk->CallGameScriptEx(am_scene, "gml_Script_AmScene", Self, nullptr, {});
  return am_scene.ToBoolean();
}

PFUNC_YYGMLScript waitForTween = nullptr;
RValue &WaitForTween(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  if (Args[0]->ToString() == "@@MyScene@@" && AmScene(Self))
    ReturnValue = MySceneFrame();
  else
    waitForTween(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

void DoGachaPull(GachaType &gacha, int pull_count)
{
  if (!yytk->CallGameScript("gml_Script_item_has", {"jersey", pull_count}).ToBoolean())
    return;
  yytk->CallGameScript("gml_Script_item_delete", {"jersey", pull_count});
  if (gacha_scene_progress > gacha_scene_progress_start) {
    yytk->CallGameScript("gml_Script_SceneClear", {});
    SceneDestroy();
  }
  gacha_pull.resize(pull_count);
  for (int i = 0; i < pull_count; i++) {
    GachaResult *result = gacha_pull.data() + i;
    double rng = yytk->CallBuiltin("random", {1}).ToDouble();
    int rarity = 3;
    double weight = gacha.rates.weight_5;
    if (rng <= weight) rarity = 5;
    else {
      weight += gacha.rates.weight_4;
      if (rng <= weight) rarity = 4;
    }
    switch (rarity) {
    case 5:
    case 4: {
      result->type = GACHA_BEASTIE;
      result->rarity = rarity;
      std::vector<BeastieDrop> &drops = rarity == 5 ? gacha.rates.drops_5 : gacha.rates.drops_4;
      double weight = 0;
      for (BeastieDrop &drop : drops)
        weight += drop.weight;
      double rng = yytk->CallBuiltin("random", {weight}).ToDouble();
      weight = 0;
      for (BeastieDrop &drop : drops) {
        weight += drop.weight;
        if (rng <= weight) {
          GachaResultBeastie beastie_result;
          RValue beastie = CreateBeastie(drop.family, beastie_result);
          result->beastie = beastie_result;
          break;
        }
      }
      break;
    }
    case 3: {
      result->type = GACHA_ITEM;
      std::vector<ItemDrop> &drops = gacha.rates.drops_other;
      double weight = 0;
      for (ItemDrop &drop : drops)
        weight += drop.weight;
      double rng = yytk->CallBuiltin("random", {weight}).ToDouble();
      weight = 0;
      for (ItemDrop &drop : drops) {
        weight += drop.weight;
        if (rng <= weight) {
          yytk->CallGameScript("gml_Script_item_get", {drop.item});
          result->item = {drop.item};
          result->rarity = drop.rarity;
          break;
        }
      }
    }
    }
  }
  CInstance *global_inst = nullptr;
  yytk->GetGlobalInstance(&global_inst);
  RValue global = global_inst->ToRValue();
  yytk->CallGameScript("gml_Script_SceneLayerIn", {});
  if (!Utils::InstanceGet(Utils::GetObjectInstance("objLevel"), "level_data")["name"].ToString().starts_with("gacha"))
    yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "data_save_level")});
  yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "savedata_save")});
  yytk->CallGameScript("gml_Script_Fade", {0, 1, 0.25, 1, -3});
  yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "menu_level_out_all")});
  yytk->CallGameScript("gml_Script_LevelTransition", {gacha.level_name, -3});
  yytk->CallGameScript("gml_Script_WaitForTween", {"@@MyScene@@"}); // My Scene - Begin
  for (int i = 0; i < pull_count; i++) {
    GachaResult &result = gacha_pull[i];
    yytk->CallGameScript("gml_Script_WaitForTween", {"@@MyScene@@"}); // My Scene - Gacha Index
    yytk->CallGameScript("gml_Script_WaitForInput", {result.rarity < 4});
  }
  yytk->CallGameScript("gml_Script_WaitForTween", {"@@MyScene@@"}); // My Scene - Post
  RValue menu = Utils::InstanceGet(global, "mn_gacha_results");
  menu["selectX"] = 0;
  menu["selectY"] = 0;
  yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "menu_level_in"), menu});
  yytk->CallGameScript("gml_Script_WaitForMenuOut", {menu});
  yytk->CallGameScript("gml_Script_Fade", {0, 1, 0.25, 1, -3});
  yytk->CallGameScript("gml_Script_Wait", {0.27});
  yytk->CallGameScript("gml_Script_WaitForTween", {"@@MyScene@@"}); // My Scene - Destroy
  yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "data_load_level")});
  yytk->CallGameScript("gml_Script_Wait", {0.1});
  yytk->CallGameScript("gml_Script_Fade", {0, 0, 0.25, 1, -3});
  yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "savedata_save")});
  yytk->CallGameScript("gml_Script_SceneEnd", {});
  yytk->CallGameScript("gml_Script_SceneLayerOut", {});
  gacha_scene_progress = gacha_scene_progress_start;
}

int gacha_open = 0;

void DrawTextPoses(std::vector<TextPos> &text_array, bool after_beasties)
{
  for (TextPos &text : text_array)
  {
    if (text.front_of_beasties != after_beasties) continue;
    RValue text_str = text.text;
    if (text.scribble) {
      text_str = yytk->CallGameScript("gml_Script_scribble", {text.text});
      if (text.color != 0xFFFFFF) {
        Utils::CallStructMethod(text_str, "blend", {text.color});
      }
    }
    yytk->CallGameScript("gml_Script_draw_text_pgram", {yytk->CallGameScript("gml_Script_canvas_x", {text.x, text.y}), yytk->CallGameScript("gml_Script_canvas_y", {text.y}), text_str, text.scale, 0.25,  0, 0, text.color});
  }
}

void DrawBeastieLayout(std::vector<BeastiePos> &beastie_layout, bool after_text)
{
  RValue char_dic = Utils::GlobalGet("char_dic");
  for (BeastiePos &beastie : beastie_layout)
  {
    if (beastie.front_of_text != after_text) continue;
    RValue species = yytk->CallBuiltin("ds_map_find_value", {char_dic, beastie.species});
    yytk->CallGameScript("gml_Script_draw_monster_menu", {
      species,
      yytk->CallGameScript("gml_Script_canvas_x", {beastie.x, beastie.y}), yytk->CallGameScript("gml_Script_canvas_y", {beastie.y}),
      1.0, beastie.alpha, beastie.x_scale, beastie.y_scale, beastie.rotation, beastie.animation
      });
  }
}

void DrawSprites(std::vector<SpritePos> &sprites, bool after_beasties)
{
  for (SpritePos &sprite : sprites)
  {
    if (sprite.front_of_beastie != after_beasties) continue;
    if (sprite.rotating_speed) sprite.rotation += sprite.rotating_speed;
    RValue sprite_asset = yytk->CallBuiltin("asset_get_index", {sprite.sprite});
    yytk->CallBuiltin("draw_sprite_ext", {
      sprite_asset, sprite.index,
      yytk->CallGameScript("gml_Script_canvas_x", {sprite.x, sprite.y}), yytk->CallGameScript("gml_Script_canvas_y", {sprite.y}),
      sprite.x_scale, sprite.y_scale, sprite.rotation, 0xFFFFFF, 1.0
      });
  }
}

double DrawMenuButton(double x, double y, const RValue &text, double selection_x, double selection_y, const RValue &menu, bool selected, bool hover_button = false)
{
  // args: x, y, text, scalex, scaley, color, ?, selection x, selection y, menu, selected, ?, ?, ?, ?
  return yytk->CallGameScript("gml_Script_draw_text_button_menu", {
      yytk->CallGameScript("gml_Script_canvas_x", {x, y}), yytk->CallGameScript("gml_Script_canvas_y", {y}), text, 1, 0.25, selected ? Utils::GlobalGet("menu_color")[5] : 0, 0, selection_x, selection_y, menu
    }).ToBoolean() && (hover_button || yytk->CallGameScript("gml_Script_buttonlist_released_affirmative_raw", {}).ToBoolean());
}

bool DrawPullButton(const char *text, const PullButtonPos &pos, const RValue &menu)
{
  return DrawMenuButton(pos.x, pos.y, Scribble(text), pos.selection_x, pos.selection_y, menu, false);
  // return yytk->CallGameScript("gml_Script_draw_text_button", {
  //   yytk->CallGameScript("gml_Script_canvas_x", {pos.x, pos.y}), yytk->CallGameScript("gml_Script_canvas_y", {pos.y}), yytk->CallGameScript("gml_Script_scribble", {text})}).ToBoolean();
}

void DrawTextPgram(double x, double y, const RValue &text, double scale_x = 1, double scale_y = 0.25, double color = 0, double align = 0)
{
  yytk->CallGameScript("gml_Script_draw_text_pgram", {yytk->CallGameScript("gml_Script_canvas_x", {x, y}), yytk->CallGameScript("gml_Script_canvas_y", {y}), text, scale_x, scale_y, color, align});
}

void DrawJerseyCount()
{
  DrawTextPgram(0.98, 0.035, Scribble(RValue(std::format(scentered"[scale,0.5][sprItems,6] {}", yytk->CallGameScript("gml_Script_item_count", {"jersey"}).ToString()))), 1, 0.25, 0, 2);
}

void DrawControls()
{
  RValue game = Utils::GetObjectInstance("objGame");
  RValue command = Utils::InstanceGet(game, "menu_command_scribble");
  double x = yytk->CallBuiltin("display_get_gui_width", {}).ToDouble() - 10;
  double y = yytk->CallBuiltin("display_get_gui_height", {}).ToDouble() - Utils::CallStructMethod(command, "get_height", {}).ToDouble() / 2;
  yytk->CallGameScript("gml_Script_draw_text_pgram_edge", {x, y, command, 1, 0.25, 0, 2});
}

struct Vec2 {
  double x;
  double y;
};

Vec2 GetMenuPos(double x, double y)
{
  return {yytk->CallGameScript("gml_Script_canvas_x", {x, y}).ToDouble(), yytk->CallGameScript("gml_Script_canvas_y", {y}).ToDouble()};
}

void SetFont()
{
  yytk->CallBuiltin("draw_set_font", {yytk->CallGameScript("gml_Script_font_get", {2})});
}

void GachaResultsHandleInput(RValue &menu)
{
  int old_x = menu["selectX"].ToInt32();
  int old_y = menu["selectY"].ToInt32();
  int new_x = old_x;
  int new_y = old_y;
  size_t gacha_count = gacha_pull.size();
  int pull_buttons_y = gacha_count == 1 ? 1 : 2;
  if (yytk->CallGameScript("gml_Script_menu_pressed_right", {}).ToBoolean()) new_x += new_y != pull_buttons_y ? 1 : 2;
  else if (yytk->CallGameScript("gml_Script_menu_pressed_left", {}).ToBoolean()) new_x -= new_y != pull_buttons_y ? 1 : 2;
  if (yytk->CallGameScript("gml_Script_menu_pressed_down", {}).ToBoolean()) new_y += 1;
  else if (yytk->CallGameScript("gml_Script_menu_pressed_up", {}).ToBoolean()) new_y -= 1;

  if (new_y < 0) new_y = 0;
  if (new_y > pull_buttons_y) new_y = pull_buttons_y;
  if (new_x > 4 && (gacha_count != 1 || new_y == pull_buttons_y)) new_x = 4;
  if (new_y == 0 && gacha_count == 1) new_x = 0;
  if (new_x < 0) new_x = 0;

  if (new_y == pull_buttons_y && (new_x < 2 || new_x == 3)) new_x = new_x == 3 ? 4 : 2;

  menu["selectX"] = new_x;
  menu["selectY"] = new_y;
}

const double gacha_pull_top = 0.1;
const double gacha_pull_x_sep = 0.02;
const double gacha_pull_y_sep = 0.025;
const double gacha_pull_width = 0.18;
const double gacha_pull_height = 0.35;
const double gacha_pull_text_scale = 0.6;
const double gacha_pull_beastie_scale = 0.45;
const double gacha_pull_item_scale = 2;

void DrawGachaResult(size_t i, bool one, GachaResult &result, double canvas_width, double canvas_height, const RValue &menu)
{
  double x = one ? 0.5 - gacha_pull_width / 2 : (gacha_pull_width + gacha_pull_x_sep) * (i % 5);
  double y = one ? 0.5 - gacha_pull_height / 2 : (gacha_pull_height + gacha_pull_y_sep) * floor(i / 5) + gacha_pull_top;
  double width_pix = canvas_width * gacha_pull_width;
  std::string text;
  RValue beastieOrItem;
  if (result.type == GACHA_BEASTIE) {
    beastieOrItem = Utils::GlobalGet("GACHA_SCENE_RENDERERS")[i]["char"];
    text = Utils::CallStructMethod(beastieOrItem, "displayname", {}).ToString();
  }
  else {
    beastieOrItem = yytk->CallBuiltin("ds_map_find_value", {Utils::GlobalGet("item_dic"), RValue(result.item.id)});
    text = beastieOrItem["name"].ToString();
  }
  RValue scribbled = Utils::CallStructMethod(Utils::CallStructMethod(Scribble(RValue(text), RValue(std::format("GachaResult{}", i))),
    "align", {1, 1}),
    "scale", {gacha_pull_text_scale});
  double height_nowrap = Utils::CallStructMethod(scribbled, "get_height", {}).ToDouble() / canvas_height;
  Utils::CallStructMethod(scribbled, "wrap", {width_pix});
  double height = Utils::CallStructMethod(scribbled, "get_height", {}).ToDouble() / canvas_height;
  {
    Vec2 pos = GetMenuPos(x, y + height_nowrap * 0.85);
    Vec2 end = GetMenuPos(x + gacha_pull_width, y + gacha_pull_height);
    yytk->CallBuiltin("draw_set_color", {0xFFFFFF});
    yytk->CallGameScript("gml_Script_draw_pgram", {pos.x, pos.y, end.x, end.y});
  }
  std::string sub_text = "[scale,0.2][d#0]";
  for (int rarity_i = 0; rarity_i < result.rarity; rarity_i++) {
    sub_text += "[sprBall,2]";
  }
  sub_text += "\n[/s]";
  if (result.type == GACHA_BEASTIE) {
    bool in_party = false;
    std::vector<RValue> party = Utils::GlobalGet("team_party").ToVector();
    for (RValue &beastie : party) {
      if (beastie["pid"].ToString() == result.beastie.pid) {
        in_party = true;
        break;
      }
    }
    RValue beastie = yytk->CallGameScript("gml_Script_char_find_by_pid", {RValue(result.beastie.pid)});
    if (DrawMenuButton(x + gacha_pull_width / 2, y + height / 2, scribbled, (double)(i % 5), floor(i / 5), menu, false)) {
      yytk->CallGameScript("gml_Script_menu_level_in", {Utils::InstanceGet(Utils::GetObjectInstance("objGame"), in_party ? "mn_char" : "mn_reserve_char"), beastie});
      yytk->CallGameScript("gml_Script_container_play", {"ui_menu_sub_open"});
    }
    double duplicates = GetBeastieDuplicates(beastieOrItem);
    sub_text += std::format("COACHED {:.0f}/6", duplicates);
    RValue species = Utils::CallStructMethod(beastie, "specie_data", {});
    double float_dist = species["loco"]["float_dist"].ToDouble();
    double float_y = (gacha_pull_height - height) * float_dist / 120 * gacha_pull_beastie_scale;
    Vec2 pos = GetMenuPos(x + gacha_pull_width / 2, y + gacha_pull_height * 0.98 - float_y);
    yytk->CallGameScript("gml_Script_draw_monster_menu", {beastieOrItem, pos.x, pos.y, gacha_pull_beastie_scale});
  }
  else {
    DrawMenuButton(x + gacha_pull_width / 2, y + height / 2, scribbled, (double)(i % 5), floor(i / 5), menu, false);
    double count = yytk->CallGameScript("gml_Script_item_count", {RValue(result.item.id)}).ToDouble();
    sub_text += std::format("Owned: {:.0f}", count);
    RValue items = yytk->CallBuiltin("asset_get_index", {"sprItems"});
    Vec2 pos = GetMenuPos(x + gacha_pull_width / 2, y + height * 2 + (gacha_pull_height - height_nowrap * 2) / 2);
    yytk->CallBuiltin("draw_sprite_ext", {items, beastieOrItem["img"], pos.x - 32 * gacha_pull_item_scale, pos.y - 32 * gacha_pull_item_scale, gacha_pull_item_scale, gacha_pull_item_scale, 0, 0xFFFFFF, 1});
  }
  RValue sub = Scribble(RValue(sub_text));
  Utils::CallStructMethod(Utils::CallStructMethod(Utils::CallStructMethod(sub, "align", {1, 0}), "scale", {gacha_pull_text_scale}), "wrap", {width_pix});
  yytk->CallBuiltin("draw_set_color", {0});
  Vec2 pos = GetMenuPos(x + gacha_pull_width / 2, y + height + 0.01);
  Utils::CallStructMethod(sub, "draw", {pos.x, pos.y});
  DrawJerseyCount();
}

void DrawGachaResultsMenu(RValue &current_menu)
{
  RValue menu = Utils::GlobalGet("mn_gacha_results");
  if (menu.m_Object != current_menu.m_Object) {
    return;
  }
  GachaResultsHandleInput(menu);
  RValue game = Utils::GetObjectInstance("objGame");
  double canvas_width = Utils::InstanceGet(game, "canvas_x2a").ToDouble() - Utils::InstanceGet(game, "canvas_x1a").ToDouble();
  double canvas_height = Utils::InstanceGet(game, "canvas_y2").ToDouble() - Utils::InstanceGet(game, "canvas_y1").ToDouble();
  size_t gacha_count = gacha_pull.size();
  SetFont();
  for (size_t i = 0; i < gacha_count; i++) {
    DrawGachaResult(i, gacha_count == 1, gacha_pull[i], canvas_width, canvas_height, menu);
  }
  int pull_buttons_y = gacha_count == 1 ? 1 : 2;
  if (DrawPullButton(scentered"Recruit 1 [sprItems,6]x1", {0.5, 0.9, 2, pull_buttons_y}, menu))
    DoGachaPull(*active_gacha, 1);
  if (DrawPullButton(scentered"Recruit 10 [sprItems,6]x10", {0.8, 0.9, 4, pull_buttons_y}, menu))
    DoGachaPull(*active_gacha, 10);
}

double max_gacha_scroll = 1.0;

void GachaRatesHandleInput(RValue &menu)
{
  double y_pos = menu["selectX"].ToDouble();
  y_pos = max(min(y_pos, max_gacha_scroll), 0);
  if (yytk->CallGameScript("gml_Script_input_axis_y", {}).ToDouble() < 0)
    y_pos -= 0.03;
  if (yytk->CallGameScript("gml_Script_input_axis_y", {}).ToDouble() > 0)
    y_pos += 0.03;
  if (yytk->CallBuiltin("mouse_wheel_down", {}).ToBoolean())
    y_pos += 0.08;
  if (yytk->CallBuiltin("mouse_wheel_up", {}).ToBoolean())
    y_pos -= 0.08;
  menu["selectX"] = y_pos;
}

const double rates_text_height = 0.1;
const double rates_small_text_spacing = 0.06;
const double rates_small_text_height = 0.0375;
const double rates_small_back_spacing = 0.0215;
const double rates_display_min = -rates_text_height / 2;
const double rates_display_max = 1.0 + rates_text_height / 2;

struct DropRate {
  std::string name;
  double y_pos;
  int rarity = 0;
  double weight = 0;
  double *total_rarity_weight;
  double *total_weight;
};

void AddRatesForBeastie(double &y_pos, double *rarity_weight, double *total_rarity_weight, const RValue &char_dic, std::vector<DropRate> &rates, BeastieDrop &drop, int rarity)
{
  y_pos += rates_text_height;
  *rarity_weight += drop.weight;
  if (y_pos > rates_display_min || y_pos < rates_display_max) {
    RValue beastie = yytk->CallBuiltin("ds_map_find_value", {char_dic, drop.family});
    rates.push_back({beastie["name"].ToString(), y_pos, rarity, drop.weight, rarity_weight, total_rarity_weight});
  }
  bool is_divergant = strcmp(drop.family, "shroom1") > -1 || strcmp(drop.family, "spirit1") > -1;
  std::vector<std::string> line = metamorph_lines[drop.family];
  size_t line_size = line.size();
  for (size_t i = 1; i < line_size; i++) {
    y_pos += i > 1 ? rates_small_text_height : rates_small_text_spacing;
    if (y_pos < rates_display_min || y_pos > rates_display_max) continue;
    RValue beastie = yytk->CallBuiltin("ds_map_find_value", {char_dic, RValue(line[i])});
    int meta_pos = is_divergant ? 3 : (int)floor(i / (double)(line_size) * 6.0);
    rates.push_back({std::format("[scale,0.5]Can metamorph to {} at COACHED {}/6", beastie["name"].ToString(), meta_pos), y_pos});
  }
  if (line_size > 1) y_pos -= rates_small_back_spacing;
}

void DrawGachaRatesMenu(RValue &current_menu)
{
  RValue menu = Utils::GlobalGet("mn_gacha_rates");
  if (menu.m_Object != current_menu.m_Object) {
    DrawGachaResultsMenu(current_menu);
    return;
  }
  SetFont();
  GachaRatesHandleInput(menu);
  double y_pos_start = menu["selectX_anim"].ToDouble();
  double y_pos = 0 - y_pos_start;
  GachaType &gacha = *active_gacha;
  RValue item_dic = Utils::GlobalGet("item_dic");
  std::vector<DropRate> rates;
  RValue char_dic = Utils::GlobalGet("char_dic");
  y_pos += rates_text_height;
  DrawTextPgram(0.5, y_pos, std::format("Drop Rates for {}", gacha.name).c_str());
  y_pos += rates_text_height;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Recruiting a Beastie you already have at COACHED 0 - 5 will increase the COACHED level of the Beastie.", gacha.rates.weight_5 * 100))));
  y_pos += rates_small_text_spacing;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Total 5[scale,0.125][sprBall,2][scale,0.5] Drop Chance {:.0f}%", gacha.rates.weight_5 * 100))));
  y_pos += rates_small_text_spacing;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Total 4[scale,0.125][sprBall,2][scale,0.5] Drop Chance {:.0f}%", gacha.rates.weight_4 * 100).c_str())));
  y_pos += rates_small_text_spacing;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Total 1-3[scale,0.125][sprBall,2][scale,0.5] Drop Chance {:.0f}%", gacha.rates.weight_other * 100))));
  double weight_5 = 0;
  for (BeastieDrop &drop : gacha.rates.drops_5) {
    AddRatesForBeastie(y_pos, &weight_5, &gacha.rates.weight_5, char_dic, rates, drop, 5);
  }
  double weight_4 = 0;
  for (BeastieDrop &drop : gacha.rates.drops_4) {
    AddRatesForBeastie(y_pos, &weight_4, &gacha.rates.weight_4, char_dic, rates, drop, 4);
  }
  double weight_other = 0;
  for (int rarity = 3; rarity > 0; rarity--) {
    for (ItemDrop &drop : gacha.rates.drops_other)
    {
      if (drop.rarity == rarity) {
        y_pos += rates_text_height;
        weight_other += drop.weight;
        if (y_pos < rates_display_min || y_pos > rates_display_max) continue;
        RValue item = yytk->CallBuiltin("ds_map_find_value", {item_dic, drop.item});
        rates.push_back({std::format("[sprItems,{}]{}", item["img"].ToString(), item["name"].ToString()), y_pos, rarity, drop.weight, &weight_other, &gacha.rates.weight_other});
      }
    }
  }
  max_gacha_scroll = y_pos_start + y_pos - 1 + rates_text_height * 1.5;
  for (DropRate rate : rates) {
    double drop_percent = rate.rarity ? (rate.weight / (*rate.total_rarity_weight) * (*rate.total_weight) * 100) : 0;
    DrawTextPgram(0.5, rate.y_pos, Scribble(RValue(rate.rarity ? std::format(scentered"{} - {}[scale,0.25][sprBall,2][/s] - {:.5f}%", rate.name, rate.rarity, drop_percent) : scentered + rate.name)));
  }
  DrawControls();
}

void OpenGachaRates()
{
  RValue menu = Utils::GlobalGet("mn_gacha_rates");
  RValue game = Utils::GetObjectInstance("objGame");
  Utils::InstanceSet(game, "pause_manual", true);
  yytk->CallGameScript("gml_Script_menu_level_in", {menu});
  menu["selectX"] = 0.0;
  menu["selectX_anim"] = 0.0;
}

void HandleInput(const RValue &menu)
{
  GachaType &gacha = *active_gacha;
  int old_x = menu["selectX"].ToInt32();
  int old_y = menu["selectY"].ToInt32();
  int new_x = old_x;
  int new_y = old_y;
  if (yytk->CallGameScript("gml_Script_menu_pressed_right", {}).ToBoolean()) new_x += 1;
  else if (yytk->CallGameScript("gml_Script_menu_pressed_left", {}).ToBoolean()) new_x -= 1;
  if (yytk->CallGameScript("gml_Script_menu_pressed_down", {}).ToBoolean()) {
    if (new_y == 0 && gacha.down_x_pos != -1) new_x = gacha.down_x_pos;
    new_y += 1;
  }
  else if (yytk->CallGameScript("gml_Script_menu_pressed_up", {}).ToBoolean()) new_y -= 1;

  if (old_x == new_x && old_y == new_y) return;

  int min_x = 0;
  int max_x = 0;
  int max_y = 0;
  for (int i = 0; i < 3; i++)
  {
    PullButtonPos pos = gacha.button_pos[i];
    if (pos.selection_x > max_x) max_x = pos.selection_x;
    if (pos.selection_x < min_x) min_x = pos.selection_x;
    if (pos.selection_y > max_y) max_y = pos.selection_y;
  }

  if (new_y < 0) new_y = 0;
  else if (new_y > max_y) new_y = max_y;
  else {
    if (new_y == 0) {
      if (new_x < 0) new_x = 0;
      if (new_x >= gacha_count) new_x = gacha_count - 1;
    }
    else {
      if (new_x > max_x) new_x = max_x;
      if (new_x < min_x) new_x = min_x;
      if (old_x == new_x && old_y == new_y) return;
      PullButtonPos *match = nullptr;
      for (int i = 0; i < 3; i++)
      {
        PullButtonPos *pos = &gacha.button_pos[i];
        if (pos->selection_x == old_x && pos->selection_y == old_y) continue;
        if (pos->selection_x == new_x && pos->selection_y == new_y) { match = nullptr; break; }
        if (
          (pos->selection_y == new_y && abs(pos->selection_x - new_x) == 1) ||
          (pos->selection_x == new_x && abs(pos->selection_y - new_y) == 1)
          ) match = pos;
      }
      if (match) {
        new_x = match->selection_x;
        new_y = match->selection_y;
      }
    }
  }
  Utils::InstanceSet(menu, "selectX", new_x);
  Utils::InstanceSet(menu, "selectY", new_y);
}

std::vector<std::string> GetVisibleGachas()
{
  std::vector<std::string> visible_gachas = {"amberstone", "starter"};
  std::string level_palette = yytk->CallGameScript("gml_Script_level_get_data", {})["palette_name"].ToString();
  if (level_palette == "cliffs") {
    visible_gachas[0] = "amberstone";
  }
  gacha_count = visible_gachas.size();
  return visible_gachas;
}

void DrawGachaMenu()
{
  if (do_scene_render)
    DrawResultText();
  if (!Utils::GlobalGet("menu_open").ToBoolean())
    return;
  RValue menu = Utils::GlobalGet("mn_gacha");
  RValue current_menu = Utils::GlobalGet("menu_tab_open");
  if (!current_menu.ToBoolean())
    return;
  RValue game = Utils::GetObjectInstance("objGame");
  if (!(Utils::InstanceGet(game, "menu_open_fx").ToDouble() > 0 || Utils::InstanceGet(game, "phone_fx").ToDouble() > 0))
    return;
  if (current_menu.m_Object != menu.m_Object) {
    DrawGachaRatesMenu(current_menu);
    return;
  }
  SetFont();
  HandleInput(menu);
  std::vector<std::string> visible_gachas = GetVisibleGachas();
  size_t visible_count = visible_gachas.size();
  double header_dist = 1. / double(visible_count + 1);
  double x_pos = 0;
  for (size_t i = 0; i < visible_count; i++) {
    GachaType &gacha = gachas[visible_gachas[i]];
    x_pos += header_dist;
    if (DrawMenuButton(x_pos, 0.075, gacha.name, i, 0, menu, gacha_open == i))
      gacha_open = i;
    if (gacha_open == i) {
      active_gacha = &gacha;
      DrawSprites(gacha.sprites, false);
      DrawBeastieLayout(gacha.beastie_layout, false);
      DrawTextPoses(gacha.text, false);
      DrawBeastieLayout(gacha.beastie_layout, true);
      DrawTextPoses(gacha.text, true);
      DrawSprites(gacha.sprites, true);
      if (DrawPullButton(scentered"Recruit 1 [sprItems,6]x1", gacha.button_pos[0], menu))
        DoGachaPull(gacha, 1);
      if (DrawPullButton(scentered"Recruit 10 [sprItems,6]x10", gacha.button_pos[1], menu))
        DoGachaPull(gacha, 10);
      if (DrawPullButton(scentered"[scale,0.75]View Drop Rates", gacha.button_pos[2], menu))
        OpenGachaRates();
      DrawJerseyCount();
    }
  }
  DrawControls();
}

std::string editing_gacha = "amberstone";
int editing_beastie = 0;
int editing_text = 0;
int editing_sprites = 0;
void EditGachaMenu()
{
  if (ImGui::Button("Give 10 Jerseys"))
    yytk->CallGameScript("gml_Script_item_get", {"jersey", 10});
  if (gachas[editing_gacha].rates.weight_4 < 0.5 && ImGui::Button("Only 4+"))
    gachas[editing_gacha].rates.weight_4 = 0.98;
  else if (gachas[editing_gacha].rates.weight_4 > 0.5 && ImGui::Button("Normal Rates"))
    gachas[editing_gacha].rates.weight_4 = 0.20;
  if (ImGui::BeginCombo("Editing", gachas[editing_gacha].name))
  {
    for (auto g = gachas.begin(); g != gachas.end(); g++)
    {
      if (ImGui::Selectable(g->second.name, g->first == editing_gacha))
        editing_gacha = g->first;
    }
    ImGui::EndCombo();
  }
  GachaType &gacha = gachas[editing_gacha];
  ImGui::Text(gacha.name);
  size_t beastie_count = gacha.beastie_layout.size();
  if (editing_beastie >= beastie_count) editing_beastie = 0;
  if (ImGui::BeginCombo("Beastie", gacha.beastie_layout[editing_beastie].species)) {
    for (int i = 0; i < beastie_count; i++)
      if (ImGui::Selectable(gacha.beastie_layout[i].species))
        editing_beastie = i;
    ImGui::EndCombo();
  }
  ImGui::BeginChild("beastieedit", {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
  BeastiePos &beastie = gacha.beastie_layout[editing_beastie];
  ImGui::InputDouble("X", &beastie.x, 0.01, 0.1, "%f");
  ImGui::InputDouble("Y", &beastie.y, 0.01, 0.1, "%f");
  ImGui::InputDouble("X Scale", &beastie.x_scale, 0.01, 0.1, "%f");
  ImGui::InputDouble("Y Scale", &beastie.y_scale, 0.01, 0.1, "%f");
  ImGui::InputDouble("Rotation", &beastie.rotation, 1.0, 10.0, "%f");
  ImGui::Checkbox("In front of text", &beastie.front_of_text);
  ImGui::InputDouble("Alpha", &beastie.alpha, 0.0, 0.0);
  ImGui::EndChild();

  size_t text_count = gacha.text.size();
  if (editing_text >= text_count) editing_text = 0;
  if (ImGui::BeginCombo("Text", gacha.text[editing_text].text)) {
    for (int i = 0; i < text_count; i++)
      if (ImGui::Selectable(gacha.text[i].text))
        editing_text = i;
    ImGui::EndCombo();
  }
  ImGui::BeginChild("textedit", {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
  TextPos &text = gacha.text[editing_text];
  ImGui::Text(text.text);
  ImGui::InputDouble("X", &text.x, 0.01, 0.1, "%f");
  ImGui::InputDouble("Y", &text.y, 0.01, 0.1, "%f");
  ImGui::InputDouble("Scale", &text.scale, 0.01, 0.1, "%f");
  ImGui::Checkbox("In front of beasties", &text.front_of_beasties);
  ImGui::Checkbox("Scribble", &text.scribble);
  ImGui::EndChild();

  size_t sprites_count = gacha.sprites.size();
  if (editing_sprites >= sprites_count) editing_sprites = 0;
  if (ImGui::BeginCombo("Sprite", std::format("{} - {}", editing_sprites, gacha.sprites[editing_sprites].sprite).c_str())) {
    for (int i = 0; i < sprites_count; i++)
      if (ImGui::Selectable(std::format("{} - {}", i, gacha.sprites[i].sprite).c_str()))
        editing_sprites = i;
    ImGui::EndCombo();
  }
  if (editing_sprites < sprites_count) {
    ImGui::BeginChild("spriteedit", {0, 0}, ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);
    SpritePos &sprite = gacha.sprites[editing_sprites];
    ImGui::Text(sprite.sprite);
    ImGui::InputDouble("Index", &sprite.index, 0.01, 0.1);
    ImGui::InputDouble("X", &sprite.x, 0.01, 0.1);
    ImGui::InputDouble("Y", &sprite.y, 0.01, 0.1);
    ImGui::InputDouble("Scale X", &sprite.x_scale, 0.01, 0.1);
    ImGui::InputDouble("Scale Y", &sprite.y_scale, 0.01, 0.1);
    ImGui::InputDouble("Rotation", &sprite.rotation, 0.01, 0.1);
    ImGui::InputDouble("Rotating Speed", &sprite.rotating_speed, 0.01, 0.1);
    ImGui::EndChild();
  }
}

const char *menu_string = R"({
  "name": "Recruit", "open": 0,
  "selectX": 0, "selectY": 0,
  "onEnter": null, "onExit": null,
  "selectX_anim": 0, "selectY_anim": 0,
  "open_anim": 0, "ending": false, "end_anim": 0,
  "has_default_input": true, "has_phone": true,
  "default_layout": true, "open_anim_style": -0.4,
  "bg_x1": 0.08, "bg_x2": 1.04,
  "bg_ymargin": 0, "bg_black_overlay": true,
  "mouse_selected": false, "onSelectChange": null,
  "menu_img": 20, "commands": null
})";

void MenuSetup()
{
  RValue menu = yytk->CallBuiltin("json_parse", {menu_string});
  menu["commands"] = yytk->CallBuiltin("ds_list_create", {});
  Utils::GlobalSet("mn_gacha", menu);
  menu = yytk->CallBuiltin("json_parse", {menu_string});
  menu["commands"] = yytk->CallBuiltin("ds_list_create", {});
  Utils::GlobalSet("mn_gacha_rates", menu);
  menu = yytk->CallBuiltin("json_parse", {menu_string});
  menu["commands"] = yytk->CallBuiltin("ds_list_create", {});
  Utils::GlobalSet("mn_gacha_results", menu);
}

std::map<std::string, int> jersey_value = {
  {"jersey", 1}, {"jersey2", 2}, {"jersey3", 3}, {"jersey4", 4}, {"jersey5", 5},
};
const int jersey_price = 160;

bool other_setup_done = false;

void OtherSetup()
{
  RValue char_dic = Utils::GlobalGet("char_dic");
  RValue shloom = yytk->CallBuiltin("ds_map_find_value", {char_dic, "shroommon"});
  if (!shloom.ToBoolean()) return;
  RValue shlecruit = shloom["recruit"];
  shlecruit["description"] = "Recruit in the [sprMainmenu,21]Recruit menu.";
  std::vector<RValue> keys = yytk->CallBuiltin("ds_map_keys_to_array", {char_dic}).ToVector();
  for (RValue key : keys) {
    yytk->CallBuiltin("ds_map_find_value", {char_dic, key})["recruit"] = shlecruit;
  }
  RValue item_dic = Utils::GlobalGet("item_dic");

  if (yytk->CallBuiltin("ds_map_empty", {item_dic})) return;
  for (auto pair = jersey_value.begin(); pair != jersey_value.end(); pair++)
  {
    RValue item = yytk->CallBuiltin("ds_map_find_value", {item_dic, RValue(pair->first)});
    item["value"] = pair->second * jersey_price;
    if (pair->second > 1) {
      item["desc"] = std::format("Gives you {} Jerseys to use to recruit Beasties in the [sprMainmenu,21]Recruit menu.", pair->second).c_str();
    }
    else {
      item["desc"] = "Used to recruit a Beastie in the [sprMainmenu,21]Recruit menu.";
    }
  }
  other_setup_done = true;
}

void ReduceJerseys()
{
  int jersey_count = 0;
  RValue inventory = Utils::GlobalGet("inventory");
  for (auto pair = jersey_value.begin(); pair != jersey_value.end(); pair++)
  {
    if (pair->second == 1) continue;
    if (Utils::InstanceExists(inventory, pair->first)) {
      RValue item = Utils::InstanceGet(inventory, pair->first);
      int count = item["num"].ToInt32();
      if (count) {
        jersey_count += pair->second * count;
        yytk->CallGameScript("gml_Script_item_delete", {RValue(pair->first), count});
      }
    }
  }
  if (jersey_count)
    yytk->CallGameScript("gml_Script_item_get", {"jersey", jersey_count});
}

void GachaHooks()
{
  RequestHook(NULL, "gml_Script_feedback_submit", "IW feedback_submit", FeedbackSubmit, reinterpret_cast<PVOID *>(&feedbackSubmit));
  RequestHook(NULL, "gml_Script_analytics_submit", "IW analytics_submit", AnalyticsSubmit, reinterpret_cast<PVOID *>(&analyticsSubmit));
  RequestHook(NULL, "gml_Script_file_read_string", "IW file_read_string", FileReadString, reinterpret_cast<PVOID *>(&fileReadString));
  RequestHook(NULL, "gml_Script_file_write_string", "IW file_write_string", FileWriteString, reinterpret_cast<PVOID *>(&fileWriteString));
  RequestHook("gml_Script_tame_adjust", "@class_beastie", "IW tame_adjust", TameAdjust, reinterpret_cast<PVOID *>(&tameAdjust));
  RequestHook("gml_Script_ev_adjust", "@class_beastie", "IW ev_adjust", EvAdjust, reinterpret_cast<PVOID *>(&evAdjust));
  RequestHook("gml_Script_tame_rating_string", "@class_beastie", "IW class_beastie tame_rating_string", ClassBeastieTameRatingString, reinterpret_cast<PVOID *>(&classBeastieTameRatingString));
  RequestHook(NULL, "gml_Script_beastie_encounter_generate", "IW beastie_encounter_generate", BeastieEncounterGenerate, reinterpret_cast<PVOID *>(&beastieEncounterGenerate));
  RequestHook("gml_Script_can_evolve", "@class_beastie", "IW class_beastie can_evolve", ClassBeastieCanEvolve, reinterpret_cast<PVOID *>(&classBeastieCanEvolve));
  RequestHook("gml_Script_waiting_to_morph", "@class_beastie", "IW class_beastie waiting_to_morph", ClassBeastieWaitingToMorph, reinterpret_cast<PVOID *>(&classBeastieWaitingToMorph));
  RequestHook(NULL, "gml_Script_beastie_specie_in_party_array", "IW beastie_specie_in_party_array", BeastieSpecieInPartyArray, reinterpret_cast<PVOID *>(&beastieSpecieInPartyArray));
  RequestHook(NULL, "gml_Script_beastie_specie_in_party", "IW beastie_specie_in_party", BeastieSpecieInParty, reinterpret_cast<PVOID *>(&beastieSpecieInParty));
  RequestHook(NULL, "gml_Script_anon@700@gml_Object_objEvolvetrickies_Other_10", "IW trickies", TrickiesCheck, reinterpret_cast<PVOID *>(&trickiesCheck));
  RequestHook(NULL, "gml_Script_data_update_player_pos", "IW player_pos", DataUpdatePlayerPos, reinterpret_cast<PVOID *>(&dataUpdatePlayerPos));

  RequestHook(NULL, "gml_Script_WaitForTween", "IW WaitForTween", WaitForTween, reinterpret_cast<PVOID *>(&waitForTween));

  BuiltinHook("IW file_exists", "file_exists", FileExists, reinterpret_cast<PVOID *>(&file_exists));
  BuiltinHook("IW file_delete", "file_delete", FileDelete, reinterpret_cast<PVOID *>(&file_delete));
  BuiltinHook("IW file_copy", "file_copy", FileCopy, reinterpret_cast<PVOID *>(&file_copy));
  BuiltinHook("IW keyboard_check_pressed", "keyboard_check_pressed", KeyboardCheckPressed, reinterpret_cast<PVOID *>(&keyboard_check_pressed));

  // Setup Menus
  MenuSetup();
}

void AddSubmenu()
{
  RValue game = Utils::GetObjectInstance("objGame");
  RValue gacha_menu = Utils::GlobalGet("mn_gacha");
  RValue main_menu = Utils::InstanceGet(game, "mn_main");
  if (!main_menu.ToBoolean())
    return;
  RValue submenus = main_menu["submenus"];
  if (gacha_menu.m_Object != yytk->CallBuiltin("ds_list_find_value", {submenus, 0}).m_Object)
    yytk->CallBuiltin("ds_list_insert", {submenus, 0, gacha_menu});
}

void OpenMenu()
{
  if (Utils::ObjectInstanceExists("objInit") || Utils::ObjectInstanceExists("objTitle"))
    return;
  bool menu_open = Utils::GlobalGet("menu_open").ToBoolean();
  RValue current_menu = Utils::GlobalGet("menu_tab_open");
  RValue menu = Utils::GlobalGet("mn_gacha");
  if (menu_open) {
    if (current_menu.ToBoolean() && current_menu.m_Object == menu.m_Object)
      yytk->CallGameScript("gml_Script_menu_level_out", {});
    return;
  }
  if (yytk->CallGameScript("gml_Script_scene_capturing_input", {}).ToBoolean() ||
    Utils::GlobalGet("SCENE_PLAYING").ToBoolean() ||
    Utils::GlobalGet("GAME_ACTIVE").ToDouble() != -4
    )
    return;
  yytk->CallGameScript("gml_Script_container_play", {"ui_menu_sub_open"});
  yytk->CallGameScript("gml_Script_buttonlist_affirmative_reset_release", {});
  yytk->CallGameScript("gml_Script_io_clear_all", {});
  RValue game = Utils::GetObjectInstance("objGame");
  Utils::InstanceSet(game, "pause_manual", true);
  yytk->CallGameScript("gml_Script_menu_level_in", {menu});
  menu["selectX"] = 0.0;
  menu["selectY"] = 0.0;
}

void GachaTab(bool *open)
{
  AddSubmenu();
  Utils::GlobalSet("HIDE_FEEDBACK", true);
  if (!other_setup_done) {
    OtherSetup();
  }
  else {
    ReduceJerseys();
  }
  if (Utils::ObjectInstanceExists("objLevel") && Utils::InstanceGet(Utils::GetObjectInstance("objLevel"), "level_data")["name"].ToString().starts_with("gacha") && !Utils::GlobalGet("SCENE_PLAYING").ToBoolean())
  {
    std::vector<RValue> player_pos = Utils::InstanceGet(Utils::GlobalGet("data"), "player_pos").ToVector();
    RValue level = CheatsTab::FindLevel(player_pos[0].ToDouble(), player_pos[1].ToDouble());
    if (level.ToBoolean())
      yytk->CallGameScript("gml_Script_level_goto", {level["name"], true});
    else
      yytk->CallGameScript("gml_Script_level_goto", {"hometown"});
  }
  RenderScene();
  if (yytk->CallBuiltin("keyboard_check_pressed", {114.0}).ToBoolean())
    OpenMenu();
  #ifdef DO_INFOWINDOW
  if (!ImGui::Begin("Gacha", open, ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::End();
    return;
  }
  if (ImGui::Button("Open Menu"))
    OpenMenu();
  EditGachaMenu();
  ImGui::End();
	#endif
}

void Store()
{
}

}