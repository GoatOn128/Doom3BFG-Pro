#include "engine/guis/swf/swf_bitstream.h"

#include <cassert>
#include <cstdlib>
#include <cstring>

namespace {
std::uint32_t Mask(unsigned int bits) { return bits == 32 ? 0xFFFFFFFFu : ((1u << bits) - 1u); }
}

idSWFBitStream& idSWFBitStream::operator=(idSWFBitStream& other) {
  if (this == &other) return *this;
  Free(); free=other.free; startp=other.startp; readp=other.readp; endp=other.endp;
  currentBit=other.currentBit; currentByte=other.currentByte;
  if(other.free) other.free=false;
  return *this;
}
void idSWFBitStream::Free(){if(free)std::free(const_cast<std::uint8_t*>(startp));free=false;startp=endp=readp=nullptr;ResetBits();}
void idSWFBitStream::Load(const std::uint8_t* data,std::uint32_t len,bool copy){Free();if(copy){free=true;auto* owned=static_cast<std::uint8_t*>(std::malloc(len));std::memcpy(owned,data,len);startp=owned;}else startp=data;endp=startp+len;readp=startp;ResetBits();}
std::uint32_t idSWFBitStream::ReadEncodedU32(){std::uint32_t result=0;for(int i=0;i<5;++i){const std::uint8_t b=ReadU8();result|=(b&0x7Fu)<<(7*i);if((b&0x80u)==0)return result;}return result;}
const std::uint8_t* idSWFBitStream::ReadData(int size){assert(readp>=startp&&readp<=endp);ResetBits();if(readp+size>endp){assert(false);readp=endp;return startp;}const auto* result=readp;readp+=size;return result;}
unsigned int idSWFBitStream::ReadInternalU(std::uint64_t& bit,std::uint64_t& byte,unsigned int numBits){assert(numBits<=32);const std::uint64_t extra=(numBits-bit+7)>>3;bit=bit+(extra<<3)-numBits;for(std::uint64_t i=0;i<extra;++i)byte=(byte<<8)|readp[i];readp+=extra;return static_cast<unsigned int>((byte>>bit)&Mask(numBits));}
int idSWFBitStream::ReadInternalS(std::uint64_t& bit,std::uint64_t& byte,unsigned int numBits){const unsigned int value=ReadInternalU(bit,byte,numBits);if(numBits==0)return 0;const std::uint32_t sign=1u<<(numBits-1);return static_cast<int>((value^sign)-sign);}
unsigned int idSWFBitStream::ReadU(unsigned int numBits){return ReadInternalU(currentBit,currentByte,numBits);}
int idSWFBitStream::ReadS(unsigned int numBits){return ReadInternalS(currentBit,currentByte,numBits);}
double idSWFBitStream::ReadDouble(){const auto* input=ReadData(8);std::uint8_t bytes[8]={input[4],input[5],input[6],input[7],input[0],input[1],input[2],input[3]};double result;std::memcpy(&result,bytes,8);return result;}
const char* idSWFBitStream::ReadString(){return reinterpret_cast<const char*>(ReadData(static_cast<int>(std::strlen(reinterpret_cast<const char*>(readp)))+1));}
void idSWFBitStream::ReadRect(swfRect_t& rect){std::uint64_t bit=0,byte=0;const unsigned n=ReadInternalU(bit,byte,5);rect.tl.x=ReadInternalS(bit,byte,n)/20.0f;rect.br.x=ReadInternalS(bit,byte,n)/20.0f;rect.tl.y=ReadInternalS(bit,byte,n)/20.0f;rect.br.y=ReadInternalS(bit,byte,n)/20.0f;currentBit=bit;currentByte=byte;}
void idSWFBitStream::ReadMatrix(swfMatrix_t& matrix){std::uint64_t bit=0,byte=0;int xx=65536,yy=65536,xy=0,yx=0;if(ReadInternalU(bit,byte,1)){const unsigned n=ReadInternalU(bit,byte,5);xx=ReadInternalS(bit,byte,n);yy=ReadInternalS(bit,byte,n);}if(ReadInternalU(bit,byte,1)){const unsigned n=ReadInternalU(bit,byte,5);yx=ReadInternalS(bit,byte,n);xy=ReadInternalS(bit,byte,n);}const unsigned n=ReadInternalU(bit,byte,5);const int tx=ReadInternalS(bit,byte,n),ty=ReadInternalS(bit,byte,n);currentBit=bit;currentByte=byte;matrix.xx=xx/65536.0f;matrix.yy=yy/65536.0f;matrix.yx=yx/65536.0f;matrix.xy=xy/65536.0f;matrix.tx=tx/20.0f;matrix.ty=ty/20.0f;}
void idSWFBitStream::ReadColorXFormRGBA(swfColorXform_t& cxf){std::uint64_t bit=0,byte=0;const bool hasAdd=ReadInternalU(bit,byte,1)!=0,hasMul=ReadInternalU(bit,byte,1)!=0;const unsigned n=ReadInternalU(bit,byte,4);int m[4]={256,256,256,256},a[4]={0,0,0,0};if(hasMul)for(int& v:m)v=ReadInternalS(bit,byte,n);if(hasAdd)for(int& v:a)v=ReadInternalS(bit,byte,n);currentBit=bit;currentByte=byte;for(int i=0;i<4;++i){cxf.mul[i]=m[i]/256.0f;cxf.add[i]=a[i]/256.0f;}}
void idSWFBitStream::ReadColorRGB(swfColorRGB_t& c){ResetBits();c.r=*readp++;c.g=*readp++;c.b=*readp++;}
void idSWFBitStream::ReadColorRGBA(swfColorRGBA_t& c){ReadColorRGB(c);c.a=*readp++;}
void idSWFBitStream::ReadGradient(swfGradient_t& g,bool rgba){g.numGradients=ReadU8()&0xF;for(int i=0;i<g.numGradients;++i){auto&r=g.gradientRecords[i];r.startRatio=ReadU8();if(rgba)ReadColorRGBA(r.startColor);else ReadColorRGB(r.startColor);r.endRatio=r.startRatio;r.endColor=r.startColor;}}
void idSWFBitStream::ReadMorphGradient(swfGradient_t& g){g.numGradients=ReadU8()&0xF;for(int i=0;i<g.numGradients;++i){auto&r=g.gradientRecords[i];r.startRatio=ReadU8();ReadColorRGBA(r.startColor);r.endRatio=ReadU8();ReadColorRGBA(r.endColor);}}
