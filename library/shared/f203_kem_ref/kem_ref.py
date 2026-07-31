#!/usr/bin/env python3
# coding=utf-8
"""
ML-KEM（FIPS 203）KEM 层 golden 后端：**liboqs 优先，缺失时回落仓内已验证的 Python 参考**。

## 本文件在流水线中的位置

各 KEM 用例（Alg.19 KeyGen / Alg.20 Encaps / Alg.21 Decaps，探针与 stable 算子）的 `gen_data.py`
用本模块产出 `golden/{ek,dk,c,K}.bin`，再由 `run.sh` 与设备输出 `cmp` 对拍。本模块**只是黑盒
oracle**，不是 AscendC 必须复刻的实现规格。

## 为什么要有回落路径

背景（2026-07-31，借入 910B4 实机）：原先 8 个 KEM 用例的 golden 一律走 `scripts/liboqs_kem_ref`，
该可执行由 `thirdparty/liboqs` 编出，而 `thirdparty/` 不入 git。实机上无法安装 thirdparty 时，
`gen_data.py` 会直接挂在 `missing thirdparty/liboqs/build/lib/liboqs.{so,a}`，KEM 用例全部跑不起来
（PKE 用例因 LUT 已 vendored 不受影响）。

结论（用户 2026-07-31 拍板）：**有 liboqs 就用 liboqs**（保留权威外部交叉），**没有则回落**到仓内
已验证的 PKE golden + 标准 SHA3 组装出 KEM 层 golden，并在日志显式打印 golden 来源，避免假绿。
回落路径**不改变**任何已锁定参数（m 仍默认 urandom、SEED_D 不变）。

## 回落路径用了什么

KEM 层相对 PKE 层只多出哈希与拼装，计算内核仍是仓内已验证件：

* KeyGen：`ek_kem = ek_pke`；`dk_kem = dk_pke ‖ ek_pke ‖ H(ek_pke) ‖ z`（FIPS 203 Alg.16）。
  PKE KeyGen 由调用方传入（各 keygen 用例的 `keygen_golden.build_full_keygen`）。
* Encaps（Alg.17）：`(K, r) = G(m ‖ H(ek))`；`c = PKE.Encrypt(ek, m, r)`。
  PKE Encrypt 由调用方传入（`host_golden/golden_c.golden_encrypt`，stable Encrypt 对拍用的同一份）。
* Decaps（Alg.18）：`m' = PKE.Decrypt(dk_pke, c)`；`(K', r') = G(m' ‖ h)`；`c' = Encrypt(ek, m', r')`；
  `c == c'` 则 K = K'，否则 K = J(z ‖ c)。拒绝分支只用 SHAKE256，无需任何 PKE 件。

H = SHA3-256，G = SHA3-512，J = SHAKE256(·, 32B)，与 FIPS 203 §4.1 一致。

## 与 golden 的关系

同一 (seed/m, ek) 下两条路径必须给出**逐字节相同**的 ek/dk/c/K；`KEM_GOLDEN_CROSS=1` 时本模块会在
liboqs 可用且回落件也齐备时同时算两遍并比对，用于定期自检回落实现。
"""
from __future__ import annotations

import hashlib
import os
import subprocess
from pathlib import Path
from typing import Callable, Optional

EK_BYTES = 1568
DK_PKE_BYTES = 1536
DK_KEM_BYTES = 3168
CT_BYTES = 1568
M_BYTES = 32

# 调用方注入的 PKE golden 件签名：
#   PkeKeygenFn: d(32B) → (ek_pke 1568B, dk_pke 1536B)
#   PkeEncryptFn: (ek 1568B, m 32B, coins 32B) → c 1568B
#   PkeDecryptFn: (dk_pke 1536B, c 1568B) → m' 32B
PkeKeygenFn = Callable[[bytes], "tuple[bytes, bytes]"]
PkeEncryptFn = Callable[[bytes, bytes, bytes], bytes]
PkeDecryptFn = Callable[[bytes, bytes], bytes]


def _repo_root(start: Path) -> Path:
    """自 start 向上找含 AGENTS.md 与 scripts/ 的仓库根（兼容 ml-kem 参数组多层嵌套）。"""
    p = start.resolve()
    for d in [p, *p.parents]:
        if (d / "AGENTS.md").is_file() and (d / "scripts").is_dir():
            return d
    raise RuntimeError(f"cannot locate ascendc repo root from {start}")


REPO = _repo_root(Path(__file__))


def h_sha3_256(data: bytes) -> bytes:
    """FIPS 203 H：SHA3-256（用于 H(ek)）。"""
    return hashlib.sha3_256(data).digest()


