/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 * 
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/// Config header for RetroArch
//
//

#ifndef __CONFIG_DEF_H
#define __CONFIG_DEF_H

#include "boolean.h"
#include "libretro.h"
#include "driver.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

enum 
{
   VIDEO_GL = 0,
   VIDEO_XVIDEO,
   VIDEO_SDL,
   VIDEO_EXT,
   VIDEO_WII,
   VIDEO_XENON360,
   VIDEO_XDK_D3D,
   VIDEO_PSP1,
   VIDEO_VITA,
   VIDEO_D3D9,
   VIDEO_VG,
   VIDEO_NULL,

   AUDIO_RSOUND,
   AUDIO_OSS,
   AUDIO_ALSA,
   AUDIO_ALSATHREAD,
   AUDIO_ROAR,
   AUDIO_AL,
   AUDIO_SL,
   AUDIO_JACK,
   AUDIO_SDL,
   AUDIO_XAUDIO,
   AUDIO_PULSE,
   AUDIO_EXT,
   AUDIO_DSOUND,
   AUDIO_COREAUDIO,
   AUDIO_PS3,
   AUDIO_XENON360,
   AUDIO_WII,
   AUDIO_XDK360,
   AUDIO_NULL,

   INPUT_ANDROID,
   INPUT_SDL,
   INPUT_X,
   INPUT_DINPUT,
   INPUT_PS3,
   INPUT_PSP,
   INPUT_XENON360,
   INPUT_WII,
   INPUT_XINPUT,
   INPUT_LINUXRAW,
   INPUT_NULL
};

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES) || defined(__CELLOS_LV2__)
#define VIDEO_DEFAULT_DRIVER VIDEO_GL
#elif defined(GEKKO)
#define VIDEO_DEFAULT_DRIVER VIDEO_WII
#elif defined(XENON)
#define VIDEO_DEFAULT_DRIVER VIDEO_XENON360
#elif (defined(_XBOX1) || defined(_XBOX360)) && (defined(HAVE_D3D8) || defined(HAVE_D3D9))
#define VIDEO_DEFAULT_DRIVER VIDEO_XDK_D3D
#elif defined(HAVE_WIN32_D3D9)
#define VIDEO_DEFAULT_DRIVER VIDEO_D3D9
#elif defined(HAVE_VG)
#define VIDEO_DEFAULT_DRIVER VIDEO_VG
#elif defined(SN_TARGET_PSP2)
#define VIDEO_DEFAULT_DRIVER VIDEO_VITA
#elif defined(PSP)
#define VIDEO_DEFAULT_DRIVER VIDEO_PSP1
#elif defined(HAVE_XVIDEO)
#define VIDEO_DEFAULT_DRIVER VIDEO_XVIDEO
#elif defined(HAVE_SDL)
#define VIDEO_DEFAULT_DRIVER VIDEO_SDL
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
#define VIDEO_DEFAULT_DRIVER VIDEO_EXT
#else
#define VIDEO_DEFAULT_DRIVER VIDEO_NULL
#endif

#if defined(__CELLOS_LV2__)
#define AUDIO_DEFAULT_DRIVER AUDIO_PS3
#elif defined(XENON)
#define AUDIO_DEFAULT_DRIVER AUDIO_XENON360
#elif defined(_XBOX360)
#define AUDIO_DEFAULT_DRIVER AUDIO_XDK360
#elif defined(GEKKO)
#define AUDIO_DEFAULT_DRIVER AUDIO_WII
#elif defined(HAVE_ALSA) && defined(HAVE_VIDEOCORE)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSATHREAD
#elif defined(HAVE_ALSA)
#define AUDIO_DEFAULT_DRIVER AUDIO_ALSA
#elif defined(HAVE_PULSE)
#define AUDIO_DEFAULT_DRIVER AUDIO_PULSE
#elif defined(HAVE_OSS)
#define AUDIO_DEFAULT_DRIVER AUDIO_OSS
#elif defined(HAVE_JACK)
#define AUDIO_DEFAULT_DRIVER AUDIO_JACK
#elif defined(HAVE_COREAUDIO)
#define AUDIO_DEFAULT_DRIVER AUDIO_COREAUDIO
#elif defined(HAVE_AL)
#define AUDIO_DEFAULT_DRIVER AUDIO_AL
#elif defined(HAVE_SL)
#define AUDIO_DEFAULT_DRIVER AUDIO_SL
#elif defined(HAVE_DSOUND)
#define AUDIO_DEFAULT_DRIVER AUDIO_DSOUND
#elif defined(HAVE_SDL)
#define AUDIO_DEFAULT_DRIVER AUDIO_SDL
#elif defined(HAVE_XAUDIO)
#define AUDIO_DEFAULT_DRIVER AUDIO_XAUDIO
#elif defined(HAVE_RSOUND)
#define AUDIO_DEFAULT_DRIVER AUDIO_RSOUND
#elif defined(HAVE_ROAR)
#define AUDIO_DEFAULT_DRIVER AUDIO_ROAR
#elif defined(HAVE_DYLIB) && !defined(ANDROID)
#define AUDIO_DEFAULT_DRIVER AUDIO_EXT
#else
#define AUDIO_DEFAULT_DRIVER AUDIO_NULL
#endif

