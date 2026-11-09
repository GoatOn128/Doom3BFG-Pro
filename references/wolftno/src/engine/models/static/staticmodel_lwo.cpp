#include "engine/models/static/staticmodel_lwo.h"

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/str.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr unsigned int LWID(char a, char b, char c, char d) {
  return (static_cast<unsigned int>(static_cast<unsigned char>(a)) << 24) |
         (static_cast<unsigned int>(static_cast<unsigned char>(b)) << 16) |
         (static_cast<unsigned int>(static_cast<unsigned char>(c)) << 8) |
         static_cast<unsigned int>(static_cast<unsigned char>(d));
}
constexpr unsigned int ID_GRAD = LWID('G', 'R', 'A', 'D');
constexpr unsigned int ID_IMAP = LWID('I', 'M', 'A', 'P');
constexpr unsigned int ID_PROC = LWID('P', 'R', 'O', 'C');
constexpr unsigned int ID_STIL = LWID('S', 'T', 'I', 'L');
constexpr unsigned int ID_ISEQ = LWID('I', 'S', 'E', 'Q');
constexpr unsigned int ID_ANIM = LWID('A', 'N', 'I', 'M');
constexpr unsigned int ID_XREF = LWID('X', 'R', 'E', 'F');
constexpr unsigned int ID_STCC = LWID('S', 'T', 'C', 'C');
constexpr unsigned int ID_ADTR = LWID('A', 'D', 'T', 'R');
constexpr unsigned int ID_ALPH = LWID('A', 'L', 'P', 'H');
constexpr unsigned int ID_AVAL = LWID('A', 'V', 'A', 'L');
constexpr unsigned int ID_BLOK = LWID('B', 'L', 'O', 'K');
constexpr unsigned int ID_BUMP = LWID('B', 'U', 'M', 'P');
constexpr unsigned int ID_CLRF = LWID('C', 'L', 'R', 'F');
constexpr unsigned int ID_CLRH = LWID('C', 'L', 'R', 'H');
constexpr unsigned int ID_COLR = LWID('C', 'O', 'L', 'R');
constexpr unsigned int ID_DIFF = LWID('D', 'I', 'F', 'F');
constexpr unsigned int ID_GLOS = LWID('G', 'L', 'O', 'S');
constexpr unsigned int ID_GVAL = LWID('G', 'V', 'A', 'L');
constexpr unsigned int ID_LINE = LWID('L', 'I', 'N', 'E');
constexpr unsigned int ID_LUMI = LWID('L', 'U', 'M', 'I');
constexpr unsigned int ID_REFL = LWID('R', 'E', 'F', 'L');
constexpr unsigned int ID_RFOP = LWID('R', 'F', 'O', 'P');
constexpr unsigned int ID_RIMG = LWID('R', 'I', 'M', 'G');
constexpr unsigned int ID_RIND = LWID('R', 'I', 'N', 'D');
constexpr unsigned int ID_RSAN = LWID('R', 'S', 'A', 'N');
constexpr unsigned int ID_SHDR = LWID('S', 'H', 'D', 'R');
constexpr unsigned int ID_SHRP = LWID('S', 'H', 'R', 'P');
constexpr unsigned int ID_SIDE = LWID('S', 'I', 'D', 'E');
constexpr unsigned int ID_SMAN = LWID('S', 'M', 'A', 'N');
constexpr unsigned int ID_SPEC = LWID('S', 'P', 'E', 'C');
constexpr unsigned int ID_TIMG = LWID('T', 'I', 'M', 'G');
constexpr unsigned int ID_TRAN = LWID('T', 'R', 'A', 'N');
constexpr unsigned int ID_TRNL = LWID('T', 'R', 'N', 'L');
constexpr unsigned int ID_TROP = LWID('T', 'R', 'O', 'P');

int flen = 0;

void* LwoAlloc(std::size_t size, bool zero = true) {
  if (size == 0)
    return nullptr;
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\static\\StaticModel_lwo.cpp(38) : TAG_RENDERER",
      size, TAG_RENDERER, zero, ALIGN_16, HEAP_DEFAULTHEAP);
}

void FreeTextureList(st_lwTexture* texture) {
  while (texture != nullptr) {
    st_lwTexture* next = texture->next;
    lwFreeTexture(texture);
    texture = next;
  }
}

template <typename T> __forceinline void AppendNode(T** list, T* node) {
  if (*list == nullptr) {
    *list = node;
    return;
  }
  T* tail = *list;
  while (tail->next != nullptr)
    tail = tail->next;
  tail->next = node;
  node->prev = tail;
}

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8) | (value >> 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value << 24) | ((value << 8) & 0x00FF0000u) |
         ((value >> 8) & 0x0000FF00u) | (value >> 24);
}

} // namespace

void lwFree(void* pointer) { idMem::Singleton()->Free(pointer, ALIGN_16); }

template <typename T> T* lwAlloc() {
  return static_cast<T*>(LwoAlloc(sizeof(T), true));
}

template <typename T> T* lwAllocArray(int count) {
  if (count <= 0)
    return nullptr;
  return static_cast<T*>(LwoAlloc(sizeof(T) * static_cast<std::size_t>(count), true));
}

template st_lwPlugin* lwAlloc<st_lwPlugin>();
template st_lwKey* lwAlloc<st_lwKey>();
template st_lwObject* lwAlloc<st_lwObject>();
template st_lwLayer* lwAlloc<st_lwLayer>();
template int* lwAllocArray<int>(int);
template float** lwAllocArray<float*>(int);
template float* lwAllocArray<float>(int);

int compare_keys(st_lwKey* first, st_lwKey* second) {
  if (first->time > second->time)
    return 1;
  if (first->time < second->time)
    return -1;
  return 0;
}

void lwListFree(void* list, void (*freeNode)(void*)) {
  while (list != nullptr) {
    void* next = *static_cast<void**>(list);
    freeNode(list);
    list = next;
  }
}

void lwListInsert(void** list, void* item, int (*compare)(void*, void*)) {
  auto** link = static_cast<void**>(item);
  void** current = static_cast<void**>(*list);
  if (current == nullptr) {
    *list = item;
    return;
  }
  void** previous = nullptr;
  while (current != nullptr) {
    if (compare(current, item) > 0)
      break;
    previous = current;
    current = static_cast<void**>(*current);
  }
  if (previous != nullptr) {
    *previous = item;
    link[1] = previous;
    if (current != nullptr) {
      *link = current;
      current[1] = item;
    }
  } else {
    *list = item;
    *link = current;
    current[1] = item;
  }
}

unsigned char getU1(idFile* file) {
  if (flen == -9999)
    return 0;
  unsigned char value = 0;
  if (file->Read(&value, 1) != 1) {
    flen = -9999;
    return 0;
  }
  ++flen;
  return value;
}

int getVX(idFile* file) {
  if (flen == -9999)
    return 0;
  unsigned char first = 0;
  if (file->Read(&first, 1) != 1)
    return 0;
  if (first != 0xFF) {
    unsigned char second = 0;
    if (file->Read(&second, 1) == 1) {
      flen += 2;
      return (static_cast<int>(first) << 8) | second;
    }
  } else {
    unsigned char bytes[3]{};
    if (file->Read(bytes, 3) == 3) {
      flen += 4;
      return (static_cast<int>(bytes[0]) << 16) |
             (static_cast<int>(bytes[1]) << 8) | bytes[2];
    }
  }
  return 0;
}

void lwFreePolygons(st_lwPolygonList* polygons) {
  if (polygons == nullptr)
    return;
  if (polygons->pol != nullptr) {
    if (polygons->polVertVMaps != nullptr) {
      for (int polygon = 0; polygon < polygons->count; ++polygon) {
        for (int vertex = 0; vertex < polygons->pol[polygon].nverts; ++vertex)
          lwFree(polygons->polVertVMaps[polygons->pol[polygon].vindex + vertex].vm);
      }
      lwFree(polygons->polVertVMaps);
    }
    lwFree(polygons->polVerts);
    lwFree(polygons->pol);
  }
  std::memset(polygons, 0, sizeof(*polygons));
}

void lwGetBoundingBox(st_lwPointList* points, float bbox[6]) {
  if (points->count == 0)
    return;
  for (int index = 0; index < 6; ++index)
    if (bbox[index] != 0.0f)
      return;
  bbox[0] = bbox[1] = bbox[2] = 1.0e20f;
  bbox[3] = bbox[4] = bbox[5] = -1.0e20f;
  for (int point = 0; point < points->count; ++point) {
    for (int axis = 0; axis < 3; ++axis) {
      bbox[axis] = std::min(bbox[axis], points->pt[point].pos[axis]);
      bbox[axis + 3] = std::max(bbox[axis + 3], points->pt[point].pos[axis]);
    }
  }
}

void lwFreePointPolygons(st_lwPointList* points) {
  if (points == nullptr)
    return;
  lwFree(points->ptPoly);
  points->ptPoly = nullptr;
  lwFree(points->pointPolyIndices);
  points->pointPolyIndices = nullptr;
}

void lwFreeTags(st_lwTagList* tags) {
  if (tags == nullptr)
    return;
  for (int index = 0; index < tags->count; ++index)
    lwFree(tags->tag[index]);
  lwFree(tags->tag);
  std::memset(tags, 0, sizeof(*tags));
}

void lwFreePlugin(st_lwPlugin* plugin) {
  if (plugin == nullptr)
    return;
  lwFree(plugin->ord);
  lwFree(plugin->name);
  lwFree(plugin->data);
  lwFree(plugin);
}

void lwFreeTexture(st_lwTexture* texture) {
  if (texture == nullptr)
    return;
  lwFree(texture->ord);
  if (texture->type == ID_GRAD) {
    lwFree(texture->param.grad.key);
    lwFree(texture->param.grad.ikey);
  } else if (texture->type == ID_IMAP) {
    lwFree(texture->param.imap.vmap_name);
  } else if (texture->type == ID_PROC) {
    lwFree(texture->param.proc.name);
    lwFree(texture->param.proc.data);
  }
  lwFree(texture->tmap.ref_object);
  lwFree(texture);
}

void lwFreeSurface(st_lwSurface* surface) {
  if (surface == nullptr)
    return;
  lwFree(surface->name);
  lwFree(surface->srcname);
  st_lwPlugin* shader = surface->shader;
  while (shader != nullptr) {
    st_lwPlugin* next = shader->next;
    lwFreePlugin(shader);
    shader = next;
  }
  FreeTextureList(surface->color.tex);
  FreeTextureList(surface->luminosity.tex);
  FreeTextureList(surface->diffuse.tex);
  FreeTextureList(surface->specularity.tex);
  FreeTextureList(surface->glossiness.tex);
  FreeTextureList(surface->reflection.val.tex);
  FreeTextureList(surface->transparency.val.tex);
  FreeTextureList(surface->eta.tex);
  FreeTextureList(surface->translucency.tex);
  FreeTextureList(surface->bump.tex);
  lwFree(surface);
}

int compare_shaders(st_lwPlugin* first, st_lwPlugin* second) {
  return std::strcmp(first->ord, second->ord);
}

