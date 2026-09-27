// 3DS-specific main file for Super Metroid port
// Based on snesrev/sm with minimal modifications for 3DS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <sys/stat.h>
#include "SDL2/SDL.h"
#include <3ds.h>

#include "src/snes/ppu.h"
#include "src/types.h"
#include "src/sm_rtl.h"
#include "src/sm_cpu_infra.h"
#include "src/ida_types.h"
#include "src/variables.h"
#include "src/config.h"
#include "src/util.h"
#include "src/spc_player.h"
#include "bottom_screen.h"
#include "gpu_presenter.h"
#include "ppu_gpu.h"

enum Button {
  BTN_A = 0,
  BTN_B = 1,
  BTN_SELECT = 2,
  BTN_START = 3,
  BTN_DPAD_R = 4,
  BTN_DPAD_L = 5,
  BTN_DPAD_U = 6,
  BTN_DPAD_D = 7,
  BTN_R = 8,
  BTN_L = 9,
  BTN_X = 10,
  BTN_Y = 11,
  BTN_ZL = 14,
  BTN_ZR = 15,
};

static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int len);
static void HandleInput(int keyCode, int keyMod, bool pressed);
static void HandleCommand(uint32 j, bool pressed);

bool g_debug_flag;
bool g_is_turbo;
bool g_want_dump_memmap_flags;
bool g_new_ppu = true;
bool g_other_image;
struct SpcPlayer *g_spc_player;

static uint8_t *g_pixels;
static uint8_t g_my_pixels[256 * 4 * 240];

int g_got_mismatch_count;

static const char kWindowTitle[] = "Super Metroid 3DS";
static SDL_Window *g_window;
static bool g_gpu_presenter;

static uint8 g_paused, g_turbo, g_replay_turbo = true;
static uint8 g_gamepad_buttons;
static int16_t g_circle_axis[2];
static int g_input1_state;
static bool g_display_perf;
static int g_curr_fps;
static int g_ppu_render_flags = 0;
static int g_snes_width = 256, g_snes_height = 240;
static int g_sdl_audio_mixer_volume = SDL_MIX_MAXVOLUME;

extern Snes *g_snes;

void NORETURN Die(const char *error) {
  fprintf(stderr, "Error: %s\n", error);
  exit(1);
}

void Warning(const char *error) {
  fprintf(stderr, "Warning: %s\n", error);
}

void RtlDrawPpuFrame(uint8 *pixel_buffer, size_t pitch, uint32 render_flags) {
  uint8 *ppu_pixels = g_pixels; //g_other_image ? g_my_pixels : g_pixels;
  for (size_t y = 0; y < 240; y++)
    memcpy((uint8_t *)pixel_buffer + y * pitch, ppu_pixels + y * 256 * 4, 256 * 4);
}

static void DrawPpuFrame(void) {
    u8 *fb = gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
    uint8_t *src = g_pixels;

    const int src_w = 256;
    const int src_h = 224;

    const int fb_w  = 400;
    const int fb_h  = 240;

    const int dst_w = 274;
    const int dst_h = fb_h;                            // 240

    const int x_off = (fb_w - dst_w) / 2;
    const int y_off = 0;

    static uint8_t xmap[274];
    static uint8_t ymap[240];
    static bool maps_ready;
    if (!maps_ready) {
        for (int x = 0; x < dst_w; x++)
            xmap[x] = (x * src_w) / dst_w;
        for (int y = 0; y < dst_h; y++)
            ymap[y] = (y * src_h) / dst_h;
        maps_ready = true;
    }

    for (int dy = 0; dy < dst_h; dy++) {
        const uint8_t *src_row = &src[ymap[dy] * src_w * 4];
        uint8_t *dst_col = &fb[((x_off * fb_h) + (fb_h - 1 - (dy + y_off))) * 4];

        for (int dx = 0; dx < dst_w; dx++) {
            const uint8_t *s = &src_row[xmap[dx] * 4];

            uint8_t b = s[0];
            uint8_t g = s[1];
            uint8_t r = s[2];

            dst_col[1] = b;
            dst_col[2] = g;
            dst_col[3] = r;
            dst_col += fb_h * 4;
        }
    }
}

