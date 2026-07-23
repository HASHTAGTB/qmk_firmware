/* Copyright 2024 ~ 2025 @ Keychron (https://www.keychron.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include QMK_KEYBOARD_H
#include "keychron_common.h"
#include "keychron_debounce.h"

/* ====================================================================
 * LAYER MAP
 * ====================================================================
 *  0  COLEMAK   Colemak-DH base          physical switch: left / Mac side
 *  1  MACRO     Numpad digit overlay      TG via NumLock key
 *  2  QWERTY    QWERTY gaming base        physical switch: right / Win side
 *  3  FN        Shared FN layer           hold right FN key (either base)
 *  4  NAV       Navigation                hold Caps Lock  (tap = ESC)
 *
 * Fn row:  F1–F12 on base layer; media/brightness keys on FN layer.
 * Num row: Numbers by default; SYM_TG swaps to symbols (shift inverts).
 * Numpad:  Utilities always-on on base (AC_CLK, AC_FS, SYM_TG, media,
 *          BASE_SWAP).  MACRO layer overlays numpad digits via NumLock.
 *
 * PHYSICAL SWITCH NOTE:
 *   v5_max.c hard-codes default layer as 0 (Mac/left) or 2 (Win/right).
 *   COLEMAK sits at 0 and QWERTY at 2 to match.  MACRO sits at index 1
 *   (the former placeholder slot) — the DIP switch never touches index 1,
 *   so this is safe and eliminates the dummy layer.
 *
 *   QWERTY only defines keys that physically differ from COLEMAK; all
 *   others are transparent and fall through to COLEMAK.
 *   default_layer_state_set_user keeps layer 0 explicitly active
 *   whenever QWERTY is the default layer (DIP switch or BASE_SWAP).
 *   Note: dip_switch_update_user is already claimed by factory_test.c
 *   in keychron_common.mk and cannot be redefined in the keymap.
 *
 * SYM:
 *   Implemented as a static bool flag rather than a layer.  SYM_TG
 *   toggles it.  The number↔symbol pairs are defined once in a PROGMEM
 *   table.  No empty flag layers needed.
 * ==================================================================== */
enum layers {
    COLEMAK,  /* 0 */
    MACRO,    /* 1 — DIP switch never activates this; fills the index-1 gap */
    QWERTY,   /* 2 */
    FN,       /* 3 */
    NAV,      /* 4 */
};

/* ====================================================================
 * CUSTOM KEYCODES
 * ====================================================================
 * AC_CLK    toggle left-click autoclicker     (needs MOUSEKEY_ENABLE)
 * AC_FS     toggle F→Space alternating clicker (mutually exclusive with AC_CLK)
 * BASE_SWAP software toggle COLEMAK ↔ QWERTY  (session-only; DIP wins on boot)
 * SYM_TG    toggle symbol-row mode            (number row → symbols / vice versa)
 * ==================================================================== */
enum custom_keycodes {
    AC_CLK = SAFE_RANGE,
    AC_FS,
    BASE_SWAP,
    SYM_TG,
};

/* Autoclicker timing (ms).
 * AC_CLK uses explicit register/unregister alternation — each half-state
 * lasts AC_CLK_INTERVAL ms, so the full press+release cycle is 2×.
 * AC_FS alternates between KC_F and KC_SPC at AC_FS_INTERVAL per step. */
#define AC_CLK_INTERVAL 100
#define AC_FS_INTERVAL  250

static bool     ac_clk_on    = false;
static bool     ac_fs_on     = false;
static bool     ac_fs_state  = false;
static uint32_t ac_timer     = 0;

static bool sym_on = false;

/* ====================================================================
 * TRANSFORM TABLE
 * ====================================================================
 * SYM (number row):  plain = symbol  |  shift + key = number
 * Each row is {base_keycode, symbol_keycode}.
 * ==================================================================== */
