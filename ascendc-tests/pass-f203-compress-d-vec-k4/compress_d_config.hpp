#ifndef COMPRESS_D_CONFIG_HPP
#define COMPRESS_D_CONFIG_HPP

#ifndef F203_COMPRESS_D
#define F203_COMPRESS_D 4
#endif

// 0=标量 fallback | 1=默认向量 per-lane（tail 抄此）；见 docs/notes/F203-Compress-Decompress-向量实现指南.md
#ifndef COMPRESS_D_VEC
#define COMPRESS_D_VEC 1
#endif

#if F203_COMPRESS_D != 4 && F203_COMPRESS_D != 5 && F203_COMPRESS_D != 10 && F203_COMPRESS_D != 11
#error "F203_COMPRESS_D must be 4, 5, 10 (ML-KEM-768 u), or 11 (ML-KEM-1024 u)"
#endif

#endif