int add_texture(st_lwSurface* surface, st_lwTexture* texture) {
  st_lwTexture** destination = nullptr;
  switch (texture->chan) {
  case LWID('C', 'O', 'L', 'R'): destination = &surface->color.tex; break;
  case LWID('L', 'U', 'M', 'I'): destination = &surface->luminosity.tex; break;
  case LWID('D', 'I', 'F', 'F'): destination = &surface->diffuse.tex; break;
  case LWID('S', 'P', 'E', 'C'): destination = &surface->specularity.tex; break;
  case LWID('G', 'L', 'O', 'S'): destination = &surface->glossiness.tex; break;
  case LWID('R', 'E', 'F', 'L'): destination = &surface->reflection.val.tex; break;
  case LWID('T', 'R', 'A', 'N'): destination = &surface->transparency.val.tex; break;
  case LWID('R', 'I', 'N', 'D'): destination = &surface->eta.tex; break;
  case LWID('T', 'R', 'N', 'L'): destination = &surface->translucency.tex; break;
  case LWID('B', 'U', 'M', 'P'): destination = &surface->bump.tex; break;
  default: return 0;
  }
  st_lwTexture* current = *destination;
  st_lwTexture* previous = nullptr;
  while (current != nullptr) {
    if (current->ord == nullptr || texture->ord == nullptr ||
        std::strcmp(current->ord, texture->ord) > 0)
      break;
    previous = current;
    current = current->next;
  }
  if (previous == nullptr) {
    *destination = texture;
    texture->next = current;
    if (current != nullptr)
      current->prev = texture;
  } else {
    previous->next = texture;
    texture->prev = previous;
    texture->next = current;
    if (current != nullptr)
      current->prev = texture;
  }
  return 1;
}

void lwFreeVMap(st_lwVMap* vmap) {
  if (vmap == nullptr)
    return;
  lwFree(vmap->name);
  lwFree(vmap->vindex);
  lwFree(vmap->pindex);
  if (vmap->val != nullptr)
    lwFree(vmap->val[0]);
  lwFree(vmap->val);
  lwFree(vmap);
}

void lwFreeClip(st_lwClip* clip) {
  if (clip == nullptr)
    return;
  st_lwPlugin* plugin = clip->ifilter;
  while (plugin != nullptr) {
    st_lwPlugin* next = plugin->next;
    lwFreePlugin(plugin);
    plugin = next;
  }
  plugin = clip->pfilter;
  while (plugin != nullptr) {
    st_lwPlugin* next = plugin->next;
    lwFreePlugin(plugin);
    plugin = next;
  }
  if (clip->type == ID_ANIM || clip->type == ID_ISEQ)
    lwFree(clip->source.seq.suffix);
  else if (clip->type == ID_STCC)
    lwFree(clip->source.still.name);
  else if (clip->type == ID_STIL || clip->type == ID_XREF)
    lwFree(clip->source.still.name);
  lwFree(clip);
}

void lwFreeEnvelope(st_lwEnvelope* envelope) {
  if (envelope == nullptr)
    return;
  lwFree(envelope->name);
  st_lwKey* key = envelope->key;
  while (key != nullptr) {
    st_lwKey* next = key->next;
    lwFree(key);
    key = next;
  }
  st_lwPlugin* plugin = envelope->cfilter;
  while (plugin != nullptr) {
    st_lwPlugin* next = plugin->next;
    lwFreePlugin(plugin);
    plugin = next;
  }
  lwFree(envelope);
}

void* getbytes(idFile* file, int size) {
  if (flen == -9999)
    return nullptr;
  if (size < 0) {
    flen = -9999;
    return nullptr;
  }
  void* bytes = LwoAlloc(static_cast<std::size_t>(size), true);
  if (bytes != nullptr && file->Read(bytes, static_cast<std::size_t>(size)) ==
                              static_cast<std::size_t>(size)) {
    flen += size;
    return bytes;
  }
  flen = -9999;
  lwFree(bytes);
  return nullptr;
}

short getI2(idFile* file) {
  if (flen == -9999)
    return 0;
  std::uint16_t value = 0;
  if (file->Read(&value, sizeof(value)) != sizeof(value)) {
    flen = -9999;
    return 0;
  }
  flen += 2;
  return static_cast<short>(ByteSwap16(value));
}

int getI4(idFile* file) {
  if (flen == -9999)
    return 0;
  std::uint32_t value = 0;
  if (file->Read(&value, sizeof(value)) != sizeof(value)) {
    flen = -9999;
    return 0;
  }
  flen += 4;
  return static_cast<int>(ByteSwap32(value));
}

