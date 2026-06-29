# examples/incubating — 研究中算子
**自包含**（2026-06-29）：与探针同约束，见 [用例自包含与设备全链约束.md](../../docs/engineering/用例自包含与设备全链约束.md)；KeyGen 见各目录 `SELF_CONTAINED.md`。


**前缀**：`exp-<简述>/`（如 `exp-mlkem-ntt/`）。

**规则**（见 Rule）：研究类代码**只能先写此处**；定型后**复制**到 `examples/stable/stable-*`，本目录副本**保留**。

**废弃实验** → [../frozen/INDEX.md](../frozen/INDEX.md)（`frozen-exp-*` — **路线关闭，禁止抄码、禁止用其 customspec**）。

---

## 当前实验

| 目录 | 方案简述 | 状态 |
|------|----------|------|
| [exp-sepolyvec8-ntt-k8/](exp-sepolyvec8-ntt-k8/) | **纯 $k{=}8$ 批 NTT**（8 条互异随机 poly；交错 S0；**非** KeyGen 集成）；[PDF](exp-sepolyvec8-ntt-k8/exp-sepolyvec8-ntt-k8-实现方案.pdf) | **CPU ✓ / SIM ✓**；NTT 内核回归对照；[STATUS](exp-sepolyvec8-ntt-k8/STATUS.md) |
| [exp-mlkem-f203-stage1-encode-vec/](exp-mlkem-f203-stage1-encode-vec/) | F203 Stage1 纯向量 encode；[customspec](exp-mlkem-f203-stage1-encode-vec/exp-mlkem-f203-stage1-encode-vec-实现方案-customspec.pdf) | `aiv=1/2/8` 对拍 |
| [exp-mlkem-f203-stage3-routea-mod-vec/](exp-mlkem-f203-stage3-routea-mod-vec/) | F203 Stage3 RouteA+mod 向量预研；[customspec](exp-mlkem-f203-stage3-routea-mod-vec/exp-mlkem-f203-stage3-routea-mod-vec-实现方案-customspec.pdf) | `aiv=1/2/8` 对拍 |
| [exp-mlkem-f203-pke-keygen-k4/](exp-mlkem-f203-pke-keygen-k4/) | FIPS 203 **Alg.13 PKE KeyGen** k=4（**已晋级** [`stable-mlkem-f203-pke-keygen-k4`](../stable/stable-mlkem-f203-pke-keygen-k4/)）；[customspec](exp-mlkem-f203-pke-keygen-k4/exp-mlkem-f203-pke-keygen-k4-实现方案-customspec.pdf) | 副本保留；交付以 **stable** 为准 · [STATUS](exp-mlkem-f203-pke-keygen-k4/STATUS.md) |
| [exp-mlkem-f203-alg13-16171820-2s1e-k4/](exp-mlkem-f203-alg13-16171820-2s1e-k4/) | Alg.13 行 16–20：2s1e MIX+UB；**Host Python** 提供 FIPS CBD $\mathbf{s}$/$\mathbf{e}$（	exttt{src.bin}）；ByteEncode **prefetch**；[customspec](exp-mlkem-f203-alg13-16171820-2s1e-k4/exp-mlkem-f203-alg13-16171820-2s1e-k4-实现方案-customspec.pdf) | **CPU ✓ / SIM ✓** tick≈**78k** |

NTT 主路径：`AicMmad` + merged\_kyber FSM（非 `Matmul<>`）。**块紧凑 S0 `[HI_8|LO_8]` 已否决** → [`frozen-exp-mlkem-sepolyvec8-ntt-k4-block`](../frozen/frozen-exp-mlkem-sepolyvec8-ntt-k4-block/) + 探针 `poly8-block-s123`（**禁止参考**）。**8-poly 紧凑向量终态** → [`pass-fix-f203-stage123-ntt-intt-polyvec8-vec`](../../ascendc-tests/pass-fix-f203-stage123-ntt-intt-polyvec8-vec/)；历史 exp → `exp-sepolyvec8-ntt-k8`；全链路 → `exp-k4` / `vec-k4-v2`。

**行 8–15 设备 $s$/$e$**：[`pass-fix-f203-alg13-lines8-15-se-k4`](../../ascendc-tests/pass-fix-f203-alg13-lines8-15-se-k4/)（向量 V3 ✅）；标量对照 [`frozen-fix-f203-alg13-se-device-scalar-k4`](../../ascendc-tests/frozen/frozen-fix-f203-alg13-se-device-scalar-k4/)（2026-06-26 冻结）。

---

## 维护

新增 `exp-*` → 增加一行；晋级 stable 后**不删除**本行（可标「已复制至 stable-…」）。  
`Matmul<>` NTT 相关实验已迁至 `examples/frozen/` — **路线关闭，禁止抄**；见 [2026-06-11 冻结纪要](../../qa/2026-06/2026-06-11-ascendc-engineering-notes与数据搬运.md#ntt-matmul路线废弃冻结)。