def g_sha3_512(data: bytes) -> "tuple[bytes, bytes]":
    """FIPS 203 G：SHA3-512(data) 切成 (K, coins) 各 32B。"""
    kr = hashlib.sha3_512(data).digest()
    return kr[:32], kr[32:]


def j_shake256(z: bytes, c: bytes) -> bytes:
    """FIPS 203 J：SHAKE256(z ‖ c, 32B)，Alg.18 隐式拒绝分支的共享密钥。"""
    return hashlib.shake_256(z + c).digest(32)


def liboqs_ref() -> Optional[Path]:
    """
    定位可用的 `scripts/liboqs_kem_ref`。

    与旧写法的区别：**不再**在缺失时强行调 `build_liboqs_kem_ref.sh` 并让异常冒泡。
    这里只在 `thirdparty/liboqs` 确实编好时才尝试构建；否则返回 None 交给回落路径。
    这样实机（无 thirdparty）不会因缺 liboqs 而整条 gen_data 崩掉。

    @return 可执行文件路径；不可用时 None
    """
    if os.environ.get("KEM_GOLDEN_BACKEND") == "python":
        return None  # 显式强制回落（用于在有 liboqs 的机器上验证 Python 路径）
    ref = REPO / "scripts" / "liboqs_kem_ref"
    if ref.is_file() and os.access(ref, os.X_OK):
        return ref
    lib_dir = REPO / "thirdparty" / "liboqs" / "build" / "lib"
    if not ((lib_dir / "liboqs.so").exists() or (lib_dir / "liboqs.a").exists()):
        return None
    try:
        subprocess.check_call(["bash", str(REPO / "scripts" / "build_liboqs_kem_ref.sh")])
    except (subprocess.CalledProcessError, OSError):
        return None
    return ref if ref.is_file() else None


# ─────────────────── SEED_D → (d, z) 与密钥对自举（供 Decaps / Encaps 用） ───────────────────
SEED_D_DEFAULT = 20260619


def derand_d(seed_d: int) -> bytes:
    """d：与设备 prep DerandFromSeedD 对齐（library/shared/fips203_se_sample）。"""
    import sys

    se = str(REPO / "library" / "shared" / "fips203_se_sample")
    if se not in sys.path:
        sys.path.insert(0, se)
    from golden_se_sampling import derand_bytes_from_seed

    return derand_bytes_from_seed(seed_d)


def derand_z(seed_d: int) -> bytes:
    """z：与设备 kem/f203_kem_kg_derand_ub.hpp 的 DerandZFromSeedD 同式（域分离串锁定）。"""
    return hashlib.sha3_256(f"exp-mlkem-f203-kem-k4:SEED_Z={seed_d}".encode()).digest()


def stable_pke_keygen(seed_d: int) -> PkeKeygenFn:
    """
    回落路径的 PKE KeyGen golden：借 stable KEM KeyGen 算子的 `keygen_golden`。

    该算子自带 vendored LUT（不依赖 `thirdparty/`），且以 seed_d 为入口（内部同样
    d = derand_bytes_from_seed(seed_d)）。这里闭包 seed_d 并断言传入 d 一致，
    避免 d 与 seed_d 走岔产出错 golden。仅在真正回落时才加载，正常机器不多载脚本树。
    """
    kg_scripts = REPO / "examples/stable/ml-kem/ml-kem-1024/stable-fips203-mlkem-kem-keygen-k4/scripts"

    def _pke_keygen(d: bytes) -> "tuple[bytes, bytes]":
        if d != derand_d(seed_d):
            raise SystemExit("[kem_ref] d 与 SEED_D 不一致，拒绝生成 golden")
        import importlib.util

        spec = importlib.util.spec_from_file_location(
            "stable_kem_keygen_golden", kg_scripts / "keygen_golden.py"
        )
        if spec is None or spec.loader is None:
            raise SystemExit(f"[kem_ref] 无法加载 {kg_scripts / 'keygen_golden.py'}")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        kg = mod.build_full_keygen(seed_d)
        return kg["ek_pke"].tobytes(), kg["dk_pke"].tobytes()

    return _pke_keygen