static SDL_mutex *g_audio_mutex;
static uint8 *g_audiobuffer, *g_audiobuffer_cur, *g_audiobuffer_end;
static int g_frames_per_block;
static uint8 g_audio_channels;
static SDL_AudioDeviceID g_audio_device;
#ifdef SM3DS_PROFILE
static volatile uint32_t g_profile_audio_callbacks;
static volatile uint32_t g_profile_audio_nonzero;
static volatile uint64_t g_profile_audio_ticks;
extern uint64_t g_profile_game_ticks;
extern uint64_t g_profile_ppu_ticks;
extern uint64_t g_profile_ppu_sprite_ticks;
extern uint64_t g_profile_ppu_main_ticks;
extern uint64_t g_profile_ppu_sub_ticks;
extern uint64_t g_profile_ppu_compose_ticks;
extern uint32_t g_profile_ppu_phase_frames;
extern uint32_t g_profile_color_map_rebuilds;
extern uint32_t g_profile_fixed_map_rebuilds;
extern uint32_t g_profile_backdrop_map_rebuilds;
extern uint32_t g_profile_halfadd_spans;
extern uint32_t g_profile_generic_sub_spans;
extern uint32_t g_profile_generic_key;
extern uint64_t g_profile_audio_lock_ticks;
extern uint64_t g_profile_audio_generate_ticks;
extern uint64_t g_profile_audio_copy_ticks;
extern uint64_t g_profile_hdma_ticks;
extern uint32_t g_profile_hdma_calls;
extern uint32_t g_profile_hdma_active_channels;
extern uint32_t g_profile_hdma_bytes;
extern uint32_t g_profile_hdma_bbus[256];
extern uint64_t g_profile_pica_prepare_ticks;
extern uint32_t g_profile_pica_gpu_frames;
extern uint32_t g_profile_pica_cpu_frames;
extern uint32_t g_profile_pica_vertices;
extern uint32_t g_profile_pica_decodes;
extern uint32_t g_profile_pica_wide_gpu_frames;
extern uint32_t g_profile_pica_wide_cpu_frames;
#endif

static bool CreateEmulatorDspMarker(void) {
  struct stat st;
  if (stat("sdmc:/3ds/dspfirm.cdc", &st) == 0)
    return false;

  mkdir("sdmc:/3ds", 0755);
  FILE *fp = fopen("sdmc:/3ds/dspfirm.cdc", "wb");
  if (!fp)
    return false;
  fclose(fp);
  return true;
}

static bool EnableSystemCoreTime(void) {
  static const u32 candidates[] = { 80, 70, 50, 30 };
  for (unsigned i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
    if (R_SUCCEEDED(APT_SetAppCpuTimeLimit(candidates[i]))) {
      u32 actual = 0;
      if (R_SUCCEEDED(APT_GetAppCpuTimeLimit(&actual)) && actual > 0)
        return true;
    }
  }
  return false;
}

void RtlApuLock(void) {
  SDL_LockMutex(g_audio_mutex);
}

void RtlApuUnlock(void) {
  SDL_UnlockMutex(g_audio_mutex);
}

static void SDLCALL AudioCallback(void *userdata, Uint8 *stream, int len) {
#ifdef SM3DS_PROFILE
  g_profile_audio_callbacks++;
#endif
  while (len != 0) {
    if (g_audiobuffer_end - g_audiobuffer_cur == 0) {
#ifdef SM3DS_PROFILE
      uint64_t profile_audio_before = SDL_GetPerformanceCounter();
#endif
      RtlRenderAudio((int16 *)g_audiobuffer, g_frames_per_block, g_audio_channels);
#ifdef SM3DS_PROFILE
      g_profile_audio_ticks += SDL_GetPerformanceCounter() - profile_audio_before;
#endif
      g_audiobuffer_cur = g_audiobuffer;
      g_audiobuffer_end = g_audiobuffer + g_frames_per_block * g_audio_channels * sizeof(int16);
    }
    int n = IntMin(len, g_audiobuffer_end - g_audiobuffer_cur);
    if (g_sdl_audio_mixer_volume == SDL_MIX_MAXVOLUME) {
      memcpy(stream, g_audiobuffer_cur, n);
    } else {
      SDL_memset(stream, 0, n);
      SDL_MixAudioFormat(stream, g_audiobuffer_cur, AUDIO_S16, n, g_sdl_audio_mixer_volume);
    }
#ifdef SM3DS_PROFILE
    const int16_t *profile_samples = (const int16_t *)g_audiobuffer_cur;
    for (int i = 0; i < n / (int)sizeof(int16_t); i++) {
      if (profile_samples[i] != 0) {
        g_profile_audio_nonzero++;
        break;
      }
    }
#endif
    g_audiobuffer_cur += n;
    stream += n;
    len -= n;
  }
}

