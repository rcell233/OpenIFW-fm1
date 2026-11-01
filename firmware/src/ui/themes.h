/* SPDX-License-Identifier: GPL-3.0-only
 * Named RGB565 palettes. Four colors keep a consistent left-to-right knob map.
 * SLOOP's blue/green/yellow/orange visual organization inspired the first row.
 */
#define UI_THEME_COUNT 8u
static const struct { const char *name; uint16_t color[4]; } ui_themes[UI_THEME_COUNT]={
  {"SLOOP",  {RGB(47,128,255),RGB(0,211,119),RGB(255,213,31),RGB(255,112,26)}},
  {"FOREST",  {RGB(89,211,155),RGB(162,221,115),RGB(224,209,117),RGB(99,190,173)}},
  {"AMBER", {RGB(255,172,69),RGB(255,210,105),RGB(255,235,179),RGB(248,125,77)}},
  {"MONO",  {RGB(226,230,233),RGB(184,194,203),RGB(239,241,243),RGB(158,173,185)}},
  {"OCEAN", {RGB(80,155,255),RGB(68,215,230),RGB(125,182,255),RGB(103,227,193)}},
  {"VIOLET", {RGB(168,131,255),RGB(239,131,215),RGB(115,165,255),RGB(225,184,255)}},
  {"EMBER",  {RGB(255,111,89),RGB(255,165,76),RGB(255,212,125),RGB(236,137,160)}},
  {"ICE",  {RGB(147,223,244),RGB(187,211,255),RGB(170,238,214),RGB(226,225,252)}}
};
