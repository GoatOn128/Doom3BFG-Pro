#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

struct idXMLAttribute {
  idStr name;
  idStr value;
};

class idXMLWriter {
public:
  idXMLWriter();
  ~idXMLWriter();
  bool CloseFile();
  bool CloseElement();
  bool CloseDocument();
  bool OpenFile(const char* relativePath, bool overwrite = true, bool savePath = false);
  bool OpenElement(const char* name);
  bool OpenElement(const char* name, const idList<idXMLAttribute, 44>& attributes);
  bool WriteElement(const char* name, const char* value);
  bool WriteElement(const char* name, const char* value,
                    const idList<idXMLAttribute, 44>& attributes);

private:
  struct stackXMLTag_t;
  struct tagStack_t {
    stackXMLTag_t* first;
    stackXMLTag_t* last;
  };

  void* xmlFile;
  int tabLevel;
  bool hasRoot;
  tagStack_t openTags;
};

static_assert(sizeof(idXMLAttribute) == 80, "idXMLAttribute ABI drift");
static_assert(sizeof(idXMLWriter) == 32, "idXMLWriter ABI drift");
