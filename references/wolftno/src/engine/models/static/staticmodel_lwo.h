#pragma once

#include <cstddef>
#include <cstdint>

class idFile;

struct st_lwNode {
  st_lwNode* next;
  st_lwNode* prev;
  void* data;
};
using lwNode = st_lwNode;

struct st_lwPlugin {
  st_lwPlugin* next;
  st_lwPlugin* prev;
  char* ord;
  char* name;
  int flags;
  std::uint8_t padding24[4];
  void* data;
};
using lwPlugin = st_lwPlugin;

struct st_lwKey {
  st_lwKey* next;
  st_lwKey* prev;
  float value;
  float time;
  unsigned int shape;
  float tension;
  float continuity;
  float bias;
  float param[4];
};
using lwKey = st_lwKey;

struct st_lwEnvelope {
  st_lwEnvelope* next;
  st_lwEnvelope* prev;
  int index;
  int type;
  char* name;
  st_lwKey* key;
  int nkeys;
  int behavior[2];
  std::uint8_t padding34[4];
  st_lwPlugin* cfilter;
  int ncfilters;
  std::uint8_t padding44[4];
};
using lwEnvelope = st_lwEnvelope;

struct st_lwEParam { float val; int eindex; };
using lwEParam = st_lwEParam;
struct st_lwVParam { float val[3]; int eindex; };
using lwVParam = st_lwVParam;

struct st_lwClipStill { char* name; };
struct st_lwClipSeq {
  char* prefix; char* suffix; int digits; int flags; int offset; int start; int end;
  std::uint8_t padding24[4];
};
struct st_lwClipAnim { char* name; char* server; void* data; };
struct st_lwClipXRef { char* string; int index; std::uint8_t padding0C[4]; struct st_lwClip* clip; };
struct st_lwClipCycle { char* name; int lo; int hi; };

struct st_lwClip {
  st_lwClip* next;
  st_lwClip* prev;
  int index;
  unsigned int type;
  union {
    st_lwClipStill still;
    st_lwClipSeq seq;
    st_lwClipAnim anim;
    st_lwClipXRef xref;
    st_lwClipCycle cycle;
  } source;
  float start_time;
  float duration;
  float frame_rate;
  st_lwEParam contrast;
  st_lwEParam brightness;
  st_lwEParam saturation;
  st_lwEParam hue;
  st_lwEParam gamma;
  int negative;
  st_lwPlugin* ifilter;
  int nifilters;
  std::uint8_t padding84[4];
  st_lwPlugin* pfilter;
  int npfilters;
  std::uint8_t padding94[4];
};
using lwClip = st_lwClip;

struct st_lwTMap {
  st_lwVParam size;
  st_lwVParam center;
  st_lwVParam rotate;
  st_lwVParam falloff;
  int fall_type;
  std::uint8_t padding44[4];
  char* ref_object;
  int coord_sys;
  std::uint8_t padding54[4];
};
using lwTMap = st_lwTMap;

struct st_lwImageMap {
  int cindex;
  int projection;
  char* vmap_name;
  int axis;
  int wrapw_type;
  int wraph_type;
  st_lwEParam wrapw;
  st_lwEParam wraph;
  float aa_strength;
  int aas_flags;
  int pblend;
  st_lwEParam stck;
  st_lwEParam amplitude;
};
struct st_lwProcedural { int axis; float value[3]; char* name; void* data; };
struct st_lwGradKey { st_lwGradKey* next; st_lwGradKey* prev; float value; float rgba[4]; };
struct st_lwGradient {
  char* paramname; char* itemname; float start; float end; int repeat;
  std::uint8_t padding1C[4]; st_lwGradKey* key; short* ikey;
};

struct st_lwTexture {
  st_lwTexture* next;
  st_lwTexture* prev;
  char* ord;
  unsigned int type;
  unsigned int chan;
  st_lwEParam opacity;
  short opac_type;
  short enabled;
  short negative;
  short axis;
  struct {
    st_lwImageMap imap;
    st_lwProcedural proc;
    st_lwGradient grad;
  } param;
  st_lwTMap tmap;
};
using lwTexture = st_lwTexture;

struct st_lwTParam { float val; int eindex; st_lwTexture* tex; };
using lwTParam = st_lwTParam;
struct st_lwCParam { float rgb[3]; int eindex; st_lwTexture* tex; };
using lwCParam = st_lwCParam;
struct st_lwRMap { st_lwTParam val; int options; int cindex; float seam_angle; std::uint8_t padding1C[4]; };
using lwRMap = st_lwRMap;
struct st_lwLine { short enabled; unsigned short flags; st_lwEParam size; };
using lwLine = st_lwLine;

