#!/usr/bin/env python3
"""
gen_data.py — Alg.21 Decaps exp-k3 全链 input/golden 生成器。

流水线位置：
  - 本脚本只服务 `examples/incubating/ml-kem/ml-kem-768/exp-fips203-mlkem-kem-decaps-ct-k3`；
  - 默认用本目录 vendored KeyGen host oracle 生成 `dk_kem=dk_pke‖ek‖H(ek)‖z`，
    再用本目录 vendored Encrypt host golden 生成合法密文
    `c=Encrypt(ek,m;G(m‖H(ek)).coins)`；
  - 最终 golden 只验 `K.bin`，不要求设备实现与 Python 过程同构。

生产 I/O（锁定 §3.3）：
  输入 `dk_kem.bin` 2400B + `c.bin` 1088B + NTT/INTT LUT；输出 `K.bin` 32B。

路径：
  合法路径：K = G(m‖H(ek)).K，高半 `coins` 同时驱动重加密 c'，因此 FO 应接受。
  拒绝路径（KEM_DECAPS_REJECT=1）：随机/外部假 c，K = J(z‖c)=SHAKE256(z‖c,32)。
"""
from __future__ import annotations

import hashlib
import os
import sys
from pathlib import Path

import numpy as np


ROOT = Path(__file__).resolve().parent.parent
SCRIPTS = ROOT / "scripts"
HOST_GOLDEN = SCRIPTS / "host_golden"
sys.path.insert(0, str(SCRIPTS))
sys.path.insert(0, str(HOST_GOLDEN))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402
import keygen_golden  # noqa: E402

EK_BYTES = 1184
DK_BYTES = 2400
CT_BYTES = 1088
M_BYTES = 32
SEED_D_DEFAULT = 20260619


def g_mh(m: bytes, h: bytes) -> tuple[bytes, bytes]:
    """FIPS 203 G：SHA3-512(m‖h) → K'‖coins。"""
    kr = hashlib.sha3_512(m + h).digest()
    return kr[:32], kr[32:]


def j_zc(z: bytes, c: bytes) -> bytes:
    """Alg.21 拒绝路径：J(z,c)=SHAKE256(z‖c,32)。"""
    return hashlib.shake_256(z + c).digest(32)


def _lut_planar_stacked(lut: np.ndarray, even: bool) -> np.ndarray:
    """
    将 int8 LUT 转为 AIC MMAD 用的平面堆叠布局。

    NTT/INTT k3 仍沿用 active B5/D14 的 `[top,bottom]` LUT 堆叠约定；
    本脚本只写 oracle 输入，不改变 kernel 几何。
    """
    if even:
        top = lut[:, 0:N:2]
        bottom = lut[:, N:512:2]
    else:
        top = lut[:, 1:N:2]
        bottom = lut[:, N + 1 : 512 : 2]
    return np.concatenate([top, bottom], axis=0)


def _gen_luts(inp: Path) -> None:
    """写 Decrypt/Encrypt 共用的 NTT/INTT stacked LUT 文件。"""
    lut_ntt = load_lut_t_i8("ntt")
    lut_intt = load_lut_t_i8("intt")
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_ntt_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_ntt_odd_stacked.bin")
    _lut_planar_stacked(lut_intt, True).tofile(inp / "lut_intt_even_stacked.bin")
    _lut_planar_stacked(lut_intt, False).tofile(inp / "lut_intt_odd_stacked.bin")
    # Phase-D Decrypt 读取历史别名；内容与 ntt_* 一致。
    _lut_planar_stacked(lut_ntt, True).tofile(inp / "lut_even_stacked.bin")
    _lut_planar_stacked(lut_ntt, False).tofile(inp / "lut_odd_stacked.bin")


def _load_or_generate_keypair() -> tuple[bytes, bytes, str]:
    """
    读取外部 k3 KEM 密钥，或默认按 D19 k3 oracle 从 SEED_D 生成。

    外部覆盖用于 roundtrip/KAT；默认路径用于本 exp 自洽验收。无论哪条路径，
    都严格检查 1184/2400B，防止误喂 k4 fixture。
    """
    if os.environ.get("EK_KEM_SRC") or os.environ.get("DK_KEM_SRC"):
        ek_path = Path(os.environ.get("EK_KEM_SRC", ""))
        dk_path = Path(os.environ.get("DK_KEM_SRC", ""))
        if not ek_path.is_file() or not dk_path.is_file():
            print(f"[gen_data] missing override ek/dk: {ek_path} {dk_path}", file=sys.stderr)
            sys.exit(2)
        ek = ek_path.read_bytes()
        dk = dk_path.read_bytes()
        label = f"override:{ek_path.name}/{dk_path.name}"
    else:
        seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
        kg = keygen_golden.build_full_keygen(seed_d, mix_pass=0)
        ek = bytes(kg["ek_pke"])
        dk_pke = bytes(kg["dk_pke"])
        h_ek = hashlib.sha3_256(ek).digest()
        z = hashlib.sha3_256(f"exp-mlkem-f203-kem-k3:SEED_Z={seed_d}".encode()).digest()
        dk = dk_pke + ek + h_ek + z
        label = f"local-keygen-seed-{seed_d}"
    if len(ek) != EK_BYTES or len(dk) != DK_BYTES:
        raise RuntimeError(f"bad k3 KEM key sizes ek={len(ek)} dk={len(dk)}")
    return ek, dk, label


