/* SPDX-License-Identifier: GPL-3.0-only
 * Group taps, four mapped knobs, lists, hold layers and action confirmations
 * follow Ghoulbox/Felucca (Jason Persinger; Leo Kuroshita / Hügelton
 * Instruments). All edits enter the existing parameter/event bridge; synthesis
 * is not implemented here.
 */
static void ui_message(const char *s) {
  notice = s;
  ui.notice_until = fm1_ms + 1800;
  dirty = 1;
}
static const char *ui_preset_name(uint32_t n) {
  if (n < FACTORY_COUNT)
    return preset_names[n];
  n -= FACTORY_COUNT;
  return n < 3 && ui_prefs.user_name[n][0]
             ? ui_prefs.user_name[n]
             : preset_names[FACTORY_COUNT + (n % 3)];
}
static void ui_storage_load(void) {
  memset(&ui_prefs, 0, sizeof ui_prefs);
  ui_prefs.animate = 1;
  ui_prefs.names = 1;
  ui_prefs.hold = 3;
  ui_prefs.used = saved.magic ? 7 : 0;
  for (uint32_t i = 0; i < 3; i++)
    str_cpy(ui_prefs.user_name[i], preset_names[FACTORY_COUNT + i], 13);
  IFWUiPrefs p;
  if (!st_read(SETTINGS_AT + UI_PREFS_AT, &p, sizeof p) &&
      p.magic == 0x55465749u &&
      p.crc == app_crc((const uint8_t *)&p + 8, sizeof p - 8)) {
    if (p.palette < 4 && p.style < 2 && p.animate < 2 && p.hold <= 8 &&
        p.accel < 2 && p.names < 2 && p.scope < 3 && p.filter < 4)
      ui_prefs = p;
  }
  ui_prefs.used &= 7;
  if (!saved.magic)
    ui_prefs.used = 0;
  ui_prefs.favorites &= 511;
  for (uint32_t i = 0; i < 3; i++)
    ui_prefs.user_name[i][12] = 0;
}
static int ui_storage_write(void) {
  ui_prefs.magic = 0x55465749u;
  ui_prefs.crc = app_crc((const uint8_t *)&ui_prefs + 8, sizeof ui_prefs - 8);
  return fl_write(SETTINGS_AT + UI_PREFS_AT, (const uint8_t *)&ui_prefs,
                  sizeof ui_prefs);
}
static void ui_before_preset(void) {
  ui.undo = edit_patch;
  ui.undo_preset = preset;
  ui.undo_dirty = patch_dirty;
  ui.undo_valid = 1;
}
static void ui_after_preset(void) {
  ui.hot = 0;
  ui.hot_until = 0;
  dirty = 1;
}
static void ui_send_patch(const IFWFixedPatch *p) {
#if !defined(IFW_HOST) && !defined(IFW_EMU)
  uint32_t irq = irq_save();
#endif
  ui_transfer = *p;
  RING_PUBLISH();
  if (!ui_transfer_pending) {
    ui_transfer_pending = 1;
    enqueue(9, 0, 0);
  }
#if !defined(IFW_HOST) && !defined(IFW_EMU)
  irq_restore(irq);
#endif
}
static void ui_undo(void) {
  if (!ui.undo_valid) {
    ui_message("NOTHING TO UNDO");
    return;
  }
  IFWFixedPatch p = edit_patch;
  uint32_t n = preset;
  uint8_t d = patch_dirty;
  edit_patch = ui.undo;
  preset = ui.undo_preset;
  patch_dirty = ui.undo_dirty;
  ui.undo = p;
  ui.undo_preset = n;
  ui.undo_dirty = d;
  ui_send_patch(&edit_patch);
  ui_message("UNDO / HOLD SAVE TO REDO");
}
static uint32_t ui_group(void) { return ui_pages[page].group; }
static uint32_t ui_first(uint32_t g) {
  for (uint32_t p = 0; p < UP_COUNT; p++)
    if (ui_pages[p].group == g)
      return p;
  return UP_HOME;
}
static void ui_show(uint32_t p) {
  if (p >= UP_COUNT)
    return;
  if(page==UP_PERF && p!=UP_PERF && perf_mode) {
    perf_mode=0;perf_reset();
  }
  page = p;
  home = (p == UP_HOME);
  ui.memory[ui_pages[p].group] = (uint16_t)p;
  ui.hot = 0;
  ui.hot_until = 0;
  dirty = 1;
}
static void ui_next_page(int32_t d) {
  uint32_t g = ui_group(), list[UP_COUNT], n = 0, index = 0;
  for (uint32_t p = 0; p < UP_COUNT; p++)
    if (ui_pages[p].group == g) {
      if (p == page)
        index = n;
      list[n++] = p;
    }
  if (!n)
    return;
  int32_t i = ((int32_t)index + d) % (int32_t)n;
  if (i < 0)
    i += (int32_t)n;
  ui_show(list[i]);
}
static void ui_open_group(uint32_t g) {
  if (ui_group() == g)
    ui_next_page(1);
  else
    ui_show(ui.memory[g]);
}
static uint32_t ui_resolve(uint32_t id) {
  if (id == UC_MODSRC1)
    return P_Mod1Source1 + ui.mod_slot * 4;
  if (id == UC_MODSRC2)
    return P_Mod1Source2 + ui.mod_slot * 4;
  if (id == UC_MODDEST)
    return P_Mod1Destination + ui.mod_slot * 4;
  if (id == UC_MODDEPTH)
    return P_Mod1Depth + ui.mod_slot * 4;
  return id;
}
static uint32_t ui_control_id(uint32_t knob) {
  if (knob >= 4)
    return UI_NONE;
  if (ui.layer) {
    static const uint16_t layer[6][4] = {
        {161, 178, 189, 217}, /* FX mixes: phaser, chorus, delay 1, reverb */
        {P_Amp1Level, P_Amp2Level, P_MasterLevel, P_BreathSmoothTime},
        {P_Transpose, P_MasterTune, 226, P_PitchBendRange},
        {P_Osc1SawLevel, P_Osc1TriLevel, P_Osc1PwmLevel, P_Osc1PwmWidth},
        {UC_MODSRC1, UC_MODSRC2, UC_MODDEST, UC_MODDEPTH},
        {UI_NONE, UI_NONE, UI_NONE, UI_NONE}};
    return ui_resolve(layer[ui.layer - 1][knob]);
  }
  if (ui_pages[page].kind == UK_ADVANCED)
    return (ui.raw + knob) % 237u;
  return ui_resolve(ui_pages[page].id[knob]);
}
static uint32_t ui_fx_disabled(uint32_t i) {
  return edit_patch.fx[i][IFW_FX_Bypass] || ((edit_patch.fx_bypass >> i) & 1u);
}
static void ui_toggle_fx(uint32_t i) {
  if (i >= 5)
    return;
  int32_t disabled = (int32_t)ui_fx_disabled(i);
  uint32_t id = 156 + i * 14;
  int32_t mask = (int32_t)edit_patch.fx_bypass & ~(int32_t)(1u << i);
  set_word(&edit_patch, 228, mask);
  enqueue(3, 228, mask);
  set_word(&edit_patch, id, !disabled);
  enqueue(3, (int32_t)id, !disabled);
  patch_dirty = 1;
  dirty = 1;
  ui_message(disabled ? "EFFECT ON" : "EFFECT BYPASSED");
}
static int ui_visible_preset(uint32_t n) {
  if (n >= PRESET_COUNT)
    return 0;
  if (n >= FACTORY_COUNT && !(ui_prefs.used & (1u << (n - FACTORY_COUNT))))
    return 0;
  return ui_prefs.filter == 0 ||
         (ui_prefs.filter == 1 && (ui_prefs.favorites & (1u << n))) ||
         (ui_prefs.filter == 2 && n < FACTORY_COUNT) ||
         (ui_prefs.filter == 3 && n >= FACTORY_COUNT);
}
static void ui_preset_turn(int32_t d) {
  if (!d)
    return;
  int32_t n = (int32_t)preset;
  int32_t direction = d > 0 ? 1 : -1;
  uint32_t remaining = (uint32_t)(d > 0 ? d : -d);
  if (remaining > PRESET_COUNT)
    remaining = PRESET_COUNT;
  while (remaining--) {
    int32_t p = n;
    for (uint32_t k = 0; k < PRESET_COUNT; k++) {
      p += direction;
      if (p < 0 || p >= (int32_t)PRESET_COUNT)
        break;
      if (ui_visible_preset((uint32_t)p)) {
        n = p;
        break;
      }
    }
  }
  if (n != (int32_t)preset)
    load_preset((uint32_t)n);
  if (page != UP_PRESETS)
    ui_show(UP_PRESETS);
  else
    dirty = 1;
}
static void ui_queue_save(uint32_t kind, uint32_t slot) {
  ui.save_kind = (uint8_t)kind;
  ui.save_slot = slot;
  ui.save_patch = edit_patch;
  save_pending = 1;
  ui_message("RELEASE KEYS TO SAVE");
}
static void ui_begin_name(uint32_t slot) {
  ui.naming = 1;
  ui.confirm = 0;
  ui.slot = (uint16_t)slot;
  ui.name_pos = 0;
  ui.name_char = 0;
  str_cpy(ui.name, ui_prefs.user_name[slot], 13);
  dirty = 1;
}
static void ui_confirm_action(uint32_t action) {
  uint32_t slot = ui.slot;
  if (action == UM_SAVE) {
    ui_begin_name(slot);
    return;
  }
  if (action == UM_LOAD) {
    load_preset(FACTORY_COUNT + slot);
    ui_message("USER SOUND LOADED");
  }
  if (action == UM_ERASE) {
    ui_prefs.used &= (uint8_t)~(1u << slot);
    make_preset(&saved.user[slot], 0);
    ui_queue_save(UM_ERASE, slot);
  }
  if (action == UM_CLEAR_LEARN) {
    for (uint32_t i = 0; i < 128; i++)
      saved.learn[i] = -1;
    ui_message("MIDI LEARN CLEARED");
  }
  if (action == UM_SETTINGS)
    ui_queue_save(UM_SETTINGS, slot);
  ui.confirm = 0;
  dirty = 1;
}
static void ui_action(void) {
  if (ui.confirm) {
    ui_confirm_action(ui.confirm);
    return;
  }
  if (ui.naming) {
    if (!ui.name[0])
      str_cpy(ui.name, preset_names[FACTORY_COUNT + ui.slot], 13);
    str_cpy(ui_prefs.user_name[ui.slot], ui.name, 13);
    ui.naming = 0;
    ui_queue_save(UM_SAVE, ui.slot);
    return;
  }
  if (page != UP_SAVE)
    return;
  uint32_t slot = ui.slot, action = ui.action ? ui.action : UM_SAVE;
  if (action == UM_LOAD && !(ui_prefs.used & (1u << slot))) {
    ui_message("EMPTY USER SLOT");
    return;
  }
  if (action == UM_LOAD && !patch_dirty) {
    ui_confirm_action(action);
    return;
  }
  if (action == UM_SAVE && !(ui_prefs.used & (1u << slot))) {
    ui_begin_name(slot);
    return;
  }
  ui.confirm = (uint16_t)action;
  ui.confirm_slot = (uint16_t)slot;
  dirty = 1;
}
static uint32_t ui_menu_count(uint32_t tab) {
  static const uint8_t counts[] = {5, 2, 4, 4, 5};
  return counts[tab % 5];
}
static int32_t ui_menu_value(uint32_t tab, uint32_t row) {
  if (tab == 0) {
    uint8_t a[] = {ui_prefs.palette, ui_prefs.style, ui_prefs.animate,
                   ui_prefs.scope, ui_prefs.names};
    return a[row % 5];
  }
  if (tab == 1)
    return row ? ui_prefs.accel : ui_prefs.hold;
  if (tab == 2) {
    static const uint16_t ids[] = {226, P_MasterLevel, P_Amp1Level,
                                   P_Amp2Level};
    return word(&edit_patch, ids[row % 4]);
  }
  if (tab == 3) {
    if (row == 0)
      return saved.channel;
    if (row == 1)
      return saved.wind;
    if (row == 2)
      return learn_armed;
    return 0;
  }
  return 0;
}
static void ui_menu_edit(int32_t d) {
  uint32_t t = ui.menu_tab, r = ui.menu_row;
  if (!d)
    return;
  if (t == 0) {
    uint8_t *p[] = {&ui_prefs.palette, &ui_prefs.style, &ui_prefs.animate,
                    &ui_prefs.scope, &ui_prefs.names};
    uint8_t max[] = {3, 1, 1, 2, 1};
    *p[r] = (uint8_t)ifw_clamp(*p[r] + d, 0, max[r]);
  }
  if (t == 1) {
    if (!r)
      ui_prefs.hold = (uint8_t)ifw_clamp(ui_prefs.hold + d, 1, 8);
    else
      ui_prefs.accel = (uint8_t)ifw_clamp(ui_prefs.accel + d, 0, 1);
  }
  if (t == 2) {
    static const uint16_t ids[] = {226, P_MasterLevel, P_Amp1Level,
                                   P_Amp2Level};
    edit_value(ids[r], d, 0);
  }
  if (t == 3) {
    if (r < 2)
      edit_value(r ? 229 : 230, d, 0);
    else if (r == 2) {
      learn_armed = d > 0;
      ui_message(learn_armed ? "MOVE A CC TO LEARN" : "LEARN CANCELLED");
    } else if (d > 0) {
      ui.confirm = UM_CLEAR_LEARN;
    }
  }
  if (t == 4) {
    if (r == 0 && d > 0)
      ui.confirm = UM_SETTINGS;
    if (r == 1 && d > 0) {
      ui.menu = 0;
      ui_show(UP_ADVANCED);
    }
    if (r == 3 && d > 0) {
      ui.menu = 0;
      ui_show(UP_PERF);
    }
    if (r == 4 && d > 0)
      panic_panel();
  }
  dirty = 1;
}
static void ui_change(uint32_t id, int32_t d) {
  if (id >= 233) {
    ui_message("READ ONLY");
    return;
  }
  if (id >= 156 && id < 226 && (id - 156) % 14 == 0) {
    uint32_t k = (id - 156) / 14;
    int32_t value = ifw_clamp((int32_t)ui_fx_disabled(k) + d, 0, 1);
    int32_t mask = (int32_t)edit_patch.fx_bypass & ~(int32_t)(1u << k);
    set_word(&edit_patch, 228, mask);
    enqueue(3, 228, mask);
    set_word(&edit_patch, id, value);
    enqueue(3, (int32_t)id, value);
    patch_dirty = 1;
    dirty = 1;
    return;
  }
  if (id == P_Lfo1BPMSync || id == P_Lfo2BPMSync) {
    uint32_t other = id + 1;
    int32_t v = !!(word(&edit_patch, id) || word(&edit_patch, other));
    v = ifw_clamp(v + d, 0, 1);
    set_word(&edit_patch, id, v);
    set_word(&edit_patch, other, v);
    enqueue(3, (int32_t)id, v);
    enqueue(3, (int32_t)other, v);
    patch_dirty = 1;
    dirty = 1;
    last_control = id;
    return;
  }
  /* Display aliases (TRI/WAVE, PWM WIDTH/PHASE) share a parameter value.
   * A backwards turn must pass that value instead of getting stuck on it. */
  if (panel_params[id].choice) {
    uint32_t choice = panel_params[id].choice, count = 0, at = 0;
    int32_t values[64], value = word(&edit_patch, id);
    for (uint32_t i = 0; i < option_count[choice]; i++) {
      int32_t v = options[choice][i].value;
      uint32_t j = 0;
      while (j < count && values[j] != v)
        j++;
      if (j < count)
        continue;
      if (v == value)
        at = count;
      values[count++] = v;
    }
    value = values[ifw_clamp((int32_t)at + d, 0, (int32_t)count - 1)];
    set_word(&edit_patch, id, value);
    enqueue(3, (int32_t)id, value);
    patch_dirty = 1;
    last_control = id;
    dirty = 1;
    return;
  }
  edit_value(id, d, 0);
}
static void ui_knob(uint32_t k, int32_t d) {
  if (!d || k > 3)
    return;
  ui.hot = (uint16_t)k;
  ui.hot_until = fm1_ms + 1300;
  if (ui.confirm) {
    dirty = 1;
    return;
  }
  if (ui.menu) {
    ui_menu_edit(d);
    return;
  }
  if (ui.naming) {
    if (k == 0) {
      ui.name_char = (uint16_t)ifw_clamp((int32_t)ui.name_char + d, 0, 26);
      uint32_t len = 0;
      while (ui.name[len])
        len++;
      ui.name[ui.name_pos] =
          (char)(ui.name_char == 26 ? ' ' : 'A' + ui.name_char);
      if (ui.name_pos >= len)
        ui.name[ui.name_pos + 1] = 0;
    } else if (k == 1) {
      uint32_t len = 0;
      while (ui.name[len])
        len++;
      ui.name_pos = (uint16_t)ifw_clamp((int32_t)ui.name_pos + d, 0,
                                        (int32_t)(len < 11 ? len : 11));
    } else if (k == 3 && d > 0)
      ui_action();
    dirty = 1;
    return;
  }
  uint32_t id = control_id(k);
  if (id == UC_PERFMODE) {
    perf_mode=(uint32_t)ifw_clamp((int32_t)perf_mode+d,0,2);
    perf_reset();dirty=1;
    return;
  }
  if (id == UC_PERFRESET) {
    if(d>0) {
      perf_reset();ui_poll_max_gap=0;
#ifdef IFW_TARGET
      audio_yields=0;
#endif
    }
    dirty=1;return;
  }
  if (id == UC_PERFFREEZE) {
    if(d>0) {perf_snapshot();perf_freeze=1;}
    else perf_freeze=0;
    dirty=1;return;
  }
  if (id == UC_PERFSTATE) {
    ui_message("XRUN / DMA LATE MUST STAY 0");return;
  }
  if (id == UC_PRESET) {
    ui_preset_turn(d);
    return;
  }
  if (id == UC_CORE) {
    ui_message("OPENIFW / SINGLE MONO CORE");
    return;
  }
  if (id == UC_FAV) {
    if (d > 0)
      ui_prefs.favorites |= (uint16_t)(1u << preset);
    else
      ui_prefs.favorites &= (uint16_t)~(1u << preset);
    dirty = 1;
    return;
  }
  if (id == UC_LIST) {
    ui_prefs.filter = (uint8_t)ifw_clamp((int32_t)ui_prefs.filter + d, 0, 3);
    dirty = 1;
    return;
  }
  if (id == UC_MODSLOT) {
    ui.mod_slot = (uint16_t)ifw_clamp((int32_t)ui.mod_slot + d, 0, 7);
    dirty = 1;
    return;
  }
  if (id == UC_SLOT) {
    ui.slot = (uint16_t)ifw_clamp((int32_t)ui.slot + d, 0, 2);
    saved.slot = (uint8_t)ui.slot;
    dirty = 1;
    return;
  }
  if (id == UC_LOAD || id == UC_ERASE || id == UC_SAVE) {
    if (d > 0)
      ui.action = (uint16_t)(id == UC_LOAD    ? UM_LOAD
                             : id == UC_ERASE ? UM_ERASE
                                              : UM_SAVE);
    else
      ui.action = 0;
    dirty = 1;
    return;
  }
  if (id == UI_NONE) {
    ui_message("NO CONTROL ON THIS KNOB");
    return;
  }
  if (ui_prefs.accel && fm1_ms - ui.knob_time < 50 && !panel_params[id].choice)
    d *= 2;
  ui.knob_time = fm1_ms;
  ui_change(id, d);
}
static void ui_tap(uint32_t b) {
  if (ui.confirm) {
    if (b == BTN_OCT_UP)
      ui_action();
    else if (b == BTN_OCT_DOWN || b == BTN_HOME) {
      ui.confirm = 0;
      ui_message("CANCELLED");
    }
    return;
  }
  if (ui.naming) {
    if (b == BTN_OCT_UP)
      ui_action();
    else if (b == BTN_OCT_DOWN) {
      if (ui.name_pos)
        ui.name_pos--;
      ui.name[ui.name_pos] = 0;
      dirty = 1;
    } else if (b == BTN_HOME) {
      ui.naming = 0;
      ui_message("CANCELLED");
    }
    return;
  }
  if (ui.menu) {
    if (b == BTN_HOME) {
      ui.menu = 0;
      dirty = 1;
    } else if (b == BTN_OCT_UP)
      ui_menu_edit(1);
    else if (b == BTN_OCT_DOWN)
      ui_menu_edit(-1);
    return;
  }
  if (b == BTN_OCT_UP || b == BTN_OCT_DOWN) {
    if (page == UP_SAVE) {
      if (b == BTN_OCT_UP)
        ui_action();
      else {
        ui.action = 0;
        ui_show(UP_HOME);
      }
    } else {
      edit_value(231, b == BTN_OCT_UP ? 1 : -1, 0);
    }
    return;
  }
  if (b == BTN_PLAY) {
    if (meter.held)
      panic_panel();
    else
      note_start(69, 100);
    dirty = 1;
    return;
  }
  if (b == BTN_REC) {
    learn_armed = !learn_armed;
    ui_message(learn_armed ? "MOVE A CC TO LEARN" : "LEARN CANCELLED");
    return;
  }
  if (b == BTN_SAVE) {
    if (page == UP_SAVE)
      ui_show(UP_PRESETS);
    else {
      ui.slot = saved.slot;
      ui.action = UM_SAVE;
      ui_show(UP_SAVE);
    }
    return;
  }
  if (b == BTN_HOME) {
    if (home)
      ui_show(UP_MIX);
    else
      ui_show(UP_HOME);
    return;
  }
  static const uint8_t buttons[] = {BTN_FX,   BTN_SEL, BTN_ENV, BTN_LFO,
                                    BTN_EDIT, BTN_GLO, BTN_ARP, BTN_SEQ};
  static const uint8_t group[] = {UG_FX,   UG_TUNE,   UG_ENV, UG_LFO,
                                  UG_EDIT, UG_GLOBAL, UG_AMP, UG_MOD};
  for (uint32_t i = 0; i < 8; i++)
    if (b == buttons[i]) {
      ui_open_group(group[i]);
      return;
    }
}
static uint32_t ui_layer_for(uint32_t b) {
  switch (b) {
  case BTN_FX:
    return 1;
  case BTN_GLO:
    return 2;
  case BTN_SEL:
    return 3;
  case BTN_EDIT:
    return 4;
  case BTN_SEQ:
    return 5;
  case BTN_REC:
    return 6;
  default:
    return 0;
  }
}
static void ui_poll(uint32_t pressed, uint32_t released) {
  uint32_t down = fm1_in.buttons, now = fm1_ms;
  for (uint32_t b = 0; b < 14; b++) {
    uint32_t bit = 1u << b;
    if (pressed & bit) {
      ui.button_time[b] = now ? now : 1u;
      ui.long_mask &= ~bit;
      uint32_t layer = ui_layer_for(b);
      if (ui.layer_locked) {
        ui.layer = ui.layer_locked = 0;
        ui.long_mask |= bit;
        dirty = 1;
        continue;
      }
      if (layer && !ui.menu && !ui.confirm && !ui.naming &&
          ui.tap_time[b] && now - ui.tap_time[b] < 240) {
        ui.layer = (uint8_t)layer;
        ui.layer_locked = 1;
        ui.long_mask |= bit;
        dirty = 1;
        continue;
      }
#ifdef IFW_HOST
      if (!(down & bit)) {
        ui_tap(b);
        ui.button_time[b] = 0;
        continue;
      } /* scripted host tap */
#endif
    }
    if ((down & bit) && ui.button_time[b] && !(ui.long_mask & bit) &&
        (int32_t)(now - ui.button_time[b]) >=
            (int32_t)(200u + ui_prefs.hold * 100u)) {
      ui.long_mask |= bit;
      if (b == BTN_HOME && !ui.confirm && !ui.naming) {
        ui.menu = !ui.menu;
        ui.layer = 0;
        ui.menu_tab = ui.menu_row = 0;
        dirty = 1;
      } else if (b == BTN_SAVE && !ui.menu && !ui.confirm && !ui.naming)
        ui_undo();
      else if (!ui.menu && !ui.confirm && !ui.naming) {
        ui.layer = (uint8_t)ui_layer_for(b);
        dirty = 1;
      }
    }
    if ((released & bit) && ui.button_time[b]) {
      if (ui.layer == ui_layer_for(b) && !ui.layer_locked &&
          ui.long_mask & bit) {
        ui.layer = 0;
        dirty = 1;
      }
      if (!(ui.long_mask & bit)) {
        ui_tap(b);
        ui.tap_time[b] = now;
      }
      ui.button_time[b] = 0;
      ui.long_mask &= ~bit;
    }
  }
}
static void ui_module(int32_t d) {
  if (ui_group() == UG_MOD) {
    ui.mod_slot = (uint16_t)ifw_clamp((int32_t)ui.mod_slot + d, 0, 7);
    dirty = 1;
    return;
  }
  if (ui_group() == UG_EDIT) {
    static const uint16_t p[] = {UP_OSC1,    UP_OSC2,    UP_OSC3,
                                 UP_OSC4,    UP_FILTER1, UP_FILTER2,
                                 UP_FILTER3, UP_VOICE,   UP_SMOOTH};
    uint32_t n = 0;
    while (n + 1 < sizeof p / sizeof *p && p[n + 1] <= page)
      n++;
    n = (uint32_t)ifw_clamp((int32_t)n + d, 0,
                            (int32_t)(sizeof p / sizeof *p) - 1);
    ui_show(p[n]);
    return;
  }
  if (ui_group() == UG_FX) {
    static const uint16_t p[] = {UP_FX,      UP_PHASER,   UP_DELAY1,
                                 UP_DELAY2,  UP_REVERB,   UP_CHORUS,
                                 UP_EXCITER, UP_ENHANCER, UP_EQ};
    uint32_t n = 0;
    while (n + 1 < sizeof p / sizeof *p && p[n + 1] <= page)
      n++;
    n = (uint32_t)ifw_clamp((int32_t)n + d, 0,
                            (int32_t)(sizeof p / sizeof *p) - 1);
    ui_show(p[n]);
    return;
  }
  ui_next_page(d);
}
static void ui_encoders(void) {
  int32_t select = fm1_enc_take(ENC_SELECT),
          algorithm = fm1_enc_take(ENC_ALGORITHM),
          presets = fm1_enc_take(ENC_PRESETS);
  if (ui.confirm) { /* never navigate a list behind its confirmation */
  } else if (ui.menu) {
    if (algorithm) {
      ui.menu_tab = (uint16_t)ifw_clamp((int32_t)ui.menu_tab + algorithm, 0, 4);
      ui.menu_row = 0;
      dirty = 1;
    }
    if (select || presets) {
      ui.menu_row =
          (uint16_t)ifw_clamp((int32_t)ui.menu_row + select + presets, 0,
                              (int32_t)ui_menu_count(ui.menu_tab) - 1);
      dirty = 1;
    }
  } else if (ui.naming) {
    if (select || presets)
      ui_knob(0, select + presets);
    if (algorithm)
      ui_knob(1, algorithm);
  } else if (ui.layer) {
    if (presets)
      ui_message("RELEASE LAYER TO LOAD SOUND");
    if (select || algorithm) {
      if (ui.layer == 5) {
        ui.mod_slot = (uint16_t)ifw_clamp(
            (int32_t)ui.mod_slot + select + algorithm, 0, 7);
        dirty = 1;
      }
    }
  } else {
    if (presets)
      ui_preset_turn(presets);
    if (select) {
      if (page == UP_PRESETS)
        ui_preset_turn(select);
      else if (page == UP_SAVE)
        ui_knob(0, select);
      else if (page == UP_ADVANCED) {
        ui.raw = (uint16_t)ifw_clamp((int32_t)ui.raw + select * 4, 0, 233);
        dirty = 1;
      } else
        ui_next_page(select);
    }
    if (algorithm)
      ui_module(algorithm);
  }
  for (uint32_t k = 0; k < 4; k++) {
    int32_t n = fm1_enc_take(ENC_KNOB1 + k);
    if (n)
      ui_knob(k, n);
  }
}
static int ui_key(uint32_t k) {
  if (ui.naming) {
    uint32_t pos = ui.name_pos, len = 0;
    if (pos > 11)
      pos = 11;
    while (ui.name[len])
      len++;
    ui.name[pos] = (char)(k == 26 ? ' ' : 'A' + k);
    if (pos >= len)
      ui.name[pos + 1] = 0;
    ui.name_pos = (uint16_t)(pos < 11 ? pos + 1 : pos);
    dirty = 1;
    return 1;
  }
  if (!ui.layer)
    return ui.confirm != 0;
  static const uint8_t white[] = {0, 2, 4, 6, 7, 9, 11, 12};
  uint32_t key = 8;
  for (uint32_t i = 0; i < 8; i++)
    if (k == white[i])
      key = i;
  if (key >= 8)
    return 1;
  if (ui.layer == 1) {
    if (key < 5)
      ui_toggle_fx(key);
    else {
      static const uint16_t p[] = {UP_EXCITER, UP_ENHANCER, UP_EQ};
      ui.layer = 0;
      ui_show(p[key - 5]);
    }
  }
  if (ui.layer == 4) {
    static const uint16_t p[] = {UP_OSC1,    UP_OSC2,    UP_OSC3,    UP_OSC4,
                                 UP_FILTER1, UP_FILTER2, UP_FILTER3, UP_VOICE};
    ui.layer = 0;
    ui_show(p[key]);
  }
  if (ui.layer == 5) {
    ui.mod_slot = (uint16_t)key;
    dirty = 1;
  }
  if (ui.layer == 6) {
    if (key == 0) {
      learn_armed = 1;
      ui_message("MOVE A CC TO LEARN");
    }
    if (key == 1) {
      learn_armed = 0;
      ui_message("LEARN CANCELLED");
    }
    if (key == 2) {
      ui.confirm = UM_CLEAR_LEARN;
      dirty = 1;
    }
  }
  return 1;
}
static void ui_tick(void) {
  if (ui.notice_until && (int32_t)(fm1_ms - ui.notice_until) >= 0) {
    ui.notice_until = 0;
    dirty = 1;
  }
  if (ui.hot_until && (int32_t)(fm1_ms - ui.hot_until) >= 0) {
    ui.hot_until = 0;
    dirty = 1;
  }
  if (save_pending && meter.quiet && qread == qwrite) {
    if (ui.save_kind == UM_SAVE) {
      saved.user[ui.save_slot] = ui.save_patch;
      saved.slot = (uint8_t)ui.save_slot;
      ui_prefs.used |= (uint8_t)(1u << ui.save_slot);
    }
    int rc = storage_save();
    if (!rc && ui.save_kind == UM_SAVE &&
        !memcmp(&edit_patch, &ui.save_patch, sizeof edit_patch)) {
      preset = FACTORY_COUNT + ui.save_slot;
      patch_dirty = 0;
    }
    save_pending = 0;
    ui_message(rc ? "SAVE FAILED" : "SAVED");
  }
}
static void ui_leds(void) {
  static const uint8_t button[] = {BTN_HOME, BTN_EDIT, BTN_ENV, BTN_LFO,
                                   BTN_SEQ,  BTN_FX,   BTN_SEL, BTN_ARP,
                                   BTN_GLO,  BTN_SAVE};
  for (uint32_t i = 0; i < 10; i++)
    fm1_led_key(button[i], !ui.menu && ui_group() == i);
  fm1_led_key(BTN_REC, (int)learn_armed);
  fm1_led_key(BTN_PLAY, meter.held != 0);
  fm1_led_key(BTN_OCT_UP, ui.confirm || ui.naming || page == UP_SAVE);
  fm1_led_key(BTN_OCT_DOWN, ui.confirm || ui.naming || page == UP_SAVE);
}
static void ui_init(void) {
  memset(&ui, 0, sizeof ui);
  ui.slot = saved.slot;
  ui.action = UM_SAVE;
  for (uint32_t g = 0; g < UG_COUNT; g++)
    ui.memory[g] = (uint16_t)ui_first(g);
  ui_show(UP_HOME);
}
