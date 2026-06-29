#ifndef F203_MOD_CONFIG_HPP
#define F203_MOD_CONFIG_HPP

/**
 * 行 18 final mod（Σ basemul + ê 之后）与 Stage3 解耦。
 * 0=标量 int64 %（调试）；1=Barrett 向量（默认交付）；2=Cast+Div 向量。
 * C ref golden 固定 HAT_MOD_SCALAR_I64，与设备变体无关。
 */
#ifndef F203_MOD_VARIANT
#define F203_MOD_VARIANT 1
#endif

#endif
