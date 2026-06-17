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

/* ====================================================================
 * BEFORE COMPILING
 * ====================================================================
 * rules.mk — add:
 *   MOUSEKEY_ENABLE = yes      (required for AC_CLK left-click autoclicker)
 *
 * config.h — optional tuning:
 *   #define TAPPING_TERM 200   (ms; raise if CAPS_LT misfires, lower if sluggish)
 *
 * PHYSICAL SWITCH NOTE:
 *   keychron_common.c hard-codes the default layer as 0 (Mac/left side)
 *   or 2 (Win/right side).  COLEMAK sits at 0 and QWERTY at 2 to keep
 *   the switch working without touching keychron_common.c.
 *   Layer 1 (_UNUSED) is an intentional placeholder to preserve that
 *   index gap — do not remove it.
 * ==================================================================== */

/* ====================================================================
 * LAYER MAP
 * ====================================================================
 *  0  COLEMAK   Colemak-DH base          physical switch: left / Mac side
 *  1  _UNUSED   Placeholder              keeps QWERTY at index 2
 *  2  QWERTY    QWERTY gaming base       physical switch: right / Win side
 *  3  FN        Shared FN layer          hold right FN key (either base)
 *  4  NAV       Navigation               hold Caps Lock  (tap = ESC)
 *  5  SYM       Symbols on number row    TG via numpad [4] in MACRO mode
 *  6  FLOCK     F-key lock               TG via numpad [5] in MACRO mode
 *  7  MACRO     Numpad macro pad         TG via NumLock key
 * ==================================================================== */
enum layers {
    COLEMAK,   /* 0 */
    _UNUSED,   /* 1 — placeholder, must stay at this index */
    QWERTY,    /* 2 */
    FN,        /* 3 */
    NAV,       /* 4 */
    SYM,       /* 5 */
    FLOCK,     /* 6 */
    MACRO,     /* 7 */
};

/* ====================================================================
 * AUTOCLICKER KEYCODES
 * ====================================================================
 * AC_CLK  toggle left-click autoclicker  (needs MOUSEKEY_ENABLE = yes)
 * AC_FS   toggle F → Space → F → Space   (mutually exclusive with AC_CLK)
 *
 * Enabling either one disables the other automatically.
 * ==================================================================== */
enum custom_keycodes {
    AC_CLK = SAFE_RANGE,
    AC_FS,
};

/* Tune these to taste (milliseconds) */
#define AC_CLK_INTERVAL 100   /* ~10 left-clicks / sec  */
#define AC_FS_INTERVAL  250   /* 250 ms per keypress    */

