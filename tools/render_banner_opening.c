/* Render the opening cue from a user-supplied LoROM with our native SPC
 * player. No ROM bytes are included in this tool. Output: 32 kHz stereo PCM. */
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "spc_player.h"

static unsigned offset(unsigned address) {
  return ((address >> 16) & 0x7f) * 0x8000 + (address & 0x7fff);
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: %s user-rom.sfc opening.pcm\n", argv[0]);
    return 1;
  }
  FILE *f = fopen(argv[1], "rb");
  if (!f) return 1;
  fseek(f, 0, SEEK_END);
  long size = ftell(f);
  unsigned header = size % 0x8000 == 512 ? 512 : 0;
  if (size - header < 0x300000) { fclose(f); return 1; }
  uint8_t *rom = malloc(size - header);
  if (!rom) { fclose(f); return 1; }
  fseek(f, header, SEEK_SET);
  if (fread(rom, 1, size - header, f) != (size_t)(size - header)) return 1;
  fclose(f);
  SpcPlayer *p = SpcPlayer_Create();
  SpcPlayer_Initialize(p);
  SpcPlayer_Upload(p, rom + offset(0xcf8000));
  const uint8_t *ptr = rom + offset(0x8fe7e1) + 3;
  unsigned address = ptr[0] | (ptr[1] << 8) | (ptr[2] << 16);
  if (offset(address) >= (unsigned)(size - header)) return 1;
  SpcPlayer_Upload(p, rom + offset(address));
  p->input_ports[0] = 5; /* Opening, before the main title theme (cue 6). */
  f = fopen(argv[2], "wb");
  if (!f) return 1;
  int16_t samples[534 * 2];
  for (unsigned frame = 0; frame < 180; frame++) {
    SpcPlayer_GenerateSamples(p);
    dsp_getSamples(p->dsp, samples, 534);
    if (fwrite(samples, sizeof(samples), 1, f) != 1) return 1;
  }
  return fclose(f) != 0;
}
