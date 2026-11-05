# 上游化（libfprint MR）

本目录是**可直接提交上游**的补丁与说明。

> 想先在本地用起来？见 [docs/INSTALL.md](../../docs/INSTALL.md)。
> 发布/发版流程见 [docs/MAINTAINING.md](../../docs/MAINTAINING.md)。

## 1. 补丁内容

`0001-drivers-Add-support-for-OMS-match-on-chip-sensors.patch`
（`git format-patch --binary`，相对 upstream master，已实测可编译 + 通过 umockdev 回放）：

| 路径 | 说明 |
|---|---|
| `libfprint/drivers/omsmoc/omsmoc.c` / `.h` | 驱动本体（与 `goodixmoc/`、`mafpmoc/` 等 MoC 驱动同布局） |
| `libfprint/drivers/omsmoc/omsmoc_proto.c` / `.h` | EF01 帧编解码（纯函数，命名沿用 `goodix_proto.c` 的 `<driver>_proto.c` 约定） |
| `tests/test-omsmoc-proto.c` | 帧编解码单元测试，注册进 `unit-tests`（14 个用例） |
| `examples/oms-smoke.c` | 硬件调试/验收工具（只依赖公开 API） |
| `tests/omsmoc/custom.py` | umockdev 测试脚本（只读流程：list / verify / identify） |
| `tests/omsmoc/custom.pcapng` | 回放数据（实测录制，序列号已脱敏） |
| `tests/omsmoc/device` | umockdev 设备描述（同样已脱敏） |
| `meson.build` | 驱动加入默认集（`drivers_info`） |
| `libfprint/meson.build` | 驱动源文件注册 |
| `tests/meson.build` | 测试注册（`drivers_tests` + 协议单测） |
| `data/autosuspend.hwdb` | 支持列表加入 `usb:v33A7p2388*`，并从 known-unsupported 移除 |
| `libfprint/fprint-list-udev-hwdb.c` | 同上：把该 ID 从 "known unsupported" 表里删掉（否则每次构建都会告警重复） |

> ⚠️ **上游会追问的一点**：生成的 hwdb 会给所有受支持设备写 `ID_AUTOSUSPEND=1`，
> 但这颗芯片实测扛不住 autosuspend。发行版里靠一条 `power/control=on` 的 udev 规则兜底
> （写在 `aur/libfprint-omsmoc/PKGBUILD` 的 `package()` 里）；要真正进上游，得让驱动实现
> 复位/恢复，或给 hwdb 生成器加例外。这一点建议在 MR 描述里主动写明。

## 2. 提交步骤（fork + MR）

> ⚠️ **前置：freedesktop GitLab 新账号默认受限**（防垃圾）：不能创建项目、不能 fork，
> 而 fork 是提 MR 的前提。需要先申请权限（一般几小时到几天）：
>
> 1. 注册并验证邮箱：<https://gitlab.freedesktop.org/users/sign_up>
>    （确认邮件后账号约 10–15 分钟生效）
> 2. 填 **User verification** 模板的 issue：
>    <https://gitlab.freedesktop.org/freedesktop/freedesktop/issues/new?issuable_template=User%20verification>
>    正文大意（英文，照模板即可）：
>
>    ```text
>    I cannot create new projects or fork an existing one. I want to contribute
>    a driver for the OMS match-on-chip fingerprint sensor (USB 33a7:2388) to
>    libfprint, with a recorded umockdev test case. Please add me to the list of
>    internal users so that I can fork libfprint and open a merge request.
>    ```
> 3. 顺手把 SSH key 加上：<https://gitlab.freedesktop.org/-/profile/keys>
>
> 备选（无需 fork 权限）：在 libfprint 提一个 **Driver Request** issue，说明驱动已就绪并给出
> 仓库/补丁链接，由维护者决定如何接手。

```bash
# 1. 在 GitLab 上 fork libfprint（https://gitlab.freedesktop.org/libfprint/libfprint）
git clone https://gitlab.freedesktop.org/<你的账号>/libfprint.git
cd libfprint
git checkout -b omsmoc-driver origin/master

# 2. 应用补丁
git am --3way /path/to/0001-drivers-Add-support-for-OMS-match-on-chip-sensors.patch

# 3. 构建 + 跑回放测试（无需硬件）
meson setup build -Ddrivers=default && ninja -C build
meson test -C build omsmoc

# 4. 推送并开 MR（描述见下）
git push -u origin omsmoc-driver
```

## 3. MR 描述（可直接粘贴）

**标题**：`drivers: Add support for OMS match-on-chip sensors`

**正文**：

```markdown
This adds a driver for the Jiangxi OMS Microelectronics fingerprint module
(33a7:2388), a match-on-chip sensor found in several laptops. There is no
kernel driver; the device exposes a vendor specific USB interface with two
bulk endpoints and is accessed from userspace.

The protocol was reverse engineered from the vendor's Windows driver: the
project kept USB captures of that driver (usbmon), a static analysis of the
driver DLLs and a pyusb prototype that was validated against the hardware
before the driver was written. Everything is plain text — no handshake, no
key exchange and no encryption.

Implemented: identify, verify, enroll, delete, clear-storage and listing of
the stored prints. The chip does the matching, so templates are opaque; the
identify result reports the slot it matched, which makes verification exact
even with several enrolled fingers and provides duplicate detection.

Testing:
- `fprintd-enroll` (six stages) and `fprintd-verify` (`verify-match`) pass on
  the hardware, including the negative cases (`verify-no-match` for an
  unenrolled finger and for an enrolled finger that is not the requested
  print).
- The included umockdev test case replays a recorded session without
  hardware (`meson test omsmoc`); device identifying data was removed from
  the recording before it was added here.

Known limitations are documented in the driver: the two parameter bytes of
the identify command are echoed but do not restrict the search, and the
semantics of the 32 byte chip serial number are unknown (used for logging
only).
```

## 4. 上游规范核对（HACKING.md）

- ✅ 无专有代码/二进制：驱动为独立实现，不包含厂商文件；模板按不透明 blob 处理
- ✅ 使用 `fpi_` 内部 API + `fpi_device`/`fpi_ssm` 框架（参考 `goodixmoc`、`elanmoc`）
- ✅ 公开 API 未改动（无需 gtk-doc 更新）
- ✅ 测试数据随驱动提交（上游要求），且已脱敏
- ⚠️ 上游对「libfprint 开发者提交驱动」要求 3 台独立设备 + 协议规范；本项目走
  「enterprising hacker 自带驱动补丁」路径（HACKING.md 明确允许）
