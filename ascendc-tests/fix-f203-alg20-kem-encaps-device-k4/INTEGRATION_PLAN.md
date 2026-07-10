# INTEGRATION_PLAN — fix-f203-alg20-kem-encaps-device-k4

**定位**：Alg.20 KEM Encaps **无 vendor** 设备主线；经 **Alg.17 Encaps_internal** 调用 **stable 对齐**的 Alg.14 Encrypt。

**基线对照**：[`fix-f203-alg20-kem-encaps-correctness-k4`](../fix-f203-alg20-kem-encaps-correctness-k4/) · [`docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md`](../../docs/notes/F203-KEM-Alg19-KeyGen设备全链技术总结.md)（Encaps 增量见 correctness §1）

## T19a 要点

- 移除 `vendor/pke_encrypt` 与 `vendor_sync_from_alg14_encrypt.sh`。
- Encrypt 段接 [`stable-fips203-mlkem-pke-encrypt-k4`](../../examples/stable/stable-fips203-mlkem-pke-encrypt-k4/) 或 [`pass-fix-f203-alg14-pke-encrypt-device-k4`](../pass-fix-f203-alg14-pke-encrypt-device-k4/) 已验证 launch 拓扑。
- `m` device UB；`H(ek)`/`G` 设备 SHA3；输出 `c`+`K` 几何不变。

## 上游

| 段 | 来源 |
|----|------|
| `ek_kem` | [`pass-fix-f203-alg19-kem-keygen-device-k4`](../pass-fix-f203-alg19-kem-keygen-device-k4/)（**PASS**）或 correctness 对照 |
