#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <cstdint>
#include <cstring>

using _QWORD = std::uint64_t;

namespace {

const __m128i vector_short_1_0 = _mm_set1_epi16(1);
const __m128i vector_short_2 = _mm_set1_epi16(2);
const __m128i vector_short_3 = _mm_set1_epi16(3);
const __m128i vector_short_4 = _mm_set1_epi16(4);
const __m128i vector_short_6 = _mm_set1_epi16(6);
const __m128i vector_short_8 = _mm_set1_epi16(8);
const __m128i vector_short_16 = _mm_set1_epi16(16);
const __m128i vector_short_255_0 = _mm_set1_epi16(255);

__m128i LoadPair64(const void* lowSource, const void* highSource) {
  const __m128i low = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(lowSource));
  const __m128i high = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(highSource));
  return _mm_unpacklo_epi64(low, high);
}

__m128i LoadPair16(const std::int16_t* source, std::ptrdiff_t lowIndex, std::ptrdiff_t highIndex) {
  return LoadPair64(source + lowIndex, source + highIndex);
}

void StoreHigh64(double* destination, __m128i value) {
  const std::uint64_t high =
      static_cast<std::uint64_t>(_mm_cvtsi128_si64(_mm_srli_si128(value, 8)));
  std::memcpy(destination, &high, sizeof(high));
}

} // namespace

void InvTransform4x4Stage1_4(__m128i* p) {
  __m128i si128;
  __m128i v2;
  __m128i v3;
  __m128i v4;
  __m128i v5;
  __m128i v6;
  __m128i v7;
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;
  __m128i v40;
  __m128i v41;
  __m128i v42;
  __m128i v43;
  __m128i v44;
  __m128i v45;
  __m128i v46;
  __m128i v47;
  __m128i v48;
  __m128i v49;
  __m128i v50;
  __m128i v51;
  __m128i v52;
  __m128i v53;
  __m128i v54;
  __m128i v55;
  __m128i v56;
  __m128i v57;
  __m128i v58;
  __m128i v59;
  __m128i v60;
  __m128i v61;
  __m128i v62;
  __m128i v63;
  __m128i v64;
  __m128i v65;
  __m128i v66;
  __m128i v67;
  __m128i v68;
  __m128i v69;
  __m128i v70;
  __m128i v71;
  __m128i v72;
  __m128i v73;
  __m128i v74;
  __m128i v75;
  __m128i v76;
  __m128i v77;
  __m128i v78;
  __m128i v79;
  __m128i v80;
  __m128i v81;
  __m128i v82;
  __m128i v83;
  __m128i v84;
  __m128i v85;
  __m128i v86;
  __m128i v87;
  __m128i v88;
  __m128i v89;
  __m128i v90;
  __m128i v91;
  __m128i v92;
  __m128i v93;
  __m128i v94;
  __m128i v95;
  __m128i v96;
  __m128i v97;
  __m128i v98;
  __m128i v99;
  __m128i v100;
  __m128i v101;
  __m128i v102;
  __m128i v103;
  __m128i v104;
  __m128i v105;
  __m128i v106;
  __m128i v107;
  __m128i v108;
  __m128i v109;
  __m128i v110;
  __m128i v111;
  __m128i v112;
  __m128i v113;
  __m128i v114;
  __m128i v115;
  __m128i v116;
  __m128i v117;

  si128 = _mm_load_si128(p + 2);
  v2 = _mm_load_si128(p + 10);
  v3 = _mm_load_si128(p + 3);
  v4 = _mm_load_si128(p + 11);
  v5 = _mm_load_si128(p);
  v6 = _mm_unpackhi_epi16(v5, si128);
  v7 = _mm_unpacklo_epi16(v5, si128);
  v8 = _mm_load_si128(p + 8);
  v9 = _mm_unpackhi_epi16(v8, v2);
  v10 = _mm_unpacklo_epi16(v8, v2);
  v11 = _mm_load_si128(p + 9);
  v12 = _mm_unpackhi_epi16(v11, v4);
  v13 = _mm_unpackhi_epi32(v10, v9);
  v14 = _mm_unpacklo_epi32(v10, v9);
  v15 = v7;
  v16 = _mm_unpackhi_epi32(v7, v6);
  v17 = _mm_unpacklo_epi32(v15, v6);
  v18 = _mm_load_si128(p + 1);
  v19 = v16;
  v20 = _mm_unpackhi_epi32(v16, v13);
  v21 = _mm_unpackhi_epi16(v18, v3);
  v22 = v17;
  v23 = _mm_unpackhi_epi32(v17, v14);
  v24 = _mm_unpacklo_epi32(v19, v13);
  v25 = _mm_unpacklo_epi16(v18, v3);
  v26 = _mm_load_si128(&vector_short_1_0);
  v27 = _mm_unpacklo_epi32(v22, v14);
  v28 = _mm_unpackhi_epi64(v23, v23);
  v29 = _mm_unpacklo_epi16(v11, v4);
  v115 = _mm_sub_epi16(v23, v24);
  v114 = v27;
  v30 = _mm_unpackhi_epi32(v29, v12);
  v31 = _mm_unpacklo_epi32(v29, v12);
  v32 = _mm_unpacklo_epi32(v25, v21);
  v33 = _mm_unpackhi_epi32(v25, v21);
  v34 = _mm_unpackhi_epi32(v32, v31);
  v35 = _mm_unpacklo_epi32(v32, v31);
  v36 = _mm_unpacklo_epi32(v33, v30);
  v37 = _mm_unpackhi_epi32(v33, v30);
  v116 = _mm_unpackhi_epi64(v27, v27);
  v38 = _mm_unpackhi_epi64(v36, v36);
  v39 = _mm_unpackhi_epi64(v37, v37);
  v114 = _mm_add_epi16(_mm_load_si128(&v114), v20);
  v40 = _mm_load_si128(&v115);
  v41 = _mm_srai_epi16(_mm_sub_epi16(_mm_add_epi16(v114, v26), v40), 1u);
  v42 = _mm_unpacklo_epi64(_mm_unpackhi_epi64(v24, v24), v34);
  v43 = _mm_unpacklo_epi64(_mm_unpackhi_epi64(v20, v20), v37);
  v44 = _mm_load_si128(&vector_short_4);
  v45 = _mm_sub_epi16(v41, v20);
  v46 = _mm_sub_epi16(_mm_unpacklo_epi64(v28, v36), v43);
  v47 = _mm_add_epi16(_mm_unpackhi_epi64(v35, v35), v39);
  v48 = _mm_load_si128(&v116);
  v117 = v45;
  v115 = _mm_add_epi16(v40, v45);
  v49 = _mm_add_epi16(_mm_unpacklo_epi64(v48, v35), v42);
  v50 = _mm_sub_epi16(_mm_unpackhi_epi64(v34, v34), v38);
  v51 = _mm_srai_epi16(v47, 1u);
  v52 = _mm_sub_epi16(v42, _mm_srai_epi16(v49, 1u));
  v53 = _mm_add_epi16(v43, _mm_srai_epi16(_mm_add_epi16(v46, v26), 1u));
  v54 = _mm_sub_epi16(v41, v24);
  v55 = _mm_sub_epi16(
      v46, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v49, 1u), v49), v44), 3u));
  v56 = _mm_srai_epi16(_mm_add_epi16(v55, v26), 1u);
  v57 = _mm_add_epi16(
      v49, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v55, 1u), v55), v44), 3u));
  v58 = _mm_sub_epi16(
      v53, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v52, 1u), v52), v44), 3u));
  v59 = _mm_srai_epi16(_mm_add_epi16(v57, v26), 1u);
  v60 = _mm_srai_epi16(v50, 1u);
  v61 = _mm_add_epi16(v38, v60);
  v62 = _mm_add_epi16(_mm_slli_epi16(v58, 1u), v58);
  v63 = _mm_sub_epi16(v58, v59);
  v64 = _mm_add_epi16(v57, v63);
  v65 = _mm_sub_epi16(v56, _mm_add_epi16(v52, _mm_srai_epi16(_mm_add_epi16(v62, v44), 3u)));
  v66 = _mm_sub_epi16(
      _mm_sub_epi16(v39, v51),
      _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v61, 1u), v61), vector_short_3),
                     3u));
  v67 = _mm_sub_epi16(v55, v65);
  v68 = _mm_add_epi16(
      v61, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v66, 1u), v66), vector_short_3), 2u));
  v69 = _mm_sub_epi16(v60, v68);
  v70 = _mm_add_epi16(
      v51,
      _mm_sub_epi16(v66, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v68, 1u), v68), v44), 3u)));
  v71 = _mm_unpacklo_epi64(_mm_unpackhi_epi64(v64, v64), _mm_unpackhi_epi64(v65, v65));
  v72 = _mm_sub_epi16(_mm_unpacklo_epi64(v64, v67), v71);
  v73 = _mm_sub_epi16(v69, v50);
  v74 = _mm_unpacklo_epi64(v69, v70);
  v75 = _mm_unpacklo_epi64(_mm_sub_epi16(v47, v70), v73);
  v76 = _mm_add_epi16(_mm_unpacklo_epi64(_mm_sub_epi16(_mm_load_si128(&v114), v54), v115), v75);
  v77 = _mm_unpacklo_epi64(_mm_unpackhi_epi64(v67, v67), _mm_unpackhi_epi64(v63, v63));
  v78 = _mm_sub_epi16(_mm_unpacklo_epi64(v65, v63), v77);
  v79 = _mm_srai_epi16(_mm_sub_epi16(v76, v72), 1u);
  v80 = _mm_sub_epi16(v79, v71);
  v81 = _mm_sub_epi16(v79, v75);
  v82 = _mm_sub_epi16(v76, v80);
  v83 = _mm_add_epi16(v72, v81);
  v84 = _mm_unpackhi_epi64(v81, v80);
  v85 = _mm_add_epi16(_mm_unpacklo_epi64(_mm_load_si128(&v117), v54), v74);
  v86 = _mm_unpacklo_epi64(v81, v80);
  v87 = _mm_unpacklo_epi64(v82, v83);
  v88 = _mm_unpackhi_epi64(v82, v83);
  v89 = _mm_unpacklo_epi16(v87, v88);
  v90 = _mm_unpackhi_epi16(v87, v88);
  v91 = _mm_srai_epi16(_mm_sub_epi16(v85, v78), 1u);
  v92 = _mm_sub_epi16(v91, v77);
  v93 = _mm_sub_epi16(v91, v74);
  v94 = _mm_add_epi16(v78, v93);
  v95 = _mm_sub_epi16(v85, v92);
  v96 = _mm_unpacklo_epi64(v93, v92);
  v97 = _mm_unpackhi_epi64(v95, v94);
  v98 = _mm_unpacklo_epi64(v95, v94);
  v99 = _mm_unpackhi_epi64(v93, v92);
  v100 = _mm_unpacklo_epi16(v86, v84);
  v101 = _mm_unpackhi_epi16(v86, v84);
  v102 = _mm_unpacklo_epi16(v98, v97);
  v103 = _mm_unpackhi_epi16(v98, v97);
  v104 = _mm_unpacklo_epi16(v96, v99);
  v105 = _mm_unpackhi_epi16(v96, v99);
  v106 = _mm_unpacklo_epi32(v89, v90);
  v107 = _mm_unpacklo_epi32(v102, v103);
  v108 = _mm_unpackhi_epi32(v102, v103);
  v109 = _mm_unpacklo_epi32(v104, v105);
  v110 = _mm_unpacklo_epi32(v100, v101);
  v111 = _mm_unpackhi_epi32(v100, v101);
  v112 = _mm_unpackhi_epi32(v104, v105);
  *p = _mm_unpacklo_epi32(v106, v107);
  p[2] = _mm_unpackhi_epi32(v106, v107);
  v113 = _mm_unpackhi_epi32(v89, v90);
  p[8] = _mm_unpacklo_epi32(v113, v108);
  p[10] = _mm_unpackhi_epi32(v113, v108);
  p[1] = _mm_unpacklo_epi32(v110, v109);
  p[3] = _mm_unpackhi_epi32(v110, v109);
  p[9] = _mm_unpacklo_epi32(v111, v112);
  p[11] = _mm_unpackhi_epi32(v111, v112);
}