float getF4(idFile* file) {
  const std::uint32_t bits = static_cast<std::uint32_t>(getI4(file));
  if ((bits & 0x7F800000u) == 0 && (bits & 0x007FFFFFu) != 0)
    return 0.0f;
  float value = 0.0f;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

char* getS0(idFile* file) {
  if (flen == -9999)
    return nullptr;
  const std::int64_t start = file->Tell();
  int length = 1;
  unsigned char value = 0;
  if (file->Read(&value, 1) != 1) {
    flen = -9999;
    return nullptr;
  }
  while (value != 0) {
    ++length;
    if (file->Read(&value, 1) != 1) {
      flen = -9999;
      return nullptr;
    }
  }
  const int padded = length + (length & 1);
  char* text = static_cast<char*>(LwoAlloc(static_cast<std::size_t>(padded), true));
  if (text == nullptr || file->Seek(start, FS_SEEK_SET) != 0 ||
      file->Read(text, static_cast<std::size_t>(padded)) != static_cast<std::size_t>(padded)) {
    flen = -9999;
    lwFree(text);
    return nullptr;
  }
  flen += padded;
  return text;
}

char* sgetS0(unsigned char** bytes) {
  if (flen == -9999)
    return nullptr;
  const int length = idStr::Length(reinterpret_cast<const char*>(*bytes)) + 1;
  const int padded = length + (length & 1);
  char* text = static_cast<char*>(LwoAlloc(static_cast<std::size_t>(padded), true));
  if (text == nullptr) {
    flen = -9999;
    return nullptr;
  }
  std::memmove(text, *bytes, static_cast<std::size_t>(padded));
  *bytes += padded;
  flen += padded;
  return text;
}

int add_clip(char* text, st_lwClip** clips, int* clipCount) {
  auto* clip = static_cast<st_lwClip*>(LwoAlloc(sizeof(st_lwClip), true));
  if (clip == nullptr)
    return 0;
  clip->contrast.val = 1.0f;
  clip->brightness.val = 1.0f;
  clip->saturation.val = 1.0f;
  clip->gamma.val = 1.0f;
  if (char* sequence = std::strstr(text, "(sequence)")) {
    sequence[-1] = '\0';
    clip->type = ID_ISEQ;
    clip->source.seq.prefix = text;
    clip->source.seq.digits = 3;
  } else {
    clip->type = ID_STIL;
    clip->source.still.name = text;
  }
  clip->index = ++*clipCount;
  if (*clips == nullptr) {
    *clips = clip;
  } else {
    st_lwClip* tail = *clips;
    while (tail->next != nullptr)
      tail = tail->next;
    tail->next = clip;
    clip->prev = tail;
  }
  return clip->index;
}

int add_tvel(float position[3], float velocity[3], st_lwEnvelope** envelopes,
             int* envelopeCount) {
  st_lwEnvelope* last = nullptr;
  for (int axis = 0; axis < 3; ++axis) {
    auto* envelope = static_cast<st_lwEnvelope*>(LwoAlloc(sizeof(st_lwEnvelope), true));
    auto* key0 = static_cast<st_lwKey*>(LwoAlloc(sizeof(st_lwKey), true));
    auto* key1 = static_cast<st_lwKey*>(LwoAlloc(sizeof(st_lwKey), true));
    if (envelope == nullptr || key0 == nullptr || key1 == nullptr)
      return 0;
    key0->next = key1;
    key0->value = position[axis];
    key1->prev = key0;
    key1->value = position[axis] + velocity[axis] * 30.0f;
    key1->time = 1.0f;
    key0->shape = key1->shape = LWID('L', 'I', 'N', 'E');
    envelope->index = *envelopeCount + axis + 1;
    envelope->type = 0x0301 + axis;
    envelope->name = static_cast<char*>(LwoAlloc(11, true));
    if (envelope->name != nullptr) {
      std::memcpy(envelope->name, "Position.X", 11);
      envelope->name[9] = static_cast<char>(envelope->name[9] + axis);
    }
    envelope->key = key0;
    envelope->nkeys = 2;
    envelope->behavior[0] = envelope->behavior[1] = 5;
    if (*envelopes == nullptr) {
      *envelopes = envelope;
    } else {
      st_lwEnvelope* tail = *envelopes;
      while (tail->next != nullptr)
        tail = tail->next;
      tail->next = envelope;
      envelope->prev = tail;
    }
    last = envelope;
  }
  *envelopeCount += 3;
  return last->index - 2;
}

st_lwTexture* get_texture(char* text) {
  auto* texture = static_cast<st_lwTexture*>(LwoAlloc(sizeof(st_lwTexture), true));
  if (texture == nullptr)
    return nullptr;
  texture->tmap.size.val[0] = texture->tmap.size.val[1] =
      texture->tmap.size.val[2] = 1.0f;
  texture->opacity.val = 1.0f;
  texture->enabled = 1;
  if (std::strstr(text, "Image Map") != nullptr) {
    texture->type = ID_IMAP;
    if (std::strstr(text, "Planar") != nullptr) texture->param.imap.projection = 0;
    else if (std::strstr(text, "Cylindrical") != nullptr) texture->param.imap.projection = 1;
    else if (std::strstr(text, "Spherical") != nullptr) texture->param.imap.projection = 2;
    else if (std::strstr(text, "Cubic") != nullptr) texture->param.imap.projection = 3;
    else if (std::strstr(text, "Front") != nullptr) texture->param.imap.projection = 4;
    texture->param.imap.aa_strength = 1.0f;
    texture->param.imap.amplitude.val = 1.0f;
    lwFree(text);
  } else {
    texture->type = ID_PROC;
    texture->param.proc.name = text;
  }
  return texture;
}

void lwFreePoints(st_lwPointList* points) {
  if (points == nullptr)
    return;
  lwFree(points->pt);
  if (points->ptVMaps != nullptr) {
    for (int index = 0; index < points->count; ++index)
      lwFree(points->ptVMaps[index].vm);
    lwFree(points->ptVMaps);
  }
  lwFreePointPolygons(points);
  std::memset(points, 0, sizeof(*points));
}

int lwAllocPolygons(st_lwPolygonList* polygons, int polygonCount,
                    int vertexCount) {
  const int oldPolygonCount = polygons->count;
  st_lwPolygon* oldPolygons = polygons->pol;
  polygons->offset = oldPolygonCount;
  polygons->count += polygonCount;
  polygons->pol = static_cast<st_lwPolygon*>(
      LwoAlloc(sizeof(st_lwPolygon) * static_cast<std::size_t>(polygons->count), true));
  if (polygons->pol == nullptr)
    return 0;
  if (oldPolygons != nullptr) {
    std::memmove(polygons->pol, oldPolygons,
                 sizeof(st_lwPolygon) * static_cast<std::size_t>(oldPolygonCount));
    lwFree(oldPolygons);
  }

  const int oldVertexCount = polygons->vcount;
  st_lwPolVert* oldVertices = polygons->polVerts;
  polygons->voffset = oldVertexCount;
  polygons->vcount += vertexCount;
  polygons->polVerts = static_cast<st_lwPolVert*>(
      LwoAlloc(sizeof(st_lwPolVert) * static_cast<std::size_t>(polygons->vcount), true));
  if (polygons->polVerts == nullptr)
    return 0;
  if (oldVertices != nullptr) {
    std::memmove(polygons->polVerts, oldVertices,
                 sizeof(st_lwPolVert) * static_cast<std::size_t>(oldVertexCount));
    lwFree(oldVertices);
  }
  for (int index = 1; index < polygons->offset; ++index)
    polygons->pol[index].vindex =
        polygons->pol[index - 1].vindex + polygons->pol[index - 1].nverts;
  return 1;
}

void lwGetPolyNormals(st_lwPointList* points, st_lwPolygonList* polygons) {
  for (int index = 0; index < polygons->count; ++index) {
    st_lwPolygon& polygon = polygons->pol[index];
    if (polygon.nverts < 3)
      continue;
    const st_lwPoint& first = points->pt[polygons->polVerts[polygon.vindex].index];
    const st_lwPoint& second = points->pt[polygons->polVerts[polygon.vindex + 1].index];
    const st_lwPoint& last =
        points->pt[polygons->polVerts[polygon.vindex + polygon.nverts - 1].index];
    const float firstEdge[3] = {second.pos[0] - first.pos[0],
                                second.pos[1] - first.pos[1],
                                second.pos[2] - first.pos[2]};
    const float lastEdge[3] = {last.pos[0] - first.pos[0],
                               last.pos[1] - first.pos[1],
                               last.pos[2] - first.pos[2]};
    polygon.norm[0] = firstEdge[1] * lastEdge[2] - firstEdge[2] * lastEdge[1];
    polygon.norm[1] = firstEdge[2] * lastEdge[0] - firstEdge[0] * lastEdge[2];
    polygon.norm[2] = firstEdge[0] * lastEdge[1] - firstEdge[1] * lastEdge[0];
    const float length = std::sqrt(std::max(
        polygon.norm[0] * polygon.norm[0] + polygon.norm[1] * polygon.norm[1] +
            polygon.norm[2] * polygon.norm[2],
        1.17549435e-38f));
    polygon.norm[0] /= length;
    polygon.norm[1] /= length;
    polygon.norm[2] /= length;
  }
}

int lwGetPointPolygons(st_lwPointList* points, st_lwPolygonList* polygons) {
  points->ptPoly = static_cast<st_lwPointPoly*>(
      LwoAlloc(sizeof(st_lwPointPoly) * static_cast<std::size_t>(points->count), true));
  if (points->ptPoly == nullptr && points->count > 0)
    return 0;
  int references = 0;
  for (int polygon = 0; polygon < polygons->count; ++polygon) {
    for (int vertex = 0; vertex < polygons->pol[polygon].nverts; ++vertex) {
      const int pointIndex =
          polygons->polVerts[polygons->pol[polygon].vindex + vertex].index;
      ++points->ptPoly[pointIndex].npols;
      ++references;
    }
  }
  if (references == 0)
    return 1;
  points->pointPolyIndices = static_cast<int*>(
      LwoAlloc(sizeof(int) * static_cast<std::size_t>(references), true));
  if (points->pointPolyIndices == nullptr)
    return 0;
  int offset = 0;
  for (int point = 0; point < points->count; ++point) {
    if (points->ptPoly[point].npols != 0) {
      points->ptPoly[point].pol = points->pointPolyIndices + offset;
      offset += points->ptPoly[point].npols;
      points->ptPoly[point].npols = 0;
    }
  }
  for (int polygon = 0; polygon < polygons->count; ++polygon) {
    for (int vertex = 0; vertex < polygons->pol[polygon].nverts; ++vertex) {
      const int pointIndex =
          polygons->polVerts[polygons->pol[polygon].vindex + vertex].index;
      st_lwPointPoly& point = points->ptPoly[pointIndex];
      point.pol[point.npols++] = polygon;
    }
  }
  return 1;
}

void lwForceSmoothSurfaces(st_lwSurface* surfaces, float smooth) {
  if (smooth <= 0.0f)
    return;
  for (st_lwSurface* surface = surfaces; surface != nullptr; surface = surface->next) {
    if (surface->smooth <= 0.0f) {
      surface->smooth = smooth;
      surface->smoothCos = std::cos(smooth);
    }
  }
}

void lwGetVertNormals(st_lwPointList* points, st_lwPolygonList* polygons) {
  for (int polygonIndex = 0; polygonIndex < polygons->count; ++polygonIndex) {
    st_lwPolygon& polygon = polygons->pol[polygonIndex];
    for (int vertex = 0; vertex < polygon.nverts; ++vertex) {
      st_lwPolVert& polygonVertex = polygons->polVerts[polygon.vindex + vertex];
      std::memcpy(polygonVertex.norm, polygon.norm, sizeof(polygon.norm));
      if (polygon.surf->smooth <= 0.0f || polygon.smoothgrp == 511)
        continue;
      const st_lwPointPoly& shared = points->ptPoly[polygonVertex.index];
      for (int sharedIndex = 0; sharedIndex < shared.npols; ++sharedIndex) {
        const int adjacentIndex = shared.pol[sharedIndex];
        if (adjacentIndex == polygonIndex)
          continue;
        const st_lwPolygon& adjacent = polygons->pol[adjacentIndex];
        const float dot = polygon.norm[0] * adjacent.norm[0] +
                          polygon.norm[1] * adjacent.norm[1] +
                          polygon.norm[2] * adjacent.norm[2];
        if (polygon.smoothgrp == adjacent.smoothgrp && polygon.surf->smoothCos < dot) {
          polygonVertex.norm[0] += adjacent.norm[0];
          polygonVertex.norm[1] += adjacent.norm[1];
          polygonVertex.norm[2] += adjacent.norm[2];
        }
      }
      const float length = std::sqrt(std::max(
          polygonVertex.norm[0] * polygonVertex.norm[0] +
              polygonVertex.norm[1] * polygonVertex.norm[1] +
              polygonVertex.norm[2] * polygonVertex.norm[2],
          1.17549435e-38f));
      polygonVertex.norm[0] /= length;
      polygonVertex.norm[1] /= length;
      polygonVertex.norm[2] /= length;
    }
  }
}

namespace {
unsigned short BufferU2(unsigned char** bytes) {
  const unsigned short value =
      static_cast<unsigned short>(((*bytes)[0] << 8) | (*bytes)[1]);
  *bytes += 2;
  flen += 2;
  return value;
}

int BufferVX(unsigned char** bytes) {
  if ((*bytes)[0] == 0xFF) {
    const int value = ((*bytes)[1] << 16) | ((*bytes)[2] << 8) | (*bytes)[3];
    *bytes += 4;
    flen += 4;
    return value;
  }
  return BufferU2(bytes);
}
} // namespace

int lwGetPolygons(idFile* file, int chunkSize, st_lwPolygonList* polygons,
                  int pointOffset) {
  if (chunkSize == 0)
    return 1;
  flen = 0;
  const unsigned int type = static_cast<unsigned int>(getI4(file));
  auto* buffer = static_cast<unsigned char*>(getbytes(file, chunkSize - 4));
  if (buffer == nullptr || flen != chunkSize) {
    lwFree(buffer);
    return 0;
  }
  unsigned char* cursor = buffer;
  unsigned char* end = buffer + chunkSize - 4;
  int polygonCount = 0;
  int vertexCount = 0;
  while (cursor < end) {
    const int count = BufferU2(&cursor) & 0x3FF;
    vertexCount += count;
    ++polygonCount;
    for (int vertex = 0; vertex < count; ++vertex)
      BufferVX(&cursor);
  }
  if (!lwAllocPolygons(polygons, polygonCount, vertexCount)) {
    lwFree(buffer);
    lwFreePolygons(polygons);
    return 0;
  }
  cursor = buffer;
  int vertexOffset = polygons->voffset;
  for (int index = 0; index < polygonCount; ++index) {
    st_lwPolygon& polygon = polygons->pol[polygons->offset + index];
    const unsigned short encoded = BufferU2(&cursor);
    polygon.flags = encoded & 0xFC00;
    polygon.nverts = encoded & 0x03FF;
    polygon.type = type;
    polygon.vindex = vertexOffset;
    for (int vertex = 0; vertex < polygon.nverts; ++vertex)
      polygons->polVerts[vertexOffset + vertex].index =
          BufferVX(&cursor) + pointOffset;
    vertexOffset += polygon.nverts;
  }
  lwFree(buffer);
  return 1;
}

int lwGetPolygons5(idFile* file, int chunkSize, st_lwPolygonList* polygons,
                   int pointOffset) {
  if (chunkSize == 0)
    return 1;
  flen = 0;
  auto* buffer = static_cast<unsigned char*>(getbytes(file, chunkSize));
  if (buffer == nullptr)
    goto fail;
  {
    unsigned char* cursor = buffer;
    unsigned char* end = buffer + chunkSize;
    int polygonCount = 0;
    int vertexCount = 0;
    while (cursor < end) {
      const int count = BufferU2(&cursor);
      vertexCount += count;
      ++polygonCount;
      cursor += 2 * count;
      const int surface = static_cast<short>(BufferU2(&cursor));
      if (surface < 0)
        cursor += 2;
    }
    if (!lwAllocPolygons(polygons, polygonCount, vertexCount))
      goto fail;
    cursor = buffer;
    int vertexOffset = polygons->voffset;
    for (int index = 0; index < polygonCount; ++index) {
      st_lwPolygon& polygon = polygons->pol[polygons->offset + index];
      polygon.nverts = BufferU2(&cursor);
      polygon.type = LWID('F', 'A', 'C', 'E');
      polygon.vindex = vertexOffset;
      for (int vertex = 0; vertex < polygon.nverts; ++vertex)
        polygons->polVerts[vertexOffset + vertex].index =
            BufferU2(&cursor) + pointOffset;
      int surface = static_cast<short>(BufferU2(&cursor));
      if (surface < 0) {
        surface = -surface;
        cursor += 2;
      }
      polygon.surf = reinterpret_cast<st_lwSurface*>(
          static_cast<std::intptr_t>(surface - 1));
      vertexOffset += polygon.nverts;
    }
  }
  lwFree(buffer);
  return 1;
fail:
  lwFree(buffer);
  lwFreePolygons(polygons);
  return 0;
}

int lwGetPoints(idFile* file, int chunkSize, st_lwPointList* points) {
  if (chunkSize == 1)
    return 1;
  const int added = chunkSize / 12;
  st_lwPoint* oldPoints = points->pt;
  points->offset = points->count;
  points->count += added;
  points->pt = static_cast<st_lwPoint*>(
      LwoAlloc(sizeof(st_lwPoint) * static_cast<std::size_t>(points->count), true));
  if (points->pt == nullptr)
    return 0;
  if (oldPoints != nullptr) {
    std::memmove(points->pt, oldPoints,
                 sizeof(st_lwPoint) * static_cast<std::size_t>(points->offset));
    lwFree(oldPoints);
  }
  auto* bytes = static_cast<unsigned char*>(getbytes(file, chunkSize));
  if (bytes == nullptr)
    return 0;
  for (int point = 0; point < added; ++point) {
    for (int axis = 0; axis < 3; ++axis) {
      const int offset = (point * 3 + axis) * 4;
      const std::uint32_t bits =
          (static_cast<std::uint32_t>(bytes[offset]) << 24) |
          (static_cast<std::uint32_t>(bytes[offset + 1]) << 16) |
          (static_cast<std::uint32_t>(bytes[offset + 2]) << 8) |
          bytes[offset + 3];
      std::memcpy(&points->pt[points->offset + point].pos[axis], &bits,
                  sizeof(bits));
    }
  }
  lwFree(bytes);
  return 1;
}

int lwGetTags(idFile* file, int chunkSize, st_lwTagList* tags) {
  if (chunkSize == 0)
    return 1;
  flen = 0;
  auto* buffer = static_cast<unsigned char*>(getbytes(file, chunkSize));
  if (buffer == nullptr)
    return 0;
  int added = 0;
  for (unsigned char* cursor = buffer; cursor < buffer + chunkSize; ++added) {
    const int length = idStr::Length(reinterpret_cast<char*>(cursor)) + 1;
    cursor += length + (length & 1);
  }
  char** oldTags = tags->tag;
  tags->offset = tags->count;
  tags->count += added;
  tags->tag = static_cast<char**>(
      LwoAlloc(sizeof(char*) * static_cast<std::size_t>(tags->count), true));
  if (tags->tag == nullptr) {
    lwFree(buffer);
    return 0;
  }
  if (oldTags != nullptr) {
    std::memmove(tags->tag, oldTags,
                 sizeof(char*) * static_cast<std::size_t>(tags->offset));
    lwFree(oldTags);
  }
  unsigned char* cursor = buffer;
  for (int index = 0; index < added; ++index)
    tags->tag[tags->offset + index] = sgetS0(&cursor);
  lwFree(buffer);
  return 1;
}

int lwGetPolygonTags(idFile* file, int chunkSize, st_lwTagList* tags,
                     st_lwPolygonList* polygons) {
  flen = 0;
  const unsigned int type = static_cast<unsigned int>(getI4(file));
  constexpr unsigned int surfaceTag = LWID('S', 'U', 'R', 'F');
  constexpr unsigned int partTag = LWID('P', 'A', 'R', 'T');
  constexpr unsigned int smoothingTag = LWID('S', 'M', 'G', 'P');
  if (type != surfaceTag && type != partTag && type != smoothingTag) {
    file->Seek(chunkSize - 4, FS_SEEK_CUR);
    return 1;
  }
  if (polygons->count > 0xFFFFFF)
    return 0;
  while (flen < chunkSize) {
    const int polygonIndex = polygons->offset + getVX(file);
    const int tagIndex = tags->offset + getVX(file);
    if (flen < 0 || flen > chunkSize || polygonIndex < 0 ||
        polygonIndex >= polygons->count)
      return 0;
    if (type == partTag)
      polygons->pol[polygonIndex].part = tagIndex;
    else if (type == smoothingTag)
      polygons->pol[polygonIndex].smoothgrp = tagIndex;
    else
      polygons->pol[polygonIndex].surf =
          reinterpret_cast<st_lwSurface*>(static_cast<std::intptr_t>(tagIndex));
  }
  return 1;
}

st_lwSurface* lwDefaultSurface() {
  auto* surface = static_cast<st_lwSurface*>(LwoAlloc(sizeof(st_lwSurface), true));
  if (surface != nullptr) {
    surface->color.rgb[0] = 0.78430998f;
    surface->color.rgb[1] = 0.78430998f;
    surface->color.rgb[2] = 0.78430998f;
    surface->diffuse.val = 1.0f;
    surface->glossiness.val = 0.40000001f;
    surface->bump.val = 1.0f;
    surface->eta.val = 1.0f;
    surface->sideflags = 1;
  }
  return surface;
}

int lwResolvePolySurfaces(st_lwPolygonList* polygons, st_lwTagList* tags,
                          st_lwSurface** surfaces, int* surfaceCount) {
  if (tags->count == 0)
    return 1;
  auto** resolved = static_cast<st_lwSurface**>(
      LwoAlloc(sizeof(st_lwSurface*) * static_cast<std::size_t>(tags->count), true));
  if (resolved == nullptr)
    return 0;
  for (int tag = 0; tag < tags->count; ++tag) {
    for (st_lwSurface* surface = *surfaces; surface != nullptr; surface = surface->next) {
      if (surface->name != nullptr && std::strcmp(surface->name, tags->tag[tag]) == 0) {
        resolved[tag] = surface;
        break;
      }
    }
  }
  for (int index = 0; index < polygons->count; ++index) {
    const int tag = static_cast<int>(reinterpret_cast<std::intptr_t>(polygons->pol[index].surf));
    if (tag < 0 || tag > tags->count) {
      lwFree(resolved);
      return 0;
    }
    if (resolved[tag] == nullptr) {
      resolved[tag] = lwDefaultSurface();
      if (resolved[tag] == nullptr) {
        lwFree(resolved);
        return 0;
      }
      const int length = idStr::Length(tags->tag[tag]) + 1;
      resolved[tag]->name = static_cast<char*>(LwoAlloc(length, true));
      if (resolved[tag]->name == nullptr) {
        lwFree(resolved);
        return 0;
      }
      std::memcpy(resolved[tag]->name, tags->tag[tag], length);
      if (*surfaces == nullptr) {
        *surfaces = resolved[tag];
      } else {
        st_lwSurface* tail = *surfaces;
        while (tail->next != nullptr)
          tail = tail->next;
        tail->next = resolved[tag];
        resolved[tag]->prev = tail;
      }
      ++*surfaceCount;
    }
    polygons->pol[index].surf = resolved[tag];
  }
  lwFree(resolved);
  return 1;
}

void lwFreeObject(st_lwObject* object) {
  if (object == nullptr)
    return;
  st_lwLayer* layer = object->layer;
  while (layer != nullptr) {
    st_lwLayer* next = layer->next;
    lwFree(layer->name);
    lwFreePoints(&layer->point);
    lwFreePolygons(&layer->polygon);
    st_lwVMap* vmap = layer->vmap;
    while (vmap != nullptr) {
      st_lwVMap* nextMap = vmap->next;
      lwFreeVMap(vmap);
      vmap = nextMap;
    }
    lwFree(layer);
    layer = next;
  }
  st_lwEnvelope* envelope = object->env;
  while (envelope != nullptr) {
    st_lwEnvelope* next = envelope->next;
    lwFreeEnvelope(envelope);
    envelope = next;
  }
  st_lwClip* clip = object->clip;
  while (clip != nullptr) {
    st_lwClip* next = clip->next;
    lwFreeClip(clip);
    clip = next;
  }
  st_lwSurface* surface = object->surf;
  while (surface != nullptr) {
    st_lwSurface* next = surface->next;
    lwFreeSurface(surface);
    surface = next;
  }
  lwFreeTags(&object->taglist);
  lwFree(object);
}

int lwGetPointVMaps(st_lwPointList* points, st_lwVMap* vmaps) {
  if (vmaps == nullptr)
    return 1;
  points->ptVMaps = static_cast<st_lwVMapList*>(
      LwoAlloc(sizeof(st_lwVMapList) * static_cast<std::size_t>(points->count), true));
  if (points->ptVMaps == nullptr && points->count > 0)
    return 0;
  for (st_lwVMap* map = vmaps; map != nullptr; map = map->next)
    if (!map->perpoly)
      for (int index = 0; index < map->nverts; ++index)
        ++points->ptVMaps[map->vindex[index]].nvmaps;
  for (int point = 0; point < points->count; ++point) {
    st_lwVMapList& list = points->ptVMaps[point];
    if (list.nvmaps != 0) {
      list.vm = static_cast<st_lwVMapPt*>(
          LwoAlloc(sizeof(st_lwVMapPt) * static_cast<std::size_t>(list.nvmaps), true));
      if (list.vm == nullptr)
        return 0;
      list.nvmaps = 0;
    }
  }
  for (st_lwVMap* map = vmaps; map != nullptr; map = map->next) {
    if (map->perpoly)
      continue;
    for (int index = 0; index < map->nverts; ++index) {
      st_lwVMapList& list = points->ptVMaps[map->vindex[index]];
      list.vm[list.nvmaps++] = {map, index, {}};
    }
  }
  return 1;
}

int lwGetPolyVMaps(st_lwPolygonList* polygons, st_lwVMap* vmaps) {
  if (vmaps == nullptr)
    return 1;
  polygons->polVertVMaps = static_cast<st_lwVMapList*>(
      LwoAlloc(sizeof(st_lwVMapList) * static_cast<std::size_t>(polygons->vcount), true));
  if (polygons->polVertVMaps == nullptr && polygons->vcount > 0)
    return 0;
  const auto findVertex = [polygons](const st_lwVMap& map, int entry) {
    const st_lwPolygon& polygon = polygons->pol[map.pindex[entry]];
    for (int vertex = 0; vertex < polygon.nverts; ++vertex)
      if (polygons->polVerts[polygon.vindex + vertex].index == map.vindex[entry])
        return polygon.vindex + vertex;
    return -1;
  };
  for (st_lwVMap* map = vmaps; map != nullptr; map = map->next) {
    if (!map->perpoly)
      continue;
    for (int entry = 0; entry < map->nverts; ++entry) {
      const int vertex = findVertex(*map, entry);
      if (vertex >= 0)
        ++polygons->polVertVMaps[vertex].nvmaps;
    }
  }
  for (int vertex = 0; vertex < polygons->vcount; ++vertex) {
    st_lwVMapList& list = polygons->polVertVMaps[vertex];
    if (list.nvmaps != 0) {
      list.vm = static_cast<st_lwVMapPt*>(
          LwoAlloc(sizeof(st_lwVMapPt) * static_cast<std::size_t>(list.nvmaps), true));
      if (list.vm == nullptr)
        return 0;
      list.nvmaps = 0;
    }
  }
  for (st_lwVMap* map = vmaps; map != nullptr; map = map->next) {
    if (!map->perpoly)
      continue;
    for (int entry = 0; entry < map->nverts; ++entry) {
      const int vertex = findVertex(*map, entry);
      if (vertex >= 0) {
        st_lwVMapList& list = polygons->polVertVMaps[vertex];
        list.vm[list.nvmaps++] = {map, entry, {}};
      }
    }
  }
  return 1;
}

st_lwVMap* lwGetVMap(idFile* file, int chunkSize, int, int, int perPolygon) {
  flen = 0;
  auto* buffer = static_cast<unsigned char*>(getbytes(file, chunkSize));
  if (buffer == nullptr)
    return nullptr;
  auto* map = static_cast<st_lwVMap*>(LwoAlloc(sizeof(st_lwVMap), true));
  if (map == nullptr) {
    lwFree(buffer);
    return nullptr;
  }
  unsigned char* cursor = buffer;
  map->perpoly = perPolygon;
  map->type = (static_cast<unsigned int>(cursor[0]) << 24) |
              (static_cast<unsigned int>(cursor[1]) << 16) |
              (static_cast<unsigned int>(cursor[2]) << 8) | cursor[3];
  map->dim = (cursor[4] << 8) | cursor[5];
  cursor += 6;
  flen = 6;
  map->name = sgetS0(&cursor);
  unsigned char* records = cursor;
  int count = 0;
  while (cursor < buffer + chunkSize) {
    BufferVX(&cursor);
    if (perPolygon)
      BufferVX(&cursor);
    cursor += 4 * map->dim;
    flen += 4 * map->dim;
    ++count;
  }
  map->nverts = count;
  if (count == 0)
    goto fail;
  map->vindex = lwAllocArray<int>(count);
  if (map->vindex == nullptr)
    goto fail;
  if (perPolygon) {
    map->pindex = lwAllocArray<int>(count);
    if (map->pindex == nullptr)
      goto fail;
  }
  if (map->dim > 0) {
    map->val = lwAllocArray<float*>(count);
    float* values = lwAllocArray<float>(count * map->dim);
    if (map->val == nullptr || values == nullptr) {
      lwFree(values);
      goto fail;
    }
    for (int index = 0; index < count; ++index)
      map->val[index] = values + index * map->dim;
  }
  cursor = records;
  for (int entry = 0; entry < count; ++entry) {
    map->vindex[entry] = BufferVX(&cursor);
    if (perPolygon)
      map->pindex[entry] = BufferVX(&cursor);
    for (int component = 0; component < map->dim; ++component) {
      const std::uint32_t bits =
          (static_cast<std::uint32_t>(cursor[0]) << 24) |
          (static_cast<std::uint32_t>(cursor[1]) << 16) |
          (static_cast<std::uint32_t>(cursor[2]) << 8) | cursor[3];
      cursor += 4;
      float value = 0.0f;
      if ((bits & 0x7F800000u) != 0 || (bits & 0x007FFFFFu) == 0)
        std::memcpy(&value, &bits, sizeof(value));
      map->val[entry][component] = value;
    }
  }
  lwFree(buffer);
  return map;

fail:
  lwFree(buffer);
  lwFreeVMap(map);
  return nullptr;
}

namespace {
constexpr unsigned int ID_CHAN = LWID('C', 'H', 'A', 'N');
constexpr unsigned int ID_OPAC = LWID('O', 'P', 'A', 'C');
constexpr unsigned int ID_ENAB = LWID('E', 'N', 'A', 'B');
constexpr unsigned int ID_NEGA = LWID('N', 'E', 'G', 'A');
constexpr unsigned int ID_AXIS = LWID('A', 'X', 'I', 'S');
constexpr unsigned int ID_TMAP = LWID('T', 'M', 'A', 'P');
constexpr unsigned int ID_SIZE = LWID('S', 'I', 'Z', 'E');
constexpr unsigned int ID_CNTR = LWID('C', 'N', 'T', 'R');
constexpr unsigned int ID_ROTA = LWID('R', 'O', 'T', 'A');
constexpr unsigned int ID_FALL = LWID('F', 'A', 'L', 'L');
constexpr unsigned int ID_OREF = LWID('O', 'R', 'E', 'F');
constexpr unsigned int ID_CSYS = LWID('C', 'S', 'Y', 'S');
constexpr unsigned int ID_PROJ = LWID('P', 'R', 'O', 'J');
constexpr unsigned int ID_VMAP = LWID('V', 'M', 'A', 'P');
constexpr unsigned int ID_IMAG = LWID('I', 'M', 'A', 'G');
constexpr unsigned int ID_WRAP = LWID('W', 'R', 'A', 'P');
constexpr unsigned int ID_WRPW = LWID('W', 'R', 'P', 'W');
constexpr unsigned int ID_WRPH = LWID('W', 'R', 'P', 'H');
constexpr unsigned int ID_AAST = LWID('A', 'A', 'S', 'T');
constexpr unsigned int ID_PIXB = LWID('P', 'I', 'X', 'B');
constexpr unsigned int ID_STCK = LWID('S', 'T', 'C', 'K');
constexpr unsigned int ID_TAMP = LWID('T', 'A', 'M', 'P');
constexpr unsigned int ID_VALU = LWID('V', 'A', 'L', 'U');
constexpr unsigned int ID_FUNC = LWID('F', 'U', 'N', 'C');
constexpr unsigned int ID_PNAM = LWID('P', 'N', 'A', 'M');
constexpr unsigned int ID_INAM = LWID('I', 'N', 'A', 'M');
constexpr unsigned int ID_GRST = LWID('G', 'R', 'S', 'T');
constexpr unsigned int ID_GREN = LWID('G', 'R', 'E', 'N');
constexpr unsigned int ID_GRPT = LWID('G', 'R', 'P', 'T');
constexpr unsigned int ID_FKEY = LWID('F', 'K', 'E', 'Y');
constexpr unsigned int ID_IKEY = LWID('I', 'K', 'E', 'Y');
constexpr unsigned int ID_TIME = LWID('T', 'I', 'M', 'E');
constexpr unsigned int ID_CONT = LWID('C', 'O', 'N', 'T');
constexpr unsigned int ID_BRIT = LWID('B', 'R', 'I', 'T');
constexpr unsigned int ID_SATR = LWID('S', 'A', 'T', 'R');
constexpr unsigned int ID_HUE = LWID('H', 'U', 'E', ' ');
constexpr unsigned int ID_GAMM = LWID('G', 'A', 'M', 'M');
constexpr unsigned int ID_IFLT = LWID('I', 'F', 'L', 'T');
constexpr unsigned int ID_PFLT = LWID('P', 'F', 'L', 'T');
constexpr unsigned int ID_TYPE = LWID('T', 'Y', 'P', 'E');
constexpr unsigned int ID_NAME = LWID('N', 'A', 'M', 'E');
constexpr unsigned int ID_PRE = LWID('P', 'R', 'E', ' ');
constexpr unsigned int ID_POST = LWID('P', 'O', 'S', 'T');
constexpr unsigned int ID_KEY = LWID('K', 'E', 'Y', ' ');
constexpr unsigned int ID_SPAN = LWID('S', 'P', 'A', 'N');
constexpr unsigned int ID_TCB = LWID('T', 'C', 'B', ' ');
constexpr unsigned int ID_BEZI = LWID('B', 'E', 'Z', 'I');
constexpr unsigned int ID_HERM = LWID('H', 'E', 'R', 'M');
constexpr unsigned int ID_BEZ2 = LWID('B', 'E', 'Z', '2');
constexpr unsigned int ID_FLAG = LWID('F', 'L', 'A', 'G');
constexpr unsigned int ID_VLUM = LWID('V', 'L', 'U', 'M');
constexpr unsigned int ID_VDIF = LWID('V', 'D', 'I', 'F');
constexpr unsigned int ID_VSPC = LWID('V', 'S', 'P', 'C');
constexpr unsigned int ID_RFLT = LWID('R', 'F', 'L', 'T');
constexpr unsigned int ID_BTEX = LWID('B', 'T', 'E', 'X');
constexpr unsigned int ID_CTEX = LWID('C', 'T', 'E', 'X');
constexpr unsigned int ID_DTEX = LWID('D', 'T', 'E', 'X');
constexpr unsigned int ID_LTEX = LWID('L', 'T', 'E', 'X');
constexpr unsigned int ID_RTEX = LWID('R', 'T', 'E', 'X');
constexpr unsigned int ID_STEX = LWID('S', 'T', 'E', 'X');
constexpr unsigned int ID_TTEX = LWID('T', 'T', 'E', 'X');
constexpr unsigned int ID_TFLG = LWID('T', 'F', 'L', 'G');
constexpr unsigned int ID_TSIZ = LWID('T', 'S', 'I', 'Z');
constexpr unsigned int ID_TCTR = LWID('T', 'C', 'T', 'R');
constexpr unsigned int ID_TFAL = LWID('T', 'F', 'A', 'L');
constexpr unsigned int ID_TVEL = LWID('T', 'V', 'E', 'L');
constexpr unsigned int ID_TCLR = LWID('T', 'C', 'L', 'R');
constexpr unsigned int ID_TVAL = LWID('T', 'V', 'A', 'L');
constexpr unsigned int ID_TAAS = LWID('T', 'A', 'A', 'S');
constexpr unsigned int ID_TREF = LWID('T', 'R', 'E', 'F');
constexpr unsigned int ID_TOPC = LWID('T', 'O', 'P', 'C');
constexpr unsigned int ID_SDAT = LWID('S', 'D', 'A', 'T');
constexpr unsigned int ID_TFP0 = LWID('T', 'F', 'P', '0');
constexpr unsigned int ID_TFP1 = LWID('T', 'F', 'P', '1');
constexpr unsigned int ID_FORM = LWID('F', 'O', 'R', 'M');
constexpr unsigned int ID_LWO2 = LWID('L', 'W', 'O', '2');
constexpr unsigned int ID_LWOB = LWID('L', 'W', 'O', 'B');
constexpr unsigned int ID_LAYR = LWID('L', 'A', 'Y', 'R');
constexpr unsigned int ID_PNTS = LWID('P', 'N', 'T', 'S');
constexpr unsigned int ID_POLS = LWID('P', 'O', 'L', 'S');
constexpr unsigned int ID_VMAD = LWID('V', 'M', 'A', 'D');
constexpr unsigned int ID_PTAG = LWID('P', 'T', 'A', 'G');
constexpr unsigned int ID_BBOX = LWID('B', 'B', 'O', 'X');
constexpr unsigned int ID_TAGS = LWID('T', 'A', 'G', 'S');
constexpr unsigned int ID_ENVL = LWID('E', 'N', 'V', 'L');
constexpr unsigned int ID_CLIP = LWID('C', 'L', 'I', 'P');
constexpr unsigned int ID_SURF = LWID('S', 'U', 'R', 'F');
constexpr unsigned int ID_SRFS = LWID('S', 'R', 'F', 'S');

unsigned int GetU4(idFile* file) { return static_cast<unsigned int>(getI4(file)); }
unsigned short GetU2(idFile* file) { return static_cast<unsigned short>(getI2(file)); }

bool FinishSubchunk(idFile* file, int paddedSize) {
  const int read = flen;
  if (read < 0 || read > paddedSize)
    return false;
  return read >= paddedSize || file->Seek(paddedSize - read, FS_SEEK_CUR) == 0;
}
} // namespace

int lwGetTHeader(idFile* file, int headerSize, st_lwTexture* texture) {
  flen = 0;
  const std::int64_t start = file->Tell();
  texture->ord = getS0(file);
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0)
    return 0;
  while (true) {
    size += size & 1;
    flen = 0;
    switch (id) {
    case ID_CHAN: texture->chan = GetU4(file); break;
    case ID_OPAC:
      texture->opac_type = static_cast<short>(GetU2(file));
      texture->opacity.val = getF4(file);
      texture->opacity.eindex = getVX(file);
      break;
    case ID_ENAB: texture->enabled = static_cast<short>(GetU2(file)); break;
    case ID_NEGA: texture->negative = static_cast<short>(GetU2(file)); break;
    case ID_AXIS: texture->axis = static_cast<short>(GetU2(file)); break;
    default: break;
    }
    if (!FinishSubchunk(file, size))
      return 0;
    if (file->Tell() - start >= headerSize)
      break;
    flen = 0;
    id = GetU4(file);
    size = GetU2(file);
    if (flen != 6)
      return 0;
  }
  flen = static_cast<int>(file->Tell() - start);
  return 1;
}

