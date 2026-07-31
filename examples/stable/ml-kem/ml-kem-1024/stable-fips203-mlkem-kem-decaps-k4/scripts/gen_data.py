#!/usr/bin/env python3
"""
gen_data.py — Alg.21 Decaps **全链** golden 与 input 生成（stable-fips203-mlkem-kem-decaps-k4）。

## 两条路径

### 合法路径（默认，KEM_DECAPS_REJECT 未设或为 0）
1. 从 stash 读 ek_kem / dk_kem（或 EK_KEM_SRC / DK_KEM_SRC）。
2. KEM encaps(ek, m) → input/c.bin + golden/K.bin（共享密钥 K）。
3. 用 G(m,h) 得 coins，按 Encrypt 参考链算 **golden_v**（INTT(tr̂)+μ+e₂），供 CPU pack 对拍。
4. 写 m_prime_ref.bin、coins.bin、LUT 等全链 input。

**M_FILE / M_HEX**：KAT 或 roundtrip 若外部传入 C_SRC（合法密文），须同时提供 m（M_FILE 或 M_HEX），
否则无法重建 golden_v（Phase-E CPU 路径需要与 c 对应的 m）。

### Gate E3 拒绝路径（KEM_DECAPS_REJECT=1）
1. 写随机或 C_SRC 指定的**假密文** c（1568B），几乎必然 Decrypt 失败。
2. golden/K.bin = J(z‖c)（FIPS 203 Alg.21 拒绝分支）；有 liboqs 时再跑一遍 Decaps 交叉确认相等。

## golden 后端（2026-07-31）
c/K 交 library/shared/f203_kem_ref：**liboqs 优先，缺失则回落**仓内已验证参考
（G(m‖H(ek)) + 本目录 host_golden 的 golden_c.golden_encrypt；拒绝分支纯 SHAKE256）。
背景：借入实机装不了 thirdparty/liboqs，原先硬依赖会让本算子直接跑不起来。
例外：外部 C_SRC 且未给 K_ENC_SRC 时需完整 Decaps（重加密比对），回落不覆盖，须有 liboqs。
3. 写 golden/mode_reject 标记；golden_v 填占位零（拒绝路径不要求 c' 正确）。
4. liboqs Decaps API **不暴露**内部重加密 c'；E3 验收对象是最终 **K**，非 c'。

## 路径解析（stable 自包含）
ROOT = 本用例目录；REPO = examples/stable/stable-* 上溯三级到仓库根。
HOST_GOLDEN = 本目录 scripts/host_golden（vendored，不依赖其它 examples 路径）。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


import numpy as np

ROOT = Path(__file__).resolve().parent.parent
REPO = _ascendc_repo_root(ROOT)
HOST_GOLDEN = ROOT / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(REPO / "library" / "shared" / "fips203_host_rng"))
sys.path.insert(0, str(REPO / "library" / "shared" / "f203_kem_ref"))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402
import kem_ref  # noqa: E402

STASH = Path(os.environ.get("KEM_KEYPAIR_STASH", str(REPO / "output" / "kem_keypair_stash")))
EK_BYTES = 1568
DK_BYTES = 3168
CT_BYTES = 1568
M_BYTES = 32


def g_mh(m: bytes, h: bytes) -> tuple[bytes, bytes]:
    """
    FIPS 203 G：Kr = SHA3-512(m ‖ h)；返回 (K, coins)。
    coins 用于 Encrypt 噪声 r,e₁,e₂ 的 CBD 播种。
    """
    kr = hashlib.sha3_512(m + h).digest()
    return kr[:32], kr[32:]


def _j_zc(z: bytes, c: bytes) -> bytes:
    """Alg.21 拒绝路径：J(z, c) = SHAKE256(z ‖ c, 256)。"""
    return hashlib.shake_256(z + c).digest(32)


def _lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """
    将 int8 LUT 转为 AIC MMAD 用的平面堆叠布局（even/odd 列分离后上下拼接）。
    供 input/lut_*_stacked.bin 写入设备 workspace。
    """
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def _gen_luts(inp: Path) -> None:
    """生成 NTT/INTT 四套 stacked LUT bin（与 Encrypt 用例命名一致）。"""
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_ntt_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_ntt_odd_stacked.bin")
    _lut_planar_stacked(lut_intt, True).tofile(inp / "lut_intt_even_stacked.bin")
    _lut_planar_stacked(lut_intt, False).tofile(inp / "lut_intt_odd_stacked.bin")
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_odd_stacked.bin")


def _write_cpu_golden_v_placeholder(inp: Path, ek: bytes) -> None:
    """
    Gate E3 拒绝路径：CPU pack 仍需 v 形状缓冲，但 c' 几乎必 ≠ 假 c，填零即可。
    coins 同样占位（设备自产噪声，拒绝路径不校验 Encrypt 中间量）。
    """
    np.zeros((N,), dtype=np.int32).tofile(inp / "golden_v.bin")
    (inp / "coins.bin").write_bytes(bytes(32))


def main() -> None:
    inp = ROOT / "input"
    golden = ROOT / "golden"
    inp.mkdir(parents=True, exist_ok=True)
    golden.mkdir(parents=True, exist_ok=True)
    (ROOT / "output").mkdir(parents=True, exist_ok=True)

    ek_path = Path(os.environ.get("EK_KEM_SRC", str(STASH / "ek_kem.bin")))
    dk_path = Path(os.environ.get("DK_KEM_SRC", str(STASH / "dk_kem.bin")))
    if not ek_path.is_file() or not dk_path.is_file():
        # stash 落在 output/（不入 git），新机器/借入实机首跑时不存在。
        # 与其 exit 2 让用例跑不起来，不如按 SEED_D derand 现造一对（与 KeyGen 用例同式）。
        # 需固定钥时仍可 EK_KEM_SRC / DK_KEM_SRC 或先跑 kem_keypair_stash_bootstrap.sh。
        print(f"[gen_data] stash 缺 ek/dk（{ek_path}），改用 SEED_D derand 自举密钥对")
        ek_path, dk_path = kem_ref.bootstrap_keypair(golden)

    ek = ek_path.read_bytes()
    dk = dk_path.read_bytes()
    assert len(ek) == EK_BYTES and len(dk) == DK_BYTES
    h = dk[3104:3136]
    z = dk[3136:3168]

    reject = os.environ.get("KEM_DECAPS_REJECT", "0") == "1"
    (inp / "dk_kem.bin").write_bytes(dk)
    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "h.bin").write_bytes(h)
    (inp / "z.bin").write_bytes(z)
    _gen_luts(inp)

    if reject:
        # ══════════ Gate E3 REJECT：假密文 → K = J(z‖c) ══════════
        if os.environ.get("C_SRC"):
            c = Path(os.environ["C_SRC"]).read_bytes()
        else:
            c = os.urandom(CT_BYTES)
        assert len(c) == CT_BYTES
        (inp / "c.bin").write_bytes(c)

        # 拒绝分支的期望 K 本就是 J(z‖c)（纯 SHAKE256，无需任何外部库）。
        # 有 liboqs 时仍跑一遍 Decaps 做权威交叉，二者必须一致；无 liboqs 时直接落 J(z‖c)。
        k_j = _j_zc(z, c)
        if kem_ref.liboqs_ref() is not None:
            kem_ref.kem_decaps(dk, c, golden / "K.bin", dk_path=dk_path, c_path=inp / "c.bin")
            k_liboqs = (golden / "K.bin").read_bytes()
            if k_liboqs != k_j:
                print("[gen_data] BUG: liboqs Decaps(dk,c_bad) != J(z||c)", file=sys.stderr)
                sys.exit(3)
            k_src = "liboqs Decaps≡J(z||c)"
        else:
            (golden / "K.bin").write_bytes(k_j)
            k_src = "J(z||c)（无 liboqs，纯 SHAKE256）"
        (golden / "K_reject.bin").write_bytes(k_j)
        (golden / "mode_reject").write_text("1\n", encoding="utf-8")
        _write_cpu_golden_v_placeholder(inp, ek)
        print(f"[gen_data] REJECT c=urandom/C_SRC prefix={c[:8].hex()}… golden K={k_src}")
        return

    # ══════════ 合法路径：encaps → 全链 golden ══════════
    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    else:
        m = os.urandom(M_BYTES)
    assert len(m) == M_BYTES

    if os.environ.get("C_SRC"):
        c = Path(os.environ["C_SRC"]).read_bytes()
        assert len(c) == CT_BYTES
        (inp / "c.bin").write_bytes(c)
        # 不带 K_ENC_SRC 的分支需要完整 Decaps（含重加密比对），回落路径不覆盖 →
        # 无 liboqs 时 kem_ref 会给出可操作报错；roundtrip 脚本本身也依赖 liboqs 产 c。
        if os.environ.get("K_ENC_SRC"):
            (golden / "K.bin").write_bytes(Path(os.environ["K_ENC_SRC"]).read_bytes())
            k_src = "K_ENC_SRC"
        else:
            k_src = kem_ref.kem_decaps(dk, c, golden / "K.bin", dk_path=dk_path, c_path=inp / "c.bin")
    else:
        # 标准：encaps 同时生成 c 与 K（liboqs 优先；无 liboqs 回落 G(m‖H(ek)) + PKE Encrypt golden）
        k_src = kem_ref.kem_encaps(
            ek, m, inp / "c.bin", golden / "K.bin", pke_encrypt=gc.golden_encrypt, ek_path=ek_path
        )
    (inp / "m_prime_ref.bin").write_bytes(m)
    _k_ref, coins = g_mh(m, h)
    (inp / "coins.bin").write_bytes(coins)

    t_hat = gc.decode_t_hat(ek[: gc.EK_T_BYTES])
    r, _e1, e2 = gc.build_re(coins)
    r_hat = stage123_transform(r, "ntt")
    tr_hat = gc.golden_tr_hat(t_hat, r_hat)
    tr_pad = np.zeros((K, N), dtype=np.int32)
    tr_pad[0] = tr_hat
    tr = stage123_transform(tr_pad, "intt")[0]
    v = embed_message(tr, m)
    v = ((v.astype(np.int64) + e2.astype(np.int64)) % Q).astype(np.int32)
    v.tofile(inp / "golden_v.bin")

    (golden / "mode_reject").unlink(missing_ok=True)
    print(f"[gen_data] full-chain dk←{dk_path.name} m={m.hex()[:16]}… golden K via {k_src}")


if __name__ == "__main__":
    main()
