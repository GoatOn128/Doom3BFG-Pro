#pragma once

#include <cstddef>
#include <cstdint>

struct alignas(8) rbitstream {
  std::uint8_t byte;
  std::uint8_t bytePadding[3];
  int bits_avail;
  std::uint64_t dataSize;
  const std::uint8_t* data;
  const std::uint8_t* cur_ptr;
  std::uint64_t read_count;
  std::uint64_t mark_stream_position;
};

struct alignas(8) macroblock_s {
  int* data;
  int* pred_dclp;
  std::uint32_t lp_quant : 8;
  std::uint32_t hp_quant : 8;
  std::int32_t mbhp_pred_mode : 3;
  std::int32_t have_qnt : 1;
  int hp_cbp;
  int hp_diff_cbp;
  unsigned int hp_model_bits[16];
};

struct jxr_strip_t {
  macroblock_s* up4;
  macroblock_s* up3;
  macroblock_s* up2;
  macroblock_s* up1;
  macroblock_s* cur;
  int* upsample_memory_y;
  int* upsample_memory_x;
};

struct model_s {
  int bits[16];
  int state[16];
};

struct cbp_model_s {
  int state[16];
  int count0[16];
  int count1[16];
};

struct jxr_vlc_table_t {
  int discriminant;
  int discriminant2;
  int table;
  int deltatable;
  int delta2table;
};

struct jxr_tile_channel_qp {
  std::uint8_t dc_qp;
  std::uint8_t num_lp;
  std::uint8_t num_hp;
  std::uint8_t lp_qp[16];
  std::uint8_t hp_qp[16];
};

struct jxr_tile_qp {
  int component_mode;
  jxr_tile_channel_qp channel[16];
  std::uint8_t* lp_map;
  std::uint8_t* hp_map;
  std::uint8_t dc_quant_ch[16];
  std::uint8_t lp_quant_ch[16][16];
  std::uint8_t hp_quant_ch[16][16];
};

struct alignas(8) wbitstream {
  std::uint8_t byte;
  std::uint8_t bytePadding[3];
  int bits_ready;
  std::uint64_t dataSize;
  std::uint8_t* data;
  std::uint8_t* cur_ptr;
  std::uint64_t write_count;
  std::uint64_t mark_stream_position;
};

struct jxr_image;
using jxr_io_callback_t = void (*)(jxr_image*, int, int, int*);

