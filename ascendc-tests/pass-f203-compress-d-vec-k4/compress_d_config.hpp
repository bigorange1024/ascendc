#ifndef COMPRESS_D_CONFIG_HPP
#define COMPRESS_D_CONFIG_HPP

#ifndef F203_COMPRESS_D
#define F203_COMPRESS_D 4
#endif

#ifndef COMPRESS_D_VEC
#define COMPRESS_D_VEC 1
#endif

#if F203_COMPRESS_D != 4 && F203_COMPRESS_D != 5 && F203_COMPRESS_D != 10 && F203_COMPRESS_D != 11
#error "F203_COMPRESS_D must be 4, 5, 10 (ML-KEM-768 u), or 11 (ML-KEM-1024 u)"
#endif

#endif