def _write_golden_v(inp: Path, ek: bytes, m: bytes, coins: bytes) -> None:
    """
    计算 Phase-E CPU 分段需要的 v 系数：v=INTT(<t̂,r̂>)+μ+e₂。

    SIM 路径在设备内完整计算；CPU twin 为节省调试复杂度，从 input/golden_v.bin
    读取该中间态。该文件是对拍辅助，不是设备实现规格。
    """
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


def main() -> None:
    """写 input/ 与 golden/；合法/拒绝两种路径均只以 K.bin 为最终验收。"""
    inp = ROOT / "input"
    golden = ROOT / "golden"
    inp.mkdir(parents=True, exist_ok=True)
    golden.mkdir(parents=True, exist_ok=True)
    (ROOT / "output").mkdir(parents=True, exist_ok=True)

    ek, dk, key_label = _load_or_generate_keypair()
    # dk_kem = dk_pke(1152)‖ek(1184)‖h(32)‖z(32)
    h = dk[2336:2368]
    z = dk[2368:2400]
    if len(h) != 32 or len(z) != 32:
        raise RuntimeError("bad dk_kem h/z slice")

    (inp / "dk_kem.bin").write_bytes(dk)
    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "h.bin").write_bytes(h)
    (inp / "z.bin").write_bytes(z)
    _gen_luts(inp)

    reject = os.environ.get("KEM_DECAPS_REJECT", "0") == "1"
    if reject:
        # CT 验收要求拒绝输出不仅等于 J(z‖c)，还要与同 dk 下合法路径 K 区分。
        # 合法路径会写 golden/K_accept.bin；拒绝分支保留该文件，供 verify 显式比较。
        if os.environ.get("C_SRC"):
            c = Path(os.environ["C_SRC"]).read_bytes()
        else:
            c = os.urandom(CT_BYTES)
        if len(c) != CT_BYTES:
            raise RuntimeError(f"bad reject c size {len(c)}")
        (inp / "c.bin").write_bytes(c)
        k_reject = j_zc(z, c)
        (golden / "K.bin").write_bytes(k_reject)
        (golden / "K_reject.bin").write_bytes(k_reject)
        (golden / "mode_reject").write_text("1\n", encoding="utf-8")
        (inp / "coins.bin").write_bytes(bytes(32))
        np.zeros((N,), dtype=np.int32).tofile(inp / "golden_v.bin")
        print(f"[gen_data] REJECT dk←{key_label} c={len(c)}B golden K=J(z||c)")
        return

    if os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
    else:
        m = os.urandom(M_BYTES)
    if len(m) != M_BYTES:
        raise RuntimeError(f"bad m size {len(m)}")

    k_ref, coins = g_mh(m, h)
    if os.environ.get("C_SRC"):
        c = Path(os.environ["C_SRC"]).read_bytes()
        if os.environ.get("K_ENC_SRC"):
            k_ref = Path(os.environ["K_ENC_SRC"]).read_bytes()
    else:
        c = bytes(gc.golden_encrypt(ek, m, coins))
    if len(c) != CT_BYTES or len(k_ref) != 32:
        raise RuntimeError(f"bad legal c/K sizes c={len(c)} K={len(k_ref)}")

    (inp / "c.bin").write_bytes(c)
    (inp / "m_prime_ref.bin").write_bytes(m)
    (inp / "coins.bin").write_bytes(coins)
    _write_golden_v(inp, ek, m, coins)
    (golden / "K.bin").write_bytes(k_ref)
    # 为 CT reject 验收保留合法路径 K；后续拒绝分支不覆盖此文件。
    (golden / "K_accept.bin").write_bytes(k_ref)
    (golden / "mode_reject").unlink(missing_ok=True)
    print(f"[gen_data] full-chain dk←{key_label} c={len(c)}B K=32B via local k3 D14")


if __name__ == "__main__":
    main()
