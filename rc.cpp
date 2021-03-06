// This parses the RC file.

#ifdef __MINGW32__
#undef __STRICT_ANSI__
#endif

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <SDL.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <assert.h>
#include <strings.h>
#include <ctype.h>

#include "rc.h"
#include "ckvp.h"
#include "pd-defs.h"
#include "romload.h"
#include "system.h"
#include "md.h"

// CTV names
const char *ctv_names[] = {
	"off", "blur", "scanline", "interlace", "swab",
	NULL
};

// Scaling algorithms names
const char *scaling_names[] = { "default", "hqx", "scale2x", NULL };

// CPU names, keep index in sync with rc-vars.h and enums in md.h
const char *emu_z80_names[] = { "none", "mz80", "cz80", "drz80", NULL };
const char *emu_m68k_names[] = { "none", "star", "musa", "cyclone", NULL };

// The table of strings and the keysyms they map to.
// The order is a bit weird, since this was originally a mapping for the SVGALib
// scancodes, and I just added the SDL stuff on top of it.
struct rc_keysym rc_keysyms[] = {
  { "ESCAPE", PDK_ESCAPE },
  { "BACKSPACE", PDK_BACKSPACE },
  { "TAB", PDK_TAB },
  { "RETURN", PDK_RETURN },
  { "ENTER", PDK_RETURN },
  { "KP_MULTIPLY", PDK_KP_MULTIPLY },
  { "SPACE", PDK_SPACE },
  { "F1", PDK_F1 },
  { "F2", PDK_F2 },
  { "F3", PDK_F3 },
  { "F4", PDK_F4 },
  { "F5", PDK_F5 },
  { "F6", PDK_F6 },
  { "F7", PDK_F7 },
  { "F8", PDK_F8 },
  { "F9", PDK_F9 },
  { "F10", PDK_F10 },
  { "KP_7", PDK_KP7 },
  { "KP_HOME", PDK_KP7 },
  { "KP_8", PDK_KP8 },
  { "KP_UP", PDK_KP8 },
  { "KP_9", PDK_KP9 },
  { "KP_PAGE_UP", PDK_KP9 },
  { "KP_PAGEUP", PDK_KP9 },
  { "KP_MINUS", PDK_KP_MINUS },
  { "KP_4", PDK_KP4 },
  { "KP_LEFT", PDK_KP4 },
  { "KP_5", PDK_KP5 },
  { "KP_6", PDK_KP6 },
  { "KP_RIGHT", PDK_KP6 },
  { "KP_PLUS", PDK_KP_PLUS },
  { "KP_1", PDK_KP1 },
  { "KP_END", PDK_KP1 },
  { "KP_2", PDK_KP2 },
  { "KP_DOWN", PDK_KP2 },
  { "KP_3", PDK_KP3 },
  { "KP_PAGE_DOWN", PDK_KP3 },
  { "KP_PAGEDOWN", PDK_KP3 },
  { "KP_0", PDK_KP0 },
  { "KP_INSERT", PDK_KP0 },
  { "KP_PERIOD", PDK_KP_PERIOD },
  { "KP_DELETE", PDK_KP_PERIOD },
  { "F11", PDK_F11 },
  { "F12", PDK_F12 },
  { "KP_ENTER", PDK_KP_ENTER },
  { "KP_DIVIDE", PDK_KP_DIVIDE },
  { "HOME", PDK_HOME },
  { "UP", PDK_UP },
  { "PAGE_UP", PDK_PAGEUP },
  { "PAGEUP", PDK_PAGEUP },
  { "LEFT", PDK_LEFT },
  { "RIGHT", PDK_RIGHT },
  { "END", PDK_END },
  { "DOWN", PDK_DOWN },
  { "PAGE_DOWN", PDK_PAGEDOWN },
  { "PAGEDOWN", PDK_PAGEDOWN },
  { "INSERT", PDK_INSERT },
  { "DELETE", PDK_DELETE },
  { "NUMLOCK", PDK_NUMLOCK },
  { "NUM_LOCK", PDK_NUMLOCK },
  { "CAPSLOCK", PDK_CAPSLOCK },
  { "CAPS_LOCK", PDK_CAPSLOCK },
  { "SCROLLOCK", PDK_SCROLLOCK },
  { "SCROLL_LOCK", PDK_SCROLLOCK },
  { "LSHIFT", PDK_LSHIFT },
  { "SHIFT_L", PDK_LSHIFT },
  { "RSHIFT", PDK_RSHIFT },
  { "SHIFT_R", PDK_RSHIFT },
  { "LCTRL", PDK_LCTRL },
  { "CTRL_L", PDK_LCTRL },
  { "RCTRL", PDK_RCTRL },
  { "CTRL_R", PDK_RCTRL },
  { "LALT", PDK_LALT },
  { "ALT_L", PDK_LALT },
  { "RALT", PDK_RALT },
  { "ALT_R", PDK_RALT },
  { "LMETA", PDK_LMETA },
  { "META_L", PDK_LMETA },
  { "RMETA", PDK_RMETA },
  { "META_R", PDK_RMETA },
  { "", 0 },
  { NULL, 0 } // Terminator
}; // Phew! ;)

