#include "../source/ppu_gpu_vertices.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void CheckTriangles(const uint16_t *indices, PicaQuad q) {
  PicaVertex indexed[4];
  PicaQuadVertices(indexed, &q);
  int16_t depth = q.depth >> 1;
  if (!depth) depth = 1;
  PicaVertex a = {q.x0,q.y0,depth,1,q.u0,q.v0,q.r,q.g,q.b,q.a};
  PicaVertex b = a, c = a, d = a;
  b.x = d.x = q.x1; b.u = d.u = q.u1;
  c.y = d.y = q.y1; c.v = d.v = q.v1;
  PicaVertex previous[6] = {a,b,c,c,b,d};
  for (unsigned i = 0; i < 6; i++)
    assert(!memcmp(&indexed[indices[i]], &previous[i], sizeof(PicaVertex)));
}

int main(void) {
  uint16_t *indices = malloc(PICA_INDEX_COUNT * sizeof(*indices));
  assert(indices);
  PicaQuadIndices(indices);
  CheckTriangles(indices, (PicaQuad){-72,0,328,224,0,4096,3200,512,1,1,2,3,127});
  CheckTriangles(indices, (PicaQuad){72,9,80,17,1024,64,992,128,0xffff,255,77,13,255});
  /* Every chunk/group starts at its own vertex base. No index can escape the
   * chunk, including the last partial draw at the original quad capacity. */
  const unsigned groups[] = {0,4,28,PICA_INDEX_QUADS*4,PICA_INDEX_QUADS*4+4,PICA_MAX_VERTICES};
  for (unsigned g = 0; g < sizeof(groups) / sizeof(groups[0]); g++) {
    unsigned first = 0, left = groups[g], triangles = 0;
    while (left) {
      unsigned count = PicaDrawChunkVertices(left);
      assert(count && count % 4 == 0 && count <= left);
      unsigned nr = count / 4 * 6;
      assert(nr <= 32766);
      for (unsigned i = 0; i < nr; i++) assert(indices[i] < count);
      triangles += nr / 3;
      first += count;
      left -= count;
    }
    assert(first == groups[g] && triangles == groups[g] / 2);
  }
  assert(PICA_MAX_VERTICES / 4 == 262140 / 6);
  free(indices);
  puts("Indexed quads retain triangle bytes, winding, UV/depth/color and draw capacity.");
}