#if defined(XENON)
#define INPUT_DEFAULT_DRIVER INPUT_XENON360
#elif defined(_XBOX360) || defined(_XBOX) || defined(HAVE_XINPUT2) || defined(HAVE_XINPUT_XBOX1)
#define INPUT_DEFAULT_DRIVER INPUT_XINPUT
#elif defined(ANDROID)
#define INPUT_DEFAULT_DRIVER INPUT_ANDROID
#elif defined(_WIN32)
#define INPUT_DEFAULT_DRIVER INPUT_DINPUT
#elif defined(HAVE_SDL)
#define INPUT_DEFAULT_DRIVER INPUT_SDL
#elif defined(__CELLOS_LV2__)
#define INPUT_DEFAULT_DRIVER INPUT_PS3
#elif defined(SN_TARGET_PSP2) || defined(PSP)
#define INPUT_DEFAULT_DRIVER INPUT_PSP
#elif defined(GEKKO)
#define INPUT_DEFAULT_DRIVER INPUT_WII
#elif defined(HAVE_XVIDEO)
#define INPUT_DEFAULT_DRIVER INPUT_X
#else
#define INPUT_DEFAULT_DRIVER INPUT_NULL
#endif


////////////////
// Video
////////////////

// Windowed
static const float xscale = 3.0; // Real x res = aspect * base_size * xscale
static const float yscale = 3.0; // Real y res = base_size * yscale

// Fullscreen
static const bool fullscreen = false;  // To start in Fullscreen or not.
static const bool windowed_fullscreen = true;  // To use windowed mode or not when going fullscreen.
static const unsigned monitor_index = 0; // Which monitor to prefer. 0 is any monitor, 1 and up selects specific monitors, 1 being the first monitor.
static const unsigned fullscreen_x = 0; // Fullscreen resolution. A value of 0 uses the desktop resolution.
static const unsigned fullscreen_y = 0;

// Forcibly disable composition. Only valid on Windows Vista/7 for now.
static const bool disable_composition = false;

// Video VSYNC (recommended)
static const bool vsync = true;

// Smooths picture
static const bool video_smooth = true;

// On resize and fullscreen, rendering area will stay 4:3
static const bool force_aspect = true; 

// Controls aspect ratio handling.
static const float aspect_ratio = -1.0; // Automatic
static const bool aspect_ratio_auto = false; // 1:1 PAR

// Crop overscanned frames (7/8 or 15/15 for interlaced frames).
static const bool crop_overscan = true;

// Font size for on-screen messages.
static const unsigned font_size = 48;
// Attempt to scale the font size.
// The scale factor will be window_size / desktop_size.
static const bool font_scale = true;

// Offset for where messages will be placed on-screen. Values are in range [0.0, 1.0].
static const float message_pos_offset_x = 0.05;
static const float message_pos_offset_y = 0.05;
// Color of the message.
static const uint32_t message_color = 0xffff00; // RGB hex value.

// Render-to-texture before rendering to screen (multi-pass shaders)
static const bool render_to_texture = false;
static const float fbo_scale_x = 2.0;
static const float fbo_scale_y = 2.0;
static const bool second_pass_smooth = true;

// Record post-filtered (CPU filter) video rather than raw game output.
static const bool post_filter_record = false;

// Screenshots post-shaded GPU output if available.
static const bool gpu_screenshot = true;

// Record post-shaded GPU output instead of raw game footage if available.
static const bool gpu_record = false;

// OSD-messages
static const bool font_enable = true;

// The accurate refresh rate of your monitor (Hz).
// This is used to calculate audio input rate with the formula:
// audio_input_rate = game_input_rate * display_refresh_rate / game_refresh_rate.
// If the implementation does not report any values,
// SNES NTSC defaults will be assumed for compatibility.
// This value should stay close to 60Hz to avoid large pitch changes.
// If your monitor does not run at 60Hz, or something close to it, disable VSync,
// and leave this at its default.
#if defined(__CELLOS_LV2__) || defined(XENON) || defined(GEKKO)
static const float refresh_rate = 59.92; 
#else
static const float refresh_rate = 59.95; 
#endif