/* Define all the external RC variables */
#include "rc-vars.h"

static const struct {
	const char *name;
	uint32_t flag;
} keymod[] = {
	{ "shift-", KEYSYM_MOD_SHIFT },
	{ "ctrl-", KEYSYM_MOD_CTRL },
	{ "alt-", KEYSYM_MOD_ALT },
	{ "meta-", KEYSYM_MOD_META },
	{ "", 0 }
};

/* Parse a keysym.
 * If the string matches one of the strings in the keysym table above
 * or if it's another valid character, return the keysym, otherwise -1. */
intptr_t rc_keysym(const char *code, intptr_t *)
{
	struct rc_keysym *ks = rc_keysyms;
	uint32_t m = 0;
	uint32_t c;

	while (*code != '\0') {
		size_t i;

		for (i = 0; (keymod[i].name[0] != '\0'); ++i) {
			size_t l = strlen(keymod[i].name);

			if (strncasecmp(keymod[i].name, code, l))
				continue;
			m |= keymod[i].flag;
			code += l;
			break;
		}
		if (keymod[i].name[0] == '\0')
			break;
	}
	while (ks->name != NULL) {
		if (!strcasecmp(ks->name, code))
			return (m | ks->keysym);
		++ks;
	}
	/* Not in the list, so expect a single UTF-8 character instead. */
	code += utf8u32(&c, (const uint8_t *)code);
	if ((c == (uint32_t)-1) || (*code != '\0'))
		return -1;
	/* Must fit in 16 bits. */
	if (c > 0xffff)
		return -1;
	return (m | (c & 0xffff));
}

/* Convert a keysym to text. */
char *dump_keysym(intptr_t k)
{
	char buf[64];
	size_t i;
	size_t n;
	size_t l = 0;
	struct rc_keysym *ks = rc_keysyms;

	buf[0] = '\0';
	for (i = 0; ((keymod[i].name[0] != '\0') && (l < sizeof(buf))); ++i)
		if (k & keymod[i].flag) {
			n = strlen(keymod[i].name);
			if (n > (sizeof(buf) - l))
				n = (sizeof(buf) - l);
			memcpy(&buf[l], keymod[i].name, n);
			l += n;
		}
	k &= ~KEYSYM_MOD_MASK;
	while (ks->name != NULL) {
		if (ks->keysym == k) {
			n = strlen(ks->name);
			if (n > (sizeof(buf) - l))
				n = (sizeof(buf) - l);
			memcpy(&buf[l], ks->name, n);
			l += n;
			goto found;
		}
		++ks;
	}
	n = utf32u8(NULL, k);
	if ((n == 0) || (n > (sizeof(buf) - l)))
		return NULL;
	utf32u8((uint8_t *)&buf[l], k);
	l += n;
found:
	return backslashify((uint8_t *)buf, l, 0, NULL);
}

/* Parse a boolean value.
 * If the string is "yes" or "true", return 1.
 * If the string is "no" or "false", return 0.
 * Otherwise, just return atoi(value). */
intptr_t rc_boolean(const char *value, intptr_t *)
{
  if(!strcasecmp(value, "yes") || !strcasecmp(value, "true"))
    return 1;
  if(!strcasecmp(value, "no") || !strcasecmp(value, "false"))
    return 0;
  return atoi(value);
}

static const char *joypad_axis_type[] = {
	"max", "p", "positive",
	"min", "n", "negative",
	"between", "b", NULL
};

static const unsigned int joypad_axis_value[] = {
	JS_AXIS_POSITIVE, JS_AXIS_POSITIVE, JS_AXIS_POSITIVE,
	JS_AXIS_NEGATIVE, JS_AXIS_NEGATIVE, JS_AXIS_NEGATIVE,
	JS_AXIS_BETWEEN, JS_AXIS_BETWEEN
};

static const char *joypad_hat_type[] = {
	"centered", "up", "right_up", "right",
	"right_down", "down", "left_down",
	"left", "left_up", NULL
};

static const unsigned int joypad_hat_value[] = {
	JS_HAT_CENTERED, JS_HAT_UP, JS_HAT_RIGHT_UP, JS_HAT_RIGHT,
	JS_HAT_RIGHT_DOWN, JS_HAT_DOWN, JS_HAT_LEFT_DOWN,
	JS_HAT_LEFT, JS_HAT_LEFT_UP
};

/* Convert a joypad entry to text. */
char *dump_joypad(intptr_t js)
{
	char *str = NULL;
	unsigned int id = JS_GET_IDENTIFIER(js);
	const char *arg0 = NULL;
	unsigned int val0 = 0;
	const char *arg1 = NULL;
	unsigned int i;

	if (JS_IS_BUTTON(js)) {
		arg0 = "button";
		val0 = JS_GET_BUTTON(js);
	}
	else if (JS_IS_AXIS(js)) {
		arg0 = "axis";
		val0 = JS_GET_AXIS(js);
		for (i = 0; (i != elemof(joypad_axis_value)); ++i)
			if (joypad_axis_value[i] == JS_GET_AXIS_DIR(js)) {
				arg1 = joypad_axis_type[i];
				break;
			}
		if (arg1 == NULL)
			return NULL;
	}
	else if (JS_IS_HAT(js)) {
		arg0 = "hat";
		val0 = JS_GET_HAT(js);
		for (i = 0; (i != elemof(joypad_hat_value)); ++i)
			if (joypad_hat_value[i] == JS_GET_HAT_DIR(js)) {
				arg1 = joypad_hat_type[i];
				break;
			}
		if (arg1 == NULL)
			return NULL;
	}
	else
		return NULL;
	i = 0;
	while (1) {
		i = snprintf(str, i, "joystick%u-%s%u%s%s",
			     id, arg0, val0,
			     (arg1 ? "-" : ""), (arg1 ? arg1 : ""));
		if ((str != NULL) ||
		    ((str = (char *)malloc(++i)) == NULL))
			break;
	}
	return str;
}

