#include "animwebscalar.h"
#include "shared/idlib/lib.h"

void idScalarMemBlock::Alloc(int count){Free();num=count;floats=num>0?new float[num]:nullptr;}
void idScalarMemBlock::Free(){delete[]floats;floats=nullptr;num=0;}
void idScalarMemBlock::Free(idList<idAnimWebScalar>&scalars){for(int i=0;i<scalars.Num();++i)scalars[i].scalarIndex=-1;Free();}
void idScalarMemBlock::Free(idList<idAnimWebScalar,33>&scalars){for(int i=0;i<scalars.Num();++i)scalars[i].scalarIndex=-1;Free();}
void idAnimWebScalar::SetScalarPtr(float*s){if(s){scalarPtr=s;scalarIndex=-1;}}
void idAnimWebScalar::SetIndex(const idScalarMemBlock*,std::int16_t i){scalarIndex=i;scalarPtr=nullptr;}
float idAnimWebScalar::GetScalar(const idScalarMemBlock*b)const{if(scalarIndex>=0)return b->floats[scalarIndex];if(scalarPtr)return*scalarPtr;idLib::Warning("idAnimWebScalar::GetScalar - Trying to access uninitialized blend variable \"%s\"",name.str);return 0.0f;}
void idAnimWebScalar::SetScalar(idScalarMemBlock*b,float f){if(scalarIndex>=0)b->floats[scalarIndex]=f;else*scalarPtr=f;}
void idAnimWebScalar::Init(const char*n,std::uint8_t f,float*s){name.Set(n);flags=f;if(s)SetScalarPtr(s);}