static bool     ac_clk_on   = false;
static bool     ac_fs_on    = false;
static bool     ac_fs_state = false; /* false = send F next, true = send Space */
static uint32_t ac_timer    = 0;

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (record->event.pressed) {
        switch (keycode) {
            case AC_CLK:
                ac_clk_on   = !ac_clk_on;
                ac_fs_on    = false;          /* mutually exclusive */
                ac_timer    = timer_read32();
                return false;
            case AC_FS:
                ac_fs_on    = !ac_fs_on;
                ac_clk_on   = false;          /* mutually exclusive */
                ac_fs_state = false;
                ac_timer    = timer_read32();
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
     *   Bot row:  [V→D]  [B→V]  [N→K]  [M→H]
     *
     * Modifiers (swapped for Mac/Linux cross-use):
     *   Bottom-left order:  Ctrl → Cmd/GUI → Option/Alt
     *   i.e. the key labelled "Option" sends Cmd; "Cmd" sends Option.
     *
     * Set Hyprland (and any other OS) input method to plain QWERTY —
     * the keyboard outputs correct Colemak-DH at the USB/BT level.
     * ------------------------------------------------------------------ */
    [COLEMAK] = LAYOUT_iso_99(
        KC_ESC,             KC_BRID,  KC_BRIU,  KC_MCTRL, KC_LNPAD, UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,            KC_DEL,   KC_HOME,  KC_END,   KC_MUTE,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,            TG(MACRO),KC_PSLS,  KC_PAST,  KC_PMNS,
        KC_TAB,   KC_Q,     KC_W,     KC_F,     KC_P,     KC_B,     KC_J,     KC_L,     KC_U,     KC_Y,     KC_SCLN,  KC_LBRC,  KC_RBRC,                      KC_P7,    KC_P8,    KC_P9,    KC_PPLS,
        CAPS_LT,  KC_A,     KC_R,     KC_S,     KC_T,     KC_G,     KC_M,     KC_N,     KC_E,     KC_I,     KC_O,     KC_QUOT,  KC_NUHS,  KC_ENT,             KC_P4,    KC_P5,    KC_P6,
        KC_LSFT,  KC_Z,     KC_X,     KC_C,     KC_D,     KC_NUBS,  KC_V,     KC_K,     KC_H,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,  KC_UP,    KC_P1,    KC_P2,    KC_P3,    KC_PENT,
        KC_LCTL,  KC_LCMMD, KC_LOPTN,                               KC_SPC,                                 KC_RCMMD, FN_KEY,   KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT,  KC_P0,    KC_PDOT          ),

    /* ------------------------------------------------------------------
     * _UNUSED — Placeholder layer at index 1.
     * All transparent — falls through to COLEMAK on every key.
     * Do not remove; required to keep QWERTY at index 2.
     * ------------------------------------------------------------------ */
    [_UNUSED] = LAYOUT_iso_99(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                      _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),

    /* ------------------------------------------------------------------
     * QWERTY — Standard QWERTY for gaming
     *
     * Standard modifier order (no swap): Ctrl, GUI/Super, Alt.
     * Caps Lock keeps CAPS_LT — ESC is handy in games.
     * To disable the Super/GUI key during gaming (prevents accidental
     * Hyprland launcher), change KC_LGUI below to KC_NO.
     * ------------------------------------------------------------------ */
    [QWERTY] = LAYOUT_iso_99(
        KC_ESC,             KC_BRID,  KC_BRIU,  KC_MCTRL, KC_LNPAD, UG_VALD,  UG_VALU,  KC_MPRV,  KC_MPLY,  KC_MNXT,  KC_MUTE,  KC_VOLD,  KC_VOLU,            KC_DEL,   KC_HOME,  KC_END,   KC_MUTE,
        KC_GRV,   KC_1,     KC_2,     KC_3,     KC_4,     KC_5,     KC_6,     KC_7,     KC_8,     KC_9,     KC_0,     KC_MINS,  KC_EQL,   KC_BSPC,            TG(MACRO),KC_PSLS,  KC_PAST,  KC_PMNS,
        KC_TAB,   KC_Q,     KC_W,     KC_E,     KC_R,     KC_T,     KC_Y,     KC_U,     KC_I,     KC_O,     KC_P,     KC_LBRC,  KC_RBRC,                      KC_P7,    KC_P8,    KC_P9,    KC_PPLS,
        CAPS_LT,  KC_A,     KC_S,     KC_D,     KC_F,     KC_G,     KC_H,     KC_J,     KC_K,     KC_L,     KC_SCLN,  KC_QUOT,  KC_NUHS,  KC_ENT,             KC_P4,    KC_P5,    KC_P6,
        KC_LSFT,  KC_NUBS,  KC_Z,     KC_X,     KC_C,     KC_V,     KC_B,     KC_N,     KC_M,     KC_COMM,  KC_DOT,   KC_SLSH,            KC_RSFT,  KC_UP,    KC_P1,    KC_P2,    KC_P3,    KC_PENT,
        KC_LCTL,  KC_LGUI,  KC_LALT,                                KC_SPC,                                 KC_RALT,  FN_KEY,   KC_RCTL,  KC_LEFT,  KC_DOWN,  KC_RGHT,  KC_P0,    KC_PDOT          ),

    /* ------------------------------------------------------------------
     * FN — Shared function layer (sits above both COLEMAK and QWERTY)
     *
     * Fn row:   F1–F12 explicitly defined.  FLOCK (layer 6) also puts
     *           F1–F12 here — they stack redundantly, which is fine.
     *           Brightness/media return when FLOCK is toggled off.
     * Num row:  Bluetooth host 1/2/3 and 2.4 GHz (P2P4G).
     * Tab row:  RGB controls.
     * Home row: RGB controls (continued).
     * B key:    Battery level indicator.
     * ------------------------------------------------------------------ */
    [FN] = LAYOUT_iso_99(
        _______,            KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,             _______,  _______,  _______,  UG_TOGG,
        _______,  BT_HST1,  BT_HST2,  BT_HST3,  P2P4G,    _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        UG_TOGG,  UG_NEXT,  UG_VALU,  UG_HUEU,  UG_SATU,  UG_SPDU,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                      _______,  _______,  _______,  _______,
        _______,  UG_PREV,  UG_VALD,  UG_HUED,  UG_SATD,  UG_SPDD,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  BAT_LVL,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),

    /* ------------------------------------------------------------------
     * NAV — Navigation layer  (hold Caps Lock; tap Caps Lock = ESC)
     *
     * Arrow cluster follows Colemak-DH NEIO finger positions:
     *   Physical J → ←left    (outputs N in Colemak-DH)
     *   Physical K → ↓down    (outputs E in Colemak-DH)
     *   Physical L → ↑up      (outputs I in Colemak-DH)
     *   Physical ; → →right   (outputs O in Colemak-DH)
     *
     * Cluster above (physical U I O P [):
     *   U → Home   I → PgUp   O → PgDn   P → End   [ → Del
     *
     * Number row → F1–F12 (alternative to holding FN).
     *
     * Works identically on COLEMAK and QWERTY since NAV overrides both
     * and uses physical key positions, not logical letters.
     * ------------------------------------------------------------------ */
    [NAV] = LAYOUT_iso_99(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,   _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  KC_HOME,  KC_PGUP,  KC_PGDN,  KC_END,   KC_DEL,   _______,                      _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  KC_LEFT,  KC_DOWN,  KC_UP,    KC_RGHT,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),

    /* ------------------------------------------------------------------
     * SYM — Programmer symbol layer  (TG toggle via numpad [4] in MACRO)
     *
     * Number row outputs symbols directly; hold Shift to reach numbers.
     *   `→~  1→!  2→@  3→#  4→$  5→%  6→^  7→&  8→*  9→(  0→)  -→_  =→+
     *
     * Note: getting numbers back with Shift requires KEY_OVERRIDE_ENABLE
     * and key_overrides in config — as-is, toggle SYM off to type numbers.
     * Everything else falls through to the active base layer.
     * ------------------------------------------------------------------ */
    [SYM] = LAYOUT_iso_99(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        KC_TILD,  KC_EXLM,  KC_AT,    KC_HASH,  KC_DLR,   KC_PERC,  KC_CIRC,  KC_AMPR,  KC_ASTR,  KC_LPRN,  KC_RPRN,  KC_UNDS,  KC_PLUS,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                      _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),

    /* ------------------------------------------------------------------
     * FLOCK — F-key lock  (TG toggle via numpad [5] in MACRO mode)
     *
     * F1–F12 appear directly on the fn row without holding FN.
     * Useful for IDE sessions with heavy F-key use (debug, refactor, etc.)
     * To get brightness/media keys back, toggle FLOCK off via the numpad.
     * ------------------------------------------------------------------ */
    [FLOCK] = LAYOUT_iso_99(
        _______,            KC_F1,    KC_F2,    KC_F3,    KC_F4,    KC_F5,    KC_F6,    KC_F7,    KC_F8,    KC_F9,    KC_F10,   KC_F11,   KC_F12,             _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                      _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),

    /* ------------------------------------------------------------------
     * MACRO — Numpad macro pad  (TG via physical NumLock key)
     *
     * Numpad layout in MACRO mode:
     *
     *   ┌─────────────┬───────┬───────┬───────┐
     *   │ TG(MACRO)   │  /    │   *   │   -   │  ← NumLock = exit macro mode
     *   ├─────────────┼───────┼───────┼───────┤
     *   │   AC_CLK    │ AC_FS │  ---  │       │  ← autoclicker toggles
     *   ├─────────────┼───────┼───────┤   +   │
     *   │   TG(SYM)   │TG(FL) │  ---  │       │  ← mode toggles
     *   ├─────────────┼───────┼───────┼───────┤
     *   │     ---     │  ---  │  ---  │       │
     *   ├─────────────┴───────┼───────┘  Ent  │
     *   │        ---          │  ---  │       │
     *   └─────────────────────┴───────┴───────┘
     *
     *   AC_CLK   toggle left-click autoclicker  (MOUSEKEY_ENABLE required)
     *   AC_FS    toggle F/Space alternating autoclicker
     *   TG(SYM)  toggle SYM layer (symbols unshifted on number row)
     *   TG(FL)   toggle FLOCK layer (F1-F12 direct on fn row)
     *
     * All main keyboard keys pass through to COLEMAK / QWERTY below.
     * Numpad /, *, -, +, Enter remain functional (pass-through).
     * ------------------------------------------------------------------ */
    [MACRO] = LAYOUT_iso_99(
        _______,            _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            TG(MACRO),_______,  _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,                      AC_CLK,   AC_FS,    _______,  _______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            TG(SYM),  TG(FLOCK),_______,
        _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______,            _______,  _______,  _______,  _______,  _______,  _______,
        _______,  _______,  _______,                                _______,                                _______,  _______,  _______,  _______,  _______,  _______,  _______,  _______          ),
};

/* ====================================================================
 * ENCODER MAP
 * ====================================================================
 * Base layers:  knob = volume
 * FN layer:     knob = RGB brightness
 * All others:   knob = volume (fall-through behaviour)
 *
 * Add an entry here for every new layer you add in future.
 * ==================================================================== */
#if defined(ENCODER_MAP_ENABLE)
const uint16_t PROGMEM encoder_map[][NUM_ENCODERS][2] = {
    [COLEMAK] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [_UNUSED] = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [QWERTY]  = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [FN]      = {ENCODER_CCW_CW(UG_VALD, UG_VALU)},
    [NAV]     = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [SYM]     = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [FLOCK]   = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
    [MACRO]   = {ENCODER_CCW_CW(KC_VOLD, KC_VOLU)},
};
#endif // ENCODER_MAP_ENABLE

// ====================================================================
// KEY OVERRIDES — SYM layer: Shift + symbol = number
//
// ko_make_with_layers() restricts each override to only fire when the
// SYM layer (5) is active, so Shift+! elsewhere still works normally.
// ====================================================================
#define SYM_LAYER (1 << SYM)   // bitmask for layer 5

const key_override_t sym_grv  = ko_make_with_layers(MOD_MASK_SHIFT, KC_TILD, KC_GRV,  SYM_LAYER);
const key_override_t sym_1    = ko_make_with_layers(MOD_MASK_SHIFT, KC_EXLM, KC_1,    SYM_LAYER);
const key_override_t sym_2    = ko_make_with_layers(MOD_MASK_SHIFT, KC_AT,   KC_2,    SYM_LAYER);
const key_override_t sym_3    = ko_make_with_layers(MOD_MASK_SHIFT, KC_HASH, KC_3,    SYM_LAYER);
const key_override_t sym_4    = ko_make_with_layers(MOD_MASK_SHIFT, KC_DLR,  KC_4,    SYM_LAYER);
const key_override_t sym_5    = ko_make_with_layers(MOD_MASK_SHIFT, KC_PERC, KC_5,    SYM_LAYER);
const key_override_t sym_6    = ko_make_with_layers(MOD_MASK_SHIFT, KC_CIRC, KC_6,    SYM_LAYER);
const key_override_t sym_7    = ko_make_with_layers(MOD_MASK_SHIFT, KC_AMPR, KC_7,    SYM_LAYER);
const key_override_t sym_8    = ko_make_with_layers(MOD_MASK_SHIFT, KC_ASTR, KC_8,    SYM_LAYER);
const key_override_t sym_9    = ko_make_with_layers(MOD_MASK_SHIFT, KC_LPRN, KC_9,    SYM_LAYER);
const key_override_t sym_0    = ko_make_with_layers(MOD_MASK_SHIFT, KC_RPRN, KC_0,    SYM_LAYER);
const key_override_t sym_mins = ko_make_with_layers(MOD_MASK_SHIFT, KC_UNDS, KC_MINS, SYM_LAYER);
const key_override_t sym_eq   = ko_make_with_layers(MOD_MASK_SHIFT, KC_PLUS, KC_EQL,  SYM_LAYER);

const key_override_t *key_overrides[] = {
    &sym_grv, &sym_1, &sym_2, &sym_3, &sym_4,  &sym_5,  &sym_6,
    &sym_7,   &sym_8, &sym_9, &sym_0, &sym_mins, &sym_eq,
    NULL
};