/*
 * Parse a joystick/joypad command (joy_*). The following syntaxes are
 * supported:
 *
 * Normal buttons:
 *   (j|js|joystick|joypad)X-(b|button)Y
 * Axes:
 *   (j|js|joystick|joypad)X-(a|axis)Y-(max|p|positive|min|n|negative)
 * Hats:
 *   (j|js|joystick|joypad)X-(h|hat)Y-\
 *     (up|right_up|right|right_down|down|left_down|left|left_up)
 */
intptr_t rc_joypad(const char *value, intptr_t *)
{
	static const char *r_js[] = { "j", "js", "joystick", "joypad", NULL };
	static const char *r_button[] = { "b", "button", NULL };
	static const char *r_axis[] = { "a", "axis", NULL };
	static const char **r_axis_type = joypad_axis_type;
	static const unsigned int *r_axis_value = joypad_axis_value;
	static const char *r_hat[] = { "h", "hat", NULL };
	static const char **r_hat_type = joypad_hat_type;
	static const unsigned int *r_hat_value = joypad_hat_value;
	int i;
	unsigned int id;
	unsigned int arg;

	if (*value == '\0')
		return 0;
	if ((i = prefix_casematch(value, r_js)) == -1)
		return -1;
	value += strlen(r_js[i]);
	if ((i = prefix_getuint(value, &id)) == 0)
		return -1;
	value += i;
	if (*value != '-')
		return -1;
	++value;
	if ((i = prefix_casematch(value, r_button)) != -1) {
		value += strlen(r_button[i]);
		if ((i = prefix_getuint(value, &arg)) == 0)
			return -1;
		value += i;
		if (*value != '\0')
			return -1;
		return JS_BUTTON(id, arg);
	}
	if ((i = prefix_casematch(value, r_axis)) != -1) {
		value += strlen(r_axis[i]);
		if ((i = prefix_getuint(value, &arg)) == 0)
			return -1;
		value += i;
		if (*value != '-')
			return -1;
		++value;
		if ((i = prefix_casematch(value, r_axis_type)) == -1)
			return -1;
		value += strlen(r_axis_type[i]);
		if (*value != '\0')
			return -1;
		return JS_AXIS(id, arg, r_axis_value[i]);
	}
	if ((i = prefix_casematch(value, r_hat)) != -1) {
		value += strlen(r_hat[i]);
		if ((i = prefix_getuint(value, &arg)) == 0)
			return -1;
		value += i;
		if (*value != '-')
			return -1;
		++value;
		if ((i = prefix_casematch(value, r_hat_type)) == -1)
			return -1;
		value += strlen(r_hat_type[i]);
		if (*value != '\0')
			return -1;
		return JS_HAT(id, arg, r_hat_value[i]);
	}
	return -1;
}

/* Parse the CTV type. As new CTV filters get submitted expect this to grow ;)
 * Current values are:
 *  off      - No CTV
 *  blur     - blur bitmap (from DirectX DGen), by Dave <dave@dtmnt.com>
 *  scanline - attenuates every other line, looks cool! by Phillip K. Hornung <redx@pknet.com>
 */
intptr_t rc_ctv(const char *value, intptr_t *)
{
  for(int i = 0; i < NUM_CTV; ++i)
    if(!strcasecmp(value, ctv_names[i])) return i;
  return -1;
}

intptr_t rc_scaling(const char *value, intptr_t *)
{
	int i;

	for (i = 0; (i < NUM_SCALING); ++i)
		if (!strcasecmp(value, scaling_names[i]))
			return i;
	return -1;
}

/* Parse CPU types */
intptr_t rc_emu_z80(const char *value, intptr_t *)
{
	unsigned int i;

	for (i = 0; (emu_z80_names[i] != NULL); ++i)
		if (!strcasecmp(value, emu_z80_names[i]))
			return i;
	return -1;
}

intptr_t rc_emu_m68k(const char *value, intptr_t *)
{
	unsigned int i;

	for (i = 0; (emu_m68k_names[i] != NULL); ++i)
		if (!strcasecmp(value, emu_m68k_names[i]))
			return i;
	return -1;
}

intptr_t rc_region(const char *value, intptr_t *)
{
	if (strlen(value) != 1)
		return -1;
	switch (value[0] | 0x20) {
	case 'j':
	case 'x':
	case 'u':
	case 'e':
	case ' ':
		return (value[0] & ~0x20);
	}
	return -1;
}

