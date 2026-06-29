/**
 * Host golden export for gen_data.py (ctypes). Algorithm body lives in alg11_12_ref.h.
 */
#include "alg11_12_ref.h"

void alg11_multiply_ntts(int32_t *h, const int32_t *f, const int32_t *g)
{
    alg11_multiply_ntts_inline(h, f, g);
}
