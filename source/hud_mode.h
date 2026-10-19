#pragma once

/* Persisted values: append modes rather than renumbering them. */
enum MainHudMode {
  kMainHudFull,
  kMainHudEnergy,
  kMainHudEnergyMap,
  kMainHudHidden,
  kMainHudBlackBand,
  kMainHudModeCount
};

static inline int MainHudExpands(enum MainHudMode mode) {
  return mode >= kMainHudEnergy && mode <= kMainHudHidden;
}

/* Native BG3 coordinates, before widescreen's horizontal origin. */
static inline int MainHudKeepsX(enum MainHudMode mode, int x) {
  return mode == kMainHudFull ||
      ((mode == kMainHudEnergy || mode == kMainHudEnergyMap) &&
       ((x >= 8 && x < 80) || (mode == kMainHudEnergyMap && x >= 208 && x < 248)));
}

static inline int MainHudKeepsPixel(enum MainHudMode mode,int x,int y) {
  return MainHudKeepsX(mode,x) &&
      (x < 208 || mode == kMainHudFull || (y >= 7 && y < 31));
}