intptr_t rc_string(const char *value, intptr_t *)
{
	char *val;

	if ((val = strdup(value)) == NULL)
		return -1;
	/* Just in case... */
	if ((intptr_t)val == -1) {
		free(val);
		return -1;
	}
	return (intptr_t)val;
}

intptr_t rc_rom_path(const char *value, intptr_t *)
{
	intptr_t r = rc_string(value, NULL);

	if (r == -1)
		return -1;
	set_rom_path((char *)r);
	return r;
}

intptr_t rc_number(const char *value, intptr_t *)
{
	return strtol(value, NULL, 0);
}

/* This is a table of all the RC options, the variables they affect, and the
 * functions to parse their values. */
struct rc_field rc_fields[RC_FIELDS_SIZE] = {
	{ "key_pad1_up", rc_keysym, &pad1_up[0] },
	{ "joy_pad1_up", rc_joypad, &pad1_up[1] },
	{ "key_pad1_down", rc_keysym, &pad1_down[0] },
	{ "joy_pad1_down", rc_joypad, &pad1_down[1] },
	{ "key_pad1_left", rc_keysym, &pad1_left[0] },
	{ "joy_pad1_left", rc_joypad, &pad1_left[1] },
	{ "key_pad1_right", rc_keysym, &pad1_right[0] },
	{ "joy_pad1_right", rc_joypad, &pad1_right[1] },
	{ "key_pad1_a", rc_keysym, &pad1_a[0] },
	{ "joy_pad1_a", rc_joypad, &pad1_a[1] },
	{ "key_pad1_b", rc_keysym, &pad1_b[0] },
	{ "joy_pad1_b", rc_joypad, &pad1_b[1] },
	{ "key_pad1_c", rc_keysym, &pad1_c[0] },
	{ "joy_pad1_c", rc_joypad, &pad1_c[1] },
	{ "key_pad1_x", rc_keysym, &pad1_x[0] },
	{ "joy_pad1_x", rc_joypad, &pad1_x[1] },
	{ "key_pad1_y", rc_keysym, &pad1_y[0] },
	{ "joy_pad1_y", rc_joypad, &pad1_y[1] },
	{ "key_pad1_z", rc_keysym, &pad1_z[0] },
	{ "joy_pad1_z", rc_joypad, &pad1_z[1] },
	{ "key_pad1_mode", rc_keysym, &pad1_mode[0] },
	{ "joy_pad1_mode", rc_joypad, &pad1_mode[1] },
	{ "key_pad1_start", rc_keysym, &pad1_start[0] },
	{ "joy_pad1_start", rc_joypad, &pad1_start[1] },
	{ "key_pad2_up", rc_keysym, &pad2_up[0] },
	{ "joy_pad2_up", rc_joypad, &pad2_up[1] },
	{ "key_pad2_down", rc_keysym, &pad2_down[0] },
	{ "joy_pad2_down", rc_joypad, &pad2_down[1] },
	{ "key_pad2_left", rc_keysym, &pad2_left[0] },
	{ "joy_pad2_left", rc_joypad, &pad2_left[1] },
	{ "key_pad2_right", rc_keysym, &pad2_right[0] },
	{ "joy_pad2_right", rc_joypad, &pad2_right[1] },
	{ "key_pad2_a", rc_keysym, &pad2_a[0] },
	{ "joy_pad2_a", rc_joypad, &pad2_a[1] },
	{ "key_pad2_b", rc_keysym, &pad2_b[0] },
	{ "joy_pad2_b", rc_joypad, &pad2_b[1] },
	{ "key_pad2_c", rc_keysym, &pad2_c[0] },
	{ "joy_pad2_c", rc_joypad, &pad2_c[1] },
	{ "key_pad2_x", rc_keysym, &pad2_x[0] },
	{ "joy_pad2_x", rc_joypad, &pad2_x[1] },
	{ "key_pad2_y", rc_keysym, &pad2_y[0] },
	{ "joy_pad2_y", rc_joypad, &pad2_y[1] },
	{ "key_pad2_z", rc_keysym, &pad2_z[0] },
	{ "joy_pad2_z", rc_joypad, &pad2_z[1] },
	{ "key_pad2_mode", rc_keysym, &pad2_mode[0] },
	{ "joy_pad2_mode", rc_joypad, &pad2_mode[1] },
	{ "key_pad2_start", rc_keysym, &pad2_start[0] },
	{ "joy_pad2_start", rc_joypad, &pad2_start[1] },
	{ "key_fix_checksum", rc_keysym, &dgen_fix_checksum[0] },
	{ "joy_fix_checksum", rc_joypad, &dgen_fix_checksum[1] },
	{ "key_quit", rc_keysym, &dgen_quit[0] },
	{ "joy_quit", rc_joypad, &dgen_quit[1] },
	{ "key_craptv_toggle", rc_keysym, &dgen_craptv_toggle[0] },
	{ "joy_craptv_toggle", rc_joypad, &dgen_craptv_toggle[1] },
	{ "key_scaling_toggle", rc_keysym, &dgen_scaling_toggle[0] },
	{ "joy_scaling_toggle", rc_joypad, &dgen_scaling_toggle[1] },
	{ "key_screenshot", rc_keysym, &dgen_screenshot[0] },
	{ "joy_screenshot", rc_joypad, &dgen_screenshot[1] },
	{ "key_reset", rc_keysym, &dgen_reset[0] },
	{ "joy_reset", rc_joypad, &dgen_reset[1] },
	{ "key_slot_0", rc_keysym, &dgen_slot_0[0] },
	{ "joy_slot_0", rc_joypad, &dgen_slot_0[1] },
	{ "key_slot_1", rc_keysym, &dgen_slot_1[0] },
	{ "joy_slot_1", rc_joypad, &dgen_slot_1[1] },
	{ "key_slot_2", rc_keysym, &dgen_slot_2[0] },
	{ "joy_slot_2", rc_joypad, &dgen_slot_2[1] },
	{ "key_slot_3", rc_keysym, &dgen_slot_3[0] },
	{ "joy_slot_3", rc_joypad, &dgen_slot_3[1] },
	{ "key_slot_4", rc_keysym, &dgen_slot_4[0] },
	{ "joy_slot_4", rc_joypad, &dgen_slot_4[1] },
	{ "key_slot_5", rc_keysym, &dgen_slot_5[0] },
	{ "joy_slot_5", rc_joypad, &dgen_slot_5[1] },
	{ "key_slot_6", rc_keysym, &dgen_slot_6[0] },
	{ "joy_slot_6", rc_joypad, &dgen_slot_6[1] },
	{ "key_slot_7", rc_keysym, &dgen_slot_7[0] },
	{ "joy_slot_7", rc_joypad, &dgen_slot_7[1] },
	{ "key_slot_8", rc_keysym, &dgen_slot_8[0] },
	{ "joy_slot_8", rc_joypad, &dgen_slot_8[1] },
	{ "key_slot_9", rc_keysym, &dgen_slot_9[0] },
	{ "joy_slot_9", rc_joypad, &dgen_slot_9[1] },
	{ "key_save", rc_keysym, &dgen_save[0] },
	{ "joy_save", rc_joypad, &dgen_save[1] },
	{ "key_load", rc_keysym, &dgen_load[0] },
	{ "joy_load", rc_joypad, &dgen_load[1] },
	{ "key_z80_toggle", rc_keysym, &dgen_z80_toggle[0] },
	{ "joy_z80_toggle", rc_joypad, &dgen_z80_toggle[1] },
	{ "key_cpu_toggle", rc_keysym, &dgen_cpu_toggle[0] },
	{ "joy_cpu_toggle", rc_joypad, &dgen_cpu_toggle[1] },
	{ "key_stop", rc_keysym, &dgen_stop[0] },
	{ "joy_stop", rc_joypad, &dgen_stop[1] },
	{ "key_game_genie", rc_keysym, &dgen_game_genie[0] },
	{ "joy_game_genie", rc_joypad, &dgen_game_genie[1] },
	{ "key_fullscreen_toggle", rc_keysym, &dgen_fullscreen_toggle[0] },
	{ "joy_fullscreen_toggle", rc_joypad, &dgen_fullscreen_toggle[1] },
	{ "key_debug_enter", rc_keysym, &dgen_debug_enter[0] },
	{ "joy_debug_enter", rc_joypad, &dgen_debug_enter[1] },
	{ "key_prompt", rc_keysym, &dgen_prompt[0] },
	{ "joy_prompt", rc_joypad, &dgen_prompt[1] },
	{ "bool_vdp_hide_plane_a", rc_boolean, &dgen_vdp_hide_plane_a },
	{ "bool_vdp_hide_plane_b", rc_boolean, &dgen_vdp_hide_plane_b },
	{ "bool_vdp_hide_plane_w", rc_boolean, &dgen_vdp_hide_plane_w },
	{ "bool_vdp_hide_sprites", rc_boolean, &dgen_vdp_hide_sprites },
	{ "bool_vdp_sprites_boxing", rc_boolean, &dgen_vdp_sprites_boxing },
	{ "int_vdp_sprites_boxing_fg", rc_number, &dgen_vdp_sprites_boxing_fg },
	{ "int_vdp_sprites_boxing_bg", rc_number, &dgen_vdp_sprites_boxing_bg },
	{ "bool_autoload", rc_boolean, &dgen_autoload },
	{ "bool_autosave", rc_boolean, &dgen_autosave },
	{ "bool_autoconf", rc_boolean, &dgen_autoconf },
	{ "bool_frameskip", rc_boolean, &dgen_frameskip },
	{ "bool_show_carthead", rc_boolean, &dgen_show_carthead },
	{ "str_rom_path", rc_rom_path,
	  (intptr_t *)((void *)&dgen_rom_path) }, // SH
	{ "bool_raw_screenshots", rc_boolean, &dgen_raw_screenshots },
	{ "ctv_craptv_startup", rc_ctv, &dgen_craptv }, // SH
	{ "scaling_startup", rc_scaling, &dgen_scaling }, // SH
	{ "emu_z80_startup", rc_emu_z80, &dgen_emu_z80 }, // SH
	{ "emu_m68k_startup", rc_emu_m68k, &dgen_emu_m68k }, // SH
	{ "bool_sound", rc_boolean, &dgen_sound }, // SH
	{ "int_soundrate", rc_number, &dgen_soundrate }, // SH
	{ "int_soundsegs", rc_number, &dgen_soundsegs }, // SH
	{ "int_soundsamples", rc_number, &dgen_soundsamples }, // SH
	{ "int_volume", rc_number, &dgen_volume },
	{ "key_volume_inc", rc_keysym, &dgen_volume_inc[0] },
	{ "joy_volume_inc", rc_joypad, &dgen_volume_inc[1] },
	{ "key_volume_dec", rc_keysym, &dgen_volume_dec[0] },
	{ "joy_volume_dec", rc_joypad, &dgen_volume_dec[1] },
	{ "bool_mjazz", rc_boolean, &dgen_mjazz }, // SH
	{ "int_nice", rc_number, &dgen_nice },
	{ "int_hz", rc_number, &dgen_hz }, // SH
	{ "bool_pal", rc_boolean, &dgen_pal }, // SH
	{ "region", rc_region, &dgen_region }, // SH
	{ "str_region_order", rc_string,
	  (intptr_t *)((void *)&dgen_region_order) },
	{ "bool_fps", rc_boolean, &dgen_fps },
	{ "bool_buttons", rc_boolean, &dgen_buttons },
	{ "bool_fullscreen", rc_boolean, &dgen_fullscreen }, // SH
	{ "int_info_height", rc_number, &dgen_info_height }, // SH
	{ "int_width", rc_number, &dgen_width }, // SH
	{ "int_height", rc_number, &dgen_height }, // SH
	{ "int_scale", rc_number, &dgen_scale }, // SH
	{ "int_scale_x", rc_number, &dgen_x_scale }, // SH
	{ "int_scale_y", rc_number, &dgen_y_scale }, // SH
	{ "int_depth", rc_number, &dgen_depth }, // SH
	{ "bool_swab", rc_boolean, &dgen_swab }, // SH
	{ "bool_opengl", rc_boolean, &dgen_opengl }, // SH
	{ "bool_opengl_aspect", rc_boolean, &dgen_opengl_aspect }, // SH
	// deprecated, use int_width
	{ "int_opengl_width", rc_number, &dgen_width },
	// deprecated, use int_height
	{ "int_opengl_height", rc_number, &dgen_height },
	{ "bool_opengl_linear", rc_boolean, &dgen_opengl_linear }, // SH
	{ "bool_opengl_32bit", rc_boolean, &dgen_opengl_32bit }, // SH
	// deprecated, use bool_swab
	{ "bool_opengl_swap", rc_boolean, &dgen_swab }, // SH
	{ "bool_opengl_square", rc_boolean, &dgen_opengl_square }, // SH
	{ "bool_doublebuffer", rc_boolean, &dgen_doublebuffer }, // SH
	{ "bool_screen_thread", rc_boolean, &dgen_screen_thread }, // SH
	{ "bool_joystick", rc_boolean, &dgen_joystick }, // SH
	{ NULL, NULL, NULL }
};

