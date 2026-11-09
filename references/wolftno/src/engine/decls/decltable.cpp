#include "decltable.h"

#include <algorithm>
#include <cfloat>

idDeclInfoTemplate<idDeclTable> idDeclTable::resourceList("table", "idDeclTable", "m2", false);

idDeclTable::idDeclTable() : left(0.0f), right(1.0f), table(new idLookupTable) {
  resourceListPtr = &resourceList;
}

idDeclTable::~idDeclTable() {
  delete table;
  table = nullptr;
}

void idDeclTable::FreeData() { table->Clear(); }
const char* idDeclTable::DefaultDefinition() const { return "{ { 0 } }\n"; }
idDeclInfo* idDeclTable::GetDeclInfo() const { return &resourceList; }
float idDeclTable::TableLookup(float value, bool fastSearch) const {
  return table->TableLookup(value, fastSearch);
}

void idDeclTable::Parse(idParser& source) {
  table->Clear();
  bool clamp = false;
  float valueMinimum = FLT_MAX;
  float valueMaximum = -FLT_MAX;
  float implicitTime = 0.0f;
  idToken token;

  while (source.ReadToken(token)) {
    if (token == "}")
      break;
    if (token.Icmp("snap") == 0) {
      table->SetSnap(true);
    } else if (token.Icmp("clamp") == 0) {
      clamp = true;
    } else if (token.Icmp("spline") == 0) {
      table->SetSpline(true);
    } else if (token.Icmp("max") == 0) {
      table->SetRange(table->GetMinimum(), source.ParseFloat());
    } else if (token.Icmp("min") == 0) {
      table->SetRange(source.ParseFloat(), table->GetMaximum());
    } else if (token.Icmp("left") == 0) {
      left = source.ParseFloat();
    } else if (token.Icmp("right") == 0) {
      right = source.ParseFloat();
    } else if (token == "{") {
      for (;;) {
        bool error = false;
        float value = source.ParseFloat(&error);
        if (error)
          break;
        float time = implicitTime;
        if (source.CheckTokenString(":")) {
          time = value;
          value = source.ParseFloat(&error);
          if (error)
            break;
        }
        if (table->GetNumValues() >= 64) {
          source.Warning("more than %d points", 64);
          break;
        }
        valueMinimum = std::min(valueMinimum, value);
        valueMaximum = std::max(valueMaximum, value);
        table->AddValue(time, value);
        implicitTime = time + 1.0f;
        if (!source.ReadToken(token) || token == "}")
          break;
        if (token != ",") {
          source.Warning("expected comma or brace");
          break;
        }
      }
    } else {
      source.Warning("unknown token '%s'", token.data);
      break;
    }
  }

  if (table->GetNumValues() > 0 && (valueMinimum < 0.0f || valueMaximum > 1.0f))
    table->NormalizeValues(valueMinimum, valueMaximum);
  if (table->IsSpline() && table->IsSnap()) {
    source.Warning("both spline and snap specified, snap ignored");
    table->SetSnap(false);
  }
  table->SetClamp(clamp);
  table->Finalize();
}
