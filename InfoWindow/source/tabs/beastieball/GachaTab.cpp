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

struct Vec2 {
  double x;
  double y;
};

struct GachaType {
  const char *name;
  std::vector<TextPos> text;
  std::vector<BeastiePos> beastie_layout;
  std::vector<SpritePos> sprites;
  Vec2 button_pos[2];
};

GachaType gachas[] = {
  {
    "Amberstone",
    {
      {"Beasts of", 0.23, 0.4, 1.5},
      {"AMBERSTONE", 0.3, 0.5, 1.5, 0x57a2ff},
      {"Servace", 0.74, 0.29, 1.2, 0xFFFFFF, true},
      {"[fa_center][fa_middle][scale,0.5][sprIcon,3][sprIcon,3][sprIcon,3][sprIcon,3][sprIcon,3]", 0.736, 0.34, 1, 0xFFFFFF, true, true},
      {"#1 Spiker", 0.737, 0.375, 0.5, 0xFFFFFF, true},
    },
    {
      {"serval", "volley", 0.46, 0.76, 1.2, 1.2, true},
      {"possum", "good", 0.85, 0.78, -1, 1},
      {"moth", "good", 0.66, 0.84, 1, 1, true},
      {"platypus2", "spike", 0.3, 0.9, 1, 1},
      {"lyrebird", "spike", 0.15, 1, 1, 1, true},
      {"dog", "ready", 0.225, 1.0, 1, 1, true},
      {"cheerleader", "good", 0.45, 0.92, 1.3, 1.3, true},
      {"daredevil", "spike", 0.7, 0.95, -1.2, 1.2, true},
      {"kangaroo", "menu", 0.9, 0.93, -1, 1, true},
      {"bestie", "good", 0.63, 0.98, -1.1, 1.1, true},
      {"dragonfly", "volley", 0.89, 0.3, 1, 1, true},
    },
    {
      {"sprBall", 2, 0.39, 0.2, 0.7, 0.7, 0, 1.5},
    },
    { { 0.45, 0.825 }, {0.45, 0.925} },
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
    { { 0.2, 0.2}, { 0.2, 0.3 } },
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

bool DrawPullButton(const char *text, Vec2 &pos)
{
  return yytk->CallGameScript("gml_Script_draw_text_button", {
    yytk->CallGameScript("gml_Script_canvas_x", {pos.x, pos.y}), yytk->CallGameScript("gml_Script_canvas_y", {pos.y}), yytk->CallGameScript("gml_Script_scribble", {text})}).ToBoolean();
}


void DrawGachaMenu()
{
  if (!Utils::GlobalGet("menu_open").ToBoolean())
    return;
  RValue menu = Utils::GlobalGet("mn_gacha");
  RValue game = Utils::GetObjectInstance("objGame");
  RValue current_menu = Utils::GlobalGet("menu_tab_open");
  if (!current_menu.ToBoolean() || current_menu.m_Object != menu.m_Object) return;
  double header_dist = 1. / double(gacha_count + 1);
  double x_pos = 0;
  for (int i = 0; i < gacha_count; i++) {
    GachaType &gacha = gachas[i];
    x_pos += header_dist;
    if (yytk->CallGameScript("gml_Script_draw_text_button", {yytk->CallGameScript("gml_Script_canvas_x", {x_pos, 0.075}), yytk->CallGameScript("gml_Script_canvas_y", {0.075}), gacha.name}).ToBoolean())
      gacha_open = i;
    if (gacha_open == i) {
      DrawSprites(gacha.sprites, false);
      DrawBeastieLayout(gacha.beastie_layout, false);
      DrawTextPoses(gacha.text, false);
      DrawBeastieLayout(gacha.beastie_layout, true);
      DrawTextPoses(gacha.text, true);
      DrawSprites(gacha.sprites, true);
      DrawPullButton("[fa_center][fa_middle]Recruit 1 [sprItems,6]x1", gacha.button_pos[0]);
      DrawPullButton("[fa_center][fa_middle]Recruit 10 [sprItems,6]x10", gacha.button_pos[1]);
    }
  }
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
  ImGui::InputDouble("X", &beastie.x, 0.01, 0.1);
  ImGui::InputDouble("Y", &beastie.y, 0.01, 0.1);
  ImGui::InputDouble("X Scale", &beastie.x_scale, 0.01, 0.1);
  ImGui::InputDouble("Y Scale", &beastie.y_scale, 0.01, 0.1);
  ImGui::InputDouble("Rotation", &beastie.rotation, 1.0, 10.0);
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
  ImGui::InputDouble("X", &text.x, 0.01, 0.1);
  ImGui::InputDouble("Y", &text.y, 0.01, 0.1);
  ImGui::InputDouble("Scale", &text.scale, 0.01, 0.1);
  ImGui::Checkbox("In front of beasties", &text.front_of_beasties);
  ImGui::Checkbox("Scribble", &text.scribble);
  ImGui::EndChild();

  size_t sprites_count = gacha.sprites.size();
  if (editing_sprites >= sprites_count) editing_sprites = 0;
  if (ImGui::BeginCombo("Text", std::format("{} - {}", editing_sprites, gacha.sprites[editing_sprites].sprite).c_str())) {
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
  RValue menu = Utils::GlobalGet("mn_gacha");
  RValue game = Utils::GetObjectInstance("objGame");
  Utils::InstanceSet(game, "pause_manual", true);
  yytk->CallGameScript("gml_Script_menu_level_in", {menu});
  menu["selectX"] = 0.0;
  menu["selectY"] = 0.0;
}

void GachaTab(bool *open)
{
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