// Allow games to set rotation. If false, rotation requests are honored, but ignored.
// Used for setups where one manually rotates the monitor.
static const bool allow_rotate = true;

////////////////
// Audio
////////////////

// Will enable audio or not.
static const bool audio_enable = true;

// Output samplerate
static const unsigned out_rate = 48000; 

// When changing input rate on-the-fly
static const float audio_rate_step = 0.25;

// Audio device (e.g. hw:0,0 or /dev/audio). If NULL, will use defaults.
static const char *audio_device = NULL;

// Desired audio latency in milliseconds. Might not be honored if driver can't provide given latency.
static const int out_latency = 64;

// Will sync audio. (recommended) 
static const bool audio_sync = true;

// Experimental rate control
#if defined(GEKKO) || !defined(RARCH_CONSOLE)
static const bool rate_control = true;
#else
static const bool rate_control = false;
#endif

// Rate control delta. Defines how much rate_control is allowed to adjust input rate.
#ifdef GEKKO
static const float rate_control_delta = 0.006;
#else
static const float rate_control_delta = 0.005;
#endif

// Default audio volume in dB. (0.0 dB == unity gain).
static const float audio_volume = 0.0;

//////////////
// Misc
//////////////

// Enables use of rewind. This will incur some memory footprint depending on the save state buffer.
// This rewind only works when using bSNES core atm.
static const bool rewind_enable = false;

// The buffer size for the rewind buffer. This needs to be about 15-20MB per minute. Very game dependant.
static const unsigned rewind_buffer_size = 20 << 20; // 20MiB

// How many frames to rewind at a time.
static const unsigned rewind_granularity = 1;

// Pause gameplay when gameplay loses focus.
static const bool pause_nonactive = false;

// Saves non-volatile SRAM at a regular interval. It is measured in seconds. A value of 0 disables autosave.
static const unsigned autosave_interval = 0;

// When being client over netplay, use keybinds for player 1 rather than player 2.
static const bool netplay_client_swap_input = true;

// On save state load, block SRAM from being overwritten.
// This could potentially lead to buggy games.
static const bool block_sram_overwrite = false;

// When saving savestates, state index is automatically incremented before saving.
// When the ROM is loaded, state index will be set to the highest existing value.
static const bool savestate_auto_index = false;

// Automatically saves a savestate at the end of RetroArch's lifetime.
// The path is $SRAM_PATH.auto.
// RetroArch will automatically load any savestate with this path on startup.
static const bool savestate_auto_save = false;

// Slowmotion ratio.
static const float slowmotion_ratio = 3.0;

// Enable stdin/network command interface
static const bool network_cmd_enable = false;
static const uint16_t network_cmd_port = 55355;
static const bool stdin_cmd_enable = false;


////////////////////
// Keybinds, Joypad
////////////////////

// Axis threshold (between 0.0 and 1.0)
// How far an axis must be tilted to result in a button press
static const float axis_threshold = 0.5;

// Describes speed of which turbo-enabled buttons toggle.
static const unsigned turbo_period = 6;
static const unsigned turbo_duty_cycle = 3;