void InvTransform4x4Stage1_8(__m128i* p0, __m128i* p1) {
  __m128i si128;
  __m128i v3;
  __m128i v4;
  __m128i v5;
  __m128i v6;
  __m128i v7;
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;
  __m128i v40;
  __m128i v41;
  __m128i v42;
  __m128i v43;
  __m128i v44;
  __m128i v45;
  __m128i v46;
  __m128i v47;
  __m128i v48;
  __m128i v49;
  __m128i v50;
  __m128i v51;
  __m128i v52;
  __m128i v53;
  __m128i v54;
  __m128i v55;
  __m128i v56;
  __m128i v57;
  __m128i v58;
  __m128i v59;
  __m128i v60;
  __m128i v61;
  __m128i v62;
  __m128i v63;
  __m128i v64;
  __m128i v65;
  __m128i v66;
  __m128i v67;
  __m128i v68;
  __m128i v69;
  __m128i v70;
  __m128i v71;
  __m128i v72;
  __m128i v73;
  __m128i v74;
  __m128i v75;
  __m128i v76;
  __m128i v77;
  __m128i v78;
  __m128i v79;
  __m128i v80;
  __m128i v81;
  __m128i v82;
  __m128i v83;
  __m128i v84;
  __m128i v85;
  __m128i v86;
  __m128i v87;
  __m128i v88;
  __m128i v89;
  __m128i v90;
  __m128i v91;
  __m128i v92;
  __m128i v93;
  __m128i v94;
  __m128i v95;
  __m128i v96;
  __m128i v97;
  __m128i v98;
  __m128i v99;
  __m128i v100;
  __m128i v101;
  __m128i v102;
  __m128i v103;
  __m128i v104;
  __m128i v105;
  __m128i v106;
  __m128i v107;
  __m128i v108;
  __m128i v109;
  __m128i v110;
  __m128i v111;
  __m128i v112;
  __m128i v113;
  __m128i v114;
  __m128i v115;
  __m128i v116;
  __m128i v117;
  __m128i v118;
  __m128i v119;
  __m128i v120;
  __m128i v121;
  __m128i v122;
  __m128i v123;
  __m128i v124;
  __m128i v125;
  __m128i v126;
  __m128i v127;
  __m128i v128;
  __m128i v129;
  __m128i v130;
  __m128i v131;
  __m128i v132;
  __m128i v133;
  __m128i v134;
  __m128i v135;
  __m128i v136;
  __m128i v137;
  __m128i v138;
  __m128i v139;
  __m128i v140;
  __m128i v141;
  __m128i v142;
  __m128i v143;
  __m128i v144;
  __m128i v145;
  __m128i v146;
  __m128i v147;
  __m128i v148;
  __m128i v149;
  __m128i v150;
  __m128i v151;
  __m128i v152;
  __m128i v153;
  __m128i v154;
  __m128i v155;
  __m128i v156;
  __m128i v157;
  __m128i v158;
  __m128i v159;
  __m128i v160;
  __m128i v161;
  __m128i v162;
  __m128i v163;
  __m128i v164;
  __m128i v165;
  __m128i v166;
  __m128i v167;
  __m128i v168;
  __m128i v169;
  __m128i v170;
  __m128i v171;
  __m128i v172;
  __m128i v173;
  __m128i v174;
  __m128i v175;
  __m128i v176;
  __m128i v177;
  __m128i v178;
  __m128i v179;
  __m128i v180;
  __m128i v181;
  __m128i v182;
  __m128i v183;
  __m128i v184;
  __m128i v185;
  __m128i v186;
  __m128i v187;
  __m128i v188;
  __m128i v189;
  __m128i v190;
  __m128i v191;
  __m128i v192;
  __m128i v193;
  __m128i v194[11];

  si128 = _mm_load_si128(p0 + 10);
  v3 = _mm_load_si128(p0 + 2);
  v4 = _mm_load_si128(p1 + 10);
  v5 = _mm_load_si128(p1 + 2);
  v6 = _mm_load_si128(p1);
  v7 = _mm_load_si128(p0 + 8);
  v8 = _mm_unpackhi_epi16(v7, si128);
  v9 = _mm_unpacklo_epi16(v7, si128);
  v10 = _mm_load_si128(p0 + 1);
  v11 = _mm_load_si128(p0);
  v12 = _mm_load_si128(p1 + 8);
  v13 = _mm_unpackhi_epi16(v11, v3);
  v14 = _mm_unpackhi_epi16(v12, v4);
  v15 = _mm_unpacklo_epi16(v11, v3);
  v16 = _mm_unpacklo_epi16(v12, v4);
  v17 = v8;
  v18 = v9;
  v19 = _mm_unpacklo_epi16(v6, v5);
  v20 = _mm_unpackhi_epi32(v9, v16);
  v21 = _mm_unpackhi_epi16(v6, v5);
  v22 = _mm_load_si128(p1 + 3);
  v23 = _mm_unpacklo_epi32(v15, v19);
  v24 = _mm_unpackhi_epi32(v15, v19);
  v25 = _mm_unpackhi_epi32(v8, v14);
  v189 = v23;
  v26 = _mm_load_si128(p1 + 11);
  v27 = _mm_unpacklo_epi32(v18, v16);
  v28 = _mm_unpacklo_epi32(v17, v14);
  v29 = _mm_load_si128(p0 + 11);
  v30 = _mm_load_si128(p0 + 9);
  v31 = _mm_unpackhi_epi32(v13, v21);
  v32 = _mm_unpacklo_epi32(v13, v21);
  v33 = _mm_unpackhi_epi32(v31, v25);
  v34 = _mm_unpacklo_epi32(v31, v25);
  v35 = _mm_load_si128(p1 + 1);
  v187 = v33;
  v186 = v32;
  v36 = _mm_load_si128(p0 + 3);
  v37 = v24;
  v38 = _mm_unpackhi_epi32(v24, v20);
  v39 = _mm_unpacklo_epi16(v10, v36);
  v40 = _mm_unpackhi_epi16(v10, v36);
  v41 = _mm_unpacklo_epi32(v37, v20);
  v42 = _mm_load_si128(p1 + 9);
  v43 = _mm_unpacklo_epi16(v30, v29);
  v44 = _mm_unpacklo_epi16(v35, v22);
  v45 = _mm_unpackhi_epi16(v35, v22);
  v46 = _mm_unpackhi_epi16(v30, v29);
  v47 = _mm_unpacklo_epi32(v40, v45);
  v190 = _mm_unpackhi_epi32(v40, v45);
  v48 = _mm_unpacklo_epi16(v42, v26);
  v49 = _mm_unpackhi_epi16(v42, v26);
  v193 = _mm_unpacklo_epi32(v39, v44);
  v192 = _mm_unpackhi_epi32(v39, v44);
  v50 = _mm_load_si128(&v192);
  v51 = _mm_unpacklo_epi32(v43, v48);
  v52 = _mm_unpackhi_epi32(v43, v48);
  v53 = _mm_load_si128(&v189);
  v54 = v46;
  v55 = _mm_unpackhi_epi32(v46, v49);
  v56 = _mm_unpackhi_epi32(_mm_load_si128(&v190), v55);
  v57 = _mm_unpacklo_epi32(v54, v49);
  v58 = _mm_unpacklo_epi32(v190, v55);
  v59 = _mm_load_si128(&vector_short_4);
  v190 = v56;
  v60 = _mm_unpacklo_epi32(v53, v27);
  v61 = _mm_unpackhi_epi32(v53, v27);
  v62 = _mm_unpackhi_epi32(v50, v52);
  v63 = _mm_load_si128(&v187);
  v64 = _mm_load_si128(&vector_short_1_0);
  v65 = _mm_unpackhi_epi32(v193, v51);
  v191 = _mm_add_epi16(v60, v38);
  v189 = _mm_sub_epi16(v61, v41);
  v66 = _mm_srai_epi16(_mm_sub_epi16(_mm_add_epi16(v191, v64), v189), 1u);
  v67 = _mm_sub_epi16(v66, v41);
  v68 = _mm_load_si128(&v186);
  v194[0] = v67;
  v188 = _mm_sub_epi16(v66, v38);
  v69 = _mm_add_epi16(_mm_unpacklo_epi32(v68, v28), v34);
  v186 = _mm_unpackhi_epi32(v68, v28);
  v70 = _mm_sub_epi16(v186, v63);
  v71 = _mm_sub_epi16(v34, _mm_srai_epi16(v69, 1u));
  v72 = _mm_load_si128(&v193);
  v73 = _mm_add_epi16(v63, _mm_srai_epi16(_mm_add_epi16(v70, v64), 1u));
  v74 = _mm_sub_epi16(
      v70, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v69, 1u), v69), v59), 3u));
  v75 = _mm_add_epi16(
      v69, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v74, 1u), v74), v59), 3u));
  v76 = _mm_sub_epi16(
      v73, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v71, 1u), v71), v59), 3u));
  v187 = _mm_sub_epi16(v76, _mm_srai_epi16(_mm_add_epi16(v75, v64), 1u));
  v77 = _mm_sub_epi16(
      _mm_srai_epi16(_mm_add_epi16(v74, v64), 1u),
      _mm_add_epi16(v71, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v76, 1u), v76), v59), 3u)));
  v78 = _mm_unpacklo_epi32(v72, v51);
  v79 = v47;
  v80 = _mm_add_epi16(v78, v65);
  v81 = _mm_unpacklo_epi32(_mm_load_si128(&v192), v52);
  v82 = _mm_load_si128(&v190);
  v83 = _mm_sub_epi16(v81, v62);
  v84 = _mm_sub_epi16(_mm_unpackhi_epi32(v47, v57), v58);
  v85 = _mm_sub_epi16(v65, _mm_srai_epi16(v80, 1u));
  v86 = _mm_add_epi16(_mm_unpacklo_epi32(v79, v57), v82);
  v87 = _mm_add_epi16(v62, _mm_srai_epi16(_mm_add_epi16(v83, v64), 1u));
  v88 = _mm_sub_epi16(
      v83, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v80, 1u), v80), v59), 3u));
  v89 = _mm_add_epi16(
      v80, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v88, 1u), v88), v59), 3u));
  v90 = _mm_sub_epi16(
      v87, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v85, 1u), v85), v59), 3u));
  v91 = _mm_add_epi16(_mm_slli_epi16(v90, 1u), v90);
  v193 = _mm_sub_epi16(v90, _mm_srai_epi16(_mm_add_epi16(v89, v64), 1u));
  v92 = _mm_add_epi16(v89, v193);
  v93 = _mm_add_epi16(v88, v64);
  v94 = _mm_srai_epi16(v84, 1u);
  v95 = _mm_add_epi16(v58, v94);
  v96 = _mm_sub_epi16(_mm_srai_epi16(v93, 1u),
                      _mm_add_epi16(v85, _mm_srai_epi16(_mm_add_epi16(v91, v59), 3u)));
  v97 = _mm_srai_epi16(v86, 1u);
  v192 = _mm_sub_epi16(v88, v96);
  v98 = _mm_sub_epi16(
      _mm_sub_epi16(v82, v97),
      _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v95, 1u), v95), vector_short_3),
                     3u));
  v99 = _mm_add_epi16(
      v95, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v98, 1u), v98), vector_short_3), 2u));
  v100 = _mm_sub_epi16(v94, v99);
  v101 = _mm_add_epi16(
      v97,
      _mm_sub_epi16(v98, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v99, 1u), v99), v59), 3u)));
  v102 = _mm_load_si128(v194);
  v103 = _mm_load_si128(&v189);
  v104 = _mm_sub_epi16(v100, v84);
  v105 = _mm_sub_epi16(v74, v77);
  v106 = _mm_sub_epi16(v86, v101);
  v107 = _mm_sub_epi16(_mm_add_epi16(v75, v187), v92);
  v108 = _mm_sub_epi16(v77, v192);
  v109 = _mm_add_epi16(_mm_sub_epi16(_mm_load_si128(&v191), v102), v106);
  v110 = _mm_add_epi16(v102, v101);
  v186 = _mm_sub_epi16(v105, v96);
  v111 = _mm_srai_epi16(_mm_sub_epi16(v109, v107), 1u);
  v112 = _mm_sub_epi16(v111, v92);
  v113 = _mm_load_si128(&v186);
  v114 = _mm_sub_epi16(v111, v106);
  v115 = _mm_load_si128(&v188);
  v116 = _mm_add_epi16(v103, v115);
  v117 = _mm_add_epi16(v115, v100);
  v194[0] = _mm_add_epi16(v107, v114);
  v118 = _mm_add_epi16(v116, v104);
  v119 = _mm_srai_epi16(_mm_sub_epi16(v118, v113), 1u);
  v120 = _mm_srai_epi16(_mm_sub_epi16(v117, v108), 1u);
  v121 = _mm_sub_epi16(v120, v192);
  v122 = _mm_sub_epi16(v120, v100);
  v123 = _mm_load_si128(&v187);
  v124 = _mm_sub_epi16(v119, v104);
  v125 = _mm_load_si128(&v193);
  v190 = _mm_add_epi16(v108, v122);
  v126 = _mm_sub_epi16(v123, v125);
  v127 = _mm_sub_epi16(v119, v96);
  v128 = _mm_add_epi16(_mm_load_si128(&v186), v124);
  v189 = _mm_sub_epi16(v118, v127);
  v129 = _mm_sub_epi16(v109, v112);
  v130 = _mm_srai_epi16(_mm_sub_epi16(v110, v126), 1u);
  v131 = _mm_sub_epi16(v130, v125);
  v132 = _mm_sub_epi16(v110, v131);
  v133 = _mm_sub_epi16(v130, v101);
  v134 = _mm_sub_epi16(v117, v121);
  v135 = _mm_unpackhi_epi16(v134, v132);
  v136 = _mm_unpacklo_epi16(v134, v132);
  v137 = _mm_load_si128(v194);
  v187 = _mm_add_epi16(v126, v133);
  v138 = _mm_load_si128(&v189);
  v188 = v135;
  v139 = _mm_unpackhi_epi16(v129, v138);
  v140 = _mm_unpacklo_epi16(v129, v138);
  v141 = _mm_load_si128(&v187);
  v191 = v139;
  v142 = _mm_load_si128(&v190);
  v143 = v137;
  v144 = _mm_unpackhi_epi16(v142, v141);
  v145 = _mm_unpackhi_epi16(v137, v128);
  v146 = _mm_unpacklo_epi16(v143, v128);
  v147 = _mm_unpacklo_epi16(v142, v141);
  v148 = _mm_unpacklo_epi32(v140, v146);
  v149 = _mm_unpackhi_epi32(v140, v146);
  v150 = _mm_load_si128(&v191);
  v191 = _mm_unpackhi_epi32(v150, v145);
  v151 = _mm_unpacklo_epi32(v150, v145);
  v152 = v136;
  v153 = _mm_unpackhi_epi32(v136, v147);
  v154 = _mm_unpacklo_epi32(v152, v147);
  v155 = _mm_load_si128(&v188);
  v188 = _mm_unpackhi_epi32(v155, v144);
  v156 = _mm_unpacklo_epi32(v148, v154);
  v157 = _mm_unpacklo_epi32(v155, v144);
  v158 = _mm_unpackhi_epi32(v148, v154);
  v159 = _mm_load_si128(&v188);
  *p0 = v156;
  p0[2] = v158;
  v160 = v149;
  v161 = _mm_unpackhi_epi32(v149, v153);
  v162 = _mm_unpacklo_epi32(v160, v153);
  v163 = v122;
  v164 = _mm_unpackhi_epi16(v122, v133);
  p0[8] = v162;
  p0[10] = v161;
  v165 = _mm_unpacklo_epi32(v151, v157);
  v166 = _mm_unpackhi_epi32(v151, v157);
  v167 = _mm_unpacklo_epi16(v163, v133);
  v168 = _mm_load_si128(&v191);
  *p1 = v165;
  p1[2] = v166;
  v169 = v114;
  v170 = _mm_unpackhi_epi16(v114, v124);
  p1[8] = _mm_unpacklo_epi32(v168, v159);
  p1[10] = _mm_unpackhi_epi32(v168, v159);
  v171 = _mm_unpacklo_epi16(v169, v124);
  v172 = v112;
  v173 = v121;
  v174 = _mm_unpackhi_epi16(v112, v127);
  v175 = _mm_unpackhi_epi16(v121, v131);
  v176 = _mm_unpacklo_epi16(v172, v127);
  v177 = _mm_unpacklo_epi16(v173, v131);
  v178 = _mm_unpacklo_epi32(v170, v174);
  v179 = _mm_unpackhi_epi32(v171, v176);
  v180 = _mm_unpacklo_epi32(v171, v176);
  v181 = _mm_unpacklo_epi32(v167, v177);
  v182 = _mm_unpackhi_epi32(v167, v177);
  p0[1] = _mm_unpacklo_epi32(v180, v181);
  v183 = _mm_unpacklo_epi32(v164, v175);
  v184 = _mm_unpackhi_epi32(v164, v175);
  p0[3] = _mm_unpackhi_epi32(v180, v181);
  p0[9] = _mm_unpacklo_epi32(v179, v182);
  p0[11] = _mm_unpackhi_epi32(v179, v182);
  p1[1] = _mm_unpacklo_epi32(v178, v183);
  v185 = _mm_unpackhi_epi32(v170, v174);
  p1[3] = _mm_unpackhi_epi32(v178, v183);
  p1[9] = _mm_unpacklo_epi32(v185, v184);
  p1[11] = _mm_unpackhi_epi32(v185, v184);
}

