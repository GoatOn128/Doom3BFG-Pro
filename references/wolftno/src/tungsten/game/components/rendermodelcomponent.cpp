#include "tungsten/game/components/rendermodelcomponent.h"
// DIA: idRenderModelInfo::SetColor
// DIA: idRenderModelInfo::GetColor
// DIA: idRenderModelInfo::InitProperties
// DIA: idRenderModelInfo::SetColor
// DIA: idRenderModelInfo::SetColor
// DIA: idRenderModelInfo::SetColor
// DIA: idRenderModelInfo::SetColor
// DIA: idRenderModelInfo::GetColor
// DIA: idRenderModelInfo::GetColor
// DIA: idRenderModelInfo::GetScale
// DIA: idRenderModelInfo::SetScale
// DIA: idRenderModelInfo::FreeRenderModel
// DIA: idRenderModelInfo::InitRenderModel
// DIA: idRenderModelInfo::CleanRenderParms
// DIA: idRenderModelInfo::GetRenderModelMaterial
// DIA: idRenderModelInfo::SetModel
// DIA: idRenderModelInfo::SetModelByName
// DIA: idRenderModelInfo::idRenderModelInfo
// DIA: `idRenderModelInfo::idRenderModelInfo'::`1'::dtor$0
idRenderModelInfo::idRenderModelInfo()=default;void idRenderModelInfo::SetColor(const idColor&v){color=v;}void idRenderModelInfo::GetColor(idColor&out)const{out=color;}void idRenderModelInfo::InitProperties(idRenderModel*renderModel,bool specular,bindLighting_t lighting){if(renderModel)model=renderModel;staticSpecularEnabled=specular;lightingMode=lighting;}void idRenderModelInfo::SetColor(float r,float g,float b,float a){color={r,g,b,a};}void idRenderModelInfo::SetColor(float r,float g,float b){SetColor(r,g,b,1);}void idRenderModelInfo::SetColor(const idVec3&v){SetColor(v.x,v.y,v.z,1);}void idRenderModelInfo::SetColor(const idVec4&v){SetColor(v.x,v.y,v.z,v.w);}void idRenderModelInfo::GetColor(idVec3&out)const{out={color.r,color.g,color.b};}void idRenderModelInfo::GetColor(idVec4&out)const{out={color.r,color.g,color.b,color.a};}void idRenderModelInfo::GetScale(idVec3&out)const{out=scale;}void idRenderModelInfo::SetScale(const idVec3&v){scale=v;}void idRenderModelInfo::FreeRenderModel(){model=nullptr;modelResource.clear();classDebugName.clear();}void idRenderModelInfo::InitRenderModel(const idVec3&p,const idMat3&a,bool specular,bindLighting_t lighting,const std::string&name,const std::string&className){origin=p;axis=a;if(!name.empty())SetModelByName(name,className);InitProperties(model,specular,lighting);}void idRenderModelInfo::CleanRenderParms(){renderParms.erase("color");renderParms.erase("lightColor");}const std::string* idRenderModelInfo::GetRenderModelMaterial()const{return material.empty()?nullptr:&material;}void idRenderModelInfo::SetModel(idRenderModel*value,const std::string&className){model=value;modelResource.clear();classDebugName=className;}void idRenderModelInfo::SetModelByName(const std::string&name,const std::string&className){model=nullptr;modelResource=name;classDebugName=className;}