struct rc_binding rc_binding_head = {
	&rc_binding_head,
	&rc_binding_head,
	false,
	0,
	NULL,
	NULL
};

static void rc_binding_cleanup(void)
{
	struct rc_binding *rcb = rc_binding_head.next;
	struct rc_binding *next;

	while (rcb != &rc_binding_head) {
		next = rcb->next;
		assert(rcb->to != NULL);
		assert((intptr_t)rcb->to != -1);
		free(rcb->to);
#ifndef NDEBUG
		memset(rcb, 0x66, sizeof(*rcb));
#endif
		free(rcb);
		rcb = next;
	}
}

struct rc_field *rc_binding_add(const char *rc, const char *to)
{
	static bool registered = false;
	size_t rc_sz = (strlen(rc) + 1);
	struct rc_field *rcf = rc_fields;
	struct rc_binding *rcb;
	intptr_t code;
	bool type;
	size_t off;
	char *new_to;

	if ((registered == false) && (atexit(rc_binding_cleanup) == 0))
		registered = true;
	// Check if the RC name looks like a valid binding.
	for (off = 0; (rc[off] != '\0'); ++off) {
		if (RC_BIND_PREFIX[off] == '\0')
			break;
		if (rc[off] != RC_BIND_PREFIX[off])
			return NULL;
	}
	if (rc[off] == '\0')
		return NULL;
	// Extract keysym or joypad code from RC name.
	if ((type = true, ((code = rc_joypad(&rc[off], NULL)) == -1)) &&
	    (type = false, ((code = rc_keysym(&rc[off], NULL)) == -1)))
		return NULL;
	// Find a free entry in rc_fields[].
	while (rcf->fieldname != NULL)
		++rcf;
	if ((rcf - rc_fields) == (elemof(rc_fields) - 1))
		return NULL;
	assert(rcf->parser == NULL);
	assert(rcf->variable == NULL);
	// Allocate binding.
	if ((rcb = (struct rc_binding *)malloc(sizeof(*rcb) + rc_sz)) == NULL)
		return NULL;
	if (((new_to = strdup(to)) == NULL) || ((intptr_t)new_to == -1)) {
		// Either strdup() failed, or this pointer equals -1 and
		// it's invalid ((intptr_t)-1 is special).
		free(new_to);
		free(rcb);
		return NULL;
	}
	// Configure binding.
	rcb->prev = rc_binding_head.prev;
	rcb->next = &rc_binding_head;
	rcb->prev->next = rcb;
	rcb->next->prev = rcb;
	rcb->type = type;
	rcb->code = code;
	rcb->rc = ((char *)rcb + sizeof(*rcb));
	rcb->to = new_to;
	memcpy(rcb->rc, rc, rc_sz);
	// Configure RC field.
	rcf->fieldname = rcb->rc;
	rcf->parser = rc_bind;
	rcf->variable = (intptr_t *)((void *)&rcb->to);
	return rcf;
}