struct alignas(8) jxr_image {
  int user_flags;
  unsigned int width1;
  unsigned int height1;
  unsigned int extended_width;
  unsigned int extended_height;
  std::uint8_t header_flags1;
  std::uint8_t header_flags2;
  std::uint8_t header_flags_fmt;
  std::uint8_t headerFormatPadding;
  int output_clr_fmt;
  std::uint8_t use_clr_fmt;
  std::uint8_t colorFormatPadding[3];
  int ePixelFormat;
  // Retail stores trim_flexbits in the low nibble and preserves the other
  // bits when parsing the tile header.
  std::uint32_t trim_flexbits;
  std::uint8_t bands_present;
  std::uint8_t bands_present_of_primary;
  std::uint8_t chroma_centering_x;
  std::uint8_t chroma_centering_y;
  std::uint8_t num_channels;
  std::uint8_t channelPadding[3];
  unsigned int disableTileOverlapFlag;
  unsigned int tile_rows;
  unsigned int tile_columns;
  std::uint8_t tilePointerPadding[4];
  unsigned int* tile_row_height;
  unsigned int* tile_column_width;
  unsigned int* tile_row_height_input;
  unsigned int* tile_column_width_input;
  unsigned int* tile_column_position;
  unsigned int* tile_row_position;
  std::int64_t* tile_index_table;
  std::int64_t tile_index_table_length;
  std::uint16_t window_extra_top;
  std::uint16_t window_extra_left;
  std::uint16_t window_extra_bottom;
  std::uint16_t window_extra_right;
  std::uint32_t component_mode;
  std::uint8_t shift_bits;
  std::uint8_t len_mantissa;
  char exp_bias;
  std::uint8_t quantizerPadding;
  unsigned int num_lp_qps;
  unsigned int num_hp_qps;
  int cur_my;
  std::uint8_t stripPadding[4];
  jxr_strip_t strip[16];
  macroblock_s* mb_row_buffer[16];
  macroblock_s* mb_row_context[16];
  jxr_vlc_table_t vlc_table[20];
  int count_max_CBPLP;
  int count_zero_CBPLP;
  cbp_model_s hp_cbp_model;
  unsigned int lopass_scanorder[15];
  unsigned int lopass_scantotals[15];
  unsigned int hipass_hor_scanorder[15];
  unsigned int hipass_hor_scantotals[15];
  unsigned int hipass_ver_scanorder[15];
  unsigned int hipass_ver_scantotals[15];
  int dc_component_mode;
  int lp_component_mode;
  int hp_component_mode;
  std::uint8_t dc_quant_ch[16];
  std::uint8_t lp_quant_ch[16][16];
  std::uint8_t hp_quant_ch[16][16];
  std::uint8_t tileQuantPadding[4];
  jxr_tile_qp* tile_quant;
  model_s model_dc;
  model_s model_lp;
  model_s model_hp;
  model_s* model_hp_buffer;
  cbp_model_s* hp_cbp_model_buffer;
  jxr_io_callback_t out_fun;
  jxr_io_callback_t inp_fun;
  void* user_data;
  jxr_image* alpha;
  union {
    int primary;
    int default_output_flag;
  };
  std::uint8_t profile_idc;
  std::uint8_t level_idc;
  std::uint8_t lwf_test;
  std::uint8_t profilePadding;
  unsigned int container_width;
  unsigned int container_height;
  std::uint8_t container_nc;
  std::uint8_t container_alpha;
  std::uint8_t container_separate_alpha;
  std::uint8_t containerPadding;
  int container_bpc;
  int container_color;
  std::uint8_t container_image_band_presence;
  std::uint8_t container_alpha_band_presence;
  std::uint8_t container_current_separate_alpha;
  std::uint8_t containerStatePadding;
  rbitstream rbits;
  wbitstream wbits;
  unsigned int stripe_tx;
  unsigned int stripe_ty;
  unsigned int stripe_my;
  unsigned int cleanup_state;
  int freq_buffered_flag;
  int spatial_buffered_flag;
  int output_sent;
  unsigned int spatial_mb_width;
  unsigned int spatial_mb_height;
  void* temp_buffer;
  unsigned int temp_buffer_size;
  unsigned int temp_buffer_used;
};

void _jxr_send_mb_to_output(jxr_image* image, int mx, int my, int* data);
int jxr_image_size();
jxr_image* jxr_create_input(void* buffer);
void* _jxr_temp_calloc(jxr_image* image, std::uint64_t count,
                       std::uint64_t size);
void jxr_set_temp_memory(jxr_image* image, void* buffer, std::uint64_t size,
                         std::uint64_t used);
void make_mb_row_buffer(jxr_image* image, unsigned int use_height);
void _jxr_make_mbstore(jxr_image* image, int up4_flag);
void backup_dc_strip(jxr_image* image, int tileX, int tileY,
                     int macroblockY);
void backup_dclp_strip(jxr_image* image, int tileX, int tileY,
                       int macroblockY);
void backup_hp_strip(jxr_image* image, int tileX, int tileY,
                     int macroblockY);
void recover_dc_strip(jxr_image* image, int tileX, int tileY,
                      int macroblockY);
void recover_dclp_strip(jxr_image* image, int tileX, int tileY,
                        int macroblockY);
void recover_dclphp_strip(jxr_image* image, int tileX, int tileY,
                          int macroblockY);
int _jxr_frequency_mode_render_stripe(jxr_image* image);
int _jxr_r_TILE_DC(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int tileY);
int _jxr_r_TILE_LP(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int tileY);
int _jxr_r_TILE_HP(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int tileY);
int _jxr_r_TILE_FLEXBITS(jxr_image* image, rbitstream* stream,
                         unsigned int tileX, unsigned int tileY);
int _jxr_r_TILE_FLEXBITS_ESCAPE(jxr_image* image, unsigned int tileX,
                                unsigned int tileY);