int lwGetTMap(idFile* file, int mapSize, st_lwTMap* map) {
  const std::int64_t start = file->Tell();
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0)
    return 0;
  while (true) {
    size += size & 1;
    flen = 0;
    st_lwVParam* vector = nullptr;
    if (id == ID_SIZE) vector = &map->size;
    else if (id == ID_CNTR) vector = &map->center;
    else if (id == ID_ROTA) vector = &map->rotate;
    if (vector != nullptr) {
      for (float& value : vector->val)
        value = getF4(file);
      vector->eindex = getVX(file);
    } else if (id == ID_FALL) {
      map->fall_type = GetU2(file);
      for (float& value : map->falloff.val)
        value = getF4(file);
      map->falloff.eindex = getVX(file);
    } else if (id == ID_OREF) map->ref_object = getS0(file);
    else if (id == ID_CSYS) map->coord_sys = GetU2(file);
    if (!FinishSubchunk(file, size))
      return 0;
    if (file->Tell() - start >= mapSize)
      break;
    flen = 0;
    id = GetU4(file);
    size = GetU2(file);
    if (flen != 6)
      return 0;
  }
  flen = static_cast<int>(file->Tell() - start);
  return 1;
}

int lwGetImageMap(idFile* file, int totalSize, st_lwTexture* texture) {
  const std::int64_t start = file->Tell();
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0)
    return 0;
  while (true) {
    size += size & 1;
    flen = 0;
    switch (id) {
    case ID_TMAP: if (!lwGetTMap(file, size, &texture->tmap)) return 0; break;
    case ID_PROJ: texture->param.imap.projection = GetU2(file); break;
    case ID_VMAP: texture->param.imap.vmap_name = getS0(file); break;
    case ID_AXIS: texture->param.imap.axis = GetU2(file); break;
    case ID_IMAG: texture->param.imap.cindex = getVX(file); break;
    case ID_WRAP:
      texture->param.imap.wrapw_type = GetU2(file);
      texture->param.imap.wraph_type = GetU2(file);
      break;
    case ID_WRPW:
      texture->param.imap.wrapw.val = getF4(file);
      texture->param.imap.wrapw.eindex = getVX(file);
      break;
    case ID_WRPH:
      texture->param.imap.wraph.val = getF4(file);
      texture->param.imap.wraph.eindex = getVX(file);
      break;
    case ID_AAST:
      texture->param.imap.aas_flags = GetU2(file);
      texture->param.imap.aa_strength = getF4(file);
      break;
    case ID_PIXB: texture->param.imap.pblend = GetU2(file); break;
    case ID_STCK:
      texture->param.imap.stck.val = getF4(file);
      texture->param.imap.stck.eindex = getVX(file);
      break;
    case ID_TAMP:
      texture->param.imap.amplitude.val = getF4(file);
      texture->param.imap.amplitude.eindex = getVX(file);
      break;
    default: break;
    }
    if (!FinishSubchunk(file, size)) return 0;
    if (file->Tell() - start >= totalSize) break;
    flen = 0; id = GetU4(file); size = GetU2(file);
    if (flen != 6) return 0;
  }
  flen = static_cast<int>(file->Tell() - start);
  return 1;
}

