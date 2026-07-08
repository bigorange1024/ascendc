#ifndef DECOMPRESS_D_CONFIG_HPP
#define DECOMPRESS_D_CONFIG_HPP

#ifndef F203_DECOMPRESS_D
#define F203_DECOMPRESS_D 4
#endif

// 0=标量 fallback | 1=默认向量 per-lane（Decrypt 链）；见 docs/notes/F203-Compress-Decompress-向量实现指南.md
#ifndef DECOMPRESS_D_VEC
#define DECOMPRESS_D_VEC 1
#endif

#if F203_DECOMPRESS_D != 4 && F203_DECOMPRESS_D != 5 && F203_DECOMPRESS_D != 10 && F203_DECOMPRESS_D != 11
#error "F203_DECOMPRESS_D must be 4, 5, 10, or 11"
#endif

#endif
