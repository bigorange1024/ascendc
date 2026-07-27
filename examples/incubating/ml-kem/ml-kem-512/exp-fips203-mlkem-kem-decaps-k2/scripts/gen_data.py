#!/usr/bin/env python3
"""
gen_data.py — Alg.21 Decaps device-k2 全链 input/golden 生成器。

流水线位置：
  - 本脚本只服务 `ascendc-tests/ml-kem/ml-kem-512/exp-fips203-mlkem-kem-decaps-k2`；
  - 默认用 D15/D14 k2 host oracle 生成匹配 `dk_pke/ek`，本地拼 `dk_kem=dk_pke‖ek‖H(ek)‖z`，再用 D14 k2
    host golden 生成合法密文 `c=Encrypt(ek,m;G(m‖H(ek)).coins)`；
  - 最终 golden 只验 `K.bin`，不要求设备实现与 Python 过程同构。

生产 I/O（锁定 §3.3）：
  输入 `dk_kem.bin` 1632B + `c.bin` 768B + NTT/INTT LUT；输出 `K.bin` 32B。

路径：
  合法路径：K = G(m‖H(ek)).K，高半 `coins` 同时驱动重加密 c'，因此 FO 应接受。
  拒绝路径（KEM_DECAPS_REJECT=1）：随机/外部假 c，K = J(z‖c)=SHAKE256(z‖c,32)。
"""
from __future__ import annotations

import hashlib
import importlib.util
import os
import sys
from pathlib import Path

import numpy as np