int lwGetProcedural(idFile* file, int totalSize, st_lwTexture* texture) {
  const std::int64_t start = file->Tell();
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0) return 0;
  while (true) {
    size += size & 1;
    flen = 0;
    if (id == ID_TMAP) {
      if (!lwGetTMap(file, size, &texture->tmap)) return 0;
    } else if (id == ID_AXIS) texture->param.proc.axis = GetU2(file);
    else if (id == ID_VALU) {
      texture->param.proc.value[0] = getF4(file);
      if (size >= 8) texture->param.proc.value[1] = getF4(file);
      if (size >= 12) texture->param.proc.value[2] = getF4(file);
    } else if (id == ID_FUNC) {
      texture->param.proc.name = getS0(file);
      texture->param.proc.data = getbytes(file, size - flen);
    }
    if (!FinishSubchunk(file, size)) return 0;
    if (file->Tell() - start >= totalSize) break;
    flen = 0; id = GetU4(file); size = GetU2(file);
    if (flen != 6) return 0;
  }
  flen = static_cast<int>(file->Tell() - start);
  return 1;
}

int lwGetGradient(idFile* file, int totalSize, st_lwTexture* texture) {
  const std::int64_t start = file->Tell();
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0) return 0;
  while (true) {
    size += size & 1;
    flen = 0;
    if (id == ID_TMAP) {
      if (!lwGetTMap(file, size, &texture->tmap)) return 0;
    } else if (id == ID_PNAM) texture->param.grad.paramname = getS0(file);
    else if (id == ID_INAM) texture->param.grad.itemname = getS0(file);
    else if (id == ID_GRST) texture->param.grad.start = getF4(file);
    else if (id == ID_GREN) texture->param.grad.end = getF4(file);
    else if (id == ID_GRPT) texture->param.grad.repeat = GetU2(file);
    else if (id == ID_FKEY) {
      const int count = size / 20;
      texture->param.grad.key = static_cast<st_lwGradKey*>(
          LwoAlloc(sizeof(st_lwGradKey) * static_cast<std::size_t>(count), true));
      if (texture->param.grad.key == nullptr) return 0;
      for (int key = 0; key < count; ++key) {
        texture->param.grad.key[key].value = getF4(file);
        for (float& value : texture->param.grad.key[key].rgba) value = getF4(file);
      }
    } else if (id == ID_IKEY) {
      const int count = size / 2;
      texture->param.grad.ikey = static_cast<short*>(
          LwoAlloc(sizeof(short) * static_cast<std::size_t>(count), true));
      if (texture->param.grad.ikey == nullptr) return 0;
      for (int key = 0; key < count; ++key)
        texture->param.grad.ikey[key] = static_cast<short>(GetU2(file));
    }
    if (!FinishSubchunk(file, size)) return 0;
    if (file->Tell() - start >= totalSize) break;
    flen = 0; id = GetU4(file); size = GetU2(file);
    if (flen != 6) return 0;
  }
  flen = static_cast<int>(file->Tell() - start);
  return 1;
}