def bootstrap_keypair(out_dir: Path, seed_d: Optional[int] = None) -> "tuple[Path, Path]":
    """
    按 SEED_D derand 现造一对 KEM 密钥，写入 out_dir 并返回 (ek_path, dk_path)。

    用途：Decaps 各用例默认读 `output/kem_keypair_stash/`（由 stash 脚本产出、**不入 git**），
    新机器或借入实机首跑时该目录不存在，原先直接 `exit 2`。此处按与 KeyGen 用例相同的
    SEED_D → d‖z 语义现造一对，保证每个用例都能单独跑通；有 liboqs 时仍由 liboqs 产钥。

    @param out_dir 落盘目录（一般是用例 golden/）
    @param seed_d  缺省 SEED_D 环境变量，再缺省 20260619（与 KeyGen 用例默认一致）
    """
    if seed_d is None:
        seed_d = int(os.environ.get("SEED_D", str(SEED_D_DEFAULT)))
    out_dir.mkdir(parents=True, exist_ok=True)
    ek_path = out_dir / "_bootstrap_ek_kem.bin"
    dk_path = out_dir / "_bootstrap_dk_kem.bin"
    kem_seed = derand_d(seed_d) + derand_z(seed_d)
    src = kem_keygen(kem_seed, ek_path, dk_path, pke_keygen=stable_pke_keygen(seed_d))
    print(f"[kem_ref] stash 缺失 → 按 SEED_D={seed_d} 现造密钥对（via {src}）")
    return ek_path, dk_path


def _require(fn, name: str, op: str):
    """回落路径缺少必需的 PKE golden 件时给出可操作的报错，而不是让 None 崩在深处。"""
    if fn is None:
        raise SystemExit(
            f"[kem_ref] 无 liboqs 且未提供 {name}，无法生成 {op} golden。\n"
            f"  → 装好 thirdparty/liboqs（bash scripts/clone-thirdparty.sh），"
            f"或在 gen_data.py 中传入 {name}。"
        )
    return fn


# ──────────────────────────────── Alg.16 KeyGen ────────────────────────────────
def kem_keygen(
    kem_seed: bytes,
    ek_out: Path,
    dk_out: Path,
    *,
    pke_keygen: Optional[PkeKeygenFn] = None,
) -> str:
    """
    KEM KeyGen（derand）：64B `kem_seed = d ‖ z` → ek_kem(1568B) / dk_kem(3168B)。

    @param kem_seed 64B；前 32B 为 PKE KeyGen 的 d，后 32B 为隐式拒绝用的 z
    @param ek_out/dk_out 输出 bin 路径
    @param pke_keygen 回落路径所需的 PKE KeyGen golden：d → (ek_pke, dk_pke)
    @return golden 来源标识："liboqs" 或 "python"
    """
    if len(kem_seed) != 64:
        raise SystemExit(f"[kem_ref] kem_seed want 64B got {len(kem_seed)}B")

    ref = liboqs_ref()
    if ref is not None:
        subprocess.check_call([str(ref), "keygen", str(ek_out), str(dk_out), kem_seed.hex()])
        if os.environ.get("KEM_GOLDEN_CROSS") == "1" and pke_keygen is not None:
            ek_py, dk_py = _keygen_python(kem_seed, pke_keygen)
            _cross_check("keygen", {"ek": (ek_out.read_bytes(), ek_py), "dk": (dk_out.read_bytes(), dk_py)})
        return "liboqs"

    ek, dk = _keygen_python(kem_seed, _require(pke_keygen, "pke_keygen", "KeyGen"))
    ek_out.write_bytes(ek)
    dk_out.write_bytes(dk)
    return "python"


def _keygen_python(kem_seed: bytes, pke_keygen: PkeKeygenFn) -> "tuple[bytes, bytes]":
    """回落 KeyGen：dk_kem = dk_pke ‖ ek_pke ‖ H(ek_pke) ‖ z（FIPS 203 Alg.16 第 3 行）。"""
    d, z = kem_seed[:32], kem_seed[32:]
    ek_pke, dk_pke = pke_keygen(d)
    if len(ek_pke) != EK_BYTES or len(dk_pke) != DK_PKE_BYTES:
        raise SystemExit(f"[kem_ref] pke_keygen 尺寸异常 ek={len(ek_pke)} dk={len(dk_pke)}")
    dk_kem = dk_pke + ek_pke + h_sha3_256(ek_pke) + z
    assert len(dk_kem) == DK_KEM_BYTES
    return ek_pke, dk_kem