void InvTransform4x4Stage2(__m128i* p) {
  __m128i si128;
  __m128i v2;
  __m128i v3;
  __m128i v4;
  __m128i v5;
  __m128i v6;
  __m128i v7;
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;
  __m128i v40;
  __m128i v41;
  __m128i v42;
  __m128i v43;
  __m128i v44;
  __m128i v45;
  __m128i v46;
  __m128i v47;
  __m128i v48;
  __m128i v49;
  __m128i v50;
  __m128i v51;
  __m128i v52;
  __m128i v53;
  __m128i v54;
  __m128i v55;
  __m128i v56;
  __m128i v57;
  __m128i v58;
  __m128i v59;
  __m128i v60;
  __m128i v61;
  __m128i v62;
  __m128i v63;
  __m128i v64;
  __m128i v65;
  __m128i v66;

  si128 = _mm_load_si128(p + 10);
  v2 = _mm_load_si128(p + 2);
  v3 = _mm_load_si128(p + 28);
  v4 = _mm_sub_epi16(_mm_load_si128(p + 22), v3);
  v5 = _mm_load_si128(p + 20);
  v66 = _mm_add_epi16(_mm_load_si128(p), si128);
  v6 = _mm_load_si128(&vector_short_255_0);
  v7 = _mm_add_epi16(_mm_load_si128(p + 30), v5);
  v8 = _mm_load_si128(&vector_short_1_0);
  v9 = _mm_sub_epi16(_mm_load_si128(p + 8), v2);
  v10 = _mm_add_epi16(v66, v8);
  v11 = _mm_srai_epi16(v4, 1u);
  v12 = _mm_add_epi16(v3, v11);
  v13 = _mm_srai_epi16(v7, 1u);
  v14 = _mm_srai_epi16(v12, 8u);
  v15 = _mm_add_epi16(
      _mm_sub_epi16(_mm_srai_epi16(v10, 1u), _mm_srai_epi16(v9, 1u)),
      _mm_srai_epi16(_mm_sub_epi16(_mm_and_si128(v10, v8), _mm_and_si128(v9, v8)), 1u));
  v16 = _mm_and_si128(v12, v6);
  v17 = _mm_sub_epi16(v15, si128);
  v18 = _mm_sub_epi16(v15, v2);
  v19 = _mm_sub_epi16(
      _mm_sub_epi16(v5, v13),
      _mm_add_epi16(
          _mm_slli_epi16(_mm_add_epi16(_mm_slli_epi16(v14, 1u), v14), 5u),
          _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v16, 1u), v16), vector_short_3),
                         3u)));
  v20 = _mm_srai_epi16(v19, 8u);
  v21 = _mm_and_si128(v19, v6);
  v22 = _mm_add_epi16(
      v12, _mm_add_epi16(_mm_slli_epi16(_mm_add_epi16(_mm_slli_epi16(v20, 1u), v20), 6u),
                         _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v21, 1u), v21),
                                                      vector_short_3),
                                        2u)));
  v23 = _mm_and_si128(v22, v6);
  v24 = _mm_srai_epi16(v22, 8u);
  v25 = _mm_load_si128(&vector_short_4);
  v26 = _mm_sub_epi16(v11, v22);
  v27 = _mm_sub_epi16(
      v19, _mm_add_epi16(_mm_slli_epi16(_mm_add_epi16(_mm_slli_epi16(v24, 1u), v24), 5u),
                         _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v23, 1u), v23), v25), 3u)));
  v28 = _mm_sub_epi16(v26, v4);
  v29 = _mm_add_epi16(v13, v27);
  v30 = _mm_unpacklo_epi16(v26, v29);
  v31 = _mm_unpacklo_epi16(_mm_sub_epi16(v7, v29), v28);
  v32 = _mm_unpacklo_epi16(_mm_load_si128(p + 18), p[12]);
  v33 = _mm_unpacklo_epi32(v31, v30);
  v34 = _mm_unpacklo_epi16(_mm_load_si128(p + 26), p[14]);
  v35 = _mm_sub_epi16(_mm_unpacklo_epi16(_mm_load_si128(p + 16), p[4]), v32);
  v36 = _mm_add_epi16(_mm_unpacklo_epi16(_mm_load_si128(p + 24), p[6]), v34);
  v37 = _mm_and_si128(v36, v6);
  v38 = _mm_add_epi16(v32, _mm_srai_epi16(_mm_add_epi16(v35, v8), 1u));
  v39 = _mm_sub_epi16(v34, _mm_srai_epi16(v36, 1u));
  v40 = _mm_srai_epi16(v36, 8u);
  v41 = _mm_sub_epi16(
      v35, _mm_add_epi16(_mm_slli_epi16(_mm_add_epi16(_mm_slli_epi16(v40, 1u), v40), 5u),
                         _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v37, 1u), v37), v25), 3u)));
  v42 = _mm_srai_epi16(v41, 8u);
  v43 = _mm_and_si128(v41, v6);
  v44 = _mm_slli_epi16(_mm_add_epi16(_mm_slli_epi16(v42, 1u), v42), 5u);
  v45 = _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v43, 1u), v43), v25), 3u);
  v46 = _mm_and_si128(v39, v6);
  v47 = _mm_add_epi16(v36, _mm_add_epi16(v44, v45));
  v48 = _mm_srai_epi16(v39, 8u);
  v49 = _mm_sub_epi16(
      v38, _mm_add_epi16(_mm_slli_epi16(_mm_add_epi16(_mm_slli_epi16(v48, 1u), v48), 5u),
                         _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v46, 1u), v46), v25), 3u)));
  v50 = _mm_and_si128(v49, v6);
  v51 = _mm_srai_epi16(v49, 8u);
  v52 = _mm_sub_epi16(v49, _mm_srai_epi16(_mm_add_epi16(v47, v8), 1u));
  v53 = _mm_sub_epi16(
      _mm_srai_epi16(_mm_add_epi16(v41, v8), 1u),
      _mm_add_epi16(
          v39,
          _mm_add_epi16(_mm_slli_epi16(_mm_add_epi16(_mm_slli_epi16(v51, 1u), v51), 5u),
                        _mm_srai_epi16(
                            _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v50, 1u), v50), v25), 3u))));
  v54 = _mm_sub_epi16(v41, v53);
  v55 = _mm_unpacklo_epi16(v53, v52);
  v56 = _mm_unpacklo_epi16(_mm_add_epi16(v47, v52), v54);
  v57 = _mm_unpacklo_epi16(v56, v55);
  v58 = _mm_unpackhi_epi64(v57, v57);
  v59 =
      _mm_add_epi16(_mm_unpacklo_epi32(_mm_unpacklo_epi16(_mm_sub_epi16(_mm_load_si128(&v66), v18),
                                                          _mm_add_epi16(v9, v17)),
                                       _mm_unpacklo_epi16(v17, v18)),
                    v33);
  v60 = _mm_sub_epi16(_mm_unpacklo_epi32(v56, v55), v58);
  v61 = _mm_add_epi16(
      _mm_sub_epi16(_mm_srai_epi16(v59, 1u), _mm_srai_epi16(v60, 1u)),
      _mm_srai_epi16(_mm_sub_epi16(_mm_and_si128(v59, v8), _mm_and_si128(v60, v8)), 1u));
  v62 = _mm_sub_epi16(v61, v58);
  v63 = _mm_sub_epi16(v61, v33);
  v64 = _mm_sub_epi16(v59, v62);
  v65 = _mm_add_epi16(v60, v63);
  p->m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v64, 0));
  p[8].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v64, 1));
  p[2].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v64, 2));
  p[10].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v64, 3));
  p[24].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v65, 0));
  p[16].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v65, 1));
  p[26].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v65, 2));
  p[18].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v65, 3));
  p[6].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v63, 0));
  p[14].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v63, 1));
  p[4].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v63, 2));
  p[12].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v63, 3));
  p[30].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v62, 0));
  p[22].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v62, 1));
  p[28].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v62, 2));
  p[20].m128i_i16[0] = static_cast<std::int16_t>(_mm_extract_epi16(v62, 3));
}

