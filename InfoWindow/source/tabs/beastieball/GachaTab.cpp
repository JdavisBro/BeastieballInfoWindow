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

PFUNC_YYGMLScript savedataGetFilename = nullptr;
RValue &SavedataGetFilename(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  savedataGetFilename(Self, Other, ReturnValue, numArgs, Args);
  if (!is_backup && !ReturnValue.ToString().empty()) ReturnValue = RValue("gacha/" + ReturnValue.ToString());
  return ReturnValue;
}
PFUNC_YYGMLScript savedataGetBackupFilename = nullptr;
RValue &SavedataGetBackupFilename(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  is_backup = true;
  savedataGetBackupFilename(Self, Other, ReturnValue, numArgs, Args);
  is_backup = false;
  if (!ReturnValue.ToString().empty()) ReturnValue = RValue("gacha/" + ReturnValue.ToString());
  return ReturnValue;
}
PFUNC_YYGMLScript fileReadString = nullptr;
RValue &FileReadString(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  std::string name = Args[0]->ToString();
  if (!name.empty() && !(name.size() == 1 && name[0] == ' ') && name.starts_with("save") || name.starts_with("backup_save")) *Args[0] = RValue("gacha/" + name);
  fileReadString(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}
PFUNC_YYGMLScript fileWriteString = nullptr;
RValue &FileWriteString(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  std::string name = (*Args[0]).ToString();
  if (name.empty()) return ReturnValue;
  if (!name.empty() && !(name.size() == 1 && name[0] == ' ') && name.starts_with("save") || name.starts_with("backup_save")) *Args[0] = RValue("gacha/" + name);
  fileWriteString(Self, Other, ReturnValue, numArgs, Args);
  return ReturnValue;
}

PFUNC_YYGMLScript feedbackSubmit = nullptr;
RValue &FeedbackSubmit(CInstance *Self, CInstance *Other, RValue &ReturnValue, int numArgs, RValue **Args)
{
  // no feedback for you greg 😈
  return ReturnValue;
}


TRoutine file_exists = nullptr;
void FileExists(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  std::string name = Args->ToString();
  if (!name.empty() && name.starts_with("save") || name.starts_with("backup_save")) *Args = RValue("gacha/" + name);
  file_exists(Result, Self, Other, numArgs, Args);
}
TRoutine file_delete = nullptr;
void FileDelete(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  std::string name = Args->ToString();
  if (!name.empty() && name.starts_with("save") || name.starts_with("backup_save")) *Args = RValue("gacha/" + name);
  file_exists(Result, Self, Other, numArgs, Args);
}
TRoutine file_copy = nullptr;
void FileCopy(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  std::string name = Args->ToString();
  if (!name.empty() && name.starts_with("save") || name.starts_with("backup_save")) *Args = RValue("gacha/" + name);
  name = (Args + 1)->ToString();
  if (!name.empty() && name.starts_with("save") || name.starts_with("backup_save")) *(Args + 1) = RValue("gacha/" + name);
  file_exists(Result, Self, Other, numArgs, Args);
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
  double weight_other = 0.75;
};

struct GachaType {
  const char *name;
  std::vector<TextPos> text;
  std::vector<BeastiePos> beastie_layout;
  std::vector<SpritePos> sprites;
  PullButtonPos button_pos[3];
  int down_x_pos;
  DropRates rates;
};

#define scentered "[fa_center][fa_middle]"

GachaType gachas[] = {
  {
    "Amberstone",
    {
      {"Beasts of", 0.23, 0.4, 1.5},
      {"AMBERSTONE", 0.3, 0.5, 1.5, 0x57a2ff},
      {"Servace", 0.74, 0.29, 1.2, 0xFFFFFF, true},
      {scentered"[scale,0.5][sprIcon,3][sprIcon,3][sprIcon,3][sprIcon,3][sprIcon,3]", 0.736, 0.34, 1, 0xFFFFFF, true, true},
      {"#1 Spiker", 0.737, 0.375, 0.5, 0xFFFFFF, true},
      {"Drop Rate Up!", 0.808, 0.24, 0.4, 0xFFFFFF, true},
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
    },
    { { 0.45, 0.825, 1, 1 }, {0.45, 0.925, 1, 2}, {0.14, 0.27, 0, 1} },
    1,
    {
      { {"serval1", 3}, {"cheerleader1"}, {"daredevil1"}, {"bestie"} },
      { {"platypus1"}, {"moth1"}, {"possum1"}, {"lyrebird1"}, {"dog1"}, {"kangaroo1"}, {"dragonfly"}, {"disruptor"} },
      default_item_drops,
    },
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
    {}
  },
};
int gacha_count = 2;

int gacha_open = 0;

void DrawTextPoses(std::vector<TextPos> &text_array, bool after_beasties)
{
  for (TextPos &text : text_array)
  {
    if (text.front_of_beasties != after_beasties) continue;
    RValue text_str = text.scribble ? yytk->CallGameScript("gml_Script_scribble", {text.text}) : text.text;
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
  int new_x = menu["selectX"].ToInt32();
  int new_y = menu["selectY"].ToInt32();
  if (yytk->CallGameScript("gml_Script_menu_pressed_right", {}).ToBoolean()) new_x += 1;
  else if (yytk->CallGameScript("gml_Script_menu_pressed_left", {}).ToBoolean()) new_x -= 1;
  if (yytk->CallGameScript("gml_Script_menu_pressed_down", {}).ToBoolean()) {
    if (new_y == 0 && gacha.down_x_pos != -1) new_x = gacha.down_x_pos;
    new_y += 1;
  }
  else if (yytk->CallGameScript("gml_Script_menu_pressed_up", {}).ToBoolean()) new_y -= 1;

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
      PullButtonPos *match = nullptr;
      for (int i = 0; i < 3; i++)
      {
        PullButtonPos *pos = &gacha.button_pos[i];
        if (pos->selection_x == new_x && pos->selection_y == new_y) { match = nullptr; break; }
        if (pos->selection_x == new_x || pos->selection_y == new_y) match = pos;
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
const double rates_display_min = -rates_text_height / 2;
const double rates_display_max = 1.0 + rates_text_height / 2;

struct DropRate {
  std::string name;
  double y_pos;
  int rarity;
  double weight;
};

void DrawGachaRatesMenu(RValue &current_menu)
{
  RValue menu = Utils::GlobalGet("mn_gacha_rates");
  if (menu.m_Object != current_menu.m_Object) return;
  GachaRatesHandleInput(menu);
  double y_pos_start = menu["selectX_anim"].ToDouble();
  double y_pos = 0 - y_pos_start;
  GachaType &gacha = gachas[gacha_open];
  RValue item_dic = Utils::GlobalGet("item_dic");
  std::vector<DropRate> rates;
  double total_weight = 0;
  RValue char_dic = Utils::GlobalGet("char_dic");
  y_pos += rates_text_height;
  DrawTextPgram(0.5, y_pos, std::format("Drop Rates for {}", gacha.name).c_str());
  for (BeastieDrop &drop : gacha.rates.drops_5) {
    y_pos += rates_text_height;
    double weight = drop.weight * gacha.rates.weight_5;
    total_weight += weight;
    if (y_pos < rates_display_min || y_pos > rates_display_max) continue;
    RValue beastie = yytk->CallBuiltin("ds_map_find_value", {char_dic, drop.family});
    rates.push_back({beastie["name"].ToString(), y_pos, 5, weight});
  }
  for (BeastieDrop &drop : gacha.rates.drops_4) {
    y_pos += rates_text_height;
    double weight = drop.weight * gacha.rates.weight_4;
    total_weight += weight;
    if (y_pos < rates_display_min || y_pos > rates_display_max) continue;
    RValue beastie = yytk->CallBuiltin("ds_map_find_value", {char_dic, drop.family});
    rates.push_back({beastie["name"].ToString(), y_pos, 4, weight});
  }
  for (int rarity = 3; rarity > 0; rarity--) {
    for (ItemDrop &drop : gacha.rates.drops_other)
    {
      if (drop.rarity == rarity) {
        y_pos += rates_text_height;
        double weight = drop.weight * gacha.rates.weight_other;
        total_weight += weight;
        if (y_pos < rates_display_min || y_pos > rates_display_max) continue;
        RValue item = yytk->CallBuiltin("ds_map_find_value", {item_dic, drop.item});
        rates.push_back({item["name"].ToString(), y_pos, rarity, weight});
      }
    }
  }
  max_gacha_scroll = y_pos_start + y_pos - 1 + rates_text_height * 1.5;
  for (DropRate rate : rates) {
    DrawTextPgram(0.5, rate.y_pos, Scribble(RValue(std::format(scentered"{} - {}[sprIcon,3] - {:.5f}%", rate.name, rate.rarity, rate.weight / total_weight * 100))));
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
}

void DrawGachaMenu()
{
  if (!Utils::GlobalGet("menu_open").ToBoolean())
    return;
  RValue menu = Utils::GlobalGet("mn_gacha");
  RValue current_menu = Utils::GlobalGet("menu_tab_open");
  if (!current_menu.ToBoolean()) return;
  if (current_menu.m_Object != menu.m_Object) {
    DrawGachaRatesMenu(current_menu);
    return;
  }
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
      DrawPullButton(scentered"Recruit 1 [sprItems,6]x1", gacha.button_pos[0], menu);
      DrawPullButton(scentered"Recruit 10 [sprItems,6]x10", gacha.button_pos[1], menu);
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

TRoutine part_system_drawit = nullptr;
void PartSystemDrawit(RValue &Result, CInstance *Self, CInstance *Other, int numArgs, RValue *Args)
{
  part_system_drawit(Result, Self, Other, numArgs, Args);
}

const char *menu_string = R"({
  "name": "Gacha", "open": 0,
  "selectX": 0, "selectY": 0,
  "onEnter": null, "onExit": null,
  "selectX_anim": 0, "selectY_anim": 0,
  "open_anim": 0, "ending": false, "end_anim": 0,
  "has_default_input": true, "has_phone": true,
  "default_layout": true, "open_anim_style": -0.4,
  "bg_x1": 0.08, "bg_x2": 1.04,
  "bg_ymargin": 0, "bg_black_overlay": true,
  "mouse_selected": false, "onSelectChange": null,
  "menu_img": 0, "commands": null
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

bool price_setup_done = false;

void PricesSetup()
{
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
  price_setup_done = true;
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
  RequestHook(NULL, "gml_Script_savedata_get_filename", "IW savedata_get_filename", SavedataGetFilename, reinterpret_cast<PVOID *>(&savedataGetFilename));
  RequestHook(NULL, "gml_Script_savedata_get_backup_filename", "IW savedata_get_backup_filename", SavedataGetBackupFilename, reinterpret_cast<PVOID *>(&savedataGetBackupFilename));
  RequestHook(NULL, "gml_Script_feedback_submit", "IW savedata_feedback_submit", FeedbackSubmit, reinterpret_cast<PVOID *>(&feedbackSubmit));
  RequestHook(NULL, "gml_Script_file_read_string", "IW file_read_string", FileReadString, reinterpret_cast<PVOID *>(&fileReadString));
  RequestHook(NULL, "gml_Script_file_write_string", "IW file_write_string", FileWriteString, reinterpret_cast<PVOID *>(&fileWriteString));

  BuiltinHook("IW file_exists", "file_exists", FileExists, reinterpret_cast<PVOID *>(&file_exists));
  BuiltinHook("IW file_delete", "file_delete", FileDelete, reinterpret_cast<PVOID *>(&file_delete));
  BuiltinHook("IW file_copy", "file_copy", FileCopy, reinterpret_cast<PVOID *>(&file_copy));

  BuiltinHook("IW part_system_drawit", "part_system_drawit", PartSystemDrawit, reinterpret_cast<PVOID *>(&part_system_drawit));

  // Setup Menus
  MenuSetup();
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
  RValue game = Utils::GetObjectInstance("objGame");
  Utils::InstanceSet(game, "pause_manual", true);
  yytk->CallGameScript("gml_Script_menu_level_in", {menu});
  menu["selectX"] = 0.0;
  menu["selectY"] = 0.0;
}

void GachaTab(bool *open)
{
  if (!price_setup_done) {
    PricesSetup();
  }
  else {
    ReduceJerseys();
  }
  if (yytk->CallBuiltin("keyboard_check_pressed", {114.0}).ToBoolean())
    OpenMenu();
  if (!ImGui::Begin("Gacha", open, ImGuiWindowFlags_NoFocusOnAppearing))
  {
    ImGui::End();
    return;
  }
  if (ImGui::Button("Open Menu"))
    OpenMenu();
  DrawGachaMenu();
  EditGachaMenu();
  ImGui::End();
}

void Store()
{
}

}