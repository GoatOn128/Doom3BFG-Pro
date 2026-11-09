#include "tungsten/game/decls/declaccolades.h"
#include <algorithm>
// DIA: idDeclAccolade::idDeclAccolade
// DIA: `idDeclAccolade::idDeclAccolade'::`1'::dtor$0
// DIA: `idDeclAccolade::idDeclAccolade'::`1'::dtor$1
// DIA: idDeclAccolade::GetDeclInfo
// DIA: idDeclAccolade::`scalar deleting destructor'
// DIA: idDeclAccolade::GetIconName
// DIA: idSort<idDeclAccolade const * __ptr64>::`scalar deleting destructor'
// DIA: idSort_DeclAccoladePtr::~idSort_DeclAccoladePtr
// DIA: idDeclAccolade::LoadAllDecls
// DIA: `idDeclAccolade::LoadAllDecls'::`1'::dtor$0
// DIA: `dynamic initializer for 'idDeclAccolade::orderedList''
// DIA: `dynamic atexit destructor for 'idDeclAccolade::orderedList''
idList<const idDeclAccolade*,5>idDeclAccolade::orderedList;std::vector<idDeclAccolade*>idDeclAccolade::registry;
idDeclAccolade::idDeclAccolade()=default;const char*idDeclAccolade::GetDeclInfo()const{return"idDeclAccolade";}const char*idDeclAccolade::GetIconName()const{return icon.c_str();}void idDeclAccolade::Register(idDeclAccolade*d){if(d&&std::find(registry.begin(),registry.end(),d)==registry.end())registry.push_back(d);}void idDeclAccolade::LoadAllDecls(){orderedList.SetNum(static_cast<int>(registry.size()));for(int i=0;i<orderedList.Num();++i)orderedList[i]=registry[static_cast<std::size_t>(i)];idSort_Quick<const idDeclAccolade*,idSort_DeclAccoladePtr>{}.Sort(orderedList);for(int i=0;i<orderedList.Num();++i)const_cast<idDeclAccolade*>(orderedList[i])->index=i;}idSort_DeclAccoladePtr::~idSort_DeclAccoladePtr()=default;template class idSort<const idDeclAccolade*>;
