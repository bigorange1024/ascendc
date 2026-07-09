#ifndef BYTE_ENCODE12_ROM_TABLES_H
#define BYTE_ENCODE12_ROM_TABLES_H

#include "kernel_operator.h"
#include <stdint.h>

#define BYTE_ENCODE12_PAIR_COUNT 128

extern __gm__ const int32_t gByteEncode12GatherEvenByteGm[BYTE_ENCODE12_PAIR_COUNT];
extern __gm__ const int32_t gByteEncode12GatherOddByteGm[BYTE_ENCODE12_PAIR_COUNT];

#endif