void PostFilter4x4Stage1(__m128i* p0, __m128i* p1, __m128i* p2, __m128i* p3) {
  __m128i v4;
  __m128i v5;
  __m128i v6;
  __m128i si128;
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;
  __m128i v40;
  __m128i v41;
  __m128i v42;
  __m128i v43;

  v4 = _mm_loadl_epi64(p3);
  v5 = _mm_loadl_epi64(p2);
  v6 = _mm_add_epi16(_mm_loadl_epi64(p0), v4);
  si128 = _mm_load_si128(&vector_short_1_0);
  v8 = _mm_sub_epi16(_mm_loadl_epi64(p1), v5);
  v9 = _mm_srai_epi16(_mm_sub_epi16(v6, v8), 1u);
  v10 = _mm_sub_epi16(v9, v5);
  v11 = _mm_sub_epi16(v9, v4);
  v12 = _mm_shufflelo_epi16(v10, 85);
  v13 = _mm_add_epi16(v8, v11);
  v14 = _mm_shufflelo_epi16(v11, 255);
  v15 = _mm_add_epi16(_mm_shufflelo_epi16(v10, 255), v10);
  v16 = _mm_shufflelo_epi16(v11, 85);
  v17 = _mm_sub_epi16(_mm_shufflelo_epi16(v10, 170), v12);
  v18 = _mm_shufflelo_epi16(v13, 255);
  v43 = _mm_sub_epi16(v6, v10);
  v19 = _mm_shufflelo_epi16(v13, 170);
  v20 = _mm_srai_epi16(v17, 1u);
  v21 = _mm_sub_epi16(_mm_shufflelo_epi16(v13, 85), _mm_srai_epi16(_mm_add_epi16(v18, si128), 1u));
  v22 = _mm_add_epi16(v12, v20);
  v23 = _mm_sub_epi16(v13, _mm_srai_epi16(_mm_add_epi16(v19, si128), 1u));
  v24 = _mm_sub_epi16(_mm_shufflelo_epi16(v11, 170), _mm_srai_epi16(_mm_add_epi16(v14, si128), 1u));
  v42 = _mm_sub_epi16(v11, _mm_srai_epi16(_mm_add_epi16(v16, si128), 1u));
  v25 = _mm_srai_epi16(v15, 1u);
  v26 = _mm_sub_epi16(
      _mm_sub_epi16(v10, v25),
      _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v22, 1u), v22), vector_short_6),
                     3u));
  v27 = _mm_add_epi16(
      v22, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v26, 1u), v26), vector_short_2), 2u));
  v28 = _mm_add_epi16(_mm_slli_epi16(v27, 1u), v27);
  v29 = _mm_sub_epi16(v27, v20);
  v30 = _mm_load_si128(&v42);
  v31 = _mm_add_epi16(_mm_sub_epi16(v26, _mm_srai_epi16(_mm_add_epi16(v28, vector_short_4), 3u)),
                      v25);
  v32 = _mm_unpacklo_epi32(_mm_unpacklo_epi16(v31, v29),
                           _mm_unpacklo_epi16(_mm_add_epi16(v17, v29), _mm_sub_epi16(v15, v31)));
  v33 = _mm_unpacklo_epi32(
      _mm_unpacklo_epi16(v30, _mm_add_epi16(v16, _mm_srai_epi16(_mm_add_epi16(v30, si128), 1u))),
      _mm_unpacklo_epi16(v24, _mm_add_epi16(v14, _mm_srai_epi16(_mm_add_epi16(v24, si128), 1u))));
  v34 = _mm_add_epi16(v43, v32);
  v35 = _mm_sub_epi16(
      _mm_unpacklo_epi32(
          _mm_unpacklo_epi16(v23, v21),
          _mm_unpacklo_epi16(_mm_add_epi16(v19, _mm_srai_epi16(_mm_add_epi16(v23, si128), 1u)),
                             _mm_add_epi16(v18, _mm_srai_epi16(_mm_add_epi16(v21, si128), 1u)))),
      v33);
  v36 = _mm_sub_epi16(_mm_srai_epi16(v34, 1u), v32);
  v37 = _mm_add_epi16(
      v34, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v36, 1u), v36), vector_short_8), 4u));
  v38 = _mm_add_epi16(
      v36, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v37, 1u), v37), vector_short_4), 5u));
  v39 = _mm_add_epi16(
      v37, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v38, 1u), v38), vector_short_8), 4u));
  v40 = _mm_sub_epi16(_mm_srai_epi16(_mm_sub_epi16(v39, v35), 1u), v33);
  p0->m128i_i64[0] = _mm_sub_epi16(v39, v40).m128i_u64[0];
  v41 = _mm_sub_epi16(v38, _mm_srai_epi16(v35, 1u));
  p1->m128i_i64[0] = _mm_add_epi16(v35, v41).m128i_u64[0];
  p2->m128i_i64[0] = v41.m128i_i64[0];
  p3->m128i_i64[0] = v40.m128i_i64[0];
}

void PostFilter4x4Stage1_4(std::int16_t* p0, std::int16_t* p1, double* p2, double* p3,
                           std::int16_t* p4, std::int16_t* p5, double* p6, double* p7) {
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;
  __m128i v40;
  __m128i v41;
  __m128i v42;
  __m128i si128;
  __m128i v44;
  __m128i v45;
  __m128i v46;
  __m128i v47;
  __m128i v48;
  __m128i v49;
  __m128i v50;
  __m128i v51;
  __m128i v52;
  __m128i v53;
  __m128i v54;
  __m128i v55;
  __m128i v56;
  __m128i v57;
  __m128i v58;
  __m128i v59;
  __m128i v60;
  __m128i v61;
  __m128i v62;
  __m128i v63;
  __m128i v64;
  __m128i v65;
  __m128i v66;
  __m128i v67;
  __m128i v68;
  __m128i v69;
  __m128i v70;
  __m128i v71;
  __m128i v72;
  __m128i v73;
  __m128i v74;
  __m128i v75;
  __m128i v76;
  __m128i v77;
  __m128i v78;
  __m128i v79;
  __m128i v80;
  __m128i v81;
  __m128i v82;
  __m128i v83;
  __m128i v84;
  __m128i v85;
  __m128i v86;
  __m128i v87;
  __m128i v88;
  __m128i v89;
  __m128i v90;
  __m128i v91;
  __m128i v92;
  __m128i v93;
  __m128i v94;
  __m128i v95;
  __m128i v96;
  __m128i v97;
  __m128i v98;
  __m128i v99;
  __m128i v100;
  __m128i v101;
  __m128i v102;
  __m128i v103;

  v8 = LoadPair64(p1 + 64, p3 + 16);
  v9 = LoadPair64(p1 + 4, p3 + 1);
  v10 = LoadPair64(p5 + 4, p7 + 1);
  v11 = _mm_add_epi16(LoadPair64(p0 + 12, p2 + 3), v8);
  v12 = _mm_sub_epi16(LoadPair64(p0 + 72, p2 + 18), v9);
  v13 = _mm_srai_epi16(_mm_sub_epi16(v11, v12), 1u);
  v14 = _mm_sub_epi16(v13, v9);
  v15 = _mm_sub_epi16(v13, v8);
  v16 = LoadPair64(p5 + 64, p7 + 16);
  v17 = v14;
  v18 = _mm_add_epi16(LoadPair64(p4 + 12, p6 + 3), v16);
  v19 = LoadPair64(p4 + 72, p6 + 18);
  v101 = _mm_sub_epi16(v11, v14);
  v20 = _mm_sub_epi16(v19, v10);
  v21 = _mm_add_epi16(v12, v15);
  v22 = _mm_srai_epi16(_mm_sub_epi16(v18, v20), 1u);
  v23 = _mm_sub_epi16(v22, v10);
  v24 = _mm_unpackhi_epi16(v14, v23);
  v25 = _mm_sub_epi16(v22, v16);
  v103 = _mm_sub_epi16(v18, v23);
  v26 = _mm_unpacklo_epi16(v17, v23);
  v27 = _mm_add_epi16(v20, v25);
  v28 = _mm_unpacklo_epi16(v15, v25);
  v29 = _mm_unpacklo_epi16(v21, v27);
  v30 = _mm_unpackhi_epi16(v21, v27);
  v31 = _mm_unpackhi_epi16(v15, v25);
  v32 = v29;
  v33 = _mm_unpackhi_epi16(v29, v30);
  v34 = _mm_unpacklo_epi16(v28, v31);
  v35 = _mm_unpackhi_epi16(v28, v31);
  v36 = _mm_unpacklo_epi16(v32, v30);
  v37 = _mm_unpacklo_epi16(v26, v24);
  v38 = _mm_unpackhi_epi16(v26, v24);
  v39 = _mm_unpackhi_epi64(v33, v33);
  v40 = _mm_unpackhi_epi64(v34, v34);
  v41 = _mm_unpackhi_epi64(v35, v35);
  v42 = _mm_unpackhi_epi64(v37, v37);
  si128 = _mm_load_si128(&vector_short_1_0);
  v44 = _mm_unpackhi_epi64(v38, v38);
  v45 = _mm_sub_epi16(v38, v42);
  v46 = _mm_add_epi16(v44, v37);
  v47 = _mm_sub_epi16(_mm_unpackhi_epi64(v36, v36), _mm_srai_epi16(_mm_add_epi16(v39, si128), 1u));
  v48 = _mm_srai_epi16(v46, 1u);
  v100 = _mm_sub_epi16(v36, _mm_srai_epi16(_mm_add_epi16(v33, si128), 1u));
  v49 = _mm_srai_epi16(v45, 1u);
  v50 = _mm_sub_epi16(v35, _mm_srai_epi16(_mm_add_epi16(v41, si128), 1u));
  v51 = _mm_add_epi16(v42, v49);
  v52 = _mm_sub_epi16(v34, _mm_srai_epi16(_mm_add_epi16(v40, si128), 1u));
  v53 = _mm_sub_epi16(
      _mm_sub_epi16(v37, v48),
      _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v51, 1u), v51), vector_short_6),
                     3u));
  v54 = _mm_add_epi16(
      v51, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v53, 1u), v53), vector_short_2), 2u));
  v55 = _mm_add_epi16(_mm_slli_epi16(v54, 1u), v54);
  v56 = _mm_sub_epi16(v54, v49);
  v57 = _mm_add_epi16(v45, v56);
  v58 = _mm_add_epi16(_mm_sub_epi16(v53, _mm_srai_epi16(_mm_add_epi16(v55, vector_short_4), 3u)),
                      v48);
  v59 = _mm_sub_epi16(v46, v58);
  v60 = _mm_unpacklo_epi64(v58, v56);
  v61 = _mm_unpacklo_epi64(_mm_load_si128(&v100), v47);
  v62 = _mm_unpacklo_epi64(_mm_add_epi16(v33, _mm_srai_epi16(_mm_add_epi16(v100, si128), 1u)),
                           _mm_add_epi16(v39, _mm_srai_epi16(_mm_add_epi16(v47, si128), 1u)));
  v102 = _mm_unpacklo_epi16(v61, v62);
  v100 = _mm_unpackhi_epi16(v61, v62);
  v63 = _mm_unpacklo_epi64(v52, _mm_add_epi16(v40, _mm_srai_epi16(_mm_add_epi16(v52, si128), 1u)));
  v64 = _mm_unpacklo_epi64(v50, _mm_add_epi16(v41, _mm_srai_epi16(_mm_add_epi16(v50, si128), 1u)));
  v65 = _mm_unpacklo_epi16(v63, v64);
  v66 = _mm_unpackhi_epi16(v63, v64);
  v67 = _mm_unpacklo_epi16(v65, v66);
  v68 = _mm_unpackhi_epi16(v65, v66);
  v69 = _mm_load_si128(&vector_short_8);
  v70 = _mm_load_si128(&v102);
  v71 = _mm_unpacklo_epi64(v57, v59);
  v72 = _mm_load_si128(&v100);
  v73 = v60;
  v74 = _mm_unpackhi_epi16(v60, v71);
  v75 = _mm_unpacklo_epi16(v73, v71);
  v76 = _mm_unpacklo_epi16(v75, v74);
  v77 = _mm_sub_epi16(_mm_unpacklo_epi16(v70, v72), v67);
  v78 = _mm_unpackhi_epi16(v75, v74);
  v79 = _mm_add_epi16(_mm_load_si128(&v101), v76);
  v80 = _mm_sub_epi16(_mm_unpackhi_epi16(v70, v72), v68);
  v81 = _mm_sub_epi16(_mm_srai_epi16(v79, 1u), v76);
  v82 = _mm_add_epi16(
      v79, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v81, 1u), v81), v69), 4u));
  v83 = _mm_add_epi16(
      v81, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v82, 1u), v82), vector_short_4), 5u));
  v84 = _mm_add_epi16(
      v82, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v83, 1u), v83), v69), 4u));
  v85 = _mm_sub_epi16(v83, _mm_srai_epi16(v77, 1u));
  v86 = _mm_sub_epi16(v84, v77);
  v87 = _mm_add_epi16(v77, v85);
  v88 = _mm_sub_epi16(_mm_srai_epi16(v86, 1u), v67);
  v89 = _mm_sub_epi16(v84, v88);
  *((_QWORD*)p0 + 3) = v89.m128i_u64[0];
  *((_QWORD*)p0 + 18) = v87.m128i_u64[0];
  *((_QWORD*)p1 + 1) = v85.m128i_u64[0];
  *((_QWORD*)p1 + 16) = v88.m128i_u64[0];
  StoreHigh64((double*)p2 + 3, v89);
  StoreHigh64((double*)p2 + 18, v87);
  StoreHigh64((double*)p3 + 1, v85);
  v90 = _mm_load_si128(&v103);
  StoreHigh64((double*)p3 + 16, v88);
  v91 = _mm_add_epi16(v90, v78);
  v92 = _mm_sub_epi16(_mm_srai_epi16(v91, 1u), v78);
  v93 = _mm_add_epi16(
      v91, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v92, 1u), v92), v69), 4u));
  v94 = _mm_add_epi16(
      v92, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v93, 1u), v93), vector_short_4), 5u));
  v95 = _mm_add_epi16(
      v93, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v94, 1u), v94), v69), 4u));
  v96 = _mm_sub_epi16(_mm_srai_epi16(_mm_sub_epi16(v95, v80), 1u), v68);
  v97 = _mm_sub_epi16(v95, v96);
  *((_QWORD*)p4 + 3) = v97.m128i_u64[0];
  v98 = _mm_sub_epi16(v94, _mm_srai_epi16(v80, 1u));
  v99 = _mm_add_epi16(v80, v98);
  *((_QWORD*)p4 + 18) = v99.m128i_u64[0];
  *((_QWORD*)p5 + 1) = v98.m128i_u64[0];
  *((_QWORD*)p5 + 16) = v96.m128i_u64[0];
  StoreHigh64((double*)p6 + 3, v97);
  StoreHigh64((double*)p6 + 18, v99);
  StoreHigh64((double*)p7 + 1, v98);
  StoreHigh64((double*)p7 + 16, v96);
}

