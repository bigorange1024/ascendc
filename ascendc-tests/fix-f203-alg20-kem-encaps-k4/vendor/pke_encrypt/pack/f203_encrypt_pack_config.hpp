#ifndef F203_ENCRYPT_PACK_CONFIG_HPP
#define F203_ENCRYPT_PACK_CONFIG_HPP

#include "f203_encrypt_layout.h"

/** ml_kem_1024 Encrypt：c₁ 用 d_u=11，c₂ 用 d_v=5。 */
#define F203_COMPRESS_D_U 11
#define F203_COMPRESS_D_V 5
#define F203_BYTE_ENCODE_D_U 11
#define F203_BYTE_ENCODE_D_V 5

#define F203_C1_POLY_BYTES 352U
#define F203_C1_BYTES (F203_ENCRYPT_K * F203_C1_POLY_BYTES)
#define F203_C2_BYTES 160U

#endif