def _ascendc_repo_root(start: Path) -> Path:
    """自 start 向上查找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


ROOT = Path(__file__).resolve().parent.parent
REPO = _ascendc_repo_root(ROOT)
D14_ENC = ROOT
D15_DEC = ROOT
HOST_GOLDEN = D14_ENC / "scripts" / "host_golden"
D15_HOST_GOLDEN = D15_DEC / "scripts" / "host_golden"
sys.path.insert(0, str(HOST_GOLDEN))
sys.path.insert(0, str(D15_HOST_GOLDEN))

from f203_ref_common import K, N, Q, embed_message, load_lut_t_i8, stage123_transform  # noqa: E402
import golden_c as gc  # noqa: E402

_D15_EK = D15_HOST_GOLDEN / "gen_ek_pke.py"
_ek_spec = importlib.util.spec_from_file_location("mlkem512_d15_gen_ek_pke", _D15_EK)
d15_gen_ek = importlib.util.module_from_spec(_ek_spec)
assert _ek_spec.loader is not None
_ek_spec.loader.exec_module(d15_gen_ek)

_D15_DK = D15_HOST_GOLDEN / "gen_dk_pke.py"
_dk_spec = importlib.util.spec_from_file_location("mlkem512_d15_gen_dk_pke", _D15_DK)
d15_gen_dk = importlib.util.module_from_spec(_dk_spec)
assert _dk_spec.loader is not None
_dk_spec.loader.exec_module(d15_gen_dk)

EK_BYTES = 800
DK_BYTES = 1632
CT_BYTES = 768
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

    NTT/INTT k2 仍沿用 active B5/D14 的 `[top,bottom]` LUT 堆叠约定；
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


def _build_local_kem_keygen(seed_d: int) -> tuple[bytes, bytes]:
    """用活跃 D15/D14 k2 PKE host oracle 组装 KEM keypair。

    D19 k2 目录尚未落地时，本探针只需要合法 `dk_kem` fixture：
    `dk_pke` 来自 D15，`ek` 来自 D14/D15 同源 host oracle；`h=SHA3-256(ek)`；`z` 用带参数组标签的
    SHA3-256 确定性派生，保证 accept/reject 两条路径在同一输入下可复现。
    """
    ek = bytes(d15_gen_ek.build_ek_pke(seed_d))
    dk_pke = bytes(d15_gen_dk.build_dk_pke(seed_d))
    h = hashlib.sha3_256(ek).digest()
    z = hashlib.sha3_256(f"ml-kem-512-d21-z:{seed_d}".encode()).digest()
    return ek, dk_pke + ek + h + z


def _load_or_generate_keypair() -> tuple[bytes, bytes, str]:
    """读取外部 k2 KEM 密钥，或默认按 D15/D14 派生 fixture 从 SEED_D 生成。

    外部覆盖用于 roundtrip/KAT；默认路径用于本 probe 自洽验收。无论哪条路径，
    都严格检查 800/1632B，防止误喂其它参数组 fixture。
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
        ek, dk = _build_local_kem_keygen(seed_d)
        label = f"D15D14-local-seed-{seed_d}"
    if len(ek) != EK_BYTES or len(dk) != DK_BYTES:
        raise RuntimeError(f"bad k2 KEM key sizes ek={len(ek)} dk={len(dk)}")
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
    # dk_kem = dk_pke(768)‖ek(800)‖h(32)‖z(32)
    h = dk[1568:1600]
    z = dk[1600:1632]
    if len(h) != 32 or len(z) != 32:
        raise RuntimeError("bad dk_kem h/z slice")

    (inp / "dk_kem.bin").write_bytes(dk)
    (inp / "ek_kem.bin").write_bytes(ek)
    (inp / "h.bin").write_bytes(h)
    (inp / "z.bin").write_bytes(z)
    _gen_luts(inp)

    reject = os.environ.get("KEM_DECAPS_REJECT", "0") == "1"
    if reject:
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

    # 合法路径：若提供 C_SRC，优先用 host Decrypt(dk_pke,c) 恢复 m'，再派生 coins/golden_v。
    # 背景：CPU 孪生 Phase-E 读 golden_v；roundtrip 若 M_FILE 与 c 短暂不一致会假绿/假红（见 qa）。
    # 结论：C_SRC 在场时以密文反推 m 为权威；M_FILE 仅作可选一致性检查。
    if os.environ.get("C_SRC"):
        c = Path(os.environ["C_SRC"]).read_bytes()
        if len(c) != CT_BYTES:
            raise RuntimeError(f"bad C_SRC size {len(c)}")
        import golden_m as gm  # noqa: WPS440 — host oracle，非 AscendC 规格
        m = gm.golden_decrypt(dk[:768], c)
        if os.environ.get("M_FILE"):
            m_file = Path(os.environ["M_FILE"]).read_bytes()
            if m_file != m:
                print(
                    f"[gen_data] WARN M_FILE!=Decrypt(dk,c); using Decrypt m for golden_v",
                    file=sys.stderr,
                )
    elif os.environ.get("M_FILE"):
        m = Path(os.environ["M_FILE"]).read_bytes()
        c = None
    elif os.environ.get("M_HEX"):
        m = bytes.fromhex(os.environ["M_HEX"])
        c = None
    else:
        m = os.urandom(M_BYTES)
        c = None
    if len(m) != M_BYTES:
        raise RuntimeError(f"bad m size {len(m)}")

    k_ref, coins = g_mh(m, h)
    if c is None:
        c = bytes(gc.golden_encrypt(ek, m, coins))
    if os.environ.get("K_ENC_SRC"):
        k_ref = Path(os.environ["K_ENC_SRC"]).read_bytes()
    if len(c) != CT_BYTES or len(k_ref) != 32:
        raise RuntimeError(f"bad legal c/K sizes c={len(c)} K={len(k_ref)}")

    (inp / "c.bin").write_bytes(c)
    (inp / "m_prime_ref.bin").write_bytes(m)
    (inp / "coins.bin").write_bytes(coins)
    _write_golden_v(inp, ek, m, coins)
    (golden / "K.bin").write_bytes(k_ref)
    (golden / "mode_reject").unlink(missing_ok=True)
    print(f"[gen_data] full-chain dk←{key_label} c={len(c)}B K=32B via local k2 D14")


if __name__ == "__main__":
    main()