static const uint16_t sym_pairs[][2] PROGMEM = {
    {KC_GRV,  KC_TILD}, {KC_1, KC_EXLM}, {KC_2, KC_AT},   {KC_3, KC_HASH},
    {KC_4,    KC_DLR},  {KC_5, KC_PERC}, {KC_6, KC_CIRC}, {KC_7, KC_AMPR},
    {KC_8,    KC_ASTR}, {KC_9, KC_LPRN}, {KC_0, KC_RPRN}, {KC_MINS, KC_UNDS},
    {KC_EQL,  KC_PLUS},
};

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    /* SYM: intercept number row keycodes from the base layer.
     * Shift held → send the number (strip shift so host sees unshifted). */
    if (sym_on) {
        for (uint8_t i = 0; i < ARRAY_SIZE(sym_pairs); i++) {
            if (keycode != pgm_read_word(&sym_pairs[i][0])) continue;
            if (record->event.pressed) {
                bool shifted = get_mods() & MOD_MASK_SHIFT;
                if (shifted) del_mods(MOD_MASK_SHIFT);
                tap_code16(shifted ? pgm_read_word(&sym_pairs[i][0])
                                   : pgm_read_word(&sym_pairs[i][1]));
                if (shifted) add_mods(MOD_MASK_SHIFT);
            }
            return false;
        }
    }

    if (record->event.pressed) {
        switch (keycode) {
            case AC_CLK:
                ac_clk_on = !ac_clk_on;
                ac_fs_on  = false;
                ac_timer  = timer_read32();
                return false;
            case AC_FS:
                ac_fs_on    = !ac_fs_on;
                ac_clk_on   = false;
                ac_timer    = timer_read32();
                return false;
            case BASE_SWAP:
                if (get_highest_layer(default_layer_state) == COLEMAK) {
                    default_layer_set(1UL << QWERTY);
                    layer_on(COLEMAK);
                } else {
                    default_layer_set(1UL << COLEMAK);
                    layer_off(COLEMAK);
                }
                return false;
            case SYM_TG:
                sym_on = !sym_on;
                return false;
            default:
                break;
        }
    }
    return true;
}

void matrix_scan_user(void) {
    if (ac_clk_on) {
        if (timer_elapsed32(ac_timer) >= AC_CLK_INTERVAL) {
            ac_timer = timer_read32();
            tap_code(MS_BTN1);
        }
    } else if (ac_fs_on) {
        if (timer_elapsed32(ac_timer) >= AC_FS_INTERVAL) {
            ac_timer    = timer_read32();
            tap_code(ac_fs_state ? KC_SPC : KC_F);
            ac_fs_state = !ac_fs_state;
        }
    }
}

/* Keep COLEMAK (layer 0) explicitly active whenever QWERTY is the
 * default layer so that transparent QWERTY keys fall through to COLEMAK.
 * Using default_layer_state_set_user instead of dip_switch_update_user
 * because factory_test.c already owns dip_switch_update_user and there
 * is no further hook below it.  This callback fires for both the DIP
 * switch and BASE_SWAP, so both paths are covered. */
layer_state_t default_layer_state_set_user(layer_state_t state) {
    if (state == (1UL << QWERTY)) {
        layer_on(COLEMAK);
    } else {
        layer_off(COLEMAK);
    }
    return state;
}

/* Tap = ESC  |  Hold = NAV layer */
#define CAPS_LT  LT(NAV, KC_ESC)
/* Single FN key shared by both base layers */
#define FN_KEY   MO(FN)