int idx_of_btn(enum Button b) {
  switch(b) {
    case BTN_DPAD_U:
      return 0;
    case BTN_DPAD_D:
      return 1;
    case BTN_DPAD_L:
      return 2;
    case BTN_DPAD_R:
      return 3;
    case BTN_SELECT:
      return 4;
    case BTN_START:
      return 5;
    case BTN_A:
      return 6;
    case BTN_B:
      return 7;
    case BTN_X:
      return 8;
    case BTN_Y:
      return 9;
    case BTN_L:
      return 10;
    case BTN_R:
      return 11;
    default:
      return -1;
  }
}

static void HandleCommand(uint32 j, bool pressed) {
  int button_index = idx_of_btn(j);
  if (button_index < 0)
    return;
  j = 1 + button_index;
  if (j <= kKeys_Controls_Last) {
    static const uint8 kKbdRemap[] = { 0, 4, 5, 6, 7, 2, 3, 8, 0, 9, 1, 10, 11 };
    if (pressed)
      g_input1_state |= 1 << kKbdRemap[j];
    else
      g_input1_state &= ~(1 << kKbdRemap[j]);
    return;
  }

  // if (j == kKeys_Turbo) {
  //   g_turbo = pressed;
  //   return;
  // }

  // if (!pressed)
  //   return;

  // if (j <= kKeys_Load_Last) {
  //   RtlSaveLoad(kSaveLoad_Load, j - kKeys_Load);
  // } else if (j <= kKeys_Save_Last) {
  //   RtlSaveLoad(kSaveLoad_Save, j - kKeys_Save);
  // } else if (j <= kKeys_Replay_Last) {
  //   RtlSaveLoad(kSaveLoad_Replay, j - kKeys_Replay);
  // } else {
  //   switch (j) {
  //   case kKeys_Reset: RtlReset(1); break;
  //   case kKeys_Pause: g_paused = !g_paused; break;
  //   case kKeys_ReplayTurbo: g_replay_turbo = !g_replay_turbo; break;
  //   default: break;
  //   }
  // }
}

static void HandleCirclePadAxis(unsigned axis, int16_t value) {
  if (axis >= 2) return;
  g_circle_axis[axis] = value;
  const int deadzone = 8000;
  uint8 buttons = 0;
  if (g_circle_axis[0] < -deadzone) buttons |= 1 << 6;
  if (g_circle_axis[0] > deadzone) buttons |= 1 << 7;
  if (g_circle_axis[1] < -deadzone) buttons |= 1 << 4;
  if (g_circle_axis[1] > deadzone) buttons |= 1 << 5;
  g_gamepad_buttons = buttons;
}

enum {
  kDefaultFullscreen = 0,
  kMaxWindowScale = 10,
  kDefaultFreq = 32000,
  kDefaultChannels = 2,
  kDefaultSamples = 2048,
};