struct st_lwSurface {
  st_lwSurface* next;
  st_lwSurface* prev;
  char* name;
  char* srcname;
  st_lwCParam color;
  st_lwTParam luminosity;
  st_lwTParam diffuse;
  st_lwTParam specularity;
  st_lwTParam glossiness;
  st_lwRMap reflection;
  st_lwRMap transparency;
  st_lwTParam eta;
  st_lwTParam translucency;
  st_lwTParam bump;
  float smooth;
  float smoothCos;
  int sideflags;
  float alpha;
  int alpha_mode;
  st_lwEParam color_hilite;
  st_lwEParam color_filter;
  st_lwEParam add_trans;
  st_lwEParam dif_sharp;
  st_lwEParam glow;
  st_lwLine line;
  st_lwPlugin* shader;
  int nshaders;
  std::uint8_t padding13C[4];
};
using lwSurface = st_lwSurface;

struct st_lwVMap {
  st_lwVMap* next;
  st_lwVMap* prev;
  char* name;
  unsigned int type;
  int dim;
  int nverts;
  int perpoly;
  int* vindex;
  int* pindex;
  float** val;
  int offset;
  std::uint8_t padding44[4];
};
using lwVMap = st_lwVMap;
struct st_lwVMapPt { st_lwVMap* vmap; int index; std::uint8_t padding0C[4]; };
using lwVMapPt = st_lwVMapPt;
struct st_lwVMapList { int nvmaps; std::uint8_t padding04[4]; st_lwVMapPt* vm; };
using lwVMapList = st_lwVMapList;

struct st_lwPoint { float pos[3]; };
using lwPoint = st_lwPoint;
struct st_lwPointPoly { int npols; std::uint8_t padding04[4]; int* pol; };
using lwPointPoly = st_lwPointPoly;
struct st_lwPolVert { int index; float norm[3]; };
using lwPolVert = st_lwPolVert;
struct st_lwPolygon {
  st_lwSurface* surf;
  int part;
  int smoothgrp;
  int flags;
  unsigned int type;
  float norm[3];
  int nverts;
  int vindex;
};
using lwPolygon = st_lwPolygon;
struct st_lwPointList {
  int count;
  int offset;
  st_lwPoint* pt;
  st_lwVMapList* ptVMaps;
  st_lwPointPoly* ptPoly;
  int* pointPolyIndices;
};
using lwPointList = st_lwPointList;
struct st_lwPolygonList {
  int count;
  int offset;
  int vcount;
  int voffset;
  st_lwPolygon* pol;
  st_lwPolVert* polVerts;
  st_lwVMapList* polVertVMaps;
};
using lwPolygonList = st_lwPolygonList;

struct st_lwLayer {
  st_lwLayer* next;
  st_lwLayer* prev;
  char* name;
  int index;
  int parent;
  int flags;
  float pivot[3];
  float bbox[6];
  st_lwPointList point;
  st_lwPolygonList polygon;
  int nvmaps;
  std::uint8_t padding9C[4];
  st_lwVMap* vmap;
};
using lwLayer = st_lwLayer;
struct st_lwTagList { int count; int offset; char** tag; };
using lwTagList = st_lwTagList;
struct st_lwObject {
  unsigned int timeStamp;
  std::uint8_t padding04[4];
  st_lwLayer* layer;
  st_lwEnvelope* env;
  st_lwClip* clip;
  st_lwSurface* surf;
  st_lwTagList taglist;
  int nlayers;
  int nenvs;
  int nclips;
  int nsurfs;
};
using lwObject = st_lwObject;

template <typename T> T* lwAlloc();
template <typename T> T* lwAllocArray(int count);

void lwFree(void* pointer);
int compare_keys(st_lwKey* first, st_lwKey* second);
void lwListFree(void* list, void (*freeNode)(void*));
void lwListInsert(void** list, void* item, int (*compare)(void*, void*));
unsigned char getU1(idFile* file);
int getVX(idFile* file);
void lwFreePolygons(st_lwPolygonList* polygons);
void lwGetBoundingBox(st_lwPointList* points, float bbox[6]);
void lwFreePointPolygons(st_lwPointList* points);
void lwFreeTags(st_lwTagList* tags);
void lwFreePlugin(st_lwPlugin* plugin);
void lwFreeTexture(st_lwTexture* texture);
void lwFreeSurface(st_lwSurface* surface);
int compare_shaders(st_lwPlugin* first, st_lwPlugin* second);
int add_texture(st_lwSurface* surface, st_lwTexture* texture);
void lwFreeVMap(st_lwVMap* vmap);
void lwFreeClip(st_lwClip* clip);
void lwFreeEnvelope(st_lwEnvelope* envelope);
void* getbytes(idFile* file, int size);
short getI2(idFile* file);
int getI4(idFile* file);
float getF4(idFile* file);
char* getS0(idFile* file);
char* sgetS0(unsigned char** bytes);
int add_clip(char* text, st_lwClip** clips, int* clipCount);
int add_tvel(float position[3], float velocity[3], st_lwEnvelope** envelopes,
             int* envelopeCount);