/* ====================================================================
 * KEYMAPS
 * ==================================================================== */
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {

    /* ------------------------------------------------------------------
     * COLEMAK — Colemak-DH implemented in hardware
     *
     * Physical QWERTY key → Colemak-DH output:
     *   Top row:  [E→F]  [R→P]  [T→B]  [Y→J]  [U→L]  [I→U]  [O→Y]  [P→;]
     *   Home row: [S→R]  [D→S]  [F→T]  [H→M]  [J→N]  [K→E]  [L→I]  [;→O]
     *   Bot row:  [ISO→Z][Z→X]  [X→C]  [C→D]  [V→NUBS][B→V] [N→K]  [M→H]
     *
     * Modifiers: KC_LALT / KC_LGUI swapped so Super (Hyprland mod key)
     * sits directly left of spacebar.
     *
     * Fn row:  F1–F12 directly (media/brightness via FN layer).
     * Num row: Numbers by default; SYM_TG switches to symbols.
     *
     * Numpad utility layout (NumLock toggles to digit mode):
     *   P7      AC_CLK    left-click autoclicker toggle
     *   P8      AC_FS     F/Space autoclicker toggle
     *   P9      SYM_TG    symbol-row mode toggle
     *   P4      BASE_SWAP software COLEMAK ↔ QWERTY toggle
     *   P1/P2/P3  Prev / Play-Pause / Next
     * ------------------------------------------------------------------ */
    [COLEMAK] = LAYOUT_iso_99(
        KC_ESC,             KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,             KC_DEL,   KC_HOME,  KC_END,   KC_MUTE,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,            TG(MACRO),KC_PSLS,  KC_PAST,  KC_PMNS,
        KC_TAB,   KC_Q,     KC_W,     KC_F,     KC_P,     KC_B,     KC_J,     KC_L,     KC_U,     KC_Y,     KC_SCLN,  KC_LBRC,  KC_RBRC,                      AC_CLK,   AC_FS,    XXXXXXX,   KC_PPLS,
        CAPS_LT,  KC_A,     KC_R,     KC_S,     KC_T,     KC_G,     KC_M,     KC_N,     KC_E,     KC_I,     KC_O,     KC_QUOT,  KC_NUHS,  KC_ENT,             BASE_SWAP,SYM_TG,   XXXXXXX,
        KC_LSFT,  KC_Z,     KC_X,     KC_C,     KC_D,     KC_NUBS,  KC_V,     KC_K,     KC_H,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,  KC_UP,    KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_PENT,
        KC_LCTL,  KC_LALT,  KC_LGUI,                                KC_SPC,                                 KC_RALT,  FN_KEY,   KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT,  XXXXXXX,  XXXXXXX          ),

    /* ------------------------------------------------------------------
     * MACRO — Numpad digit overlay  (TG via physical NumLock key)
     *
     * Sits at index 1 (formerly _UNUSED placeholder).  The DIP switch
     * never activates this layer, so it is safe to put real content here.
     *
     * Overrides the numpad area with standard digit keycodes.  All main
     * keyboard keys pass through to COLEMAK / QWERTY below.
     *
     *   ┌─────────────┬───────┬───────┬───────┐
     *   │ TG(MACRO)   │  /    │   *   │   -   │  ← NumLock = exit digit mode
     *   ├─────────────┼───────┼───────┼───────┤
     *   │      7      │   8   │   9   │       │
     *   ├─────────────┼───────┼───────┤   +   │
     *   │      4      │   5   │   6   │       │
     *   ├─────────────┼───────┼───────┼───────┤
     *   │      1      │   2   │   3   │       │
     *   ├─────────────┴───────┼───────┘  Ent  │
     *   │        0            │   .   │       │
     *   └─────────────────────┴───────┴───────┘
     * ------------------------------------------------------------------ */
    [MACRO] = LAYOUT_iso_99(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            TG(MACRO),KC_PSLS,  KC_PAST,  KC_PMNS,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                      KC_P7,    KC_P8,    KC_P9,    KC_PPLS,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            KC_P4,    KC_P5,    KC_P6,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  KC_P1,    KC_P2,    KC_P3,    KC_PENT,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  KC_P0,    KC_PDOT          ),

    /* ------------------------------------------------------------------
     * QWERTY — Standard QWERTY for gaming
     *
     * Only keys that physically differ from COLEMAK are defined here;
     * everything else is transparent and falls through to COLEMAK
     * (including F1–F12 fn row and the number row / SYM_TG behaviour).
     * ------------------------------------------------------------------ */
    [QWERTY] = LAYOUT_iso_99(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     _______,  _______,                      _______,  _______,  _______,  _______,
        _______,  _______,  KC_S,     KC_D,     KC_F,     _______,  KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  KC_NUBS,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),

    /* ------------------------------------------------------------------
     * FN — Shared function layer (sits above both COLEMAK and QWERTY)
     *
     * Fn row:   Media / brightness (complement to base F1–F12).
     * Num row:  Bluetooth host 1/2/3 and 2.4 GHz (P2P4G).
     * Tab row:  RGB controls.
     * Home row: RGB controls (continued).
     * B key:    Battery level indicator.
     * Encoder:  UG_TOGG (overrides KC_MUTE when FN is held).
     * ------------------------------------------------------------------ */
    [FN] = LAYOUT_iso_99(
        _______,            KC_BRID,  KC_BRIU,  KC_MCTRL, KC_LNPAD, UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,            _______,  _______,  _______,  UG_TOGG,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            KC_NUM,   _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                      _______,  _______,  _______,  _______,
        _______,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  BAT_LVL,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),

    /* ------------------------------------------------------------------
     * NAV — Navigation layer  (hold Caps Lock; tap Caps Lock = ESC)
     *
     * Num row:  Plain numbers 1–0, -, = accessible here (useful when
     *           SYM mode is active and the base row outputs symbols).
     *
     * Arrow cluster follows Colemak-DH NEIO finger positions:
     *   Physical J → ←left    Physical K → ↓down
     *   Physical L → ↑up      Physical ; → →right
     *
     * Cluster above (physical U I O P [):
     *   U → Home   I → PgUp   O → PgDn   P → End   [ → Del
     *
     * Works identically on COLEMAK and QWERTY since NAV uses physical
     * key positions, not logical letters.
     * ------------------------------------------------------------------ */
    [NAV] = LAYOUT_iso_99(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  KC_HOME,  KC_PGUP,  KC_PGDN,  KC_END,   KC_DEL,   _______,                      _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  KC_LEFT,  KC_DOWN,  KC_UP,    KC_RGHT,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),
};

/* ====================================================================
 * ENCODER MAP
 * ====================================================================
 * Base / MACRO / NAV:  knob = volume
 * FN layer:            knob = RGB brightness
 * ==================================================================== */
#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [COLEMAK] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [MACRO]   = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [QWERTY]  = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [FN]      = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
    [NAV]     = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
};
#endif // ENCODER_MAP_ENABLE