void rc_binding_del(rc_field *rcf)
{
	struct rc_binding *rcb =
		containerof(rcf->variable, struct rc_binding, to);

	assert(rcf >= &rc_fields[0]);
	assert(rcf < &rc_fields[(sizeof(rc_fields) - 1)]);
	assert(rcf->fieldname != NULL);
	assert(rcf->parser != NULL);
	assert(rcf->variable != NULL);
	if (rcf->parser != rc_bind)
		return;
	assert(rcb != &rc_binding_head);
	// Clean-up.
	rcb->prev->next = rcb->next;
	rcb->next->prev = rcb->prev;
	assert(rcb->to != NULL);
	assert((intptr_t)rcb->to != -1);
	free(rcb->to);
#ifndef NDEBUG
	memset(rcb, 0x88, (sizeof(*rcb) + strlen(rcb->rc) + 1));
#endif
	free(rcb);
	// Shift the next entries.
	do {
		memcpy(rcf, (rcf + 1), sizeof(*rcf));
		++rcf;
	}
	while (rcf->fieldname != NULL);
	assert(rcf < &rc_fields[(sizeof(rc_fields) - 1)]);
}

intptr_t rc_bind(const char *value, intptr_t *variable)
{
	struct rc_binding *rcb = containerof(variable, struct rc_binding, to);
	char *to;

	assert(*variable != -1);
	assert(rcb != NULL);
	assert(rcb->prev != NULL);
	assert(rcb->next != NULL);
	assert(rcb->rc != NULL);
	assert(rcb->to != NULL);
	assert((intptr_t)rcb->to != -1);
	if (((to = strdup(value)) == NULL) || ((intptr_t)to == -1)) {
		// Either strdup() failed, or this pointer equals -1 and
		// it's invalid ((intptr_t)-1 is special).
		free(to);
		// Get the previous value.
		to = rcb->to;
	}
	else
		free(rcb->to);
	rcb->to = NULL; // Will be updated by the return value.
	// This function must always return a valid pointer.
	return (intptr_t)to;
}

