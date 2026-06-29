#ifndef HAT_DEBUG_CONFIG_HPP
#define HAT_DEBUG_CONFIG_HPP

/** 1：跳过 +ê 与 final mod，只测 A∘ŝ lazy 内积 */
#ifndef HAT_DEBUG_INNER_ONLY
#define HAT_DEBUG_INNER_ONLY 0
#endif
/* keep full +e/mod path for stage debug */

/** 1：内核 AscendC::printf 探针（p=0,j=0 的 prod 与 p=0 累加后 acc） */
#ifndef HAT_DEBUG_PRINT
#define HAT_DEBUG_PRINT 0
#endif

/** 0=仅 MTE3+PIPE_ALL；1=S3→S18 前加强 MTE2/V 屏障 */
#ifndef HAT_LINE18_STRONG_SYNC
#define HAT_LINE18_STRONG_SYNC 0
#endif

#endif
