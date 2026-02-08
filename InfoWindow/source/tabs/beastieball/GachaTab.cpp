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
  double weight_4 = 0.2;
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

GachaType gachas[] = {
  {
    "Amberstone",
    {
      {"Beasts of", 0.2, 0.38, 1.5},
      {"AMBERSTONE", 0.26, 0.48, 1.5, 0x57a2ff},
      {"Servace", 0.76, 0.29, 1.2, 0xFFFFFF, true},
      {scentered"[scale,0.5][sprIcon,3][sprIcon,3][sprIcon,3][sprIcon,3][sprIcon,3]", 0.756, 0.34, 1, 0xFFFFFF, true, true},
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
  },
  {
    "Starters",
    {
      {"some text here", 0.25, 0.5, 1.5, 0xFF0000},
    },
    {
      {"frog", "good", 0.75, 0.75, 1.5, 1.5},
    },
    { },
    { { 0.2, 0.2, 0, 1 }, { 0.5, 0.2, 1, 1 }, { 0.2, 0.4, 0, 2 } },
    -1,
    {},
    "gacha_amberstone",
  },
};
int gacha_count = 2;

enum GachaResultType {
  GACHA_BEASTIE,
  GACHA_ITEM,
};


struct GachaResultBeastie {
  std::string pid;
  bool raremorph;
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
  if ((!hidden && species["family"].ToString() == family) || (hidden && species["id"].ToString() == family) && duplicates < 6)
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
      if (BeastieCanMetamorph(beastie, family_species, family_species["id"].ToCString()) == 0)
        yytk->CallBuiltin("array_delete", {ReturnValue, i, 1});
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

PFUNC_YYGMLScript classBeastieCanEvolve = nullptr;
RValue &ClassBeastieCanEvolve(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  ReturnValue = RValue(CheckMetamorph(Self->ToRValue()) ? 0 : -1);
  return ReturnValue;
}

PFUNC_YYGMLScript classBeastieWaitingToMorph = nullptr;
RValue &ClassBeastieWaitingToMorph(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
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

bool is_my_scene = false;

const int gacha_scene_progress_start = -1;
enum GachaSceneProgress {
  GACHA_SCENE_BEGIN = -1,
  GACHA_SCENE_POST = 10,
  GACHA_SCENE_OVER = 11,
};
int gacha_scene_progress = gacha_scene_progress_start;

struct CameraLocation {
  double x;
  double y;
  double z;
  double height_angle = 80;
  double dist = -200;
};

struct ItemDrawer {
  double x;
  double y;
  double z;
  int index;
  bool draw_index = false;
  double flip_progress = 0;
};

std::vector<CameraLocation> camera_locations;
const double tween_speed = 1; // per second
double tween_progress = 0;

std::vector<ItemDrawer> item_drawers;

double Interp(double from, double to, double x)
{
  return from + (to - from) * x;
}

CameraLocation CameraInterp(CameraLocation &from, CameraLocation &to, double x)
{
  return {Interp(from.x, to.x, x), Interp(from.y, to.y, x), Interp(from.z, to.z, x), Interp(from.height_angle, to.height_angle, x), Interp(from.dist, to.dist, x)};
}

CameraLocation scene_camera;

void SetCameraLocation(CameraLocation camera, const RValue &scene_manager)
{
  scene_camera = camera;
  DbgPrint("set camera %f %f %f", camera.x, camera.y, camera.z);
  RValue shot = Utils::InstanceGet(scene_manager, "shot_overworld");
  shot["look_x"] = camera.x;
  shot["look_y"] = camera.y;
  shot["look_z"] = camera.z;
  shot["height_angle"] = camera.height_angle;
  shot["dist"] = camera.dist;
}

bool MySceneFrame()
{
  DbgPrint("MY SCENE! progress %d - tween %f", gacha_scene_progress, tween_progress);
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
    RValue item_dic = Utils::GlobalGet("item_dic");
    for (int i = 0; i < 2; i++)
    {
      RValue node = yytk->CallGameScript("gml_Script_levelnode_find", {RValue("gacha_start" + std::to_string(i))});
      camera_locations[i] = {node["x"].ToDouble(), node["y"].ToDouble(), node["z"].ToDouble()};
      DbgPrint("loc %i - x %f - y %f - z %f", i, camera_locations[i].x, camera_locations[i].y, camera_locations[i].z);
    }
    RValue global_renderers = Utils::GlobalGet("char_renderers");
    for (size_t i = 0; i < gacha_pull_size; i++) {
      GachaResult &result = gacha_pull[i];
      RValue node = yytk->CallGameScript("gml_Script_levelnode_find", {RValue("gacha_location" + std::to_string(i))});
      CameraLocation camera_location = {node["x"].ToDouble(), node["y"].ToDouble(), node["z"].ToDouble()};
      camera_locations[i + 2] = camera_location;
      DbgPrint("loc %i - x %f - y %f - z %f", i + 2, camera_locations[i + 2].x, camera_locations[i + 2].y, camera_locations[i + 2].z);
      if (result.type == GACHA_BEASTIE) {
        RValue renderer = yytk->CallGameScript("gml_Script_ElephantFromJSON", {yytk->CallBuiltin("json_parse", {R"({"_": "class_beastie_renderer"})"})});
        RValue beastie = yytk->CallGameScript("gml_Script_char_find_by_pid", {RValue(result.beastie.pid)});
        renderer["x"] = camera_location.x;
        renderer["y"] = camera_location.y;
        renderer["z"] = camera_location.z;
        Utils::CallStructMethod(renderer, "set_char", {beastie});
        renderers[i] = renderer;
        yytk->CallBuiltin("array_push", {global_renderers, renderer});
      }
      else {
        RValue item = yytk->CallBuiltin("ds_map_find_value", {item_dic, RValue(result.item.id)});
        item_drawers[i] = {camera_location.x, camera_location.y, camera_location.z, item["img"].ToInt32()};
      }
    }
    Utils::GlobalSet("GACHA_SCENE_RENDERERS", renderers);
  }
  DbgPrint("scene begin");
  RValue renderers = Utils::GlobalGet("GACHA_SCENE_RENDERERS");
  tween_progress += delta;
  bool scene_skipping = Utils::InstanceGet(scene_manager, "scene_skipping").ToBoolean();
  switch (gacha_scene_progress) {
  case GACHA_SCENE_BEGIN: {
    SetCameraLocation(CameraInterp(camera_locations[0], camera_locations[1], tween_progress / 3), scene_manager);
    if (tween_progress >= 3 || scene_skipping) {
      tween_progress = 0;
      gacha_scene_progress += 1;
      return true;
    }
    break;
  }
  case GACHA_SCENE_POST: {
    SetCameraLocation(CameraInterp(camera_locations[gacha_pull_size + 2 - 1], camera_locations[1], min(1, tween_progress / 3)), scene_manager);
    break;
  }
  default: {
    // gacha index
    SetCameraLocation(CameraInterp(camera_locations[gacha_scene_progress + 1], camera_locations[gacha_scene_progress + 2], min(1, tween_progress)), scene_manager);
    GachaResult &result = gacha_pull[gacha_scene_progress];
    bool scene_skipping_actually = scene_skipping && result.rarity < 4;
    if (scene_skipping_actually != scene_skipping)
      Utils::InstanceSet(scene_manager, "scene_skipping", scene_skipping_actually);
    if (result.type == GACHA_BEASTIE) {
      RValue renderer = renderers[gacha_scene_progress];
      if (tween_progress > 2 || scene_skipping_actually)
        yytk->CallGameScript("gml_Script_char_animation", {renderer, "good"});
    }
    else {
      ItemDrawer &drawer = item_drawers[gacha_scene_progress];
      double new_prog = 0;
      if (tween_progress < 2 && !scene_skipping_actually) {
        new_prog = (tween_progress - 1.5) * 2;
      }
      else {
        drawer.draw_index = true;
        new_prog = 3 + (tween_progress - 2) * 2;
      }
      drawer.flip_progress = min(4, max(0, new_prog));
    }
    if (tween_progress > 5 || scene_skipping_actually) {
      tween_progress = 0;
      gacha_scene_progress += 1;
      if (gacha_scene_progress >= gacha_pull_size)
        gacha_scene_progress = 10;
      return true;
    }

    break;
  }
  }
  return false;
}

void RenderSceneCharRenderers()
{
  if (!Utils::GlobalExists("GACHA_SCENE_RENDERERS"))
    return;
  RValue renderers = Utils::GlobalGet("GACHA_SCENE_RENDERERS");
  RValue sprItems = yytk->CallBuiltin("asset_get_index", {"sprItems"});
  RValue scene_manager = Utils::GetObjectInstance("objSceneManager");
  SetCameraLocation(scene_camera, scene_manager);
  Utils::InstanceSet(scene_manager, "gameplay_camera", false);
  for (size_t i = 0; i < gacha_pull.size(); i++) {
    GachaResult &result = gacha_pull[i];
    if (result.type == GACHA_BEASTIE) {
      RValue renderer = renderers[i];
      Utils::CallStructMethod(renderer, "draw", {});
    }
    else {
      ItemDrawer &drawer = item_drawers[i];
      yytk->CallGameScript("gml_Script_mstack_rotate_ext", {180 + drawer.flip_progress * 90, 0, 0});
      yytk->CallGameScript("gml_Script_mstack_translate", {drawer.x, drawer.y, drawer.z});
      yytk->CallGameScript("gml_Script_mstack_set_and_clear", {});
      yytk->CallBuiltin("shader_set", {yytk->CallBuiltin("asset_get_index", {"shader3DBillboard"})});
      yytk->CallBuiltin("draw_sprite_ext", {sprItems, drawer.draw_index ? drawer.index : 6, 32, 32, 1, 1, 180, 0xFFFFFF, 1});
      yytk->CallBuiltin("matrix_set", {2, Utils::GlobalGet("__identity_matrix")});
    }
  }
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
  if (is_my_scene && AmScene(Self))
    ReturnValue = MySceneFrame();
  else
    waitForTween(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

PFUNC_YYGMLScript sceneEnd = nullptr;
RValue &SceneEnd(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  if (AmScene(Self))
    is_my_scene = false;
  sceneEnd(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

void DoGachaPull(GachaType &gacha, int pull_count)
{
  if (!yytk->CallGameScript("gml_Script_item_has", {"jersey", pull_count}).ToBoolean())
    return;
  yytk->CallGameScript("gml_Script_item_delete", {"jersey", pull_count});
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
  is_my_scene = true;
  gacha_scene_progress = gacha_scene_progress_start;
  CInstance *global_inst = nullptr;
  yytk->GetGlobalInstance(&global_inst);
  RValue global = global_inst->ToRValue();
  yytk->CallGameScript("gml_Script_SceneLayerIn", {});
  yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "data_save_level")});
  yytk->CallGameScript("gml_Script_Fade", {0, 1, 0.25, 1, -3});
  yytk->CallGameScript("gml_Script_SceneAdd", {Utils::InstanceGet(global, "menu_level_out_all")});
  yytk->CallGameScript("gml_Script_LevelTransition", {gacha.level_name, -3});
  yytk->CallGameScript("gml_Script_WaitForTween", {1}); // My Scene
  for (int i = 0; i < pull_count; i++) {
    GachaResult &result = gacha_pull[i];
    yytk->CallGameScript("gml_Script_WaitForTween", {1}); // My Scene
    yytk->CallGameScript("gml_Script_WaitForInput", {result.rarity >= 4});
  }
  yytk->CallGameScript("gml_Script_SceneEnd", {});
  yytk->CallGameScript("gml_Script_SceneLayerOut", {});
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
      1.0, 1.0, beastie.x_scale, beastie.y_scale, beastie.rotation, beastie.animation
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

RValue Scribble(const RValue &text)
{
  return yytk->CallGameScript("gml_Script_scribble", {RValue(text)});
}

double DrawMenuButton(double x, double y, const RValue &text, double selection_x, double selection_y, const RValue &menu, bool selected, bool hover_button = false)
{
  // args: x, y, text, scalex, scaley, color, ?, selection x, selection y, menu, selected, ?, ?, ?, ?
  return yytk->CallGameScript("gml_Script_draw_text_button_menu", {
      yytk->CallGameScript("gml_Script_canvas_x", {x, y}), yytk->CallGameScript("gml_Script_canvas_y", {y}), text, 1, 0.25, selected ? Utils::GlobalGet("menu_color")[5] : 0, 0, selection_x, selection_y, menu
    }).ToBoolean() && (hover_button || yytk->CallGameScript("gml_Script_buttonlist_released_affirmative_raw", {}).ToBoolean());
}

bool DrawPullButton(const char *text, PullButtonPos &pos, const RValue &menu)
{
  return DrawMenuButton(pos.x, pos.y, Scribble(text), pos.selection_x, pos.selection_y, menu, false);
  // return yytk->CallGameScript("gml_Script_draw_text_button", {
  //   yytk->CallGameScript("gml_Script_canvas_x", {pos.x, pos.y}), yytk->CallGameScript("gml_Script_canvas_y", {pos.y}), yytk->CallGameScript("gml_Script_scribble", {text})}).ToBoolean();
}

void HandleInput(const RValue &menu)
{
  GachaType &gacha = gachas[gacha_open];
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

void DrawTextPgram(double x, double y, const RValue &text, double scale_x = 1, double scale_y = 0.25, double color = 0, double align = 0)
{
  yytk->CallGameScript("gml_Script_draw_text_pgram", {yytk->CallGameScript("gml_Script_canvas_x", {x, y}), yytk->CallGameScript("gml_Script_canvas_y", {y}), text, scale_x, scale_y, color, align});
}

void DrawControls()
{
  RValue game = Utils::GetObjectInstance("objGame");
  RValue command = Utils::InstanceGet(game, "menu_command_scribble");
  double x = yytk->CallBuiltin("display_get_gui_width", {}).ToDouble() - 10;
  double y = yytk->CallBuiltin("display_get_gui_height", {}).ToDouble() - Utils::CallStructMethod(command, "get_height", {}).ToDouble() / 2;
  yytk->CallGameScript("gml_Script_draw_text_pgram_edge", {x, y, command, 1, 0.25, 0, 2});
}

void SetFont()
{
  yytk->CallBuiltin("draw_set_font", {yytk->CallGameScript("gml_Script_font_get", {2})});
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
  if (menu.m_Object != current_menu.m_Object) return;
  SetFont();
  GachaRatesHandleInput(menu);
  double y_pos_start = menu["selectX_anim"].ToDouble();
  double y_pos = 0 - y_pos_start;
  GachaType &gacha = gachas[gacha_open];
  RValue item_dic = Utils::GlobalGet("item_dic");
  std::vector<DropRate> rates;
  RValue char_dic = Utils::GlobalGet("char_dic");
  y_pos += rates_text_height;
  DrawTextPgram(0.5, y_pos, std::format("Drop Rates for {}", gacha.name).c_str());
  y_pos += rates_text_height;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Recruiting a Beastie you already have at COACHED 0 - 5 will increase the COACHED level of the Beastie.", gacha.rates.weight_5 * 100))));
  y_pos += rates_small_text_spacing;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Total 5[sprIcon,3] Drop Chance {:.0f}%", gacha.rates.weight_5 * 100))));
  y_pos += rates_small_text_spacing;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Total 4[sprIcon,3] Drop Chance {:.0f}%", gacha.rates.weight_4 * 100).c_str())));
  y_pos += rates_small_text_spacing;
  DrawTextPgram(0.5, y_pos, Scribble(RValue(std::format(scentered"[scale,0.5]Total 1-3[sprIcon,3] Drop Chance {:.0f}%", gacha.rates.weight_other * 100))));
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
    DrawTextPgram(0.5, rate.y_pos, Scribble(RValue(rate.rarity ? std::format(scentered"{} - {}[sprIcon,3] - {:.5f}%", rate.name, rate.rarity, drop_percent) : scentered + rate.name)));
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

void DrawGachaMenu()
{
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
  double header_dist = 1. / double(gacha_count + 1);
  double x_pos = 0;
  for (int i = 0; i < gacha_count; i++) {
    GachaType &gacha = gachas[i];
    x_pos += header_dist;
    if (DrawMenuButton(x_pos, 0.075, gacha.name, i, 0, menu, gacha_open == i))
      gacha_open = i;
    if (gacha_open == i) {
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
      DrawTextPgram(0.98, 0.035, Scribble(RValue(std::format(scentered"[scale,0.5][sprItems,6] {}", yytk->CallGameScript("gml_Script_item_count", {"jersey"}).ToString()))), 1, 0.25, 0, 2);
    }
  }
  DrawControls();
}

int editing_gacha = 0;
int editing_beastie = 0;
int editing_text = 0;
int editing_sprites = 0;
void EditGachaMenu()
{
  if (ImGui::Button("Give 10 Jerseys"))
    yytk->CallGameScript("gml_Script_item_get", {"jersey", 10});
  if (ImGui::BeginCombo("Editing", gachas[editing_gacha].name))
  {
    for (int i = 0; i < gacha_count; i++)
    {
      GachaType &gacha = gachas[i];
      if (ImGui::Selectable(gacha.name, editing_gacha == i))
        editing_gacha = i;
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
  RValue menu2 = yytk->CallBuiltin("json_parse", {menu_string});
  menu2["commands"] = yytk->CallBuiltin("ds_list_create", {});
  Utils::GlobalSet("mn_gacha_rates", menu2);
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
  RequestHook("gml_Script_can_evolve", "@class_beastie", "IW class_beastie can_evolve", ClassBeastieCanEvolve, reinterpret_cast<PVOID *>(&classBeastieCanEvolve));
  RequestHook("gml_Script_waiting_to_morph", "@class_beastie", "IW class_beastie waiting_to_morph", ClassBeastieWaitingToMorph, reinterpret_cast<PVOID *>(&classBeastieWaitingToMorph));
  RequestHook(NULL, "gml_Script_beastie_specie_in_party_array", "IW beastie_specie_in_party_array", BeastieSpecieInPartyArray, reinterpret_cast<PVOID *>(&beastieSpecieInPartyArray));
  RequestHook(NULL, "gml_Script_beastie_specie_in_party", "IW beastie_specie_in_party", BeastieSpecieInParty, reinterpret_cast<PVOID *>(&beastieSpecieInParty));
  RequestHook(NULL, "gml_Script_anon@700@gml_Object_objEvolvetrickies_Other_10", "IW trickies", TrickiesCheck, reinterpret_cast<PVOID *>(&trickiesCheck));

  RequestHook(NULL, "gml_Script_SceneEnd", "IW SceneEnd", SceneEnd, reinterpret_cast<PVOID *>(&sceneEnd));
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
  RenderSceneCharRenderers();
  if (yytk->CallBuiltin("keyboard_check_pressed", {114.0}).ToBoolean())
    OpenMenu();
  if (!ImGui::Begin("Gacha", open, ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::End();
    return;
  }
  if (ImGui::Button("Open Menu"))
    OpenMenu();
  EditGachaMenu();
  ImGui::End();
}

void Store()
{
}

}