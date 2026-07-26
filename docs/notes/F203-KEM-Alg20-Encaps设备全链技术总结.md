# F203 KEM Alg.20 Encaps — 设备全链技术总结

**定稿性质**：原理优先（Alg.17 契约 + 设备布局），案例附录只作索引。  
**交付锚点**：[`examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4`](../../examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-encaps-k4/)（2026-07-15 `#验收#`）  
**行为基线**：[`pass-fix-f203-alg20-kem-encaps-device-k4`](../../ascendc-tests/ml-kem/ml-kem-1024/pass-fix-f203-alg20-kem-encaps-device-k4/)  
**预研副本**：[`exp-fips203-mlkem-kem-encaps-k4`](../../examples/incubating/ml-kem/ml-kem-1024/exp-fips203-mlkem-kem-encaps-k4/)  
**登记表**：[`docs/specs/fips203-mlkem1024-kem-encaps-baseline-registry.md`](../specs/fips203-mlkem1024-kem-encaps-baseline-registry.md)

---

## 1. 数学 / 标准契约

Alg.20 `ML-KEM.Encaps` 经 Alg.17 `Encaps_internal`：

1. 读外部消息 \(m\in\mathbb{B}^{32}\)（**输入**，非设备抽样）
2. \(h\leftarrow H(\mathrm{ek})=\mathrm{SHA3\text{-}256}(\mathrm{ek})\)
3. \((K\Vert r)\leftarrow G(m\Vert h)=\mathrm{SHA3\text{-}512}\)
4. \(c\leftarrow\) Alg.14 Encrypt\((\mathrm{ek}, m, r)\)
5. 交付 **仅** \((c,K)\)

**不变量**：Host 不得预填 \(r\) / 伪 \(H\)/\(G\)；中间 \(\hat{A}/y/e_1/e_2\) 非交付 I/O。

---

## 2. 设备布局（可复用模式）

| 项 | 选择 | 理由 |
|----|------|------|
| Launch | SIM **2** / CPU **5** | 与 stable Encrypt 同拓扑；头**并入** prep，禁第 3 launch |
| KEM 头 | block0 先 `KemEncInitHead`，再与 block1 并行 Â | 正确性写序；block1 重叠不吞掉 +~94k 头成本 |
| Encrypt | vendored `prep/`+`compute/`（stable 自包含） | device 探针可用 `STABLE_ENCRYPT_ROOT`；交付树切断外链 |
| Golden | liboqs `encaps_derand` | **I/O 等价**；禁止源码同构验收 |

相对纯 Encrypt（~628k）多出的 ~94k SIM tick **主要是**标量 `H(ek)`（~12× Keccak-f），不是接线债。

---

## 3. 验收口径

- `run.sh` CPU + `SIM_DIRECT=1` sim：`c`/`K` max=0
- 分项 KAT：固定 stash `ek` + 随机 `m`，CPU×10 + SIM×3 ↔ liboqs
- SIM tick **≈721k**（对标 device **721010**；stable 实测约 **721119**）

---

## 附录：路径

| 角色 | 路径 |
|------|------|
| customspec | `examples/stable/…/stable-…-kem-encaps-k4-实现方案-customspec.pdf` |
| KEM 头实现 | `kem/f203_kem_enc_init.hpp` · `kem/f203_kem_enc_prep_entry.cpp` |
| Host | `main_kem_encaps.cpp` |
| Encrypt 交付口径 | [F203-Alg14-Encrypt-交付口径…](F203-Alg14-Encrypt-交付口径-CPU辅助与SIM主参考.md) |