// #undef main
int main(int argc, char** argv) {
  // Use default config - no config file on 3DS
  ParseConfigFile(NULL);

  g_ppu_render_flags = kPpuRenderFlags_Height240 
                     | kPpuRenderFlags_NewRenderer
                     | kPpuRenderFlags_4x4Mode7;
  g_config.audio_freq = kDefaultFreq;
  g_config.audio_channels = kDefaultChannels;
  g_config.audio_samples = kDefaultSamples;

  // Initialize SDL
  if(SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
    printf("Failed to init SDL: %s\n", SDL_GetError());
    return 1;
  }

  // Use the faster New 3DS CPU clock when available. This is harmless on
  // original 3DS models and mirrors the setup used by the reference port.
  osSetSpeedupEnable(true);
  EnableSystemCoreTime();

  SDL_JoystickEventState(SDL_ENABLE);
  SDL_GameControllerEventState(SDL_ENABLE);

  if (SDL_NumJoysticks() > 0) {
      SDL_GameControllerOpen(0);
  }

  Result rc = romfsInit();
  if (rc)
    while(true);

  // Load ROM from romfs
  const char* filename = "romfs:/sm.smc";
  Snes *snes = SnesInit(filename);

  if(snes == NULL) {
    char buf[256];
    snprintf(buf, sizeof(buf), "Unable to load ROM: %s\nMake sure sm.smc is in romfs/", filename);
    Die(buf);
    return 1;
  }

  // Create window - 3DS top screen
  SDL_Window *window = SDL_CreateWindow(
    kWindowTitle,
    SDL_WINDOWPOS_CENTERED_DISPLAY(0),
    SDL_WINDOWPOS_CENTERED_DISPLAY(0),
    400, 240,
    SDL_WINDOW_SHOWN
  );
  if(window == NULL) {
    printf("Failed to create window: %s\n", SDL_GetError());
    return 1;
  }
  g_window = window;

  g_pixels = linearMemAlign(256 * 256 * 4, 0x80);
  if (!g_pixels)
    Die("Unable to allocate PPU framebuffer");
  memset(g_pixels, 0, 256 * 256 * 4);
  if (!BottomScreen_Init())
    Die("Unable to allocate bottom-screen framebuffer");
  g_gpu_presenter = GpuPresenter_Init();

  // Setup audio
  g_audio_mutex = SDL_CreateMutex();
  if (!g_audio_mutex) Die("No mutex");

  g_spc_player = SpcPlayer_Create();
  SpcPlayer_Initialize(g_spc_player);

  SDL_AudioSpec want = { 0 }, have = { 0 };
  want.freq = kDefaultFreq;
  want.format = AUDIO_S16;
  want.channels = 2;
  want.samples = 2048;
  want.callback = &AudioCallback;
  g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  bool created_dsp_marker = false;
  if (g_audio_device == 0 && strstr(SDL_GetError(), "dspfirm.cdc missing")) {
    created_dsp_marker = CreateEmulatorDspMarker();
    if (created_dsp_marker)
      g_audio_device = SDL_OpenAudioDevice(NULL, 0, &want, &have, 0);
  }
  if (g_audio_device == 0) {
    printf("Failed to open audio device: %s\n", SDL_GetError());
    if (created_dsp_marker)
      remove("sdmc:/3ds/dspfirm.cdc");
  } else {
    g_audio_channels = 2;
    g_frames_per_block = (534 * have.freq) / 32000;
    g_audiobuffer = (uint8 *)malloc(g_frames_per_block * have.channels * sizeof(int16));
  }

  mkdir("saves", 0755);
  RtlReadSram();

  PpuBeginDrawing(snes->snes_ppu, g_pixels, 256 * 4, 0);
  // PpuBeginDrawing(snes->my_ppu, g_my_pixels, 256 * 4, 0);

  RtlReadSram();

  bool running = true;
  uint32 lastTick = SDL_GetTicks();
  uint32 frameCtr = 0;
  uint8 audiopaused = true;
#ifdef SM3DS_PROFILE
  uint32 profileTick = lastTick;
  uint32 profileFrames = 0;
  uint64_t profileGameTicks = 0;
  uint64_t profilePpuTicks = 0;
  uint64_t profileAudioTicks = 0;
  uint64_t profileAudioLockTicks = 0;
  uint64_t profileAudioGenerateTicks = 0;
  uint64_t profileAudioCopyTicks = 0;
  uint64_t profileHdmaTicks = 0;
  uint32_t profileHdmaCalls = 0;
  uint32_t profileHdmaActiveChannels = 0;
  uint32_t profileHdmaBytes = 0;
  uint32_t profileHdmaBbus[256] = {0};
  uint64_t profilePicaPrepareTicks = 0;
  uint32_t profilePicaGpuFrames = 0;
  uint32_t profilePicaCpuFrames = 0;
  uint32_t profilePicaWideGpuFrames = 0;
  uint32_t profilePicaWideCpuFrames = 0;
  uint64_t profilePpuSpriteTicks = 0;
  uint64_t profilePpuMainTicks = 0;
  uint64_t profilePpuSubTicks = 0;
  uint64_t profilePpuComposeTicks = 0;
  uint32_t profilePpuPhaseFrames = 0;
  const uint64_t profileFreq = SDL_GetPerformanceFrequency();
  FILE *profile = fopen("sdmc:/sm3ds-profile.log", "w");
#endif

  printf("Super Metroid starting...\n");
#ifdef SM3DS_DOOR_TRACE
  {
    FILE *door_trace = fopen("sdmc:/sm3ds-door.log", "w");
    if (door_trace) {
      fputs("sm3ds door trace\n", door_trace);
      fclose(door_trace);
    }
  }
#endif

  while (running) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
      switch (event.type) {
      case SDL_JOYBUTTONDOWN:
        HandleCommand(event.jbutton.button, true);
        break;
      case SDL_JOYBUTTONUP:
        HandleCommand(event.jbutton.button, false);
        break;
      case SDL_JOYAXISMOTION:
        HandleCirclePadAxis(event.jaxis.axis, event.jaxis.value);
        break;
      case SDL_FINGERDOWN:
        BottomScreen_HandleTouch(event.tfinger.x, event.tfinger.y);
        break;
      case SDL_FINGERUP:
        BottomScreen_HandleTouchUp(event.tfinger.x, event.tfinger.y);
        break;
      case SDL_QUIT:
        running = false;
        break;
      }
    }

    if (g_paused != audiopaused) {
      audiopaused = g_paused;
      if (g_audio_device)
        SDL_PauseAudioDevice(g_audio_device, audiopaused);
    }

    if (g_paused) {
      SDL_Delay(16);
      continue;
    }

    int inputs = g_input1_state |
        ((g_input1_state & 0xf0) ? 0 : g_gamepad_buttons);
    WideConfig frameViewport = WideConfig_Create(
        BottomScreen_WidescreenEnabled() &&
        game_state >= kGameState_7_MainGameplayFadeIn &&
        game_state <= kGameState_11_LoadingNextRoom);
    RtlSetSpriteViewportMargin(frameViewport.origin_x);
    PpuGpuSetWideConfig(frameViewport);
    uint8 is_replay = RtlRunFrame(inputs);

    frameCtr++;
