/* SPDX-License-Identifier: GPL-3.0-only
 * Controls adapted from GHOULBOX (Jason Persinger, 2026), built on Felucca
 * (Leo Kuroshita / Hügelton Instruments). Plain panels and Inter Tight text.
 * Displays only; no oscillator, envelope, modulation or effect
 * state is advanced.
 */
typedef struct {
  uint16_t bg, surf, text, theme, accent, edge, mid, dim, line;
} IFWUiColors;
static IFWUiColors uc;
static uint16_t ui_mix(uint16_t a, uint16_t b, uint32_t n) {
  uint32_t r = ((a >> 11) * (100 - n) + (b >> 11) * n) / 100,
           g = (((a >> 5) & 63) * (100 - n) + ((b >> 5) & 63) * n) / 100,
           v = ((a & 31) * (100 - n) + (b & 31) * n) / 100;
  return (uint16_t)(r << 11 | g << 5 | v);
}
static void ui_palette(void) {
  static const uint16_t p[4][5] = {
      {RGB(8, 12, 16), RGB(16, 22, 28), RGB(248, 250, 252), RGB(116, 196, 248),
       RGB(255, 196, 112)},
      {RGB(8, 14, 10), RGB(16, 26, 20), RGB(248, 250, 248), RGB(144, 224, 168),
       RGB(255, 216, 128)},
      {RGB(14, 12, 8), RGB(26, 22, 16), RGB(252, 248, 240), RGB(248, 200, 128),
       RGB(255, 236, 192)},
      {RGB(0, 0, 0), RGB(0, 0, 0), RGB(255, 255, 255), RGB(255, 255, 255),
       RGB(255, 255, 255)}};
  uint32_t n = ui_prefs.palette % 4;
  uc.bg = p[n][0];
  uc.surf = p[n][1];
  uc.text = p[n][2];
  uc.theme = p[n][3];
  uc.accent = p[n][4];
  uc.edge = ui_mix(uc.theme, uc.bg, 55);
  uc.mid = ui_mix(uc.bg, uc.text, 85);
  uc.dim = ui_mix(uc.bg, uc.text, 65);
  uc.line = ui_mix(uc.bg, uc.text, 28);
}
static void ui_frame(int x, int y, int w, int h, uint16_t c) {
  cv_rect(x, y, w, 1, c);
  cv_rect(x, y + h - 1, w, 1, c);
  cv_rect(x, y, 1, h, c);
  cv_rect(x + w - 1, y, 1, h, c);
}
static void ui_window(int x, int y, int w, int h, int b) {
  cv_rect(x,y,w,h,ui_prefs.style?uc.bg:uc.surf);
  ui_frame(x,y,w,h,b==1?uc.line:uc.edge);
}
static void ui_cursor(int x, int y) {
  cv_rect(x+3,y,3,10,uc.theme);
}
static void ui_fit(int x, int y, int width, const felucca_font_t *f,
                   const char *s, uint16_t color) {
  char b[64];
  str_cpy(b, s, sizeof b);
  uint32_t n = 0;
  while (b[n])
    n++;
  if (text_w(f, b) > width) {
    while (n && text_w(f, b) + text_w(f, "..") > width)
      b[--n] = 0;
    if (n < 61) {
      b[n++] = '.';
      b[n++] = '.';
      b[n] = 0;
    }
  }
  cv_text(x, y, f, b, color);
}
static void ui_right(int right, int y, const felucca_font_t *f, const char *s,
                     uint16_t c) {
  cv_text(right - text_w(f, s), y, f, s, c);
}
static void ui_center(int x, int y, int w, const felucca_font_t *f,
                      const char *s, uint16_t c) {
  if (text_w(f, s) > w)
    ui_fit(x, y, w, f, s, c);
  else
    cv_text(x + (w - text_w(f, s)) / 2, y, f, s, c);
}
static void ui_append(char *s, const char *t, uint32_t n) {
  uint32_t i = 0;
  while (s[i] && i < n)
    i++;
  if (i < n)
    str_cpy(s + i, t, n - i);
}
static void ui_int(char *s, int32_t n) { fmt_int(s, n); }
static void ui_fixed(char *s, int32_t v, int digits) {
  int64_t a = v;
  uint32_t scale = digits == 2 ? 100u : 10u;
  int neg = a < 0;
  if (neg)
    a = -a;
  uint32_t n = (uint32_t)((a * scale + 32768) / 65536);
  char b[16];
  fmt_int(b, (int32_t)(n / scale));
  s[0] = 0;
  if (neg)
    ui_append(s, "-", 32);
  ui_append(s, b, 32);
  ui_append(s, ".", 32);
  char f[3];
  f[0] = (char)('0' + (digits == 2 ? n % 100 / 10 : n % 10));
  f[1] = digits == 2 ? (char)('0' + n % 10) : 0;
  f[2] = 0;
  ui_append(s, f, 32);
}
static void ui_value(uint32_t id, char *s, int compact) {
  s[0] = 0;
  if (id == UI_NONE) {
    str_cpy(s, "--", 32);
    return;
  }
  if (id == UC_PERFMODE) {
    static const char *const modes[]={"SYNTH","SINE","MUTE"};
    str_cpy(s,modes[perf_mode%3],32);return;
  }
  if (id == UC_PERFRESET) {str_cpy(s,"+",32);return;}
  if (id == UC_PERFFREEZE) {str_cpy(s,perf_freeze?"ON":"OFF",32);return;}
  if (id == UC_PERFSTATE) {
#if defined(IFW_HOST) || defined(IFW_EMU)
    str_cpy(s,"N/A",32);
#else
    str_cpy(s,(perf_view.overruns||perf_view.dma_late||audio_underruns)?"LATE":"OK",32);
#endif
    return;
  }
  if (id == UC_PRESET) {
    ui_int(s, (int32_t)preset + 1);
    return;
  }
  if (id == UC_CORE) {
    str_cpy(s, "IFW", 32);
    return;
  }
  if (id == UC_FAV) {
    str_cpy(s, ui_prefs.favorites & (1u << preset) ? "ON" : "OFF", 32);
    return;
  }
  if (id == UC_LIST) {
    static const char *const n[] = {"ALL", "FAV", "FACT", "USER"};
    str_cpy(s, n[ui_prefs.filter % 4], 32);
    return;
  }
  if (id == UC_SLOT) {
    ui_int(s, ui.slot + 1);
    return;
  }
  if (id == UC_MODSLOT) {
    ui_int(s, ui.mod_slot + 1);
    return;
  }
  if (id == UC_LOAD || id == UC_ERASE || id == UC_SAVE) {
    uint32_t a = id == UC_LOAD ? UM_LOAD : id == UC_ERASE ? UM_ERASE : UM_SAVE;
    str_cpy(s, ui.action == a ? "OCT+" : "--", 32);
    return;
  }
  if (id >= 237) {
    str_cpy(s, "--", 32);
    return;
  }
  int32_t v = word(&edit_patch, id);
  if (id >= 156 && id < 226 && (id - 156) % 14 == 0) {
    str_cpy(s, ui_fx_disabled((id - 156) / 14) ? "ON" : "OFF", 32);
    return;
  }
  if (id == P_Lfo1BPMSync || id == P_Lfo2BPMSync) {
    str_cpy(s, v || word(&edit_patch, id + 1) ? "ON" : "OFF", 32);
    return;
  }
  if (panel_params[id].choice) {
    format_value(s, id);
    if (compact) {
      if (id >= P_Mod1Source1 && id <= P_Mod8Depth &&
          (id - P_Mod1Source1) % 4 < 2) {
        uint32_t code = (uint32_t)v & 127u;
        int bi = v & 128;
        if (code == 0)
          str_cpy(s, "NONE", 32);
        else if (code == 1)
          str_cpy(s, bi ? "BR+/-" : "BR", 32);
        else if (code == 2)
          str_cpy(s, bi ? "VEL+-" : "VEL", 32);
        else if (code == 3)
          str_cpy(s, bi ? "KEY+-" : "KEY", 32);
        else if (code == 4)
          str_cpy(s, "BEND", 32);
        else if (code >= 8 && code <= 11) {
          str_cpy(s, "EG", 32);
          char b[16];
          ui_int(b, (int32_t)code - 7);
          ui_append(s, b, 32);
          if (bi)
            ui_append(s, "+-", 32);
        } else if (code == 12 || code == 13) {
          str_cpy(s, "LFO", 32);
          char b[16];
          ui_int(b, (int32_t)code - 11);
          ui_append(s, b, 32);
          if (bi)
            ui_append(s, "+-", 32);
        } else if (code >= 14 && code < 26) {
          str_cpy(s, "", 32);
          char b[16];
          ui_int(b, (int32_t)((code - 14) % 4) + 1);
          ui_append(s, b, 32);
          ui_append(s, code < 18 ? "SAW" : code < 22 ? "TRI" : "PWM", 32);
          if (bi)
            ui_append(s, "+", 32);
        } else if (code == 26)
          str_cpy(s, bi ? "NOIS+-" : "NOISE", 32);
      }
      if (id >= P_Mod1Destination && id <= P_Mod8Depth &&
          (id - P_Mod1Source1) % 4 == 2) {
        if (v >= 162 && v <= 169) {
          char b[16];
          ui_int(b, (v - 162) % 4 + 1);
          str_cpy(s, b, 32);
          ui_append(s, v < 166 ? "FM" : "PIT", 32);
        } else if (v >= 0 && v < 48) {
          char b[16];
          ui_int(b, v / 12 + 1);
          str_cpy(s, b, 32);
          const char *field = v % 12 == 3 ? "SAW" : v % 12 == 4 ? "WAV"
                             : v % 12 == 5 ? "PWM" : v % 12 == 6 ? "PW"
                             : v % 12 == 7 ? "NS" : "LF";
          ui_append(s, field, 32);
        } else {
          static const uint16_t dest[] = {49,50,55,56,126,127,128,129,132,
                                           156,157,158,159,160,161,149,150,151};
          static const char *const name[] = {"F1CT","F1RS","F2CT","F2RS",
              "F3CT","F3RS","F3MX","A1LV","A2LV","EG1R","EG2R",
              "EG3R","EG4R","LF1R","LF2R","EXFR","EXMX","ENMX"};
          for (uint32_t i = 0; i < sizeof dest / sizeof *dest; i++)
            if (v == dest[i])
              str_cpy(s, name[i], 32);
        }
      }
      if (id == P_FilterConnection)
        str_cpy(s, v == 1 ? "SER" : v == 2 ? "PAR1" : "PAR2", 32);
      if (id == P_Amp1BreathSubMod || id == P_Amp2BreathSubMod ||
          id == P_Filter1BreathSubMod || id == P_Filter2BreathSubMod) {
        if (!v)
          str_cpy(s, "BR", 32);
        else {
          str_cpy(s, "BRxE", 32);
          char b[16];
          ui_int(b, v);
          ui_append(s, b, 32);
        }
      }
    }
    return;
  }
  if (id >= 228) {
    ui_int(s, v);
    return;
  }
  if (id == 226) {
    ui_fixed(s, v, 1);
    return;
  }
  if (id >= 156) {
    uint32_t field = (id - 156) % 14;
    if (field == IFW_FX_Time) {
      ui_int(s, (int32_t)(((int64_t)v * 1000 + 32768) / 65536));
      if (!compact)
        ui_append(s, " ms", 32);
      return;
    }
    if (field == IFW_FX_Lowpass || field == IFW_FX_Highpass) {
      if (v >= 1000 * 65536) {
        ui_fixed(s, v / 1000, 1);
        ui_append(s, "k", 32);
      } else
        ui_fixed(s, v, 1);
      return;
    }
  }
  if (id < 48 && (id % 12 == 1)) {
    ui_int(s, v / 65536);
    return;
  }
  if (id == P_Transpose || id == P_Filter3Stage) {
    ui_int(s, v / 65536);
    return;
  }
  ui_fixed(s, v, compact ? 1 : 2);
}
static uint32_t ui_gauge(uint32_t id) {
  if (id == UC_MODSLOT)
    return ui.mod_slot * 100 / 7;
  if (id == UC_PRESET)
    return preset * 100 / (PRESET_COUNT - 1);
  if (id == UC_SLOT)
    return ui.slot * 50;
  if (id == UC_FAV)
    return (ui_prefs.favorites & (1u << preset)) ? 100 : 0;
  if (id == UI_NONE || id >= 233)
    return 0;
  const IFWPanelParam *p = &panel_params[id];
  int32_t v = word(&edit_patch, id);
  if (p->choice) {
    uint32_t n = option_count[p->choice];
    for (uint32_t i = 0; i < n; i++)
      if (options[p->choice][i].value == v)
        return n > 1 ? i * 100 / (n - 1) : 0;
    return 0;
  }
  if (p->hi <= p->lo)
    return 0;
  return (uint32_t)ifw_clamp(
      ifw_sdiv64(((int64_t)v - p->lo) * 100, (uint32_t)(p->hi - p->lo)), 0,
      100);
}
static const char *ui_card_label(uint32_t k) {
  if (ui.layer) {
    static const char *const a[6][4] = {
        {"PHA", "CHO", "DLY1", "REV"},       {"AMP1", "AMP2", "LEVEL", "SMTH"},
        {"TRANS", "CENTS", "A4 HZ", "BEND"}, {"SAW", "WAVE", "PULSE", "WIDTH"},
        {"SRC1", "SRC2", "DEST", "DEPTH"},   {"LEARN", "", "", ""}};
    return a[ui.layer - 1][k];
  }
  if (ui_pages[page].kind == UK_ADVANCED)
    return panel_params[control_id(k)].name;
  return ui_pages[page].label[k];
}
static void ui_header(void) {
  ui_window(0, 0, 240, 24, 1);
  if (ui.notice_until) {
    ui_fit(9, 3, 224, &UI_S, notice, uc.accent);
    return;
  }
  if (ui.menu) {
    cv_text(9, 2, &UI_M, "MENU", uc.text);
    ui_right(232, 4, &UI_S, "HOME CLOSE", uc.mid);
    return;
  }
  if (ui.naming) {
    cv_text(9, 2, &UI_M, "NAME USER SOUND", uc.theme);
    return;
  }
  if (ui.layer) {
    static const char *const n[] = {"FX SHORTCUTS", "MIX LAYER",
                                    "TUNING LAYER", "EDIT SHORTCUTS",
                                    "MATRIX SLOT",  "MIDI LEARN"};
    cv_text(8, 3, &UI_S, n[ui.layer - 1], uc.theme);
    return;
  }
  if (meter.held) {
    cv_line(9, 7, 15, 12, uc.theme);
    cv_line(15, 12, 9, 17, uc.theme);
    cv_line(9, 7, 9, 17, uc.theme);
  } else
    ui_frame(9, 8, 8, 8, uc.dim);
  cv_text(25, 3, &UI_S, meter.held > 1 ? "LEGATO" : "MONO", uc.mid);
  if (ui_prefs.names) {
    static const char *const n[] = {"C",  "C#", "D",  "D#", "E",  "F",
                                    "F#", "G",  "G#", "A",  "A#", "B"};
    char s[16], b[16];
    uint32_t note = (uint32_t)ifw_clamp(meter.note, 0, 127);
    str_cpy(s, n[note % 12], 16);
    fmt_int(b, (int32_t)(note / 12) - 1);
    ui_append(s, b, 16);
    ui_center(95, 2, 48, &UI_M, s, meter.held ? uc.theme : uc.dim);
  }
  char s[16];
  str_cpy(s, "P", 16);
  char b[16];
  ui_int(b, preset < FACTORY_COUNT
                ? preset_programs[preset]
                : (int32_t)preset - (int32_t)FACTORY_COUNT + 1);
  ui_append(s, b, 16);
  ui_right(210, 3, &UI_S, s, uc.theme);
  if (patch_dirty)
    cv_text(219, 3, &UI_S, "*", uc.accent);
}
static void ui_cards(void) {
  for (uint32_t k = 0; k < 4; k++) {
    int x = 2 + (int)k * 60;
    uint32_t id = control_id(k);
    int hot = ui.hot_until && ui.hot == k;
    char value[32];
    ui_window(x, 28, 56, 44, 3);
    ui_fit(x + 5, 30, 46, &UI_S, ui_card_label(k), hot ? uc.accent : uc.mid);
    ui_value(id, value, 1);
    ui_fit(x + 5, 44, 46, &UI_M, value,
           id == UI_NONE ? uc.dim
           : hot         ? uc.accent
                         : uc.theme);
    cv_rect(x + 5, 66, 46, 3, uc.line);
    cv_rect(x + 5, 66, (int32_t)ui_gauge(id) * 46 / 100, 3,
            hot ? uc.accent : uc.theme);
  }
}
static void ui_scope(void) {
  int32_t top=-cv_oy,bottom=top+(int32_t)cv_h;
  if(top<184 && bottom>96) {
  cv_line(25, 130, 215, 130, uc.line);
  int32_t peak = 1024;
  for (uint32_t i = 0; i < 96; i++) {
    int32_t n = scope[i] < 0 ? -scope[i] : scope[i];
    if (n > peak)
      peak = n;
  }
  if (ui_prefs.scope == 0) {
    for (int i = 1; i < 96; i++)
      cv_line(25 + (i - 1) * 190 / 95, 130 - scope[i - 1] * 30 / peak,
              25 + i * 190 / 95, 130 - scope[i] * 30 / peak, uc.theme);
  }
  if (ui_prefs.scope == 1) {
    cv_rect(25, 124, 190, 17, uc.line);
    cv_rect(25, 124, (peak * 190) / 32768, 17, uc.theme);
    ui_center(25, 145, 190, &UI_S, "OUTPUT LEVEL", uc.mid);
  }
  }
  for (int i = 0; i < 4; i++) {
    int w = (meter.env[i] >> 24) * 42 / 127;
    cv_rect(21 + i * 51, 187, 42, 3, uc.line);
    cv_rect(21 + i * 51, 187, w, 3, uc.theme);
  }
  ui_center(8, 169, 224, &UI_S, "EG1     EG2     EG3     EG4", uc.dim);
}
static void ui_detail(void) {
  uint32_t id = control_id(ui.hot);
  if (id >= 237)
    return;
  char b[32];
  ui_value(id, b, 0);
  ui_fit(12, 82, 216, &UI_S, panel_params[id].name, uc.mid);
  ui_fit(12, 101, 216, &UI_M, b, uc.theme);
}
static void ui_env_graph(void) {
  uint32_t eg = (page >= UP_EG1 && page <= UP_EG4) ? page - UP_EG1 : 0;
  uint32_t base = P_Eg1Attack + eg * 5;
  int a = 30 + word(&edit_patch, base) * 38 / 655360,
      d = a + 24 + word(&edit_patch, base + 1) * 30 / 655360,
      r = 190 - word(&edit_patch, base + 3) * 24 / 655360;
  if (r < d + 12)
    r = d + 12;
  int sustain = 165 - word(&edit_patch, base + 2) * 56 / 655360;
  cv_line(22, 166, a, 107, uc.theme);
  cv_line(a, 107, d, sustain, uc.theme);
  cv_line(d, sustain, r, sustain, uc.theme);
  cv_line(r, sustain, 217, 166, uc.theme);
  cv_line(22, 167, 218, 167, uc.line);
  cv_rect(23, 177, 194, 3, uc.line);
  cv_rect(23, 177, (meter.env[eg] >> 24) * 194 / 127, 3, uc.accent);
  ui_center(8, 84, 224, &UI_S, ui_pages[page].title, uc.mid);
  ui_center(8, 186, 224, &UI_S,
            word(&edit_patch, base + 4) ? "RETRIGGER ON"
                                        : "RETRIGGER OFF / LEGATO",
            uc.mid);
}
static void ui_mod_graph(void) {
  char value[32], n[16];
  uint32_t base = P_Mod1Source1 + ui.mod_slot * 4;
  str_cpy(value, "SLOT ", 32);
  ui_int(n, ui.mod_slot + 1);
  ui_append(value, n, 32);
  ui_append(value, " / SOURCE1 x SOURCE2", 32);
  ui_fit(12, 82, 216, &UI_S, value, uc.mid);
  ui_value(base, value, 0);
  ui_fit(14, 103, 212, &UI_S, value, uc.theme);
  cv_text(14, 121, &UI_S, "x", uc.dim);
  ui_value(base + 1, value, 0);
  ui_fit(34, 121, 190, &UI_S, value, uc.theme);
  cv_line(20, 142, 219, 142, uc.edge);
  ui_value(base + 2, value, 0);
  ui_fit(14, 149, 212, &UI_S, value, uc.text);
  str_cpy(value, "DEPTH ", 32);
  char v[32];
  ui_value(base + 3, v, 0);
  ui_append(value, v, 32);
  ui_fit(14, 175, 212, &UI_M, value, uc.accent);
}
static int ui_effect_index(void) {
  if (page == UP_PHASER || page == UP_PHASER2)
    return 0;
  if (page == UP_CHORUS)
    return 1;
  if (page == UP_DELAY1 || page == UP_DELAY1TONE)
    return 2;
  if (page == UP_DELAY2 || page == UP_DELAY2TONE)
    return 3;
  if (page == UP_REVERB || page == UP_WIDTH)
    return 4;
  return -1;
}
static void ui_fx_graph(void) {
  static const char *const n[] = {"PHA", "CHO", "D1", "D2", "REV"};
  for (uint32_t i = 0; i < 5; i++) {
    int x = 11 + (int)i * 44;
    uint16_t c = ui_fx_disabled(i) ? uc.dim : uc.theme;
    ui_frame(x, 91, 38, 24, c);
    ui_center(x, 96, 38, &UI_S, n[i], c);
    if (i < 4)
      cv_line(x + 38, 103, x + 43, 103, uc.edge);
  }
  int fx = ui_effect_index();
  if (fx >= 0) {
    ui_center(10, 125, 220, &UI_M,
              ui_fx_disabled((uint32_t)fx) ? "BYPASSED" : "EFFECT ON",
              ui_fx_disabled((uint32_t)fx) ? uc.dim : uc.theme);
  }
  uint32_t id = control_id(ui.hot);
  if (id < 237) {
    char v[32];
    ui_fit(13, 154, 214, &UI_S, panel_params[id].name, uc.mid);
    ui_value(id, v, 0);
    ui_fit(13, 174, 214, &UI_M, v, uc.theme);
  }
  if (page == UP_FX)
    ui_center(10, 127, 220, &UI_S, "HOLD FX: BYPASS SHORTCUTS", uc.mid);
}
static void ui_preset_list(void) {
  uint32_t list[PRESET_COUNT], n = 0, selected = 0;
  for (uint32_t i = 0; i < PRESET_COUNT; i++)
    if (ui_visible_preset(i)) {
      if (i == preset)
        selected = n;
      list[n++] = i;
    }
  if (!n) {
    ui_center(8, 119, 224, &UI_M, "NO FAVORITES", uc.mid);
    ui_center(8, 146, 224, &UI_S, "KNOB 4: CHANGE LIST", uc.dim);
    return;
  }
  uint32_t start = selected > 2 ? selected - 2 : 0;
  if (start + 6 > n)
    start = n > 6 ? n - 6 : 0;
  for (uint32_t row = 0; row < 6 && start + row < n; row++) {
    uint32_t p = list[start + row];
    int y = 83 + (int)row * 19;
    int chosen = p == preset;
    if (chosen)
      ui_cursor(10, y + 3);
    ui_fit(29, y, 172, &UI_S, ui_preset_name(p), chosen ? uc.accent : uc.text);
    char b[12];
    ui_int(b, p < FACTORY_COUNT ? preset_programs[p]
                                : (int32_t)p - (int32_t)FACTORY_COUNT + 1);
    ui_right(229, y, &UI_S, b, chosen ? uc.theme : uc.dim);
  }
}
static void ui_save_list(void) {
  for (uint32_t i = 0; i < 3; i++) {
    int y = 89 + (int)i * 31;
    if (i == ui.slot)
      ui_cursor(10, y + 4);
    char b[16];
    ui_int(b, i + 1);
    cv_text(29, y, &UI_S, b, uc.dim);
    ui_fit(47, y, 178, &UI_M,
           ui_prefs.used & (1u << i) ? ui_prefs.user_name[i] : "EMPTY",
           i == ui.slot ? uc.theme : uc.mid);
  }
  ui_center(8, 184, 224, &UI_S, "TURN ACTION / OCT+ CONFIRM", uc.mid);
}
static const char *ui_layer_name(void) {
  static const char *const n[] = {"FX",   "MIX",    "TUNE",
                                  "EDIT", "MATRIX", "LEARN"};
  return n[(ui.layer ? ui.layer : 1) - 1];
}
static void ui_layer_list(void) {
  static const char *const key[] = {"F", "G", "A", "B", "C", "D", "E", "F4"};
  static const char *const fx[] = {"PHASER", "CHORUS",  "DELAY 1",  "DELAY 2",
                                   "REVERB", "EXCITER", "ENHANCER", "EQ"};
  static const char *const edit[] = {"OSC 1",    "OSC 2",    "OSC 3",
                                     "OSC 4",    "FILTER 1", "FILTER 2",
                                     "FILTER 3", "VOICE"};
  for (uint32_t i = 0; i < 8; i++) {
    int x = 13 + (int)(i / 4) * 111, y = 86 + (int)(i % 4) * 27;
    cv_text(x, y, &UI_S, key[i], uc.dim);
    char b[12];
    const char *s = "";
    if (ui.layer == 1)
      s = fx[i];
    else if (ui.layer == 4)
      s = edit[i];
    else if (ui.layer == 5) {
      str_cpy(b, "SLOT ", 12);
      char c[16];
      ui_int(c, i + 1);
      ui_append(b, c, 12);
      s = b;
    } else if (ui.layer == 6) {
      static const char *const learn[] = {"ARM", "CANCEL", "CLEAR ALL", "",
                                          "",    "",       "",          ""};
      s = learn[i];
    }
    ui_fit(x + 20, y, 88, &UI_S, s,
           ui.layer == 5 && i == ui.mod_slot ? uc.accent : uc.text);
  }
  if (ui.layer == 2 || ui.layer == 3)
    ui_center(10, 126, 220, &UI_S, "FOUR KNOBS / LIVE CONTROLS", uc.theme);
}
static const char *ui_menu_label(uint32_t t, uint32_t row) {
  static const char *const names[5][5] = {
      {"COLOR", "STYLE", "REFRESH", "SCOPE", "NOTE NAMES"},
      {"HOLD", "KNOB ACCEL", "", "", ""},
      {"TUNING", "MASTER", "AMP 1", "AMP 2", ""},
      {"CHANNEL", "WIND CC7", "LEARN", "CLEAR LEARN", ""},
      {"SAVE SETUP", "ALL PARAMS", "VERSION", "PERFORMANCE", "PANIC"}};
  return names[t % 5][row % 5];
}
static void ui_menu_text(uint32_t t, uint32_t r, char *s) {
  s[0] = 0;
  int32_t v = ui_menu_value(t, r);
  if (t == 0) {
    if (r == 0) {
      static const char *const a[] = {"BLUE", "GREEN", "AMBER", "MONO"};
      str_cpy(s, a[v % 4], 32);
    } else if (r == 1)
      str_cpy(s, v ? "OUTLINE" : "FILLED", 32);
    else if (r == 2)
      str_cpy(s, v ? "NORMAL" : "LOW", 32);
    else if (r == 3) {
      static const char *const a[] = {"WAVE", "LEVEL", "OFF"};
      str_cpy(s, a[v % 3], 32);
    } else
      str_cpy(s, v ? "ON" : "OFF", 32);
    return;
  }
  if (t == 1) {
    if (r == 0) {
      ui_int(s, 200 + v * 100);
      ui_append(s, " ms", 32);
    } else
      str_cpy(s, v ? "ON" : "OFF", 32);
    return;
  }
  if (t == 2) {
    ui_fixed(s, v, 1);
    return;
  }
  if (t == 3) {
    if (r == 0) {
      if (v)
        ui_int(s, v);
      else
        str_cpy(s, "ALL", 32);
    } else if (r < 3)
      str_cpy(s, v ? "ON" : "OFF", 32);
    else
      str_cpy(s, "OCT+", 32);
    return;
  }
  static const char *const a[] = {"OCT+", "OPEN", "0.4.7-rc7", "OPEN", "OCT+"};
  str_cpy(s, a[r % 5], 32);
}
static void ui_draw_menu(void) {
  static const char *const tab[] = {"DISP", "CTRL", "AUDIO", "MIDI", "SYS"};
  ui_window(0, 27, 240, 180, 3);
  for (uint32_t i = 0; i < 5; i++) {
    int x = 5 + (int)i * 46;
    if (i == ui.menu_tab) {
      cv_rect(x, 32, 44, 20, uc.accent);
      ui_center(x, 34, 44, &UI_S, tab[i], uc.bg);
    } else
      ui_center(x, 34, 44, &UI_S, tab[i], uc.dim);
  }
  for (uint32_t i = 0; i < ui_menu_count(ui.menu_tab); i++) {
    int y = 65 + (int)i * 27;
    char b[32];
    int hot = i == ui.menu_row;
    if (hot)
      ui_cursor(10, y + 4);
    ui_fit(29, y, 119, &UI_S, ui_menu_label(ui.menu_tab, i),
           hot ? uc.accent : uc.text);
    ui_menu_text(ui.menu_tab, i, b);
    ui_right(230, y, &UI_S, b, hot ? uc.theme : uc.mid);
    cv_rect(29, y + 21, 201, 1, uc.line);
  }
  ui_window(0, 211, 240, 29, 1);
  ui_center(3, 212, 234, &UI_S, "ALGO TAB  PRESET ROW", uc.mid);
  ui_center(3, 224, 234, &UI_S, "KNOB VALUE / HOME CLOSE", uc.mid);
}
static void ui_footer(void) {
  ui_window(0, 208, 240, 32, 1);
  char text[32];
  const char *title = ui.layer ? ui_layer_name() : ui_pages[page].title;
  uint32_t g = ui_group(), n = 0, index = 0;
  for (uint32_t p = 0; p < UP_COUNT; p++)
    if (ui_pages[p].group == g) {
      if (p == page)
        index = n;
      n++;
    }
  if (ui.layer)
    ui_fit(8, 209, 223, &UI_S,
           ui.layer_locked ? "LOCKED / TAP BUTTON TO CLOSE"
                           : "HOLD / RELEASE TO CLOSE",
           uc.mid);
  else {
    str_cpy(text, title, 32);
    if (n > 1) {
      char b[16];
      ui_append(text, " ", 32);
      ui_int(b, index + 1);
      ui_append(text, b, 32);
      ui_append(text, "/", 32);
      ui_int(b, n);
      ui_append(text, b, 32);
    }
    ui_fit(8, 209, 224, &UI_S, text, uc.theme);
  }
  ui_fit(8, 224, 224, &UI_S, ui_preset_name(preset), uc.text);
}
static void ui_dialog(void) {
  ui_window(10, 82, 220, 111, 3);
  const char *s = ui.confirm == UM_SETTINGS ? "SAVE SETTINGS?"
                  : ui.confirm == UM_SAVE    ? "OVERWRITE USER SOUND?"
                  : ui.confirm == UM_LOAD  ? "REPLACE EDITED SOUND?"
                  : ui.confirm == UM_ERASE ? "ERASE USER SOUND?"
                                           : "CLEAR MIDI LEARN?";
  ui_center(14, 94, 212, &UI_S, s, uc.accent);
  ui_center(14, 119, 212, &UI_S,
            ui.confirm == UM_LOAD ? "HOLD SAVE CAN UNDO LOAD"
                                  : "OCT- CANCEL / OCT+ CONFIRM", uc.mid);
  ui_frame(20, 151, 93, 27, uc.edge);
  ui_frame(126, 151, 94, 27, uc.theme);
  ui_center(20, 156, 93, &UI_S, "OCT- NO", uc.mid);
  ui_center(126, 156, 94, &UI_S, "OCT+ YES", uc.theme);
}
static void ui_name_screen(void) {
  ui_window(0, 28, 240, 176, 3);
  ui_center(8, 40, 224, &UI_S, "KEYS TYPE A-Z / G5 SPACE", uc.mid);
  ui_fit(15, 69, 212, &UI_L, ui.name, uc.theme);
  int x = 15 + (int)ui.name_pos * 14;
  cv_rect(x, 97, 12, 2, uc.accent);
  ui_center(8, 118, 224, &UI_S, "SELECT / PRESETS: LETTER", uc.mid);
  ui_center(8, 138, 224, &UI_S, "ALGORITHM: CURSOR", uc.mid);
  ui_center(8, 167, 224, &UI_S, "OCT- ERASE / OCT+ SAVE", uc.theme);
  ui_footer();
}
static void ui_stage(void) {
  ui_window(0, 76, 240, 128, 3);
  if (ui.layer) {
    ui_layer_list();
    return;
  }
  uint32_t kind = ui_pages[page].kind;
  if (kind == UK_PERF) {
    ui_center(7,84,226,&UI_S,"DSP BLOCK BUDGET: 5805 us",uc.theme);
#if defined(IFW_HOST) || defined(IFW_EMU)
    ui_center(7,115,226,&UI_S,"TIMING: DEVICE ONLY",uc.mid);
    ui_center(7,140,226,&UI_S,"SINE / MUTE TEST AVAILABLE",uc.mid);
    ui_center(7,170,226,&UI_S,"HOST CPU IS NOT FM-1 CPU",uc.dim);
#else
    char a[32],b[32];
    str_cpy(a,"DSP ",32);ui_int(b,(int32_t)(perf_view.dsp_ticks/24u));ui_append(a,b,32);
    ui_append(a," MAX ",32);ui_int(b,(int32_t)(perf_view.dsp_max_ticks/24u));ui_append(a,b,32);
    ui_center(7,104,226,&UI_S,a,uc.text);
    str_cpy(a,"IRQ ",32);ui_int(b,(int32_t)(perf_view.last_ticks/24u));ui_append(a,b,32);
    ui_append(a," MAX ",32);ui_int(b,(int32_t)(perf_view.max_ticks/24u));ui_append(a,b,32);
    ui_center(7,124,226,&UI_S,a,uc.text);
    str_cpy(a,"XRUN ",32);ui_int(b,(int32_t)perf_view.overruns);ui_append(a,b,32);
    ui_append(a,"  DMA ",32);ui_int(b,(int32_t)perf_view.dma_late);ui_append(a,b,32);
    ui_center(7,144,226,&UI_S,a,(perf_view.overruns||perf_view.dma_late)?uc.accent:uc.theme);
    str_cpy(a,"PEAK ",32);ui_int(b,(int32_t)perf_view.peak);ui_append(a,b,32);
    ui_append(a," CLIP ",32);ui_int(b,(int32_t)perf_view.clips);ui_append(a,b,32);
    ui_center(7,164,226,&UI_S,a,uc.mid);
    str_cpy(a,"MAIN ",32);ui_int(b,(int32_t)ui_poll_max_gap);ui_append(a,b,32);
    ui_append(a,"ms UND ",32);ui_int(b,(int32_t)audio_underruns);ui_append(a,b,32);
    ui_center(7,184,226,&UI_S,a,uc.mid);
#endif
    return;
  }
  if (kind == UK_PRESETS) {
    ui_preset_list();
    return;
  }
  if (kind == UK_SAVE) {
    ui_save_list();
    return;
  }
  if (kind == UK_ENV) {
    ui_env_graph();
    return;
  }
  if (kind == UK_MOD) {
    ui_mod_graph();
    return;
  }
  if (ui_group() == UG_FX) {
    ui_fx_graph();
    return;
  }
  if (kind == UK_LEARN) {
    ui_center(9, 99, 222, &UI_M, learn_armed ? "LEARNING CC" : "MIDI LEARN",
              learn_armed ? uc.accent : uc.theme);
    ui_center(9, 130, 222, &UI_S, "TURN PARAM / TAP REC", uc.mid);
    ui_center(9, 150, 222, &UI_S, "THEN MOVE CONTROLLER", uc.mid);
    return;
  }
  if (page == UP_HOME) {
    ui_scope();
    return;
  }
  if (kind == UK_MIX) {
    for (int i = 0; i < 4; i++) {
      uint32_t id = control_id((uint32_t)i);
      cv_rect(26 + i * 51, 96, 20, 77, uc.line);
      int n = (int)ui_gauge(id) * 73 / 100;
      cv_rect(29 + i * 51, 172 - n, 14, n, uc.theme);
    }
    ui_center(8, 182, 224, &UI_S, "ONE VOICE / TWO AMP BRANCHES", uc.mid);
    return;
  }
  ui_detail();
  if (kind == UK_VOICE) {
    ui_center(8, 144, 224, &UI_S, "MONO / LAST NOTE PRIORITY", uc.theme);
    ui_center(8, 166, 224, &UI_S, "RETRIGGER IS SET PER EG", uc.mid);
  } else if (kind == UK_ROUTE) {
    ui_center(8, 146, 224, &UI_S, "OSC -> FILTER -> AMP", uc.theme);
    ui_center(8, 168, 224, &UI_S, "BREATH x EG / ORIGINAL ROUTING", uc.mid);
  } else if (kind == UK_LFO) {
    int id = page == UP_LFO2 || page == UP_LFO2SYNC ? 1 : 0;
    int type = word(&edit_patch, (uint32_t)P_Lfo1Waveform + (uint32_t)id * 6);
    for (int i = 1; i < 180; i++) {
      int32_t a = ifw_wave20((uint32_t)(i - 1) * 23860929u, 0, type,
                             IFW_PHASE_HALF),
              b = ifw_wave20((uint32_t)i * 23860929u, 0, type, IFW_PHASE_HALF);
      cv_line(29 + i - 1, 159 - (a >> 16), 29 + i, 159 - (b >> 16), uc.theme);
    }
  } else {
    ui_center(8, 151, 224, &UI_S,
              ui_pages[page].help[0] ? ui_pages[page].help
                                     : "SELECT PAGE / ALGO MODULE",
              uc.mid);
    ui_center(8, 174, 224, &UI_S, "FOUR KNOBS / FOUR CONTROLS", uc.dim);
  }
}
static void ui_paint(void) {
  int32_t top=-cv_oy,bottom=top+(int32_t)cv_h;
  if(top<24)ui_header();
  if (ui.menu) {
    ui_draw_menu();
    if (ui.confirm)
      ui_dialog();
    return;
  }
  if (ui.naming) {
    ui_name_screen();
    return;
  }
  if(top<76 && bottom>24)ui_cards();
  if(top<204 && bottom>76)ui_stage();
  if(bottom>204)ui_footer();
  if (ui.confirm)
    ui_dialog();
}
/* Cache the displayed model, not a hash of every pixel in every scanline.
 * A changed model invalidates its widget. DMA owns the canvas until complete. */
