# 2026-07-24 — 第7章 CT → Decaps device PASS

关键字：`CT_decaps` · **T19b/c** · `pass-fix-f203-alg21-kem-decaps-device-k4` · stable Decrypt/Encrypt 编译期引用 · `decaps_2session` · 无 vendor · CPU+SIM `K` max=0

## 决策

1. 按教材第7章已提交前瞻闭包表 `CT_decaps` 实现 Decaps device；落点目录更名为 `pass-fix-…`（废 stub `fix-…-decaps-device-k4`）。
2. **Forbidden**：不抄 `fix-*-correctness-*` / `frozen/` 的 `.cpp/.hpp`；correctness 只读契约。
3. PKE：**编译期引用** stable Decrypt fused + Encrypt；SIM 用 `prepare_dec_shim.sh` 做 `dec_*` 头隔离合单库（不改 stable 源码）。
4. KEM 头/尾：对齐 Encaps 范式 — `G` 并入 Phase-E prep；FO 在 pack/`l18_l19` 尾；`Sha3OneShot`/`Shake256OneShot`。
5. SIM 生产默认 **`ASCENDC_SIM_HOST_MODE=decaps_2session`**（CT 锁定；与 main 上 1-session 定论并存为排障对照）。

## 证据

| 项 | 结果 |
|----|------|
| CPU 合法 | `[verify] PASS` / `K` max=0 |
| SIM 合法 | `[verify] PASS`；tick D**286798**+E**763663**；根无 stray |
| 拒绝 CPU | `KEM_DECAPS_REJECT=1` → `REJECT PASS` |

## 遗留

- 教材 §实现后判决占位待改写（弱/强成功：合法+拒绝已绿；结构无 vendor）
- 拒绝路径 SIM 长测可选；KAT×10 非本轮门禁
- 未开 `examples/` exp/stable Decaps（本轮禁止）