int jxr_get_TILING_FLAG(jxr_image* image);
void _jxr_clear_strip_cur(jxr_image* image);
int _jxr_r_TILE_SPATIAL_decode_mb(jxr_image* image, rbitstream* stream,
                                  unsigned int tileX, unsigned int tileY,
                                  int macroblockX, int macroblockY);
int _jxr_r_TILE_SPATIAL_stripe(jxr_image* image, rbitstream* stream,
                               unsigned int tileX, unsigned int tileY);
void _jxr_rbitstream_mark(rbitstream* stream);
void _jxr_rbitstream_seek(rbitstream* stream, std::uint64_t offset);
void _jxr_rbitstream_syncbyte(rbitstream* stream);
bool _jxr_rbitstream_uint1(rbitstream* stream);
std::uint8_t _jxr_rbitstream_uint2(rbitstream* stream);
std::uint8_t _jxr_rbitstream_uint3(rbitstream* stream);
std::uint8_t _jxr_rbitstream_uint4(rbitstream* stream);
std::uint8_t _jxr_rbitstream_uint6(rbitstream* stream);
std::uint8_t _jxr_rbitstream_uint8(rbitstream* stream);
std::uint16_t _jxr_rbitstream_uint12(rbitstream* stream);
std::uint16_t _jxr_rbitstream_uint15(rbitstream* stream);
std::uint16_t _jxr_rbitstream_uint16(rbitstream* stream);
std::uint32_t _jxr_rbitstream_uint32(rbitstream* stream);
std::uint32_t _jxr_rbitstream_uintN(rbitstream* stream, int bitCount);
int _jxr_rbitstream_intE(rbitstream* stream, int codeSize,
                         const std::uint8_t* codeLengths,
                         const char* values);
std::uint64_t _jxr_rbitstream_intVLW(rbitstream* stream);
int _jxr_floor_div2(int x);
int _jxr_ceil_div2(int x);
int _jxr_quant_map(jxr_image* image, int x, int shift);
int _jxr_vlc_select(int band, int chromaFlag);
void _jxr_InitVLCTable(jxr_image* image, int vlcSelect);
void _jxr_AdaptVLCTable(jxr_image* image, int vlcSelect);
void AdaptVLCTable2(jxr_image* image, int vlcSelect, int maxIndex);
void _jxr_AdaptLP(jxr_image* image);
void _jxr_AdaptHP(jxr_image* image);
int _jxr_InitContext(jxr_image* image, unsigned int tileX,
                     unsigned int tileY, unsigned int macroblockX,
                     unsigned int macroblockY);
int _jxr_ResetContext(jxr_image* image, unsigned int tileX,
                      unsigned int macroblockX);
int _jxr_ResetTotals(jxr_image* image, char macroblockX);
void _jxr_InitializeModelMB(model_s* model, int band);
void _jxr_UpdateModelMB(jxr_image* image, char* lapMean, model_s* model,
                        int band);
void _jxr_InitializeCountCBPLP(jxr_image* image);
void _jxr_UpdateCountCBPLP(jxr_image* image, int cbplp, int maximum);
void _jxr_InitLPVLC(jxr_image* image);
void _jxr_InitializeAdaptiveScanLP(jxr_image* image);
void _jxr_InitializeAdaptiveScanHP(jxr_image* image);
void _jxr_InitializeCBPModel(jxr_image* image);
void _jxr_InitHPVLC(jxr_image* image);
void _jxr_InitCBPVLC(jxr_image* image);
void update_cbp_model(jxr_image* image, int channel, int originalCount);
int _jxr_PredCBP444(jxr_image* image, int* diffCbp, int channel,
                    unsigned int tileX, unsigned int macroblockX,
                    unsigned int macroblockY);
int _jxr_PredCBP422(jxr_image* image, int* diffCbp, int channel,
                    unsigned int tileX, unsigned int macroblockX,
                    unsigned int macroblockY);
int _jxr_PredCBP420(jxr_image* image, int* diffCbp, int channel,
                    unsigned int tileX, unsigned int macroblockX,
                    unsigned int macroblockY);