static int ui_same_text(const char *a,const char *b) {
  while(*a && *a==*b){a++;b++;}return *a==*b;
}
static void draw_panel(uint32_t full) {
  static uint32_t initialized,perf_draw_at,model_at,pending_full;
  static uint32_t old_layout[16],old_header[5],old_hot,old_learn;
  static uint32_t old_gauge[4],old_id[4];
  static char old_value[4][32],old_name[13],old_footer[32],old_notice[64];
  static IFWFixedPatch old_patch;
  static IFWUiPrefs old_prefs;
  static int32_t old_env[4];
  static int16_t old_scope[96];
  pending_full|=full;
  if(pending_full)ui_drawing=1;
#if defined(IFW_TARGET) || defined(IFW_DISPLAY_SCHEDULER_TEST)
  /* A pending input invalidation survives a busy DMA. Do no formatting or
   * patch comparisons until the canvas can actually be used. */
  if(lcd_busy && !ifw_display_ready())return;
#endif
  full=pending_full;pending_full=0;
  if(!initialized || full || ui_force_redraw || fm1_ms-model_at>=20u) {
  model_at=fm1_ms;
  uint32_t layout[16]={page,preset,ui.menu,ui.layer,ui.naming,ui.confirm,
      ui.mod_slot,ui.menu_tab,ui.menu_row,ui.slot,ui.action,ui.layer_locked,ui.name_pos,saved.wind,saved.channel,saved.octave};
  uint32_t header[5]={meter.held,(uint32_t)meter.note,patch_dirty,
      !!ui.notice_until,ui_prefs.names};
  uint32_t hot=ui.hot+(ui.hot_until?16u:0u);
  if(!initialized || ui_force_redraw || memcmp(layout,old_layout,sizeof layout) ||
     ui_prefs.palette!=old_prefs.palette || ui_prefs.style!=old_prefs.style) {
    ui_widgets=15u;ui_widget_cursor=0;
    memcpy(old_layout,layout,sizeof layout);ui_force_redraw=0;
  }
  if(memcmp(header,old_header,sizeof header) || !ui_same_text(notice,old_notice))ui_widgets|=1u;
  str_cpy(old_notice,notice,sizeof old_notice);
  memcpy(old_header,header,sizeof header);
  for(uint32_t i=0;i<4;i++) {
    uint32_t id=control_id(i),gauge=ui_gauge(id);char value[32]={0};
    ui_value(id,value,1);
    if(id!=old_id[i] || gauge!=old_gauge[i] || !ui_same_text(value,old_value[i]))ui_widgets|=2u;
    old_id[i]=id;old_gauge[i]=gauge;memcpy(old_value[i],value,sizeof value);
  }
  if(hot!=old_hot)ui_widgets|=6u;
  old_hot=hot;
  if(memcmp(&edit_patch,&old_patch,sizeof old_patch) ||
     memcmp(&ui_prefs,&old_prefs,sizeof old_prefs) || learn_armed!=old_learn ||
     memcmp(ui.name,old_name,sizeof old_name))ui_widgets|=(ui.menu || ui.naming)?14u:4u;
  old_patch=edit_patch;old_prefs=ui_prefs;old_learn=learn_armed;
  memcpy(old_name,ui.name,sizeof old_name);
  const char *footer=ui_preset_name(preset);
  if(!ui_same_text(footer,old_footer))ui_widgets|=8u;
  str_cpy(old_footer,footer,sizeof old_footer);
  if(!ui.menu && !ui.naming && !ui.confirm && !ui.layer) {
    for(uint32_t i=0;i<4;i++) {
      int32_t env=meter.env[i];
      if(old_env[i]!=env && page==UP_HOME)ui_widgets|=32u;
      else if(old_env[i]!=env && ui_pages[page].kind==UK_ENV)ui_widgets|=4u;
      old_env[i]=env;
    }
    if(page==UP_HOME) {
      if(ui_prefs.scope<2 && memcmp((const void *)scope,old_scope,sizeof old_scope))ui_widgets|=16u;
      memcpy(old_scope,(const void *)scope,sizeof old_scope);
    }
  }
  initialized=1;
  if(page==UP_PERF && fm1_ms-perf_draw_at>=200u) {
    perf_snapshot();perf_draw_at=fm1_ms;ui_widgets|=6u;
  }
  if(full && ui_widgets) {
    if(!ui_priority_widgets)ui_priority_at=fm1_ms;
    ui_priority_widgets|=ui_widgets;
  }
  } /* model sampling: at most 50 Hz unless an input changes the screen */
  /* A full stage includes both home damage rectangles. It always supersedes
   * them, including when a menu opens while animation is waiting. */
  if(ui_widgets&4u) {ui_widgets&=~48u;ui_priority_widgets&=~48u;}
  ui_drawing=ui_widgets!=0;
  if(!ui_widgets){last_draw=fm1_ms;return;}
#if defined(IFW_TARGET) || defined(IFW_DISPLAY_SCHEDULER_TEST)
  /* Complete audio blocks take priority. Explicit controls and a coalesced
   * scope retain bounded service when only one block is available. */
  uint32_t eligible=ui_widgets;
  /* A changing scope cannot monopolize the SPI bus at audio-slice rate. */
  for(unsigned i=4;i<UI_WIDGET_COUNT;i++)
    if(fm1_ms-ui_widget_draw_at[i]<(ui_prefs.animate?50u:100u))eligible&=~(1u<<i);
  if(!eligible)return;
  if(!audio_can_draw()) {
    if(fm1_ms-ui_priority_at<50u)return;
    eligible&=ui_priority_widgets;
    if(audio_queued())for(unsigned i=4;i<UI_WIDGET_COUNT;i++)
      if(fm1_ms-ui_widget_draw_at[i]>=100u)eligible|=ui_widgets&(1u<<i);
    if(!eligible)return;
  }
#else
  uint32_t eligible=ui_widgets;
#endif
  ui_palette();
  uint32_t widget=ui_widget_cursor;
  for(uint32_t i=0;i<UI_WIDGET_COUNT;i++,widget=(widget+1u)%UI_WIDGET_COUNT)if(eligible&(1u<<widget))break;
  static const uint16_t y[UI_WIDGET_COUNT]={0,24,76,204,96,187},h[UI_WIDGET_COUNT]={24,52,128,36,70,3};
#ifdef IFW_TARGET
  uint32_t draw_start=fm1_ticks();
#endif
  cv_begin(240,h[widget],uc.bg);cv_oy=-(int32_t)y[widget];
  ui_paint();cv_oy=0;
  lcd_blit(0,y[widget],240,h[widget],cv_px);
  ui_lcd_pixels+=240u*h[widget];ui_widget_updates[widget]++;
#ifdef IFW_TARGET
  uint32_t draw_ticks=fm1_ticks()-draw_start;
  if(draw_ticks>ui_widget_max_ticks[widget])ui_widget_max_ticks[widget]=draw_ticks;
#endif
  ui_widgets&=~(1u<<widget);ui_priority_widgets&=~(1u<<widget);
  ui_priority_at=fm1_ms;ui_widget_cursor=(widget+1u)%UI_WIDGET_COUNT;
  ui_widget_draw_at[widget]=fm1_ms;
  if(widget==2)ui_widget_draw_at[4]=ui_widget_draw_at[5]=fm1_ms;
  ui_drawing=ui_widgets!=0;
  if(!ui_drawing){last_draw=fm1_ms;ui_frame_count++;}
}
