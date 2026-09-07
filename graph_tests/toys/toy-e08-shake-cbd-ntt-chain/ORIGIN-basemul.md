# ORIGIN — basemul / MultiplyNTTs

- 参考探针（只读，未改）：`ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg11-12-multiplyntts-k4/`
- 本目录自包含：`vendor/basemul_scalar/alg11_gammas.h`（拷贝）+ `basemul_half_ub.hpp`（标量 Alg.11/12 半区）
- 路径：`ALG11_IMPL=0` 风格 Barrett + BaseCaseMultiply；**非**向量 B2 Gather
- γ 表与探针 `alg11_gammas.h` / gen_data `K_ALG11_GAMMAS` 一致