#ifdef SM3DS_PROFILE
    profileFrames++;
#endif
    g_snes->disableRender = (g_turbo ^ (is_replay & g_replay_turbo)) && (frameCtr & 0xf) != 0;

    bool gpu_frame = false;
    if (g_gpu_presenter)
      gpu_frame = GpuPresenter_DrawTop(g_pixels);
    else if (!g_snes->disableRender)
      DrawPpuFrame();

    bool bottom_updated = BottomScreen_Draw();

    if (gpu_frame) {
      GpuPresenter_DrawBottom(BottomScreen_Pixels(), bottom_updated);
      GpuPresenter_EndFrame();
    } else {
      BottomScreen_CopyToFramebuffer();
      gfxFlushBuffers();
      gfxSwapBuffers();
    }

#ifdef SM3DS_PROFILE
    uint32 profileNow = SDL_GetTicks();
    if (profile && profileNow - profileTick >= 1000) {
      uint64_t gameTicks = g_profile_game_ticks;
      uint64_t ppuTicks = g_profile_ppu_ticks;
      uint64_t audioTicks = g_profile_audio_ticks;
      uint64_t audioLockTicks = g_profile_audio_lock_ticks;
      uint64_t audioGenerateTicks = g_profile_audio_generate_ticks;
      uint64_t audioCopyTicks = g_profile_audio_copy_ticks;
      uint64_t hdmaTicks = g_profile_hdma_ticks;
      uint32_t hdmaCalls = g_profile_hdma_calls;
      uint32_t hdmaActiveChannels = g_profile_hdma_active_channels;
      uint32_t hdmaBytes = g_profile_hdma_bytes;
      uint64_t picaPrepareTicks = g_profile_pica_prepare_ticks;
      uint32_t picaGpuFrames = g_profile_pica_gpu_frames;
      uint32_t picaCpuFrames = g_profile_pica_cpu_frames;
      uint32_t picaWideGpuFrames = g_profile_pica_wide_gpu_frames;
      uint32_t picaWideCpuFrames = g_profile_pica_wide_cpu_frames;
      uint32_t picaSamples = picaGpuFrames - profilePicaGpuFrames;
      if (!picaSamples) picaSamples = 1;
      uint32_t hdmaTopCount = 0;
      unsigned hdmaTopReg = 0;
      for (unsigned reg = 0; reg < 256; reg++) {
        uint32_t count = g_profile_hdma_bbus[reg] - profileHdmaBbus[reg];
        if (count > hdmaTopCount) {
          hdmaTopCount = count;
          hdmaTopReg = reg;
        }
      }
      uint64_t ppuSpriteTicks = g_profile_ppu_sprite_ticks;
      uint64_t ppuMainTicks = g_profile_ppu_main_ticks;
      uint64_t ppuSubTicks = g_profile_ppu_sub_ticks;
      uint64_t ppuComposeTicks = g_profile_ppu_compose_ticks;
      uint32_t ppuPhaseFrames = g_profile_ppu_phase_frames;
      uint32_t ppuPhaseSamples = ppuPhaseFrames - profilePpuPhaseFrames;
      if (ppuPhaseSamples == 0) ppuPhaseSamples = 1;
      fprintf(profile, "ms=%lu fps=%lu game_us=%llu ppu_us=%llu pica_prepare_us=%llu pica_gpu=%lu pica_cpu=%lu pica_vertices=%lu pica_decodes=%lu pica_reason=%s wide=%u wide_width=%u wide_origin=%u wide_gpu=%lu wide_cpu=%lu hdma_us=%llu hdma_calls=%lu hdma_channels=%lu hdma_bytes=%lu hdma_top_reg=%02x hdma_top_count=%lu hdma_scroll=%lu hdma_cgram=%lu hdma_color=%lu audio_us=%llu audio_lock_us=%llu audio_generate_us=%llu audio_copy_us=%llu sprite_us=%llu main_us=%llu sub_us=%llu compose_us=%llu color_maps=%lu fixed_maps=%lu backdrop_maps=%lu halfadd=%lu generic_sub=%lu generic_key=%lu audio_callbacks=%lu audio_nonzero=%lu\n",
              (unsigned long)profileNow,
              (unsigned long)(profileFrames * 1000 / (profileNow - profileTick)),
              (unsigned long long)((gameTicks - profileGameTicks) * 1000000 / profileFreq / profileFrames),
              (unsigned long long)((ppuTicks - profilePpuTicks) * 1000000 / profileFreq / profileFrames),
              (unsigned long long)((picaPrepareTicks - profilePicaPrepareTicks) * 1000000 / profileFreq / picaSamples),
              (unsigned long)(picaGpuFrames - profilePicaGpuFrames),
              (unsigned long)(picaCpuFrames - profilePicaCpuFrames),
              (unsigned long)g_profile_pica_vertices,
              (unsigned long)g_profile_pica_decodes,
              PpuGpuReason(),
              frameViewport.enabled,
              frameViewport.output_width,
              frameViewport.origin_x,
              (unsigned long)(picaWideGpuFrames - profilePicaWideGpuFrames),
              (unsigned long)(picaWideCpuFrames - profilePicaWideCpuFrames),
              (unsigned long long)((hdmaTicks - profileHdmaTicks) * 1000000 / profileFreq / profileFrames),
              (unsigned long)(hdmaCalls - profileHdmaCalls),
              (unsigned long)(hdmaActiveChannels - profileHdmaActiveChannels),
              (unsigned long)(hdmaBytes - profileHdmaBytes),
              hdmaTopReg,
              (unsigned long)hdmaTopCount,
              (unsigned long)((g_profile_hdma_bbus[0x0d] - profileHdmaBbus[0x0d]) +
                              (g_profile_hdma_bbus[0x0e] - profileHdmaBbus[0x0e]) +
                              (g_profile_hdma_bbus[0x0f] - profileHdmaBbus[0x0f]) +
                              (g_profile_hdma_bbus[0x10] - profileHdmaBbus[0x10])),
              (unsigned long)((g_profile_hdma_bbus[0x21] - profileHdmaBbus[0x21]) +
                              (g_profile_hdma_bbus[0x22] - profileHdmaBbus[0x22])),
              (unsigned long)((g_profile_hdma_bbus[0x2c] - profileHdmaBbus[0x2c]) +
                              (g_profile_hdma_bbus[0x2d] - profileHdmaBbus[0x2d]) +
                              (g_profile_hdma_bbus[0x2e] - profileHdmaBbus[0x2e]) +
                              (g_profile_hdma_bbus[0x2f] - profileHdmaBbus[0x2f]) +
                              (g_profile_hdma_bbus[0x30] - profileHdmaBbus[0x30]) +
                              (g_profile_hdma_bbus[0x31] - profileHdmaBbus[0x31]) +
                              (g_profile_hdma_bbus[0x32] - profileHdmaBbus[0x32])),
              (unsigned long long)((audioTicks - profileAudioTicks) * 1000000 / profileFreq / profileFrames),
              (unsigned long long)((audioLockTicks - profileAudioLockTicks) * 1000000 / profileFreq / profileFrames),
              (unsigned long long)((audioGenerateTicks - profileAudioGenerateTicks) * 1000000 / profileFreq / profileFrames),
              (unsigned long long)((audioCopyTicks - profileAudioCopyTicks) * 1000000 / profileFreq / profileFrames),
              (unsigned long long)((ppuSpriteTicks - profilePpuSpriteTicks) * 1000000 / profileFreq / ppuPhaseSamples),
              (unsigned long long)((ppuMainTicks - profilePpuMainTicks) * 1000000 / profileFreq / ppuPhaseSamples),
              (unsigned long long)((ppuSubTicks - profilePpuSubTicks) * 1000000 / profileFreq / ppuPhaseSamples),
              (unsigned long long)((ppuComposeTicks - profilePpuComposeTicks) * 1000000 / profileFreq / ppuPhaseSamples),
              (unsigned long)g_profile_color_map_rebuilds,
              (unsigned long)g_profile_fixed_map_rebuilds,
              (unsigned long)g_profile_backdrop_map_rebuilds,
              (unsigned long)g_profile_halfadd_spans,
              (unsigned long)g_profile_generic_sub_spans,
              (unsigned long)g_profile_generic_key,
              (unsigned long)g_profile_audio_callbacks,
              (unsigned long)g_profile_audio_nonzero);
      fflush(profile);
      profileTick = profileNow;
      profileFrames = 0;
      profileGameTicks = gameTicks;
      profilePpuTicks = ppuTicks;
      profileAudioTicks = audioTicks;
      profileAudioLockTicks = audioLockTicks;
      profileAudioGenerateTicks = audioGenerateTicks;
      profileAudioCopyTicks = audioCopyTicks;
      profileHdmaTicks = hdmaTicks;
      profileHdmaCalls = hdmaCalls;
      profileHdmaActiveChannels = hdmaActiveChannels;
      profileHdmaBytes = hdmaBytes;
      memcpy(profileHdmaBbus, g_profile_hdma_bbus, sizeof(profileHdmaBbus));
      profilePicaPrepareTicks = picaPrepareTicks;
      profilePicaGpuFrames = picaGpuFrames;
      profilePicaCpuFrames = picaCpuFrames;
      profilePicaWideGpuFrames = picaWideGpuFrames;
      profilePicaWideCpuFrames = picaWideCpuFrames;
      profilePpuSpriteTicks = ppuSpriteTicks;
      profilePpuMainTicks = ppuMainTicks;
      profilePpuSubTicks = ppuSubTicks;
      profilePpuComposeTicks = ppuComposeTicks;
      profilePpuPhaseFrames = ppuPhaseFrames;
    }
#endif

    // Frame delay for 60 fps
    static const uint8 delays[3] = { 17, 17, 16 };
    lastTick += delays[frameCtr % 3];
    uint32 curTick = SDL_GetTicks();

    if (lastTick > curTick) {
      uint32 delta = lastTick - curTick;
      if (delta > 500) {
        lastTick = curTick - 500;
        delta = 500;
      }
      SDL_Delay(delta);
    } else if (curTick - lastTick > 500) {
      lastTick = curTick;
    }
  }

  // Cleanup
  SDL_PauseAudioDevice(g_audio_device, 1);
  SDL_CloseAudioDevice(g_audio_device);
  SDL_DestroyMutex(g_audio_mutex);
  free(g_audiobuffer);
  GpuPresenter_Fini();
  BottomScreen_Fini();
  linearFree(g_pixels);
  SDL_DestroyWindow(window);
  SDL_Quit();
#ifdef SM3DS_PROFILE
  if (profile)
    fclose(profile);
#endif

  return 0;
}
