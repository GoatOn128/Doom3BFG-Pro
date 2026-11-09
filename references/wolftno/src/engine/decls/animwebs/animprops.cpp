#include "animprops.h"
#include "engine/animation/animationpreload.h"

void idMD6AnimProps::SetAliasHandle(std::uint16_t handle) { aliasHandle=handle;anim=nullptr; }
void idMD6AnimProps::SetAnim(const idMD6Anim* value) { anim=value;aliasHandle=0xffff; }
int idMD6AnimProps::NumAnims(const idDeclMD6* decl)const{if(anim)return 1;if(aliasHandle==0xffff)return 0;const idMD6Alias*a=decl->FindAlias(aliasHandle);return a?a->mappings.Num():0;}
const idMD6Anim*idMD6AnimProps::SelectAnim(const idDeclMD6*decl,int index)const{if(anim)return anim;if(aliasHandle==0xffff)return nullptr;const idMD6Alias*a=decl->FindAlias(aliasHandle);return a?a->mappings[index].anim:nullptr;}
const idMD6Anim*idMD6AnimProps::SelectAndLoadAnim(const idDeclMD6*decl,int index)const{if(anim)return anim;if(aliasHandle==0xffff)return nullptr;const idMD6Alias*a=decl->FindAlias(aliasHandle);if(!a)return nullptr;return AnimationPreloadService()?AnimationPreloadService()->ResolveAnim(a->mappings[index].animRef.str):nullptr;}