# ──────────────────────────────── Alg.17 Encaps ────────────────────────────────
def kem_encaps(
    ek: bytes,
    m: bytes,
    c_out: Path,
    k_out: Path,
    *,
    pke_encrypt: Optional[PkeEncryptFn] = None,
    ek_path: Optional[Path] = None,
) -> str:
    """
    KEM Encaps（derand）：(ek, m) → 密文 c(1568B) 与共享密钥 K(32B)。

    @param ek ek_kem 字节；@param m 32B 明文种子（调用方决定 urandom / M_HEX，本模块不干预）
    @param ek_path liboqs 路径需要 ek 落盘文件；未给则在 c_out 旁临时写一份
    @param pke_encrypt 回落所需 PKE Encrypt golden：(ek, m, coins) → c
    @return "liboqs" 或 "python"
    """
    if len(m) != M_BYTES or len(ek) != EK_BYTES:
        raise SystemExit(f"[kem_ref] encaps 入参尺寸 ek={len(ek)} m={len(m)}")

    ref = liboqs_ref()
    if ref is not None:
        p = ek_path
        if p is None:
            p = c_out.parent / "_kem_ref_ek.bin"
            p.write_bytes(ek)
        subprocess.check_call([str(ref), "encaps", str(p), str(c_out), str(k_out), m.hex()])
        if os.environ.get("KEM_GOLDEN_CROSS") == "1" and pke_encrypt is not None:
            c_py, k_py = _encaps_python(ek, m, pke_encrypt)
            _cross_check("encaps", {"c": (c_out.read_bytes(), c_py), "K": (k_out.read_bytes(), k_py)})
        return "liboqs"

    c, k = _encaps_python(ek, m, _require(pke_encrypt, "pke_encrypt", "Encaps"))
    c_out.write_bytes(c)
    k_out.write_bytes(k)
    return "python"


def _encaps_python(ek: bytes, m: bytes, pke_encrypt: PkeEncryptFn) -> "tuple[bytes, bytes]":
    """回落 Encaps：(K, r) = G(m ‖ H(ek))，c = Encrypt(ek, m, r)（Alg.17 第 2–3 行）。"""
    k, coins = g_sha3_512(m + h_sha3_256(ek))
    c = pke_encrypt(ek, m, coins)
    if len(c) != CT_BYTES:
        raise SystemExit(f"[kem_ref] pke_encrypt 返回 {len(c)}B，期望 {CT_BYTES}B")
    return c, k


# ──────────────────────────────── Alg.18 Decaps ────────────────────────────────
def kem_decaps(
    dk: bytes,
    c: bytes,
    k_out: Path,
    *,
    pke_decrypt: Optional[PkeDecryptFn] = None,
    pke_encrypt: Optional[PkeEncryptFn] = None,
    dk_path: Optional[Path] = None,
    c_path: Optional[Path] = None,
) -> str:
    """
    KEM Decaps：(dk_kem, c) → K(32B)，含隐式拒绝。

    回落路径需要 PKE Decrypt 与 Encrypt 两件（要重加密比对 c'）。若只有拒绝分支的期望值，
    调用方可直接用 `j_shake256(z, c)`，不必走本函数。

    @return "liboqs" 或 "python"
    """
    if len(dk) != DK_KEM_BYTES or len(c) != CT_BYTES:
        raise SystemExit(f"[kem_ref] decaps 入参尺寸 dk={len(dk)} c={len(c)}")

    ref = liboqs_ref()
    if ref is not None:
        dp = dk_path
        if dp is None:
            dp = k_out.parent / "_kem_ref_dk.bin"
            dp.write_bytes(dk)
        cp = c_path
        if cp is None:
            cp = k_out.parent / "_kem_ref_c.bin"
            cp.write_bytes(c)
        subprocess.check_call([str(ref), "decaps", str(dp), str(cp), str(k_out)])
        return "liboqs"

    k = _decaps_python(
        dk, c, _require(pke_decrypt, "pke_decrypt", "Decaps"), _require(pke_encrypt, "pke_encrypt", "Decaps")
    )
    k_out.write_bytes(k)
    return "python"


def _decaps_python(dk: bytes, c: bytes, pke_decrypt: PkeDecryptFn, pke_encrypt: PkeEncryptFn) -> bytes:
    """
    回落 Decaps（Alg.18）：dk_kem 切成 dk_pke ‖ ek ‖ h ‖ z，
    m' = Decrypt(dk_pke, c) → (K', r') = G(m'‖h) → c' = Encrypt(ek, m', r')；
    c' 与 c 逐字节相等则取 K'，否则取隐式拒绝值 J(z‖c)。
    """
    dk_pke = dk[:1536]
    ek = dk[1536:3104]
    h = dk[3104:3136]
    z = dk[3136:3168]
    m_prime = pke_decrypt(dk_pke, c)
    k_prime, coins = g_sha3_512(m_prime + h)
    c_prime = pke_encrypt(ek, m_prime, coins)
    return k_prime if c_prime == c else j_shake256(z, c)


def _cross_check(op: str, pairs: "dict[str, tuple[bytes, bytes]]") -> None:
    """KEM_GOLDEN_CROSS=1：liboqs 与 Python 回落逐字节比对，不一致直接判失败（防假绿）。"""
    for name, (a, b) in pairs.items():
        if a != b:
            raise SystemExit(
                f"[kem_ref] CROSS FAIL {op}.{name}: liboqs={a[:16].hex()}… python={b[:16].hex()}…"
            )
    print(f"[kem_ref] CROSS OK {op}: liboqs ≡ python（{', '.join(pairs)}）")
