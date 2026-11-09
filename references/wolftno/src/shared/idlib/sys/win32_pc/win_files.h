#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

bool Sys_Mkdir(const char* path);
bool Sys_Rmdir(const char* path);
bool Sys_IsFileWritable(const char* path);
int Sys_IsFolder(const char* path);
char* Sys_CWD();
const char* Sys_Install();
char* Sys_EXEPath();
void Sys_ListFiles(const char* directory, const char* extension, idList<idStr, 44>& list);