/* Replace unprintable characters */
static char *strclean(char *s)
{
	size_t i;

	for (i = 0; (s[i] != '\0'); ++i)
		if (!isprint(s[i]))
			s[i] = '?';
	return s;
}

/* This is for cleaning up rc_str fields at exit */
struct rc_str *rc_str_list = NULL;

void rc_str_cleanup(void)
{
	struct rc_str *rs = rc_str_list;

	while (rs != NULL) {
		if (rs->val == rs->alloc)
			rs->val = NULL;
		free(rs->alloc);
		rs->alloc = NULL;
		rs = rs->next;
	}
}

/* Parse the rc file */
void parse_rc(FILE *file, const char *name)
{
	struct rc_field *rc_field = NULL;
	intptr_t potential;
	int overflow = 0;
	size_t len;
	size_t parse;
	ckvp_t ckvp = CKVP_INIT;
	char buf[1024];

	if ((file == NULL) || (name == NULL))
		return;
read:
	len = fread(buf, 1, sizeof(buf), file);
	/* Check for read errors first */
	if ((len == 0) && (ferror(file))) {
		fprintf(stderr, "rc: %s: %s\n", name, strerror(errno));
		return;
	}
	/* The goal is to make an extra pass with len == 0 when feof(file) */
	parse = 0;
parse:
	parse += ckvp_parse(&ckvp, (len - parse), &(buf[parse]));
	switch (ckvp.state) {
	case CKVP_NONE:
		/* Nothing to do */
		break;
	case CKVP_OUT_FULL:
		/*
		  Buffer is full, field is probably too large. We don't want
		  to report it more than once, so just store a flag for now.
		*/
		overflow = 1;
		break;
	case CKVP_OUT_KEY:
		/* Got a key */
		if (overflow) {
			fprintf(stderr, "rc: %s:%u:%u: key field too large\n",
				name, ckvp.line, ckvp.column);
			rc_field = NULL;
			overflow = 0;
			break;
		}
		/* Find the related rc_field in rc_fields */
		assert(ckvp.out_size < sizeof(ckvp.out));
		ckvp.out[(ckvp.out_size)] = '\0';
		for (rc_field = rc_fields; (rc_field->fieldname != NULL);
		     ++rc_field)
			if (!strcasecmp(rc_field->fieldname, ckvp.out))
				goto key_over;
		/* Try to add it as a new binding. */
		if ((rc_field = rc_binding_add(ckvp.out, "")) != NULL)
			goto key_over;
		fprintf(stderr, "rc: %s:%u:%u: unknown key `%s'\n",
			name, ckvp.line, ckvp.column, strclean(ckvp.out));
	key_over:
		break;
	case CKVP_OUT_VALUE:
		/* Got a value */
		if (overflow) {
			fprintf(stderr, "rc: %s:%u:%u: value field too large\n",
				name, ckvp.line, ckvp.column);
			overflow = 0;
			break;
		}
		assert(ckvp.out_size < sizeof(ckvp.out));
		ckvp.out[(ckvp.out_size)] = '\0';
		if ((rc_field == NULL) || (rc_field->fieldname == NULL))
			break;
		potential = rc_field->parser(ckvp.out, rc_field->variable);
		/* If we got a bad value, discard and warn user */
		if ((rc_field->parser != rc_number) && (potential == -1))
			fprintf(stderr,
				"rc: %s:%u:%u: invalid value for key"
				" `%s': `%s'\n",
				name, ckvp.line, ckvp.column,
				rc_field->fieldname, strclean(ckvp.out));
		else if ((rc_field->parser == rc_string) ||
			 (rc_field->parser == rc_rom_path)) {
			struct rc_str *rs =
				(struct rc_str *)rc_field->variable;

			if (rc_str_list == NULL) {
				atexit(rc_str_cleanup);
				rc_str_list = rs;
			}
			else if (rs->alloc == NULL) {
				rs->next = rc_str_list;
				rc_str_list = rs;
			}
			else
				free(rs->alloc);
			rs->alloc = (char *)potential;
			rs->val = rs->alloc;
		}
		else if ((rc_field->parser == rc_region) &&
			 (rc_field->variable == &dgen_region)) {
			/*
			  Another special case: updating region also updates
			  PAL and Hz settings.
			*/
			*(rc_field->variable) = potential;
			if (*(rc_field->variable)) {
				int hz;
				int pal;

				md::region_info(dgen_region, &pal, &hz,
						0, 0, 0);
				dgen_hz = hz;
				dgen_pal = pal;
			}
		}
		else
			*(rc_field->variable) = potential;
		break;
	case CKVP_ERROR:
	default:
		fprintf(stderr, "rc: %s:%u:%u: syntax error, aborting\n",
			name, ckvp.line, ckvp.column);
		return;
	}
	/* Not done with the current buffer? */
	if (parse != len)
		goto parse;
	/* If len != 0, try to read once again */
	if (len != 0)
		goto read;
}

