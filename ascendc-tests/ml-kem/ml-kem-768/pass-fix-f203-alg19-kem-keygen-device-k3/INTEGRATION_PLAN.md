# INTEGRATION_PLAN — pass-fix-f203-alg19-kem-keygen-device-k3

**定位**：ML-KEM-768 W3/D19 KEM KeyGen device probe。按参数卡 §3.3，目标是 2 launch：D13 prep → D13 compute + Alg.16 tail。

## 数据契约

```text
ek_kem = ek_pke                                      # 1184B
dk_kem = dk_pke(1152) || ek(1184) || H(ek)(32) || z(32)
```

- `d` 域分离：`exp-mlkem-f203-2s1e-k3:SEED_D=`
- `z` 域分离：`exp-mlkem-f203-kem-k3:SEED_Z=`
- PKE 几何：Â[9]、polyvec6、Inner 2+1、ByteEncode12 3×384。

## Launch 编排

| Launch | blockDim | 内容 |
|--------|----------|------|
| L1 | AIV_ONLY 2 | D13 prep：Â[9] + s/e polyvec6 + ρ |
| L2 | MIX 1 | D13 compute：NTT/Inner/ByteEncode/ek_pke；AIV0 内嵌 KEM tail |

## 验收

```bash
bash run.sh -r cpu -v Ascend910B4
SIM_DIRECT=1 bash run.sh -r sim -v Ascend910B4
```

通过标准：`verify_kem.py` 报 `ek_kem` 与 `dk_kem` max=0；SIM 根目录无 stray dump；`STATUS.md` 与 tick 登记同步刷新。
