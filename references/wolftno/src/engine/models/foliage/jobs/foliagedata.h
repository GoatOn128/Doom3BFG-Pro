#pragma once

#include <cstddef>

struct dm_detail_t;
struct dm_attribute_t;

struct dm_subTreeData_t {
  int numDetail;
  int detailOffset;
  int numAttributes;
  int attributeOffset;
};

struct dm_subTreeDataPtrs_t {
  dm_detail_t* detail;
  dm_attribute_t* attributes;
};

void SetupSubTreePtrsFromData(dm_subTreeDataPtrs_t& subTreePtrs,
                              const dm_subTreeData_t* subTree);

static_assert(sizeof(dm_subTreeData_t) == 16);
static_assert(offsetof(dm_subTreeData_t, detailOffset) == 4);
static_assert(offsetof(dm_subTreeData_t, attributeOffset) == 12);
static_assert(sizeof(dm_subTreeDataPtrs_t) == 16);
static_assert(offsetof(dm_subTreeDataPtrs_t, attributes) == 8);