/* Dump the rc file */
void dump_rc(FILE *file)
{
	const struct rc_field *rc = rc_fields;

	while (rc->fieldname != NULL) {
		intptr_t val = *rc->variable;
		char *s = backslashify((const uint8_t *)rc->fieldname,
				       strlen(rc->fieldname), 0, NULL);

		if (s == NULL) {
			++rc;
			continue;
		}
		fprintf(file, "%s = ", s);
		free(s);
		if (rc->parser == rc_number)
			fprintf(file, "%ld", (long)val);
		else if (rc->parser == rc_keysym) {
			char *ks = dump_keysym(val);

			if (ks != NULL) {
				fprintf(file, "\"%s\"", ks);
				free(ks);
			}
			else
				fputs("''", file);
		}
		else if (rc->parser == rc_boolean)
			fprintf(file, "%s", ((val) ? "true" : "false"));
		else if (rc->parser == rc_joypad) {
			char *js = dump_joypad(val);

			if (js != NULL) {
				fprintf(file, "\"%s\"", js);
				free(js);
			}
			else
				fputs("''", file);
		}
		else if (rc->parser == rc_ctv)
			fprintf(file, "%s", ctv_names[val]);
		else if (rc->parser == rc_scaling)
			fprintf(file, "%s", scaling_names[val]);
		else if (rc->parser == rc_emu_z80)
			fprintf(file, "%s", emu_z80_names[val]);
		else if (rc->parser == rc_emu_m68k)
			fprintf(file, "%s", emu_m68k_names[val]);
		else if (rc->parser == rc_region) {
			if (isgraph((char)val))
				fputc((char)val, file);
			else
				fputs("' '", file);
		}
		else if ((rc->parser == rc_string) ||
			 (rc->parser == rc_rom_path)) {
			struct rc_str *rs = (struct rc_str *)rc->variable;

			if ((rs->val == NULL) ||
			    ((s = backslashify
			      ((const uint8_t *)rs->val,
			       strlen(rs->val), 0, NULL)) == NULL))
				fprintf(file, "\"\"");
			else {
				fprintf(file, "\"%s\"", s);
				free(s);
			}
		}
		else if (rc->parser == rc_bind) {
			s = *(char **)rc->variable;
			assert(s != NULL);
			assert((intptr_t)s != -1);
			s = backslashify((uint8_t *)s, strlen(s), 0, NULL);
			fputc('"', file);
			if (s != NULL) {
				fputs(s, file);
				free(s);
			}
			fputc('"', file);
		}
		fputs("\n", file);
		++rc;
	}
}