void PostFilter4x4Stage1_8(std::int16_t* p0, std::int16_t* p1, std::int16_t* p2, std::int16_t* p3,
                           std::int16_t* p4, std::int16_t* p5, std::int16_t* p6, std::int16_t* p7,
                           int ofs) {
  __int64 v9;
  __int64 v10;
  __m128i v11;
  __int64 v12;
  __m128i v13;
  __int64 v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;
  __m128i v40;
  __m128i v41;
  __m128i v42;
  __m128i v43;
  __m128i v44;
  __m128i v45;
  __m128i v46;
  __m128i v47;
  __m128i v48;
  __m128i v49;
  __m128i v50;
  __m128i v51;
  __m128i v52;
  __m128i v53;
  __m128i v54;
  __m128i v55;
  __m128i v56;
  __m128i v57;
  __m128i v58;
  __m128i v59;
  __m128i v60;
  __m128i v61;
  __m128i v62;
  __m128i v63;
  __m128i v64;
  __m128i v65;
  __m128i v66;
  __m128i v67;
  __m128i v68;
  __m128i v69;
  __m128i v70;
  __m128i v71;
  __m128i v72;
  __m128i v73;
  __m128i v74;
  __m128i v75;
  __m128i v76;
  __m128i v77;
  __m128i si128;
  __m128i v79;
  __m128i v80;
  __m128i v81;
  __m128i v82;
  __m128i v83;
  __m128i v84;
  __m128i v85;
  __m128i v86;
  __m128i v87;
  __m128i v88;
  __m128i v89;
  __m128i v90;
  __m128i v91;
  __m128i v92;
  __m128i v93;
  __m128i v94;
  __m128i v95;
  __m128i v96;
  __m128i v97;
  __m128i v98;
  __m128i v99;
  __m128i v100;
  __m128i v101;
  __m128i v102;
  __m128i v103;
  __m128i v104;
  __m128i v105;
  __m128i v106;
  __m128i v107;
  __m128i v108;
  __m128i v109;
  __m128i v110;
  __m128i v111;
  __m128i v112;
  __m128i v113;
  __m128i v114;
  __m128i v115;
  __m128i v116;
  __m128i v117;
  __m128i v118;
  __m128i v119;
  __m128i v120;
  __m128i v121;
  __m128i v122;
  __m128i v123;
  __m128i v124;
  __m128i v125;
  __m128i v126;
  __m128i v127;
  __m128i v128;
  __m128i v129;
  __m128i v130;
  __m128i v131;
  __m128i v132;
  __m128i v133;
  __m128i v134;
  __m128i v135;
  __m128i v136;
  __m128i v137;
  __m128i v138;
  __m128i v139;
  __m128i v140;
  __m128i v141;
  __m128i v142;
  __m128i v143;
  __m128i v144;
  __m128i v145;
  __m128i v146;
  __m128i v147;
  __m128i v148;
  __m128i v149;
  __m128i v150;
  __m128i v151;
  __m128i v152;
  __m128i v153;
  __m128i v154;
  __m128i v155;
  __m128i v156;
  __m128i v157;
  __m128i v158;
  __m128i v159;
  __m128i v160;
  __m128i v161;
  __m128i v162;
  __m128i v163;
  __m128i v164;
  __m128i v165;
  __m128i v166;
  __m128i v167;
  __m128i v168;
  __m128i v169;
  __m128i v170;
  __m128i v171;
  __m128i v172;
  __m128i v173;
  __m128i v174;
  __m128i v175;
  __m128i v176;
  __m128i v177;
  __m128i v178;
  __m128i v179;
  __m128i v180;
  __m128i v181;
  __m128i v182;
  __m128i v183;
  __m128i v184;
  __m128i v185;
  __m128i v186;
  __m128i v187;
  __m128i v188;
  __m128i v189;
  __m128i v190;
  __m128i v191;
  __m128i v192;
  __m128i v193;
  __m128i v194;
  __m128i v195;
  __m128i v196;
  __m128i v197;
  __m128i v198;
  __m128i v199[11];

  v9 = ofs + 4;
  v10 = ofs + 64;
  v11 = LoadPair16(p1, 4, v9);
  v12 = ofs + 12;
  v13 = LoadPair16(p1, 64, v10);
  v14 = ofs + 72;
  v15 = _mm_add_epi16(LoadPair16(p0, 12, v12), v13);
  v16 = LoadPair16(p2, 72, v14);
  v17 = _mm_sub_epi16(LoadPair16(p0, 72, v14), v11);
  v18 = LoadPair16(p4, 72, v14);
  v19 = _mm_srai_epi16(_mm_sub_epi16(v15, v17), 1u);
  v20 = _mm_sub_epi16(v19, v11);
  v21 = LoadPair16(p3, 4, v9);
  v193 = v20;
  v22 = _mm_sub_epi16(v15, v20);
  v23 = _mm_sub_epi16(v19, v13);
  v24 = LoadPair16(p2, 12, v12);
  v195 = v22;
  v25 = _mm_sub_epi16(v16, v21);
  v26 = _mm_add_epi16(v17, v23);
  v27 = LoadPair16(p3, 64, v10);
  v28 = _mm_add_epi16(v24, v27);
  v29 = _mm_srai_epi16(_mm_sub_epi16(v28, v25), 1u);
  v30 = _mm_sub_epi16(v29, v21);
  v31 = LoadPair16(p5, 4, v9);
  v32 = _mm_sub_epi16(v29, v27);
  v33 = LoadPair16(p5, 64, v10);
  v196 = _mm_sub_epi16(v28, v30);
  v34 = _mm_sub_epi16(v18, v31);
  v35 = _mm_add_epi16(v25, v32);
  v36 = _mm_add_epi16(LoadPair16(p4, 12, v12), v33);
  v37 = _mm_srai_epi16(_mm_sub_epi16(v36, v34), 1u);
  v38 = _mm_sub_epi16(v37, v31);
  v39 = _mm_sub_epi16(v37, v33);
  v40 = _mm_add_epi16(v34, v39);
  v197 = _mm_sub_epi16(v36, v38);
  v41 = LoadPair16(p7, 64, v10);
  v42 = LoadPair16(p7, 4, v9);
  v43 = _mm_add_epi16(LoadPair16(p6, 12, v12), v41);
  v44 = _mm_sub_epi16(LoadPair16(p6, 72, v14), v42);
  v45 = _mm_srai_epi16(_mm_sub_epi16(v43, v44), 1u);
  v46 = _mm_sub_epi16(v45, v42);
  v47 = _mm_sub_epi16(v45, v41);
  v48 = _mm_unpacklo_epi16(v26, v35);
  v49 = _mm_unpackhi_epi16(v26, v35);
  v50 = _mm_add_epi16(v44, v47);
  v199[0] = _mm_sub_epi16(v43, v46);
  v51 = _mm_unpackhi_epi32(v48, v49);
  v52 = _mm_unpacklo_epi16(v40, v50);
  v53 = _mm_unpackhi_epi16(v40, v50);
  v54 = _mm_unpacklo_epi32(v48, v49);
  v55 = v52;
  v56 = _mm_unpackhi_epi32(v52, v53);
  v57 = _mm_unpacklo_epi32(v55, v53);
  v58 = v38;
  v59 = _mm_unpackhi_epi32(v51, v56);
  v60 = _mm_unpacklo_epi32(v51, v56);
  v61 = _mm_unpackhi_epi16(v38, v46);
  v194 = v59;
  v62 = v39;
  v63 = _mm_unpacklo_epi16(v23, v32);
  v64 = _mm_unpackhi_epi16(v23, v32);
  v65 = _mm_unpacklo_epi16(v58, v46);
  v66 = _mm_unpackhi_epi16(v39, v47);
  v192 = _mm_unpackhi_epi32(v63, v64);
  v67 = _mm_unpacklo_epi16(v62, v47);
  v68 = _mm_unpacklo_epi32(v63, v64);
  v69 = _mm_unpackhi_epi32(v67, v66);
  v70 = _mm_unpacklo_epi32(v67, v66);
  v71 = v192;
  v72 = _mm_load_si128(&v193);
  v73 = _mm_unpackhi_epi16(v72, v30);
  v74 = _mm_unpacklo_epi16(v72, v30);
  v75 = _mm_unpackhi_epi32(v68, v70);
  v76 = _mm_unpackhi_epi32(v192, v69);
  v77 = _mm_unpacklo_epi32(v74, v73);
  v192 = _mm_unpackhi_epi32(v74, v73);
  si128 = _mm_load_si128(&vector_short_1_0);
  v79 = _mm_unpacklo_epi32(v65, v61);
  v80 = _mm_unpackhi_epi32(v65, v61);
  v81 = _mm_unpacklo_epi32(v77, v79);
  v82 = _mm_unpackhi_epi32(v77, v79);
  v83 = _mm_load_si128(&v194);
  v84 = _mm_sub_epi16(_mm_unpackhi_epi32(v54, v57), _mm_srai_epi16(_mm_add_epi16(v83, si128), 1u));
  v85 = _mm_unpacklo_epi32(v54, v57);
  v86 = _mm_unpacklo_epi32(v71, v69);
  v87 = _mm_unpacklo_epi32(v68, v70);
  v88 = _mm_load_si128(&v192);
  v89 = _mm_add_epi16(v83, _mm_srai_epi16(_mm_add_epi16(v84, si128), 1u));
  v90 = _mm_unpacklo_epi32(v88, v80);
  v91 = _mm_add_epi16(_mm_unpackhi_epi32(v88, v80), v81);
  v92 = _mm_sub_epi16(v90, v82);
  v93 = _mm_srai_epi16(v91, 1u);
  v94 = _mm_sub_epi16(v85, _mm_srai_epi16(_mm_add_epi16(v60, si128), 1u));
  v95 = _mm_add_epi16(v60, _mm_srai_epi16(_mm_add_epi16(v94, si128), 1u));
  v96 = _mm_sub_epi16(v86, _mm_srai_epi16(_mm_add_epi16(v76, si128), 1u));
  v97 = _mm_add_epi16(v76, _mm_srai_epi16(_mm_add_epi16(v96, si128), 1u));
  v98 = _mm_sub_epi16(v87, _mm_srai_epi16(_mm_add_epi16(v75, si128), 1u));
  v99 = _mm_srai_epi16(_mm_add_epi16(v98, si128), 1u);
  v100 = _mm_srai_epi16(v92, 1u);
  v101 = _mm_add_epi16(v75, v99);
  v102 = _mm_add_epi16(v82, v100);
  v103 = _mm_sub_epi16(
      _mm_sub_epi16(v81, v93),
      _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v102, 1u), v102), vector_short_6),
                     3u));
  v104 = _mm_add_epi16(
      v102, _mm_srai_epi16(
                _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v103, 1u), v103), vector_short_2), 2u));
  v105 = _mm_add_epi16(_mm_slli_epi16(v104, 1u), v104);
  v106 = _mm_sub_epi16(v104, v100);
  v107 = _mm_unpacklo_epi16(v95, v89);
  v108 = _mm_unpackhi_epi16(v95, v89);
  v198 = _mm_unpackhi_epi32(v107, v108);
  v109 = _mm_unpacklo_epi32(v107, v108);
  v110 = _mm_add_epi16(_mm_sub_epi16(v103, _mm_srai_epi16(_mm_add_epi16(v105, vector_short_4), 3u)),
                       v93);
  v111 = _mm_sub_epi16(v91, v110);
  v112 = v94;
  v113 = _mm_unpackhi_epi16(v94, v84);
  v114 = _mm_unpacklo_epi16(v112, v84);
  v194 = _mm_unpacklo_epi32(v114, v113);
  v193 = _mm_unpackhi_epi32(v114, v113);
  v115 = _mm_unpacklo_epi16(v98, v101);
  v116 = _mm_unpackhi_epi16(v98, v101);
  v117 = _mm_add_epi16(v92, v106);
  v192 = _mm_unpackhi_epi16(v117, v111);
  v118 = _mm_unpacklo_epi16(v117, v111);
  v119 = _mm_unpacklo_epi16(v96, v97);
  v120 = _mm_unpacklo_epi32(v115, v116);
  v121 = v110;
  v122 = _mm_unpackhi_epi16(v96, v97);
  v123 = _mm_unpackhi_epi16(v110, v106);
  v124 = _mm_load_si128(&v194);
  v125 = _mm_unpacklo_epi32(v119, v122);
  v126 = _mm_unpackhi_epi32(v119, v122);
  v127 = _mm_unpacklo_epi16(v121, v106);
  v128 = _mm_load_si128(&vector_short_8);
  v129 = _mm_unpackhi_epi32(v115, v116);
  v130 = _mm_unpacklo_epi32(v120, v125);
  v131 = _mm_unpackhi_epi32(v120, v125);
  v132 = _mm_unpackhi_epi32(v129, v126);
  v133 = _mm_unpacklo_epi32(v129, v126);
  v134 = _mm_unpacklo_epi32(v127, v123);
  v135 = _mm_unpackhi_epi32(v127, v123);
  v136 = _mm_load_si128(&v192);
  v137 = _mm_load_si128(&vector_short_4);
  v138 = _mm_unpacklo_epi32(v118, v136);
  v139 = _mm_unpackhi_epi32(v118, v136);
  v140 = _mm_unpacklo_epi32(v134, v138);
  v141 = _mm_unpackhi_epi32(v134, v138);
  v142 = _mm_unpacklo_epi32(v135, v139);
  v143 = _mm_add_epi16(_mm_load_si128(&v195), v140);
  v144 = _mm_unpackhi_epi32(v135, v139);
  v145 = _mm_sub_epi16(_mm_srai_epi16(v143, 1u), v140);
  v146 = _mm_sub_epi16(_mm_unpacklo_epi32(v124, v109), v130);
  v147 = _mm_add_epi16(
      v143, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v145, 1u), v145), v128), 4u));
  v148 = _mm_add_epi16(
      v145, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v147, 1u), v147), v137), 5u));
  v149 = _mm_add_epi16(
      v147, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v148, 1u), v148), v128), 4u));
  v150 = _mm_sub_epi16(v149, v146);
  v151 = _mm_sub_epi16(v148, _mm_srai_epi16(v146, 1u));
  v152 = _mm_add_epi16(v146, v151);
  v153 = _mm_sub_epi16(_mm_srai_epi16(v150, 1u), v130);
  v154 = _mm_sub_epi16(v149, v153);
  *((_QWORD*)p0 + 3) = v154.m128i_u64[0];
  *((_QWORD*)p0 + 18) = v152.m128i_u64[0];
  *((_QWORD*)p1 + 1) = v151.m128i_u64[0];
  *((_QWORD*)p1 + 16) = v153.m128i_u64[0];
  StoreHigh64((double*)&p0[v12], v154);
  StoreHigh64((double*)&p0[v14], v152);
  StoreHigh64((double*)&p1[v9], v151);
  v155 = _mm_load_si128(&v196);
  v156 = _mm_load_si128(&v198);
  StoreHigh64((double*)&p1[v10], v153);
  v157 = _mm_add_epi16(v155, v141);
  v158 = _mm_sub_epi16(_mm_unpackhi_epi32(v124, v109), v131);
  v159 = _mm_sub_epi16(_mm_srai_epi16(v157, 1u), v141);
  v160 = _mm_load_si128(&v193);
  v161 = _mm_add_epi16(
      v157, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v159, 1u), v159), v128), 4u));
  v162 = _mm_add_epi16(
      v159, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v161, 1u), v161), v137), 5u));
  v163 = _mm_add_epi16(
      v161, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v162, 1u), v162), v128), 4u));
  v164 = _mm_sub_epi16(v162, _mm_srai_epi16(v158, 1u));
  v165 = _mm_sub_epi16(v163, v158);
  v166 = _mm_add_epi16(v158, v164);
  v167 = _mm_sub_epi16(_mm_srai_epi16(v165, 1u), v131);
  v168 = _mm_sub_epi16(v163, v167);
  v169 = _mm_load_si128(&v197);
  *((_QWORD*)p2 + 3) = v168.m128i_u64[0];
  *((_QWORD*)p2 + 18) = v166.m128i_u64[0];
  *((_QWORD*)p3 + 1) = v164.m128i_u64[0];
  *((_QWORD*)p3 + 16) = v167.m128i_u64[0];
  v170 = _mm_add_epi16(v169, v142);
  StoreHigh64((double*)&p2[v12], v168);
  StoreHigh64((double*)&p2[v14], v166);
  StoreHigh64((double*)&p3[v9], v164);
  StoreHigh64((double*)&p3[v10], v167);
  v171 = _mm_sub_epi16(_mm_srai_epi16(v170, 1u), v142);
  v172 = _mm_sub_epi16(_mm_unpacklo_epi32(v160, v156), v133);
  v173 = _mm_add_epi16(
      v170, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v171, 1u), v171), v128), 4u));
  v174 = _mm_add_epi16(
      v171, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v173, 1u), v173), v137), 5u));
  v175 = _mm_add_epi16(
      v173, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v174, 1u), v174), v128), 4u));
  v176 = _mm_sub_epi16(v174, _mm_srai_epi16(v172, 1u));
  v177 = _mm_srai_epi16(_mm_sub_epi16(v175, v172), 1u);
  v178 = _mm_add_epi16(v172, v176);
  v179 = _mm_sub_epi16(v177, v133);
  v180 = _mm_sub_epi16(v175, v179);
  *((_QWORD*)p4 + 3) = v180.m128i_u64[0];
  *((_QWORD*)p4 + 18) = v178.m128i_u64[0];
  *((_QWORD*)p5 + 1) = v176.m128i_u64[0];
  *((_QWORD*)p5 + 16) = v179.m128i_u64[0];
  StoreHigh64((double*)&p4[v12], v180);
  StoreHigh64((double*)&p4[v14], v178);
  StoreHigh64((double*)&p5[v9], v176);
  v181 = _mm_add_epi16(_mm_load_si128(v199), v144);
  v182 = _mm_sub_epi16(_mm_unpackhi_epi32(v160, v156), v132);
  StoreHigh64((double*)&p5[v10], v179);
  v183 = _mm_sub_epi16(_mm_srai_epi16(v181, 1u), v144);
  v184 = _mm_add_epi16(
      v181, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v183, 1u), v183), v128), 4u));
  v185 = _mm_add_epi16(
      v183, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v184, 1u), v184), v137), 5u));
  v186 = _mm_add_epi16(
      v184, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v185, 1u), v185), v128), 4u));
  v187 = _mm_sub_epi16(v186, v182);
  v188 = _mm_sub_epi16(v185, _mm_srai_epi16(v182, 1u));
  v189 = _mm_add_epi16(v182, v188);
  v190 = _mm_sub_epi16(_mm_srai_epi16(v187, 1u), v132);
  v191 = _mm_sub_epi16(v186, v190);
  *((_QWORD*)p6 + 3) = v191.m128i_u64[0];
  *((_QWORD*)p6 + 18) = v189.m128i_u64[0];
  *((_QWORD*)p7 + 1) = v188.m128i_u64[0];
  *((_QWORD*)p7 + 16) = v190.m128i_u64[0];
  StoreHigh64((double*)&p6[v12], v191);
  StoreHigh64((double*)&p6[v14], v189);
  StoreHigh64((double*)&p7[v9], v188);
  StoreHigh64((double*)&p7[v10], v190);
}