void _jxr_ResetTotalsAdaptiveScanLP(jxr_image* image);
void _jxr_ResetTotalsAdaptiveScanHP(jxr_image* image);
int calculate_mbdc_mode(jxr_image* image, int channel, int tileX,
                        int macroblockX, int macroblockY);
void predict_lp444(jxr_image* image, int tileX, int macroblockX, int channel,
                   int mode);
void predict_lp422(jxr_image* image, int tileX, int macroblockX, int channel,
                   int mode, int dcMode);
void predict_lp420(jxr_image* image, int tileX, int macroblockX, int channel,
                   int mode);
void _InvPermute(int* coefficients);
void _jxr_InvPermute2pt(int* a, int* b);
void _jxr_2ptT(int* a, int* b);
void _jxr_2x2IPCT(int* coefficients);
void _jxr_4x4IPCT(int* coefficients);
void _jxr_4x4OverlapFilter(int* a, int* b, int* c, int* d, int* e, int* f,
                           int* g, int* h, int* i, int* j, int* k, int* l,
                           int* m, int* n, int* o, int* p);
void _jxr_4OverlapFilter(int* a, int* b, int* c, int* d);
void _jxr_2x2OverlapFilter(int* a, int* b, int* c, int* d);
void _jxr_2OverlapFilter(int* a, int* b);
extern std::uint8_t long_word_flag;
int _jxr_read_lwf_test_flag();
void _jxr_complete_cur_dclp(jxr_image* image, int tileX, int macroblockX,
                            int macroblockY);
void jxr_output_mb_id_mono(jxr_image* image, int macroblockX,
                           std::uint8_t* buffer, int byteStride);
void jxr_output_mb_id_grayscale(jxr_image* image, int macroblockX,
                                std::uint8_t* buffer, int byteStride);
void jxr_output_mb_id_rgb(jxr_image* image, int macroblockX,
                          std::uint8_t* buffer, int byteStride);
void jxr_output_mb_id_cocg_y(jxr_image* image, int macroblockX,
                             std::uint8_t* buffer, int byteStride);
void jxr_output_mb_id_normal_y_x(jxr_image* image, int macroblockX,
                                 std::uint8_t* buffer, int byteStride);
int r_image_header(jxr_image* image, rbitstream* stream);
int _jxr_r_DC_QP(jxr_image* image, rbitstream* stream);
int _jxr_r_LP_QP(jxr_image* image, rbitstream* stream);
int r_HP_QP(jxr_image* image, rbitstream* stream);
int r_INDEX_TABLE(jxr_image* image, rbitstream* stream);
int r_PROFILE_LEVEL_INFO(jxr_image* image, rbitstream* stream);
int r_TILE_stripe(jxr_image* image, rbitstream* stream);
void _jxr_r_TILE_HEADER_DC(jxr_image* image, rbitstream* stream,
                           int alphaFlag, unsigned int tileX,
                           unsigned int tileY);
void _jxr_r_TILE_HEADER_LOWPASS(jxr_image* image, rbitstream* stream,
                                int alphaFlag, unsigned int tileX,
                                unsigned int tileY);
void _jxr_r_TILE_HEADER_HIGHPASS(jxr_image* image, rbitstream* stream,
                                 int alphaFlag, unsigned int tileX,
                                 unsigned int tileY);
unsigned int _jxr_DECODE_QP_INDEX(rbitstream* stream,
                                  unsigned int indexCount);
int r_REFINE_CBP(rbitstream* stream, int count);
int r_DECODE_FIRST_INDEX(jxr_image* image, rbitstream* stream,
                         int chromaFlag, int band);
int r_DECODE_INDEX(jxr_image* image, rbitstream* stream, int location,
                   int chromaFlag, int band, int context);
int r_DECODE_RUN(rbitstream* stream, int maximumRun);
int r_DECODE_ABS_LEVEL(jxr_image* image, rbitstream* stream, int band,
                       int chromaFlag);
int r_DECODE_BLOCK(jxr_image* image, rbitstream* stream, int chromaFlag,
                   int* coefficients, int band, int location);