// Player 1
static const struct retro_keybind retro_keybinds_1[] = {
           // SNES button               | keyboard key | js btn | js axis |
   { true, RETRO_DEVICE_ID_JOYPAD_B,         RETROK_z,       NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_Y,         RETROK_a,       NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_SELECT,    RETROK_RSHIFT,  NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_START,     RETROK_RETURN,  NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_UP,        RETROK_UP,      NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_DOWN,      RETROK_DOWN,    NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_LEFT,      RETROK_LEFT,    NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_RIGHT,     RETROK_RIGHT,   NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_A,         RETROK_x,       NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_X,         RETROK_s,       NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L,         RETROK_q,       NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R,         RETROK_w,       NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L2,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R2,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L3,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R3,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },

   { true, RARCH_TURBO_ENABLE,               RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_PLUS,         RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_MINUS,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_PLUS,         RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_MINUS,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_PLUS,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_MINUS,       RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_PLUS,        RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_MINUS,       RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
#ifdef RARCH_CONSOLE
   { true, RARCH_ANALOG_LEFT_X_DPAD_LEFT,    RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_DPAD_RIGHT,   RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_DPAD_UP,      RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_DPAD_DOWN,    RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_DPAD_LEFT,   RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_DPAD_RIGHT,  RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_DPAD_UP,     RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_DPAD_DOWN,   RETROK_UNKNOWN, NO_BTN,      AXIS_NONE },
#endif

   { true, RARCH_FAST_FORWARD_KEY,           RETROK_SPACE,  NO_BTN,      AXIS_NONE },
   { true, RARCH_FAST_FORWARD_HOLD_KEY,      RETROK_l,      NO_BTN,      AXIS_NONE },
   { true, RARCH_LOAD_STATE_KEY,             RETROK_F4,     NO_BTN,      AXIS_NONE },
   { true, RARCH_SAVE_STATE_KEY,             RETROK_F2,     NO_BTN,      AXIS_NONE },
   { true, RARCH_FULLSCREEN_TOGGLE_KEY,      RETROK_f,      NO_BTN,      AXIS_NONE },
   { true, RARCH_QUIT_KEY,                   RETROK_ESCAPE, NO_BTN,      AXIS_NONE },
   { true, RARCH_STATE_SLOT_PLUS,            RETROK_F7,     NO_BTN,      AXIS_NONE },
   { true, RARCH_STATE_SLOT_MINUS,           RETROK_F6,     NO_BTN,      AXIS_NONE },
   { true, RARCH_REWIND,                     RETROK_r,      NO_BTN,      AXIS_NONE },
   { true, RARCH_MOVIE_RECORD_TOGGLE,        RETROK_o,      NO_BTN,      AXIS_NONE },
   { true, RARCH_PAUSE_TOGGLE,               RETROK_p,      NO_BTN,      AXIS_NONE },
   { true, RARCH_FRAMEADVANCE,               RETROK_k,      NO_BTN,      AXIS_NONE },
   { true, RARCH_RESET,                      RETROK_h,      NO_BTN,      AXIS_NONE },
   { true, RARCH_SHADER_NEXT,                RETROK_m,      NO_BTN,      AXIS_NONE },
   { true, RARCH_SHADER_PREV,                RETROK_n,      NO_BTN,      AXIS_NONE },
   { true, RARCH_CHEAT_INDEX_PLUS,           RETROK_y,      NO_BTN,      AXIS_NONE },
   { true, RARCH_CHEAT_INDEX_MINUS,          RETROK_t,      NO_BTN,      AXIS_NONE },
   { true, RARCH_CHEAT_TOGGLE,               RETROK_u,      NO_BTN,      AXIS_NONE },
   { true, RARCH_SCREENSHOT,                 RETROK_F8,     NO_BTN,      AXIS_NONE },
   { true, RARCH_DSP_CONFIG,                 RETROK_c,      NO_BTN,      AXIS_NONE },
   { true, RARCH_MUTE,                       RETROK_F9,     NO_BTN,      AXIS_NONE },
   { true, RARCH_NETPLAY_FLIP,               RETROK_i,      NO_BTN,      AXIS_NONE },
   { true, RARCH_SLOWMOTION,                 RETROK_e,      NO_BTN,      AXIS_NONE },
   { true, RARCH_ENABLE_HOTKEY,              RETROK_UNKNOWN, NO_BTN,     AXIS_NONE },
   { true, RARCH_VOLUME_UP,                  RETROK_KP_PLUS, NO_BTN,     AXIS_NONE },
   { true, RARCH_VOLUME_DOWN,                RETROK_KP_MINUS, NO_BTN,    AXIS_NONE },
   { true, RARCH_OVERLAY_NEXT,               RETROK_UNKNOWN, NO_BTN,     AXIS_NONE },
};

// Player 2-5
static const struct retro_keybind retro_keybinds_rest[] = {
   { true, RETRO_DEVICE_ID_JOYPAD_B,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_Y,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_SELECT,    RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_START,     RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_UP,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_DOWN,      RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_LEFT,      RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_RIGHT,     RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_A,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_X,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L2,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R2,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_L3,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RETRO_DEVICE_ID_JOYPAD_R3,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },

   { true, RARCH_TURBO_ENABLE,               RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_PLUS,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_MINUS,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_PLUS,         RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_MINUS,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_PLUS,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_MINUS,       RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_PLUS,        RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_MINUS,       RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
#ifdef RARCH_CONSOLE
   { true, RARCH_ANALOG_LEFT_X_DPAD_LEFT,    RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_X_DPAD_RIGHT,   RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_DPAD_UP,      RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_LEFT_Y_DPAD_DOWN,    RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_DPAD_LEFT,   RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_X_DPAD_RIGHT,  RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_DPAD_UP,     RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
   { true, RARCH_ANALOG_RIGHT_Y_DPAD_DOWN,   RETROK_UNKNOWN, NO_BTN, AXIS_NONE },
#endif
};

#endif

