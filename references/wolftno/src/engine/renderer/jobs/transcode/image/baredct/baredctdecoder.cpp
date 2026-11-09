#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <cstring>

idBareDctDecoder::idBareDctDecoder() : idBareDctBase() {
  std::memset(&imageWidth, 0,
              sizeof(idBareDctDecoder) - offsetof(idBareDctDecoder, imageWidth));
  InitQuantTable();
}

void idBareDctDecoder::SetHuffmanTables(
    const idBareDCTHuffmanTable* tableYDC,
    const idBareDCTHuffmanTable* tableYAC,
    const idBareDCTHuffmanTable* tableCoCgDC,
    const idBareDCTHuffmanTable* tableCoCgAC,
    const idBareDCTHuffmanTable* tableADC,
    const idBareDCTHuffmanTable* tableAAC) {
  pHuffTableYDC = tableYDC;
  pHuffTableYAC = tableYAC;
  pHuffTableCoCgAC = tableCoCgAC;
  pHuffTableCoCgDC = tableCoCgDC;
  pHuffTableADC = tableADC;
  pHuffTableAAC = tableAAC;
}