st_lwTexture* lwGetTexture(idFile* file, int blockSize, unsigned int type) {
  auto* texture = static_cast<st_lwTexture*>(LwoAlloc(sizeof(st_lwTexture), true));
  if (texture == nullptr) return nullptr;
  texture->type = type;
  texture->tmap.size.val[0] = texture->tmap.size.val[1] =
      texture->tmap.size.val[2] = 1.0f;
  texture->opacity.val = 1.0f;
  texture->enabled = 1;
  const unsigned short headerSize = GetU2(file);
  if (!lwGetTHeader(file, headerSize, texture)) {
    lwFreeTexture(texture);
    return nullptr;
  }
  const int remaining = blockSize - headerSize - 6;
  bool ok = false;
  if (type == ID_IMAP) ok = lwGetImageMap(file, remaining, texture) != 0;
  else if (type == ID_PROC) ok = lwGetProcedural(file, remaining, texture) != 0;
  else if (type == ID_GRAD) ok = lwGetGradient(file, remaining, texture) != 0;
  else ok = file->Seek(remaining, FS_SEEK_CUR) == 0;
  if (!ok) {
    lwFreeTexture(texture);
    return nullptr;
  }
  flen = blockSize;
  return texture;
}

st_lwPlugin* lwGetShader(idFile* file, int blockSize) {
  auto* shader = static_cast<st_lwPlugin*>(LwoAlloc(sizeof(st_lwPlugin), true));
  if (shader == nullptr)
    return nullptr;
  const std::int64_t start = file->Tell();
  flen = 0;
  int headerSize = GetU2(file);
  shader->ord = getS0(file);
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0)
    goto fail;
  while (headerSize > 0) {
    const int padded = size + (size & 1);
    headerSize -= padded;
    if (id == ID_ENAB) {
      shader->flags = GetU2(file);
      break;
    }
    if (file->Seek(padded, FS_SEEK_CUR) != 0)
      goto fail;
    id = GetU4(file);
    size = GetU2(file);
  }
  id = GetU4(file);
  size = GetU2(file);
  if (flen < 0)
    goto fail;
  while (true) {
    const int padded = size + (size & 1);
    flen = 0;
    if (id == ID_FUNC) {
      shader->name = getS0(file);
      shader->data = getbytes(file, padded - flen);
    }
    if (!FinishSubchunk(file, padded))
      goto fail;
    if (file->Tell() - start >= blockSize) {
      flen = static_cast<int>(file->Tell() - start);
      return shader;
    }
    flen = 0;
    id = GetU4(file);
    size = GetU2(file);
    if (flen != 6)
      goto fail;
  }
fail:
  lwFreePlugin(shader);
  return nullptr;
}

st_lwSurface* lwGetSurface(idFile* file, int chunkSize) {
  st_lwSurface* surface = lwDefaultSurface();
  if (surface == nullptr)
    return nullptr;
  const std::int64_t start = file->Tell();
  flen = 0;
  surface->name = getS0(file);
  surface->srcname = getS0(file);
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0)
    goto fail;
  while (true) {
    const int padded = size + (size & 1);
    flen = 0;
    switch (id) {
    case ID_COLR:
      for (float& value : surface->color.rgb) value = getF4(file);
      surface->color.eindex = getVX(file); break;
    case ID_LUMI: surface->luminosity.val = getF4(file); surface->luminosity.eindex = getVX(file); break;
    case ID_DIFF: surface->diffuse.val = getF4(file); surface->diffuse.eindex = getVX(file); break;
    case ID_SPEC: surface->specularity.val = getF4(file); surface->specularity.eindex = getVX(file); break;
    case ID_GLOS: surface->glossiness.val = getF4(file); surface->glossiness.eindex = getVX(file); break;
    case ID_REFL: surface->reflection.val.val = getF4(file); surface->reflection.val.eindex = getVX(file); break;
    case ID_RFOP: surface->reflection.options = GetU2(file); break;
    case ID_RIMG: surface->reflection.cindex = getVX(file); break;
    case ID_RSAN: surface->reflection.seam_angle = getF4(file); break;
    case ID_TRAN: surface->transparency.val.val = getF4(file); surface->transparency.val.eindex = getVX(file); break;
    case ID_TROP: surface->transparency.options = GetU2(file); break;
    case ID_TIMG: surface->transparency.cindex = getVX(file); break;
    case ID_RIND: surface->eta.val = getF4(file); surface->eta.eindex = getVX(file); break;
    case ID_TRNL: surface->translucency.val = getF4(file); surface->translucency.eindex = getVX(file); break;
    case ID_BUMP: surface->bump.val = getF4(file); surface->bump.eindex = getVX(file); break;
    case ID_SMAN: surface->smooth = getF4(file); break;
    case ID_SIDE: surface->sideflags = GetU2(file); break;
    case ID_CLRH: surface->color_hilite.val = getF4(file); surface->color_hilite.eindex = getVX(file); break;
    case ID_CLRF: surface->color_filter.val = getF4(file); surface->color_filter.eindex = getVX(file); break;
    case ID_ADTR: surface->add_trans.val = getF4(file); surface->add_trans.eindex = getVX(file); break;
    case ID_SHRP: surface->dif_sharp.val = getF4(file); surface->dif_sharp.eindex = getVX(file); break;
    case ID_GVAL: surface->glow.val = getF4(file); surface->glow.eindex = getVX(file); break;
    case ID_LINE:
      surface->line.enabled = 1;
      if (padded >= 2) surface->line.flags = GetU2(file);
      if (padded >= 6) surface->line.size.val = getF4(file);
      if (padded >= 8) surface->line.size.eindex = getVX(file);
      break;
    case ID_ALPH: surface->alpha_mode = GetU2(file); surface->alpha = getF4(file); break;
    case ID_AVAL: surface->alpha = getF4(file); break;
    case ID_BLOK: {
      const unsigned int type = GetU4(file);
      if (type == ID_IMAP || type == ID_PROC || type == ID_GRAD) {
        st_lwTexture* texture = lwGetTexture(file, padded - 4, type);
        if (texture == nullptr) goto fail;
        if (!add_texture(surface, texture)) lwFreeTexture(texture);
        flen += 4;
      } else if (type == ID_SHDR) {
        st_lwPlugin* shader = lwGetShader(file, padded - 4);
        if (shader == nullptr) goto fail;
        lwListInsert(reinterpret_cast<void**>(&surface->shader), shader,
                     reinterpret_cast<int (*)(void*, void*)>(compare_shaders));
        ++surface->nshaders;
        flen += 4;
      }
      break;
    }
    default: break;
    }
    if (!FinishSubchunk(file, padded))
      goto fail;
    if (file->Tell() - start >= chunkSize) {
      surface->smoothCos = std::cos(surface->smooth);
      return surface;
    }
    flen = 0;
    id = GetU4(file);
    size = GetU2(file);
    if (flen != 6)
      goto fail;
  }
