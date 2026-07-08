#ifndef BYTE_DECODE_D_CONFIG_HPP
#define BYTE_DECODE_D_CONFIG_HPP

#ifndef F203_BYTE_DECODE_D
#define F203_BYTE_DECODE_D 4
#endif

// 0=标量 | 1=默认（d=4 向量 nibble；d=5/10/11 标量 unpack，与 0 同体）；见 docs/notes/F203-ByteEncode-ByteDecode-d-向量与标量选型.md
#ifndef BYTE_DECODE_D_VEC
#define BYTE_DECODE_D_VEC 1
#endif

#if F203_BYTE_DECODE_D != 4 && F203_BYTE_DECODE_D != 5 && F203_BYTE_DECODE_D != 10 && F203_BYTE_DECODE_D != 11
#error "F203_BYTE_DECODE_D must be 4, 5, 10, or 11"
#endif

#if F203_BYTE_DECODE_D == 4
#define F203_BYTE_DECODE_POLY_BYTES 128
#elif F203_BYTE_DECODE_D == 5
#define F203_BYTE_DECODE_POLY_BYTES 160
#elif F203_BYTE_DECODE_D == 10
#define F203_BYTE_DECODE_POLY_BYTES 320
#elif F203_BYTE_DECODE_D == 11
#define F203_BYTE_DECODE_POLY_BYTES 352
#endif

#endif