int r_DECODE_BLOCK_ADAPTIVE(jxr_image* image, rbitstream* stream, int tileX,
                            unsigned int macroblockX, int cbpFlag,
                            int chromaFlag, int channel, int block,
                            int macroblockHpMode, char modelBits);
void r_PredCBP(jxr_image* image, int* differentialCbp, unsigned int tileX,
               unsigned int tileY, unsigned int macroblockX,
               unsigned int macroblockY);
void AdaptiveHPScan(jxr_image* image, int* hpInput, int scanIndex, int value,
                    int macroblockHpMode);
void r_DECODE_FLEX(jxr_image* image, rbitstream* stream, unsigned int tileX,
                   unsigned int macroblockX, int channel, unsigned int block,
                   unsigned int coefficient, unsigned int flexbits);
void r_BLOCK_FLEXBITS(jxr_image* image, rbitstream* stream, int tileX,
                      unsigned int macroblockX, int channel,
                      unsigned int block, unsigned int modelBits);
int r_calculate_mbhp_mode(jxr_image* image, int tileX, int macroblockX,
                          int channel);
int get_is_dc_yuv(rbitstream* stream);
int jxr_read_stripe_bitstream(jxr_image* image);
int r_image_plane_header(jxr_image* image, rbitstream* stream, int alpha);
int jxr_init_read_stripe_bitstream(jxr_image* image, const std::uint8_t* data,
                                   std::uint64_t size);
void _jxr_propagate_hp_predictions(jxr_image* image, int channel,
                                   int tileX, int macroblockX,
                                   int macroblockHpMode);
void _jxr_r_MB_DC(jxr_image* image, rbitstream* stream, int alphaFlag,
                  unsigned int tileX, unsigned int tileY,
                  unsigned int macroblockX, unsigned int macroblockY);
void _jxr_r_MB_LP(jxr_image* image, rbitstream* stream, int alphaFlag,
                  int tileX, unsigned int tileY, unsigned int macroblockX,
                  unsigned int macroblockY);
int _jxr_r_MB_CBP(jxr_image* image, rbitstream* stream, int alphaFlag,
                  unsigned int tileX, unsigned int tileY,
                  unsigned int macroblockX, unsigned int macroblockY);
int _jxr_r_MB_HP(jxr_image* image, rbitstream* stream, int alphaFlag,
                 int tileX, unsigned int tileY, unsigned int macroblockX,
                 unsigned int macroblockY);
int _jxr_r_MB_FLEXBITS(jxr_image* image, rbitstream* stream, int alphaFlag,
                       int tileX, unsigned int tileY,
                       unsigned int macroblockX,
                       unsigned int macroblockY);

static_assert(sizeof(macroblock_s) == 0x60, "macroblock_s ABI drift");
static_assert(sizeof(rbitstream) == 0x30, "rbitstream ABI drift");
static_assert(offsetof(rbitstream, data) == 0x10,
              "rbitstream data offset drift");
static_assert(offsetof(rbitstream, cur_ptr) == 0x18,
              "rbitstream cursor offset drift");
static_assert(sizeof(jxr_strip_t) == 0x38, "jxr strip ABI drift");
static_assert(sizeof(model_s) == 0x80, "JXR model ABI drift");
static_assert(sizeof(cbp_model_s) == 0xC0, "JXR CBP model ABI drift");
static_assert(sizeof(jxr_vlc_table_t) == 0x14, "JXR VLC table ABI drift");
static_assert(sizeof(jxr_tile_channel_qp) == 0x23,
              "JXR tile-channel quantizer ABI drift");
static_assert(sizeof(jxr_tile_qp) == 0x458,
              "JXR tile quantizer ABI drift");
static_assert(sizeof(wbitstream) == 0x30, "wbitstream ABI drift");
static_assert(offsetof(jxr_image, header_flags1) == 0x14,
              "jxr header flags offset drift");
static_assert(offsetof(jxr_image, use_clr_fmt) == 0x1C,
              "jxr color format offset drift");
static_assert(offsetof(jxr_image, trim_flexbits) == 0x24,
              "jxr flexbits trim offset drift");
static_assert(offsetof(jxr_image, num_channels) == 0x2C,
              "jxr channel count offset drift");