fail:
  lwFreeSurface(surface);
  return nullptr;
}

st_lwClip* lwGetClip(idFile* file, int chunkSize) {
  auto* clip = static_cast<st_lwClip*>(LwoAlloc(sizeof(st_lwClip), true));
  if (clip == nullptr)
    return nullptr;
  clip->contrast.val = clip->brightness.val = clip->saturation.val =
      clip->gamma.val = 1.0f;
  const std::int64_t start = file->Tell();
  flen = 0;
  clip->index = getI4(file);
  clip->type = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0)
    goto fail;
  {
    const int padded = size + (size & 1);
    flen = 0;
    switch (clip->type) {
    case ID_STIL: clip->source.still.name = getS0(file); break;
    case ID_ISEQ:
      clip->source.seq.digits = getU1(file);
      clip->source.seq.flags = getU1(file);
      clip->source.seq.offset = getI2(file);
      clip->source.seq.start = getI2(file);
      clip->source.seq.end = getI2(file);
      clip->source.seq.prefix = getS0(file);
      clip->source.seq.suffix = getS0(file);
      break;
    case ID_ANIM:
      clip->source.anim.name = getS0(file);
      clip->source.anim.server = getS0(file);
      clip->source.anim.data = getbytes(file, padded - flen);
      break;
    case ID_XREF:
      clip->source.xref.index = getI4(file);
      clip->source.xref.string = getS0(file);
      break;
    case ID_STCC:
      clip->source.cycle.lo = getI2(file);
      clip->source.cycle.hi = getI2(file);
      clip->source.cycle.name = getS0(file);
      break;
    default: break;
    }
    if (!FinishSubchunk(file, padded))
      goto fail;
  }
  if (file->Tell() - start > chunkSize)
    goto fail;
  if (file->Tell() - start == chunkSize)
    return clip;
  while (file->Tell() - start < chunkSize) {
    flen = 0;
    const unsigned int id = GetU4(file);
    size = GetU2(file);
    if (flen != 6)
      goto fail;
    const int padded = size + (size & 1);
    flen = 0;
    switch (id) {
    case ID_TIME:
      clip->start_time = getF4(file); clip->duration = getF4(file);
      clip->frame_rate = getF4(file); break;
    case ID_CONT: clip->contrast.val = getF4(file); clip->contrast.eindex = getVX(file); break;
    case ID_BRIT: clip->brightness.val = getF4(file); clip->brightness.eindex = getVX(file); break;
    case ID_SATR: clip->saturation.val = getF4(file); clip->saturation.eindex = getVX(file); break;
    case ID_HUE: clip->hue.val = getF4(file); clip->hue.eindex = getVX(file); break;
    case ID_GAMM: clip->gamma.val = getF4(file); clip->gamma.eindex = getVX(file); break;
    case ID_NEGA: clip->negative = GetU2(file); break;
    case ID_IFLT:
    case ID_PFLT: {
      auto* filter = static_cast<st_lwPlugin*>(LwoAlloc(sizeof(st_lwPlugin), true));
      if (filter == nullptr) goto fail;
      filter->name = getS0(file);
      filter->flags = GetU2(file);
      filter->data = getbytes(file, padded - flen);
      st_lwPlugin** list = id == ID_IFLT ? &clip->ifilter : &clip->pfilter;
      if (*list == nullptr) *list = filter;
      else { st_lwPlugin* tail = *list; while (tail->next != nullptr) tail = tail->next;
        tail->next = filter; filter->prev = tail; }
      if (id == ID_IFLT) ++clip->nifilters; else ++clip->npfilters;
      break;
    }
    default: break;
    }
    if (!FinishSubchunk(file, padded))
      goto fail;
    if (file->Tell() - start > chunkSize)
      goto fail;
  }
  return clip;
fail:
  lwFreeClip(clip);
  return nullptr;
}

st_lwEnvelope* lwGetEnvelope(idFile* file, int chunkSize) {
  auto* envelope = static_cast<st_lwEnvelope*>(LwoAlloc(sizeof(st_lwEnvelope), true));
  if (envelope == nullptr)
    return nullptr;
  st_lwKey* key = nullptr;
  const std::int64_t start = file->Tell();
  flen = 0;
  envelope->index = getVX(file);
  while (file->Tell() - start < chunkSize) {
    flen = 0;
    const unsigned int id = GetU4(file);
    unsigned short size = GetU2(file);
    if (flen != 6)
      goto fail;
    const int padded = size + (size & 1);
    flen = 0;
    switch (id) {
    case ID_TYPE: envelope->type = GetU2(file); break;
    case ID_NAME: envelope->name = getS0(file); break;
    case ID_PRE: envelope->behavior[0] = GetU2(file); break;
    case ID_POST: envelope->behavior[1] = GetU2(file); break;
    case ID_KEY:
      key = static_cast<st_lwKey*>(LwoAlloc(sizeof(st_lwKey), true));
      if (key == nullptr) goto fail;
      key->time = getF4(file);
      key->value = getF4(file);
      lwListInsert(reinterpret_cast<void**>(&envelope->key), key,
                   reinterpret_cast<int (*)(void*, void*)>(compare_keys));
      ++envelope->nkeys;
      break;
    case ID_SPAN: {
      if (key == nullptr) goto fail;
      key->shape = GetU4(file);
      float values[4]{};
      const int count = std::min((padded - 4) / 4, 4);
      for (int index = 0; index < count; ++index) values[index] = getF4(file);
      if (key->shape == ID_TCB) {
        key->tension = values[0]; key->continuity = values[1]; key->bias = values[2];
      } else if (key->shape == ID_BEZI || key->shape == ID_HERM || key->shape == ID_BEZ2) {
        for (int index = 0; index < count; ++index) key->param[index] = values[index];
      }
      break;
    }
    case ID_CHAN: {
      auto* plugin = static_cast<st_lwPlugin*>(LwoAlloc(sizeof(st_lwPlugin), true));
      if (plugin == nullptr) goto fail;
      plugin->name = getS0(file);
      plugin->flags = GetU2(file);
      plugin->data = getbytes(file, padded - flen);
      if (envelope->cfilter == nullptr) envelope->cfilter = plugin;
      else { st_lwPlugin* tail = envelope->cfilter; while (tail->next != nullptr) tail = tail->next;
        tail->next = plugin; plugin->prev = tail; }
      ++envelope->ncfilters;
      break;
    }
    default: break;
    }
    if (!FinishSubchunk(file, padded))
      goto fail;
    if (file->Tell() - start > chunkSize)
      goto fail;
  }
  return envelope;
fail:
  lwFreeEnvelope(envelope);
  return nullptr;
}

st_lwSurface* lwGetSurface5(idFile* file, int chunkSize, st_lwObject* object) {
  st_lwSurface* surface = lwDefaultSurface();
  if (surface == nullptr)
    return nullptr;
  st_lwTexture* texture = nullptr;
  st_lwPlugin* shader = nullptr;
  const std::int64_t start = file->Tell();
  flen = 0;
  surface->name = getS0(file);
  unsigned int id = GetU4(file);
  unsigned short size = GetU2(file);
  if (flen < 0)
    goto fail;
  while (true) {
    const int padded = size + (size & 1);
    flen = 0;
    switch (id) {
    case ID_COLR:
      for (float& value : surface->color.rgb) value = getU1(file) / 255.0f;
      break;
    case ID_FLAG: {
      const unsigned int flags = GetU2(file);
      if (flags & 4) surface->smooth = 1.56207f;
      if (flags & 8) surface->color_hilite.val = 1.0f;
      if (flags & 16) surface->color_filter.val = 1.0f;
      if (flags & 128) surface->dif_sharp.val = 0.5f;
      if (flags & 256) surface->sideflags = 3;
      if (flags & 512) surface->add_trans.val = 1.0f;
      break;
    }
    case ID_LUMI: surface->luminosity.val = getI2(file) / 256.0f; break;
    case ID_VLUM: surface->luminosity.val = getF4(file); break;
    case ID_DIFF: surface->diffuse.val = getI2(file) / 256.0f; break;
    case ID_VDIF: surface->diffuse.val = getF4(file); break;
    case ID_SPEC: surface->specularity.val = getI2(file) / 256.0f; break;
    case ID_VSPC: surface->specularity.val = getF4(file); break;
    case ID_GLOS: surface->glossiness.val = std::log(static_cast<float>(GetU2(file))) / 20.7944f; break;
    case ID_SMAN: surface->smooth = getF4(file); break;
    case ID_REFL: surface->reflection.val.val = getI2(file) / 256.0f; break;
    case ID_RFLT: surface->reflection.options = GetU2(file); break;
    case ID_RIMG: {
      char* text = getS0(file);
      surface->reflection.cindex = add_clip(text, &object->clip, &object->nclips);
      surface->reflection.options = 3;
      break;
    }
    case ID_RSAN: surface->reflection.seam_angle = getF4(file); break;
    case ID_TRAN: surface->transparency.val.val = getI2(file) / 256.0f; break;
    case ID_RIND: surface->eta.val = getF4(file); break;
    case ID_BTEX: case ID_CTEX: case ID_DTEX: case ID_LTEX:
    case ID_RTEX: case ID_STEX: case ID_TTEX: {
      texture = get_texture(static_cast<char*>(getbytes(file, padded)));
      if (texture == nullptr) goto fail;
      st_lwTexture** list = nullptr;
      if (id == ID_BTEX) list = &surface->bump.tex;
      else if (id == ID_CTEX) list = &surface->color.tex;
      else if (id == ID_DTEX) list = &surface->diffuse.tex;
      else if (id == ID_LTEX) list = &surface->luminosity.tex;
      else if (id == ID_RTEX) list = &surface->reflection.val.tex;
      else if (id == ID_STEX) list = &surface->specularity.tex;
      else list = &surface->transparency.val.tex;
      AppendNode(list, texture);
      break;
    }
    case ID_TFLG: {
      if (texture == nullptr) goto fail;
      const unsigned int flags = GetU2(file);
      int axis = 0;
      if (flags & 2) axis = 1;
      if (flags & 4) axis = 2;
      texture->axis = static_cast<short>(axis);
      if (texture->type == ID_IMAP) texture->param.imap.axis = axis;
      else texture->param.proc.axis = axis;
      if (flags & 8) texture->tmap.coord_sys = 1;
      if (flags & 16) texture->negative = 1;
      if (flags & 32) texture->param.imap.pblend = 1;
      if (flags & 64) { texture->param.imap.aa_strength = 1.0f; texture->param.imap.aas_flags = 1; }
      break;
    }
    case ID_TSIZ: case ID_TCTR: case ID_TFAL: {
      if (texture == nullptr) goto fail;
      st_lwVParam* parameter = id == ID_TSIZ ? &texture->tmap.size :
          (id == ID_TCTR ? &texture->tmap.center : &texture->tmap.falloff);
      for (float& value : parameter->val) value = getF4(file);
      break;
    }
    case ID_TVEL: {
      if (texture == nullptr) goto fail;
      float velocity[3];
      for (float& value : velocity) value = getF4(file);
      texture->tmap.center.eindex = add_tvel(texture->tmap.center.val, velocity,
                                             &object->env, &object->nenvs);
      break;
    }
    case ID_TCLR:
      if (texture == nullptr) goto fail;
      if (texture->type == ID_PROC)
        for (float& value : texture->param.proc.value) value = getU1(file) / 255.0f;
      break;
    case ID_TVAL:
      if (texture == nullptr) goto fail;
      texture->param.proc.value[0] = getI2(file) / 256.0f; break;
    case ID_TAMP:
      if (texture == nullptr) goto fail;
      if (texture->type == ID_IMAP) texture->param.imap.amplitude.val = getF4(file);
      break;
    case ID_TIMG:
      if (texture == nullptr) goto fail;
      texture->param.imap.cindex = add_clip(getS0(file), &object->clip, &object->nclips); break;
    case ID_TAAS:
      if (texture == nullptr) goto fail;
      texture->param.imap.aa_strength = getF4(file); texture->param.imap.aas_flags = 1; break;
    case ID_TREF:
      if (texture == nullptr) goto fail;
      texture->tmap.ref_object = static_cast<char*>(getbytes(file, padded)); break;
    case ID_TOPC:
      if (texture == nullptr) goto fail;
      texture->opacity.val = getF4(file); break;
    case ID_TFP0:
      if (texture == nullptr) goto fail;
      if (texture->type == ID_IMAP) texture->param.imap.wrapw.val = getF4(file); break;
    case ID_TFP1:
      if (texture == nullptr) goto fail;
      if (texture->type == ID_IMAP) texture->param.imap.wraph.val = getF4(file); break;
    case ID_SHDR:
      shader = static_cast<st_lwPlugin*>(LwoAlloc(sizeof(st_lwPlugin), true));
      if (shader == nullptr) goto fail;
      shader->name = static_cast<char*>(getbytes(file, padded));
      AppendNode(&surface->shader, shader);
      ++surface->nshaders;
      break;
    case ID_SDAT:
      if (shader == nullptr) goto fail;
      shader->data = getbytes(file, padded); break;
    default: break;
    }
    if (!FinishSubchunk(file, padded))
      goto fail;
    if (file->Tell() - start >= chunkSize) {
      surface->smoothCos = std::cos(surface->smooth);
      return surface;
    }
    flen = 0;
    id = GetU4(file);
    size = GetU2(file);
    if (flen != 6)
      goto fail;
  }
fail:
  lwFreeSurface(surface);
  return nullptr;
}

