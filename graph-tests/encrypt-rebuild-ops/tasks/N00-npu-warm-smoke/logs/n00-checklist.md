# N00 连通检查清单（本机 WSL · 2026-09-08）

- [x] 查 `TAILSCALE_AUTHKEY` → UNSET
- [x] 查 `CANNLAB_SSH_KEY` / `~/.ssh/cannlab` → 均无
- [x] 查 `tailscale`/`tailscaled` → MISSING
- [x] 直连 TCP/SSH `100.97.98.72:2222` → timeout
- [ ] userspace tailscale up（**跳过**：无 authkey）
- [ ] SSH via SOCKS `127.0.0.1:1055`
- [ ] 远程 worktree `/mnt/workspace/ascendc-encrypt-rebuild`
- [ ] `flock` + `add_custom` NPU 冒烟

**判定**：`npu: blocked_auth` — 停止重试，交回主控补密钥后重派。
