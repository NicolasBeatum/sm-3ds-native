#pragma once

#include "ppu_gpu_model.h"

typedef struct PicaVertex {
  int16_t x, y, z, w, u, v;
  uint8_t r, g, b, a;
} PicaVertex;
_Static_assert(sizeof(PicaVertex) == 16, "GPU vertex layout");

enum { PICA_INDEX_QUADS = 5461, PICA_INDEX_COUNT = PICA_INDEX_QUADS * 6 };

static inline unsigned PicaDrawChunkVertices(unsigned remaining) {
  unsigned quads = remaining / 4;
  return (quads > PICA_INDEX_QUADS ? PICA_INDEX_QUADS : quads) * 4;
}

/* The same two triangles as the previous six-vertex stream: a,b,c; c,b,d.
 * Keep their diagonal, winding and interpolation while sharing corners. */
static inline void PicaQuadVertices(PicaVertex *v, const PicaQuad *q) {
  int16_t depth = q->depth >> 1;
  if (!depth) depth = 1;
  PicaVertex a = {q->x0,q->y0,depth,1,q->u0,q->v0,q->r,q->g,q->b,q->a};
  v[0] = v[1] = v[2] = v[3] = a;
  v[1].x = v[3].x = q->x1; v[1].u = v[3].u = q->u1;
  v[2].y = v[3].y = q->y1; v[2].v = v[3].v = q->v1;
}

static inline void PicaQuadIndices(uint16_t *indices) {
  for (unsigned i = 0; i < PICA_INDEX_QUADS; i++) {
    unsigned base = i * 4;
    uint16_t *q = indices + i * 6;
    q[0] = base; q[1] = base + 1; q[2] = base + 2;
    q[3] = base + 2; q[4] = base + 1; q[5] = base + 3;
  }
}