void PostFilter2x4Stage1(std::int16_t* pa, __m128i* pb) {
  __m128i v2;
  __m128i v3;
  __m128i v4;
  __m128i v5;
  __m128i v6;
  __m128i v7;
  __m128i si128;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;

  v2 = _mm_loadl_epi64((const __m128i*)(pa + 4));
  v3 = _mm_loadl_epi64(pb);
  v4 = _mm_shufflelo_epi16(v3, 221);
  v5 = _mm_shufflelo_epi16(v3, 136);
  v6 = _mm_shufflelo_epi16(v2, 136);
  v7 = _mm_add_epi16(_mm_shufflelo_epi16(v2, 221), v4);
  si128 = _mm_load_si128(&vector_short_1_0);
  v9 = _mm_add_epi16(v6, v5);
  v10 = _mm_srai_epi16(_mm_add_epi16(v7, si128), 1u);
  v11 = _mm_sub_epi16(v4, v10);
  v12 = _mm_srai_epi16(_mm_add_epi16(v9, si128), 1u);
  v13 = _mm_sub_epi16(_mm_sub_epi16(v5, v12), _mm_srai_epi16(_mm_add_epi16(v11, si128), 1u));
  v14 = _mm_add_epi16(v13, v12);
  v15 = _mm_load_si128(&vector_short_16);
  v16 = _mm_add_epi16(_mm_add_epi16(v11, _mm_srai_epi16(_mm_add_epi16(v13, si128), 1u)), v10);
  v17 = _mm_sub_epi16(
      v7,
      _mm_sub_epi16(v16, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v16, 1u), v16), v15), 5u)));
  v18 = _mm_sub_epi16(
      v9,
      _mm_sub_epi16(v14, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v14, 1u), v14), v15), 5u)));
  v19 = _mm_add_epi16(
      v16, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v17, 1u), v17), vector_short_8), 4u));
  v20 = _mm_add_epi16(
      v14, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v18, 1u), v18), vector_short_8), 4u));
  *((_QWORD*)pa + 1) =
      _mm_unpacklo_epi16(
          _mm_add_epi16(
              v18,
              _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v20, 1u), v20), v15), 5u)),
          _mm_add_epi16(
              v17,
              _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v19, 1u), v19), v15), 5u)))
          .m128i_u64[0];
  pb->m128i_i64[0] = _mm_unpacklo_epi16(v20, v19).m128i_u64[0];
}