st_lwObject* lwGetObject5(const char* filename, float forceSmooth,
                          unsigned int* failId, int* failPosition) {
  idFile* file = fileSystem != nullptr ? fileSystem->OpenFileRead(filename) : nullptr;
  if (file == nullptr)
    return nullptr;
  st_lwObject* object = nullptr;
  st_lwLayer* layer = nullptr;
  unsigned int id = 0;
  flen = 0;
  id = GetU4(file);
  const unsigned int formSize = GetU4(file);
  const unsigned int type = GetU4(file);
  if (flen != 12) {
    delete file;
    return nullptr;
  }
  if (id != ID_FORM || type != ID_LWOB) {
    delete file;
    if (failPosition != nullptr) *failPosition = 12;
    return nullptr;
  }
  object = lwAlloc<st_lwObject>();
  if (object == nullptr)
    goto fail;
  layer = lwAlloc<st_lwLayer>();
  if (layer == nullptr)
    goto fail;
  object->layer = layer;
  object->nlayers = 1;
  flen = 0;
  id = GetU4(file);
  unsigned int chunkSize = GetU4(file);
  if (flen != 8)
    goto fail;
  while (true) {
    const int padded = static_cast<int>(chunkSize + (chunkSize & 1));
    if (id == ID_PNTS) {
      if (!lwGetPoints(file, padded, &layer->point)) goto fail;
    } else if (id == ID_POLS) {
      if (!lwGetPolygons5(file, padded, &layer->polygon, layer->point.offset)) goto fail;
    } else if (id == ID_SRFS) {
      if (!lwGetTags(file, padded, &object->taglist)) goto fail;
    } else if (id == ID_SURF) {
      st_lwSurface* surface = lwGetSurface5(file, padded, object);
      if (surface == nullptr) goto fail;
      AppendNode(&object->surf, surface);
      ++object->nsurfs;
    } else if (file->Seek(padded, FS_SEEK_CUR) != 0) {
      goto fail;
    }
    if (formSize <= file->Tell() - 8)
      break;
    flen = 0;
    id = GetU4(file);
    chunkSize = GetU4(file);
    if (flen != 8)
      goto fail;
  }
  delete file;
  file = nullptr;
  lwForceSmoothSurfaces(object->surf, forceSmooth);
  lwGetBoundingBox(&layer->point, layer->bbox);
  if (!lwResolvePolySurfaces(&layer->polygon, &object->taglist,
                             &object->surf, &object->nsurfs) ||
      !lwGetPointPolygons(&layer->point, &layer->polygon))
    goto fail;
  lwGetPolyNormals(&layer->point, &layer->polygon);
  lwGetVertNormals(&layer->point, &layer->polygon);
  lwFreePointPolygons(&layer->point);
  return object;
fail:
  if (failId != nullptr) *failId = id;
  if (file != nullptr) {
    if (failPosition != nullptr) *failPosition = static_cast<int>(file->Tell());
    delete file;
  }
  lwFreeObject(object);
  return nullptr;
}

st_lwObject* lwGetObject(const char* filename, float forceSmooth,
                         unsigned int* failId, int* failPosition) {
  idFile* source = fileSystem != nullptr ? fileSystem->OpenFileRead(filename) : nullptr;
  if (source == nullptr)
    return nullptr;
  const std::int64_t sourceLength = source->Length();
  if (sourceLength <= 0) {
    delete source;
    return nullptr;
  }
  char* fileData = static_cast<char*>(LwoAlloc(static_cast<std::size_t>(sourceLength), false));
  if (fileData == nullptr || source->Read(fileData, static_cast<std::size_t>(sourceLength)) !=
                                 static_cast<std::size_t>(sourceLength)) {
    delete source;
    lwFree(fileData);
    return nullptr;
  }
  const std::uint32_t timestamp = source->Timestamp();
  delete source;
  idFile_Memory* file = new idFile_Memory(filename);
  file->SetReadOnlyData(fileData, static_cast<std::uint64_t>(sourceLength));
  file->timestamp = timestamp;
  st_lwObject* object = nullptr;
  st_lwLayer* layer = nullptr;
  unsigned int id = 0;
  flen = 0;
  id = GetU4(file);
  const unsigned int formSize = GetU4(file);
  const unsigned int type = GetU4(file);
  if (flen != 12) {
    delete file;
    lwFree(fileData);
    return nullptr;
  }
  if (id != ID_FORM) {
    delete file;
    lwFree(fileData);
    if (failPosition != nullptr) *failPosition = 12;
    return nullptr;
  }
  if (type != ID_LWO2) {
    delete file;
    lwFree(fileData);
    if (type == ID_LWOB)
      return lwGetObject5(filename, forceSmooth, failId, failPosition);
    if (failPosition != nullptr) *failPosition = 12;
    return nullptr;
  }
  object = lwAlloc<st_lwObject>();
  if (object == nullptr)
    goto fail;
  layer = lwAlloc<st_lwLayer>();
  if (layer == nullptr)
    goto fail;
  object->layer = layer;
  object->timeStamp = file->Timestamp();
  flen = 0;
  id = GetU4(file);
  unsigned int chunkSize = GetU4(file);
  if (flen != 8)
    goto fail;
  while (true) {
    const int padded = static_cast<int>(chunkSize + (chunkSize & 1));
    if (id == ID_LAYR) {
      if (object->nlayers > 0) {
        layer = lwAlloc<st_lwLayer>();
        if (layer == nullptr) goto fail;
        AppendNode(&object->layer, layer);
      }
      ++object->nlayers;
      flen = 0;
      layer->index = GetU2(file);
      layer->flags = GetU2(file);
      for (float& value : layer->pivot) value = getF4(file);
      layer->name = getS0(file);
      if (flen < 0 || flen > padded) goto fail;
      if (flen <= padded - 2) layer->parent = GetU2(file);
      if (flen < padded && file->Seek(padded - flen, FS_SEEK_CUR) != 0) goto fail;
    } else if (id == ID_PNTS) {
      if (!lwGetPoints(file, padded, &layer->point)) goto fail;
    } else if (id == ID_POLS) {
      if (!lwGetPolygons(file, padded, &layer->polygon, layer->point.offset)) goto fail;
    } else if (id == ID_VMAP || id == ID_VMAD) {
      st_lwVMap* map = lwGetVMap(file, padded, layer->point.offset,
                                 layer->polygon.offset, id == ID_VMAD);
      if (map == nullptr) goto fail;
      AppendNode(&layer->vmap, map);
      ++layer->nvmaps;
    } else if (id == ID_PTAG) {
      if (!lwGetPolygonTags(file, padded, &object->taglist, &layer->polygon)) goto fail;
    } else if (id == ID_BBOX) {
      flen = 0;
      for (float& value : layer->bbox) value = getF4(file);
      if (flen < 0 || flen > padded) goto fail;
      if (flen < padded && file->Seek(padded - flen, FS_SEEK_CUR) != 0) goto fail;
    } else if (id == ID_TAGS) {
      if (!lwGetTags(file, padded, &object->taglist)) goto fail;
    } else if (id == ID_ENVL) {
      st_lwEnvelope* envelope = lwGetEnvelope(file, padded);
      if (envelope == nullptr) goto fail;
      AppendNode(&object->env, envelope);
      ++object->nenvs;
    } else if (id == ID_CLIP) {
      st_lwClip* clip = lwGetClip(file, padded);
      if (clip == nullptr) goto fail;
      AppendNode(&object->clip, clip);
      ++object->nclips;
    } else if (id == ID_SURF) {
      st_lwSurface* surface = lwGetSurface(file, padded);
      if (surface == nullptr) goto fail;
      AppendNode(&object->surf, surface);
      ++object->nsurfs;
    } else if (file->Seek(padded, FS_SEEK_CUR) != 0) {
      goto fail;
    }
    if (formSize <= file->Tell() - 8)
      break;
    flen = 0;
    id = GetU4(file);
    chunkSize = GetU4(file);
    if (flen != 8)
      goto fail;
  }
  delete file;
  file = nullptr;
  lwFree(fileData);
  fileData = nullptr;
  if (object->nlayers == 0)
    object->nlayers = 1;
  lwForceSmoothSurfaces(object->surf, forceSmooth);
  for (layer = object->layer; layer != nullptr; layer = layer->next) {
    lwGetBoundingBox(&layer->point, layer->bbox);
    if (!lwResolvePolySurfaces(&layer->polygon, &object->taglist,
                               &object->surf, &object->nsurfs) ||
        !lwGetPointPolygons(&layer->point, &layer->polygon))
      goto fail;
    lwGetPolyNormals(&layer->point, &layer->polygon);
    lwGetVertNormals(&layer->point, &layer->polygon);
    lwFreePointPolygons(&layer->point);
    if (!lwGetPointVMaps(&layer->point, layer->vmap) ||
        !lwGetPolyVMaps(&layer->polygon, layer->vmap))
      goto fail;
  }
  return object;
fail:
  if (failId != nullptr) *failId = id;
  if (file != nullptr) {
    if (failPosition != nullptr) *failPosition = static_cast<int>(file->Tell());
    delete file;
  }
  lwFree(fileData);
  lwFreeObject(object);
  return nullptr;
}