st_lwTexture* get_texture(char* text);
st_lwSurface* lwGetSurface5(idFile* file, int chunkSize, st_lwObject* object);
void lwFreePoints(st_lwPointList* points);
int lwAllocPolygons(st_lwPolygonList* polygons, int polygonCount,
                    int vertexCount);
int lwGetPolygons(idFile* file, int chunkSize, st_lwPolygonList* polygons,
                  int pointOffset);
void lwGetPolyNormals(st_lwPointList* points, st_lwPolygonList* polygons);
int lwGetPointPolygons(st_lwPointList* points, st_lwPolygonList* polygons);
void lwForceSmoothSurfaces(st_lwSurface* surfaces, float smooth);
void lwGetVertNormals(st_lwPointList* points, st_lwPolygonList* polygons);
int lwGetTags(idFile* file, int chunkSize, st_lwTagList* tags);
int lwGetPolygonTags(idFile* file, int chunkSize, st_lwTagList* tags,
                     st_lwPolygonList* polygons);
int lwGetTHeader(idFile* file, int headerSize, st_lwTexture* texture);
int lwGetTMap(idFile* file, int mapSize, st_lwTMap* map);
int lwGetImageMap(idFile* file, int size, st_lwTexture* texture);
int lwGetProcedural(idFile* file, int size, st_lwTexture* texture);
int lwGetGradient(idFile* file, int size, st_lwTexture* texture);
st_lwTexture* lwGetTexture(idFile* file, int blockSize, unsigned int type);
st_lwPlugin* lwGetShader(idFile* file, int blockSize);
st_lwSurface* lwDefaultSurface();
st_lwSurface* lwGetSurface(idFile* file, int chunkSize);
st_lwVMap* lwGetVMap(idFile* file, int chunkSize, int pointOffset,
                     int polygonOffset, int perPolygon);
int lwGetPointVMaps(st_lwPointList* points, st_lwVMap* vmaps);
int lwGetPolyVMaps(st_lwPolygonList* polygons, st_lwVMap* vmaps);
st_lwClip* lwGetClip(idFile* file, int chunkSize);
st_lwEnvelope* lwGetEnvelope(idFile* file, int chunkSize);
void lwFreeObject(st_lwObject* object);
int lwGetPolygons5(idFile* file, int chunkSize, st_lwPolygonList* polygons,
                   int pointOffset);
int lwGetPoints(idFile* file, int chunkSize, st_lwPointList* points);
int lwResolvePolySurfaces(st_lwPolygonList* polygons, st_lwTagList* tags,
                          st_lwSurface** surfaces, int* surfaceCount);
st_lwObject* lwGetObject5(const char* filename, float forceSmooth,
                          unsigned int* failId, int* failPosition);
st_lwObject* lwGetObject(const char* filename, float forceSmooth,
                         unsigned int* failId, int* failPosition);

static_assert(sizeof(st_lwKey) == 0x38, "st_lwKey ABI drift");
static_assert(sizeof(st_lwPlugin) == 0x30, "st_lwPlugin ABI drift");
static_assert(sizeof(st_lwEnvelope) == 0x48, "st_lwEnvelope ABI drift");
static_assert(sizeof(st_lwClip) == 0x98, "st_lwClip ABI drift");
static_assert(sizeof(st_lwTexture) == 0x120, "st_lwTexture ABI drift");
static_assert(sizeof(st_lwSurface) == 0x140, "st_lwSurface ABI drift");
static_assert(sizeof(st_lwVMap) == 0x48, "st_lwVMap ABI drift");
static_assert(sizeof(st_lwPointList) == 0x28, "st_lwPointList ABI drift");
static_assert(sizeof(st_lwPolygonList) == 0x28, "st_lwPolygonList ABI drift");
static_assert(sizeof(st_lwLayer) == 0xA8, "st_lwLayer ABI drift");
static_assert(sizeof(st_lwObject) == 0x48, "st_lwObject ABI drift");
