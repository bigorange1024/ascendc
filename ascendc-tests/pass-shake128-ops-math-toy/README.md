# pass-shake128-ops-math-toy

以 [`add_custom`](../add_custom/) 为壳，编译 **`library/shared/shake_xof_kernel`**（rate=168）向量 SHAKE128，与 **tiny_sha3** 及 **Python `hashlib.shake_128`** 对拍。

> **不依赖** `thirdparty/ops-math` 克隆或 aclnn 算子包。设备代码溯源见 [`../../library/shared/ATTRIBUTION.md`](../../library/shared/ATTRIBUTION.md)。

## 设备核

```cpp
extern "C" __global__ __aicore__ void shake128_general(
    GM_ADDR x, GM_ADDR lengths, GM_ADDR y, GM_ADDR workspace, GM_ADDR tiling);
```

Host：`FillShakeTiling(..., SHAKE128_RATE_BYTES)`（`library/shared/shake_xof_kernel/tiling_host.hpp`）。

## 用例

| `SHAKE128_CASE` | 说明 |
|-----------------|------|
| `abc` | 3B → 32B |
| `empty` | 空消息 |
| `prf_sigma_n0` | ML-KEM PRF 形参 33B → 128B（**SHAKE128 shim**，非 FIPS 规范轨） |
| `batch_mixed` | 混合 batch |

规范轨 SHAKE256 见 [`pass-shake256-ascendc-toy`](../pass-shake256-ascendc-toy/)。

## 运行

```bash
cd ascendc-tests/pass-shake128-ops-math-toy
bash run.sh -r cpu -v Ascend910B4
bash run.sh -r sim -v Ascend910B4
```
