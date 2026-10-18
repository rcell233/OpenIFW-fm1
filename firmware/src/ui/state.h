/* SPDX-License-Identifier: GPL-3.0-only
 * OpenIFW page/navigation state. Inspired by Ghoulbox / Felucca's UI, not their
 * engines.
 */
#include "font.h"
#include "pages.h"
#define UI_NONE 65535u
#define UI_PREFS_AT 3840u
enum { UM_NONE, UM_LOAD, UM_ERASE, UM_SAVE, UM_CLEAR_LEARN, UM_SETTINGS };
typedef struct {
  uint32_t magic, crc;
  uint8_t palette, style, animate, hold, accel, names, scope, filter;
  uint16_t favorites;
  uint8_t used, reserved;
  char user_name[3][13];
} IFWUiPrefs;
_Static_assert(sizeof saved <= UI_PREFS_AT, "UI preferences overlap patch data");
_Static_assert(UI_PREFS_AT + sizeof(IFWUiPrefs) <= 4096,
               "UI preferences exceed settings sector");
static IFWUiPrefs ui_prefs;
static struct {
  uint16_t memory[UG_COUNT], hot, mod_slot, raw, menu_tab, menu_row, slot,
      action;
  uint16_t confirm, confirm_slot, name_pos, name_char;
  uint8_t menu, layer, layer_locked, naming, undo_valid, undo_dirty, save_kind;
  uint32_t button_time[14], tap_time[14], long_mask, notice_until, hot_until,
      knob_time;
  uint32_t undo_preset, save_slot;
  IFWFixedPatch undo, save_patch;
  char name[13];
} ui;
static IFWFixedPatch ui_transfer;
static volatile uint32_t ui_transfer_pending;
static uint32_t ui_control_id(uint32_t knob);
static void ui_storage_load(void);
static int ui_storage_write(void);
static void ui_before_preset(void);
static void ui_after_preset(void);
static void ui_poll(uint32_t pressed, uint32_t released);
static void ui_encoders(void);
static int ui_key(uint32_t k);
static void ui_tick(void);
static void ui_leds(void);
static void ui_init(void);
static void ui_message(const char *s);
static void draw_panel(uint32_t full);
static const char *ui_preset_name(uint32_t n);