static_assert(offsetof(jxr_image, cur_my) == 0x98,
              "jxr current macroblock row offset drift");
static_assert(offsetof(jxr_image, tile_column_position) == 0x60,
              "jxr tile column position offset drift");
static_assert(offsetof(jxr_image, tile_row_position) == 0x68,
              "jxr tile row position offset drift");
static_assert(offsetof(jxr_image, tile_index_table) == 0x70,
              "jxr tile index table offset drift");
static_assert(offsetof(jxr_image, component_mode) == 0x88,
              "jxr quantizer flags offset drift");
static_assert(offsetof(jxr_image, strip) == 0xA0,
              "jxr strip table offset drift");
static_assert(offsetof(jxr_image, strip[0].cur) == 0xC0,
              "jxr current strip offset drift");
static_assert(offsetof(jxr_image, mb_row_buffer) == 0x420,
              "jxr row buffer offset drift");
static_assert(offsetof(jxr_image, mb_row_context) == 0x4A0,
              "jxr row context offset drift");
static_assert(offsetof(jxr_image, vlc_table) == 0x520,
              "jxr VLC table offset drift");
static_assert(offsetof(jxr_image, count_max_CBPLP) == 0x6B0,
              "jxr LP count offset drift");
static_assert(offsetof(jxr_image, hp_cbp_model) == 0x6B8,
              "jxr CBP model offset drift");
static_assert(offsetof(jxr_image, lopass_scanorder) == 0x778,
              "jxr LP scan order offset drift");
static_assert(offsetof(jxr_image, hipass_hor_scanorder) == 0x7F0,
              "jxr HP horizontal scan order offset drift");
static_assert(offsetof(jxr_image, hipass_ver_scanorder) == 0x868,
              "jxr HP vertical scan order offset drift");
static_assert(offsetof(jxr_image, dc_component_mode) == 0x8E0,
              "jxr DC component mode offset drift");
static_assert(offsetof(jxr_image, dc_quant_ch) == 0x8EC,
              "jxr DC quantizer offset drift");
static_assert(offsetof(jxr_image, lp_quant_ch) == 0x8FC,
              "jxr LP quantizer offset drift");
static_assert(offsetof(jxr_image, hp_quant_ch) == 0x9FC,
              "jxr HP quantizer offset drift");
static_assert(offsetof(jxr_image, tile_quant) == 0xB00,
              "jxr tile quantizer state offset drift");
static_assert(offsetof(jxr_image, model_dc) == 0xB08,
              "jxr DC model offset drift");
static_assert(offsetof(jxr_image, model_lp) == 0xB88,
              "jxr LP model offset drift");
static_assert(offsetof(jxr_image, model_hp) == 0xC08,
              "jxr HP model offset drift");
static_assert(offsetof(jxr_image, model_hp_buffer) == 0xC88,
              "jxr HP model buffer offset drift");
static_assert(offsetof(jxr_image, hp_cbp_model_buffer) == 0xC90,
              "jxr HP CBP model buffer offset drift");
static_assert(offsetof(jxr_image, out_fun) == 0xC98,
              "jxr output callback offset drift");
static_assert(offsetof(jxr_image, alpha) == 0xCB0,
              "jxr alpha plane offset drift");
static_assert(offsetof(jxr_image, rbits) == 0xCD8,
              "jxr read stream offset drift");
static_assert(offsetof(jxr_image, wbits) == 0xD08,
              "jxr write stream offset drift");
static_assert(offsetof(jxr_image, stripe_tx) == 0xD38,
              "jxr stripe tile column offset drift");
static_assert(offsetof(jxr_image, stripe_ty) == 0xD3C,
              "jxr stripe tile row offset drift");
static_assert(offsetof(jxr_image, stripe_my) == 0xD40,
              "jxr stripe state offset drift");
static_assert(offsetof(jxr_image, output_sent) == 0xD50,
              "jxr output state offset drift");
static_assert(offsetof(jxr_image, temp_buffer) == 0xD60,
              "jxr temporary buffer offset drift");
static_assert(sizeof(jxr_image) == 0xD70, "jxr image ABI drift");