void PostFilter2x4Stage1_4(std::int16_t* pa, std::int16_t* pb, double* pc, double* pd,
                           std::int16_t* pe, std::int16_t* pf, double* pg, std::int16_t* ph) {
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i si128;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;
  __m128i v40;
  __m128i v41;
  __m128i v42;

  v8 = LoadPair64(pa + 4, pc + 1);
  v9 = LoadPair64(pb, pd);
  v10 = LoadPair64(pf, ph);
  v11 = LoadPair64(pe + 4, pg + 1);
  v12 = _mm_unpackhi_epi16(v8, v11);
  v13 = _mm_unpacklo_epi16(v8, v11);
  v14 = _mm_unpacklo_epi16(v9, v10);
  v15 = _mm_unpackhi_epi16(v9, v10);
  v16 = _mm_unpackhi_epi16(v13, v12);
  v17 = _mm_unpacklo_epi16(v13, v12);
  si128 = _mm_load_si128(&vector_short_1_0);
  v19 = _mm_unpacklo_epi16(v14, v15);
  v20 = _mm_unpackhi_epi16(v14, v15);
  v21 = _mm_unpackhi_epi16(v19, v20);
  v22 = _mm_unpacklo_epi16(v19, v20);
  v23 = _mm_add_epi16(_mm_unpackhi_epi16(v17, v16), v21);
  v24 = _mm_add_epi16(_mm_unpacklo_epi16(v17, v16), v22);
  v25 = _mm_srai_epi16(_mm_add_epi16(v23, si128), 1u);
  v26 = _mm_sub_epi16(v21, v25);
  v27 = _mm_srai_epi16(_mm_add_epi16(v24, si128), 1u);
  v28 = _mm_sub_epi16(_mm_sub_epi16(v22, v27), _mm_srai_epi16(_mm_add_epi16(v26, si128), 1u));
  v29 = _mm_add_epi16(v28, v27);
  v30 = _mm_load_si128(&vector_short_16);
  v31 = _mm_add_epi16(_mm_add_epi16(v26, _mm_srai_epi16(_mm_add_epi16(v28, si128), 1u)), v25);
  v32 = _mm_sub_epi16(
      v23,
      _mm_sub_epi16(v31, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v31, 1u), v31), v30), 5u)));
  v33 = _mm_sub_epi16(
      v24,
      _mm_sub_epi16(v29, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v29, 1u), v29), v30), 5u)));
  v34 = _mm_add_epi16(
      v31, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v32, 1u), v32), vector_short_8), 4u));
  v35 = _mm_add_epi16(
      v29, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v33, 1u), v33), vector_short_8), 4u));
  v36 = _mm_unpacklo_epi16(v35, v34);
  v37 = _mm_add_epi16(
      v32, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v34, 1u), v34), v30), 5u));
  v38 = _mm_add_epi16(_mm_slli_epi16(v35, 1u), v35);
  v39 = _mm_unpackhi_epi16(v35, v34);
  v40 = _mm_add_epi16(v33, _mm_srai_epi16(_mm_add_epi16(v38, v30), 5u));
  v41 = _mm_unpackhi_epi16(v40, v37);
  v42 = _mm_unpacklo_epi16(v40, v37);
  *((_QWORD*)pa + 1) = v42.m128i_u64[0];
  StoreHigh64((double*)pc + 1, v42);
  *((_QWORD*)pe + 1) = v41.m128i_u64[0];
  StoreHigh64((double*)pg + 1, v41);
  *(_QWORD*)pb = v36.m128i_u64[0];
  StoreHigh64(pd, v36);
  *(_QWORD*)pf = v39.m128i_u64[0];
  StoreHigh64((double*)ph, v39);
}

void PostFilter4x2Stage1(__m128i* pa, __m128i* pb) {
  __m128i v2;
  __m128i v3;
  __m128i v4;
  __m128i v5;
  __m128i v6;
  __m128i v7;
  __m128i si128;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;

  v2 = _mm_loadl_epi64(pa + 1);
  v3 = _mm_loadl_epi64(pb);
  v4 = _mm_shuffle_epi32(v3, 85);
  v5 = _mm_shuffle_epi32(v3, 0);
  v6 = _mm_shuffle_epi32(v2, 0);
  v7 = _mm_add_epi16(_mm_shuffle_epi32(v2, 85), v4);
  si128 = _mm_load_si128(&vector_short_1_0);
  v9 = _mm_add_epi16(v6, v5);
  v10 = _mm_srai_epi16(_mm_add_epi16(v7, si128), 1u);
  v11 = _mm_sub_epi16(v4, v10);
  v12 = _mm_srai_epi16(_mm_add_epi16(v9, si128), 1u);
  v13 = _mm_sub_epi16(_mm_sub_epi16(v5, v12), _mm_srai_epi16(_mm_add_epi16(v11, si128), 1u));
  v14 = _mm_add_epi16(v13, v12);
  v15 = _mm_load_si128(&vector_short_16);
  v16 = _mm_add_epi16(_mm_add_epi16(v11, _mm_srai_epi16(_mm_add_epi16(v13, si128), 1u)), v10);
  v17 = _mm_sub_epi16(
      v7,
      _mm_sub_epi16(v16, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v16, 1u), v16), v15), 5u)));
  v18 = _mm_sub_epi16(
      v9,
      _mm_sub_epi16(v14, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v14, 1u), v14), v15), 5u)));
  v19 = _mm_add_epi16(
      v16, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v17, 1u), v17), vector_short_8), 4u));
  v20 = _mm_add_epi16(
      v14, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v18, 1u), v18), vector_short_8), 4u));
  pa[1].m128i_i64[0] =
      _mm_unpacklo_epi32(
          _mm_add_epi16(
              v18,
              _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v20, 1u), v20), v15), 5u)),
          _mm_add_epi16(
              v17,
              _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v19, 1u), v19), v15), 5u)))
          .m128i_u64[0];
  pb->m128i_i64[0] = _mm_unpacklo_epi32(v20, v19).m128i_u64[0];
}

void PostFilter4x2Stage1_4(std::int16_t* pa, std::int16_t* pb, double* pc, double* pd,
                           std::int16_t* pe, std::int16_t* pf, double* pg, std::int16_t* ph) {
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i si128;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;
  __m128i v19;
  __m128i v20;
  __m128i v21;
  __m128i v22;
  __m128i v23;
  __m128i v24;
  __m128i v25;
  __m128i v26;
  __m128i v27;
  __m128i v28;
  __m128i v29;
  __m128i v30;
  __m128i v31;
  __m128i v32;
  __m128i v33;
  __m128i v34;
  __m128i v35;
  __m128i v36;
  __m128i v37;
  __m128i v38;
  __m128i v39;

  v8 = LoadPair64(pa + 8, pc + 2);
  v9 = LoadPair64(pb, pd);
  v10 = LoadPair64(pe + 8, pg + 2);
  v11 = _mm_unpackhi_epi32(v8, v10);
  v12 = _mm_unpacklo_epi32(v8, v10);
  si128 = _mm_load_si128(&vector_short_1_0);
  v14 = LoadPair64(pf, ph);
  v15 = _mm_unpacklo_epi32(v9, v14);
  v16 = _mm_unpackhi_epi32(v9, v14);
  v17 = v15;
  v18 = _mm_unpackhi_epi32(v15, v16);
  v19 = _mm_unpacklo_epi32(v17, v16);
  v20 = _mm_add_epi16(_mm_unpackhi_epi32(v12, v11), v18);
  v21 = _mm_add_epi16(_mm_unpacklo_epi32(v12, v11), v19);
  v22 = _mm_srai_epi16(_mm_add_epi16(v20, si128), 1u);
  v23 = _mm_sub_epi16(v18, v22);
  v24 = _mm_srai_epi16(_mm_add_epi16(v21, si128), 1u);
  v25 = _mm_sub_epi16(_mm_sub_epi16(v19, v24), _mm_srai_epi16(_mm_add_epi16(v23, si128), 1u));
  v26 = _mm_add_epi16(v25, v24);
  v27 = _mm_load_si128(&vector_short_16);
  v28 = _mm_add_epi16(_mm_add_epi16(v23, _mm_srai_epi16(_mm_add_epi16(v25, si128), 1u)), v22);
  v29 = _mm_sub_epi16(
      v20,
      _mm_sub_epi16(v28, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v28, 1u), v28), v27), 5u)));
  v30 = _mm_sub_epi16(
      v21,
      _mm_sub_epi16(v26, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v26, 1u), v26), v27), 5u)));
  v31 = _mm_add_epi16(
      v28, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v29, 1u), v29), vector_short_8), 4u));
  v32 = _mm_add_epi16(
      v26, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v30, 1u), v30), vector_short_8), 4u));
  v33 = _mm_unpacklo_epi32(v32, v31);
  v34 = _mm_add_epi16(
      v29, _mm_srai_epi16(_mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v31, 1u), v31), v27), 5u));
  v35 = _mm_add_epi16(_mm_slli_epi16(v32, 1u), v32);
  v36 = _mm_unpackhi_epi32(v32, v31);
  v37 = _mm_add_epi16(v30, _mm_srai_epi16(_mm_add_epi16(v35, v27), 5u));
  v38 = _mm_unpackhi_epi32(v37, v34);
  v39 = _mm_unpacklo_epi32(v37, v34);
  *((_QWORD*)pa + 2) = v39.m128i_u64[0];
  StoreHigh64((double*)pc + 2, v39);
  *((_QWORD*)pe + 2) = v38.m128i_u64[0];
  StoreHigh64((double*)pg + 2, v38);
  *(_QWORD*)pb = v33.m128i_u64[0];
  StoreHigh64(pd, v33);
  *(_QWORD*)pf = v36.m128i_u64[0];
  StoreHigh64((double*)ph, v36);
}

void PostFilter2x2Stage1(__m128i* p) {
  __m128i si128;
  __m128i v2;
  __m128i v3;
  __m128i v4;
  __m128i v5;
  __m128i v6;
  __m128i v7;
  __m128i v8;
  __m128i v9;
  __m128i v10;
  __m128i v11;
  __m128i v12;
  __m128i v13;
  __m128i v14;
  __m128i v15;
  __m128i v16;
  __m128i v17;
  __m128i v18;

  si128 = _mm_load_si128(&vector_short_1_0);
  v2 = _mm_loadl_epi64(p);
  v3 = _mm_shufflelo_epi16(v2, 170);
  v4 = _mm_shufflelo_epi16(v2, 255);
  v5 = _mm_shufflelo_epi16(v2, 85);
  v6 = _mm_add_epi16(v2, v4);
  v7 = _mm_add_epi16(v5, v3);
  v8 = _mm_srai_epi16(_mm_add_epi16(v6, si128), 1u);
  v9 = _mm_sub_epi16(v4, v8);
  v10 = _mm_srai_epi16(_mm_add_epi16(v7, si128), 1u);
  v11 = _mm_sub_epi16(_mm_sub_epi16(v3, v10), _mm_srai_epi16(_mm_add_epi16(v9, si128), 1u));
  v12 = _mm_add_epi16(v11, v10);
  v13 = _mm_load_si128(&vector_short_16);
  v14 = _mm_add_epi16(_mm_add_epi16(v9, _mm_srai_epi16(_mm_add_epi16(v11, si128), 1u)), v8);
  v15 = _mm_sub_epi16(
      v6,
      _mm_sub_epi16(v14, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v14, 1u), v14), v13), 5u)));
  v16 = _mm_sub_epi16(
      v7,
      _mm_sub_epi16(v12, _mm_srai_epi16(
                             _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v12, 1u), v12), v13), 5u)));
  v17 = _mm_add_epi16(
      v14, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v15, 1u), v15), vector_short_8), 4u));
  v18 = _mm_add_epi16(
      v12, _mm_srai_epi16(
               _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v16, 1u), v16), vector_short_8), 4u));
  p->m128i_i64[0] =
      _mm_unpacklo_epi16(
          _mm_unpacklo_epi16(
              _mm_add_epi16(
                  v15, _mm_srai_epi16(
                           _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v17, 1u), v17), v13), 5u)),
              v18),
          _mm_unpacklo_epi16(
              _mm_add_epi16(
                  v16, _mm_srai_epi16(
                           _mm_add_epi16(_mm_add_epi16(_mm_slli_epi16(v18, 1u), v18), v13), 5u)),
              v17))
          .m128i_u64[0];
}

namespace {

std::int16_t* Offset(std::int16_t* base, std::ptrdiff_t byteOffset) {
  return reinterpret_cast<std::int16_t*>(reinterpret_cast<std::uint8_t*>(base) + byteOffset);
}

__m128i* VectorAt(std::int16_t* base, std::ptrdiff_t byteOffset) {
  return reinterpret_cast<__m128i*>(Offset(base, byteOffset));
}

double* DoubleAt(std::int16_t* base, std::ptrdiff_t byteOffset) {
  return reinterpret_cast<double*>(Offset(base, byteOffset));
}

void PostFilter4x4Stage1_4At(std::int16_t* base0, std::ptrdiff_t o0, std::int16_t* base1,
                             std::ptrdiff_t o1, std::int16_t* base2, std::ptrdiff_t o2,
                             std::int16_t* base3, std::ptrdiff_t o3, std::int16_t* base4,
                             std::ptrdiff_t o4, std::int16_t* base5, std::ptrdiff_t o5,
                             std::int16_t* base6, std::ptrdiff_t o6, std::int16_t* base7,
                             std::ptrdiff_t o7) {
  PostFilter4x4Stage1_4(Offset(base0, o0), Offset(base1, o1), DoubleAt(base2, o2),
                        DoubleAt(base3, o3), Offset(base4, o4), Offset(base5, o5),
                        DoubleAt(base6, o6), DoubleAt(base7, o7));
}

void PostFilter2x4Stage1_4At(std::int16_t* base0, std::ptrdiff_t o0, std::int16_t* base1,
                             std::ptrdiff_t o1, std::int16_t* base2, std::ptrdiff_t o2,
                             std::int16_t* base3, std::ptrdiff_t o3, std::int16_t* base4,
                             std::ptrdiff_t o4, std::int16_t* base5, std::ptrdiff_t o5,
                             std::int16_t* base6, std::ptrdiff_t o6, std::int16_t* base7,
                             std::ptrdiff_t o7) {
  PostFilter2x4Stage1_4(Offset(base0, o0), Offset(base1, o1), DoubleAt(base2, o2),
                        DoubleAt(base3, o3), Offset(base4, o4), Offset(base5, o5),
                        DoubleAt(base6, o6), Offset(base7, o7));
}

void PostFilter4x2Stage1_4At(std::int16_t* base0, std::ptrdiff_t o0, std::int16_t* base1,
                             std::ptrdiff_t o1, std::int16_t* base2, std::ptrdiff_t o2,
                             std::int16_t* base3, std::ptrdiff_t o3, std::int16_t* base4,
                             std::ptrdiff_t o4, std::int16_t* base5, std::ptrdiff_t o5,
                             std::int16_t* base6, std::ptrdiff_t o6, std::int16_t* base7,
                             std::ptrdiff_t o7) {
  PostFilter4x2Stage1_4(Offset(base0, o0), Offset(base1, o1), DoubleAt(base2, o2),
                        DoubleAt(base3, o3), Offset(base4, o4), Offset(base5, o5),
                        DoubleAt(base6, o6), Offset(base7, o7));
}

void PostFilter4x4Stage1_8At(std::int16_t* base0, std::ptrdiff_t o0, std::int16_t* base1,
                             std::ptrdiff_t o1, std::int16_t* base2, std::ptrdiff_t o2,
                             std::int16_t* base3, std::ptrdiff_t o3, std::int16_t* base4,
                             std::ptrdiff_t o4, std::int16_t* base5, std::ptrdiff_t o5,
                             std::int16_t* base6, std::ptrdiff_t o6, std::int16_t* base7,
                             std::ptrdiff_t o7, int offset) {
  PostFilter4x4Stage1_8(Offset(base0, o0), Offset(base1, o1), Offset(base2, o2), Offset(base3, o3),
                        Offset(base4, o4), Offset(base5, o5), Offset(base6, o6), Offset(base7, o7),
                        offset);
}

} // namespace

void InvTransformMacroblock_SSE2_16(hdpCodec_t* codec, int mbX, int mbY) {
  const bool right = mbX == static_cast<int>(codec->widthInMacroblocks);
  const bool bottom = mbY == static_cast<int>(codec->heightInMacroblocks);
  const bool left = mbX == 0;
  const bool top = mbY == 0;
  const int edgeCase = (!left && !right ? 1 : 0) + (right ? 2 : 0) +
                       3 * ((!top && !bottom ? 1 : 0) + (bottom ? 2 : 0));
  std::uint64_t channelCount =
      codec->codecParms.colorFormat == CF_YUV_422 ? 1 : codec->codecParms.numChannels;
  const std::uint64_t thumbnailScale = codec->codecParms.thumbnailScale;
  if (thumbnailScale >= 16)
    return;

  for (std::uint64_t channel = 0; channel < channelCount; ++channel) {
    std::int16_t* previous = codec->macroBlockColumnPtr0[channel];
    std::int16_t* current = codec->macroBlockColumnPtr1[channel];
    if (!right && !bottom)
      InvTransform4x4Stage2(reinterpret_cast<__m128i*>(current));
    if (thumbnailScale >= 4)
      continue;

    switch (edgeCase) {
    case 0:
      InvTransform4x4Stage1_4(VectorAt(current, 0));
      PostFilter2x2Stage1(VectorAt(current, 0));
      PostFilter4x2Stage1(VectorAt(current, 0), VectorAt(current, 0x20));
      PostFilter2x4Stage1(Offset(current, 0), VectorAt(current, 0x80));
      PostFilter4x4Stage1(VectorAt(current, 0x18), VectorAt(current, 0x90), VectorAt(current, 0x28),
                          VectorAt(current, 0xA0));
      break;
    case 1:
      InvTransform4x4Stage1_8(VectorAt(current, -0x100), VectorAt(current, 0));
      PostFilter2x4Stage1_4At(current, -0x180, current, -0x100, current, -0x100, current, -0x80,
                              current, -0x80, current, 0, current, 0, current, 0x80);
      PostFilter4x4Stage1_4At(current, -0x180, current, -0x160, current, -0x100, current, -0xE0,
                              current, -0x80, current, -0x60, current, 0, current, 0x20);
      break;
    case 2:
      InvTransform4x4Stage1_4(VectorAt(current, -0x100));
      PostFilter2x2Stage1(VectorAt(current, -0x78));
      PostFilter2x4Stage1(Offset(current, -0x180), VectorAt(current, -0x100));
      PostFilter2x4Stage1(Offset(current, -0x100), VectorAt(current, -0x80));
      PostFilter4x2Stage1(VectorAt(current, -0x78), VectorAt(current, -0x58));
      PostFilter4x4Stage1(VectorAt(current, -0x168), VectorAt(current, -0xF0),
                          VectorAt(current, -0x158), VectorAt(current, -0xE0));
      PostFilter4x4Stage1(VectorAt(current, -0xE8), VectorAt(current, -0x70),
                          VectorAt(current, -0xD8), VectorAt(current, -0x60));
      break;
    case 3:
      InvTransform4x4Stage1_8(VectorAt(previous, 0x40), VectorAt(current, 0));
      PostFilter4x2Stage1_4At(current, 0, current, 0x20, previous, 0x60, current, 0, previous, 0x40,
                              previous, 0x60, previous, 0x20, previous, 0x40);
      PostFilter4x4Stage1_4At(previous, 0x20, previous, 0x40, previous, 0x40, previous, 0x60,
                              previous, 0x60, current, 0, current, 0, current, 0x20);
      break;
    case 4:
      InvTransform4x4Stage1_8(VectorAt(previous, -0xC0), VectorAt(previous, 0x40));
      InvTransform4x4Stage1_8(VectorAt(current, -0x100), VectorAt(current, 0));
      PostFilter4x4Stage1_8At(previous, -0x160, previous, -0x140, previous, -0x140, previous,
                              -0x120, previous, -0x120, current, -0x180, current, -0x180, current,
                              -0x160, 0x40);
      PostFilter4x4Stage1_8At(previous, -0x60, previous, -0x40, previous, -0x40, previous, -0x20,
                              previous, -0x20, current, -0x80, current, -0x80, current, -0x60,
                              0x40);
      break;
    case 5:
      InvTransform4x4Stage1_8(VectorAt(previous, -0xC0), VectorAt(current, -0x100));
      PostFilter4x2Stage1_4At(current, -0x78, current, -0x58, previous, -0x18, current, -0x78,
                              previous, -0x38, previous, -0x18, previous, -0x58, previous, -0x38);
      PostFilter4x4Stage1_8At(previous, -0x160, previous, -0x140, previous, -0x140, previous,
                              -0x120, previous, -0x120, current, -0x180, current, -0x180, current,
                              -0x160, 0x40);
      break;
    case 6:
      InvTransform4x4Stage1_4(VectorAt(previous, 0x40));
      PostFilter2x2Stage1(VectorAt(previous, 0x70));
      PostFilter4x2Stage1(VectorAt(previous, 0x20), VectorAt(previous, 0x40));
      PostFilter4x2Stage1(VectorAt(previous, 0x40), VectorAt(previous, 0x60));
      PostFilter2x4Stage1(Offset(previous, 0x70), VectorAt(previous, 0xF0));
      PostFilter4x4Stage1(VectorAt(previous, 0x38), VectorAt(previous, 0xB0),
                          VectorAt(previous, 0x48), VectorAt(previous, 0xC0));
      PostFilter4x4Stage1(VectorAt(previous, 0x58), VectorAt(previous, 0xD0),
                          VectorAt(previous, 0x68), VectorAt(previous, 0xE0));
      break;
    case 7:
      InvTransform4x4Stage1_8(VectorAt(previous, -0xC0), VectorAt(previous, 0x40));
      PostFilter2x4Stage1_4At(previous, -0x110, previous, -0x90, previous, -0x90, previous, -0x10,
                              previous, -0x10, previous, 0x70, previous, 0x70, previous, 0xF0);
      PostFilter4x4Stage1_8At(previous, -0x160, previous, -0x140, previous, -0x140, previous,
                              -0x120, previous, -0xE0, previous, -0xC0, previous, -0xC0, previous,
                              -0xA0, 0x80);
      break;
    case 8:
      InvTransform4x4Stage1_4(VectorAt(previous, -0xC0));
      PostFilter2x2Stage1(VectorAt(previous, -0x08));
      PostFilter4x2Stage1(VectorAt(previous, -0x58), VectorAt(previous, -0x38));
      PostFilter4x2Stage1(VectorAt(previous, -0x38), VectorAt(previous, -0x18));
      PostFilter2x4Stage1(Offset(previous, -0x110), VectorAt(previous, -0x90));
      PostFilter2x4Stage1(Offset(previous, -0x90), VectorAt(previous, -0x10));
      PostFilter4x4Stage1_4At(previous, -0x160, previous, -0x140, previous, -0x140, previous,
                              -0x120, previous, -0xE0, previous, -0xC0, previous, -0xC0, previous,
                              -0xA0);
      break;
    }
  }
}
