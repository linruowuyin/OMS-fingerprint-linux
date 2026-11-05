# omsmoc-driver patches

`omsmoc-driver.patch` — a libfprint patch adding a driver for the
OMS (OFilm / Jiangxi OMS Microelectronics) **match-on-chip** fingerprint
sensor `33a7:2388`.

> This repository exists to serve the patch over a stable
> `raw.githubusercontent.com` URL for the Arch Linux AUR package
> `libfprint-omsmoc-wujie14pro`. It is not a place to work on the driver.

## 用途

AUR 只接受源码包，不能随包携带本地文件，因此补丁必须托管在一个可公开
按 tag 访问的 raw URL 上。本仓库就是那个托管点。

- 补丁：`omsmoc-driver.patch`（适用 libfprint `1.94.100`）
- raw 地址：

```
https://raw.githubusercontent.com/linruowuyin/omsmoc-patches/v1.94.100/omsmoc-driver.patch
```

## 来源与许可

驱动本体来自 [BlueApple233/OMS-fingerprint-linux](https://github.com/BlueApple233/OMS-fingerprint-linux)，
许可 LGPL-2.1-or-later（见 `LICENSE`）。

[linruowuyin/OMS-fingerprint-linux](https://github.com/linruowuyin/OMS-fingerprint-linux)
是它的衍生版本，本补丁为其修改版，改动如下：

1. 协议层拆到 `libfprint/drivers/omsmoc/omsmoc_proto.c`（布局与 `goodixmoc` 等 MoC 驱动同构）
2. 新增 `tests/test-omsmoc-proto.c`（14 个用例），注册进 meson 的 `unit-tests`
3. 补上 `tests/omsmoc/` 回放文件（原发布版缺失，会导致 meson 断言失败）
4. 移除对 `data/autosuspend.hwdb` 的手工修改（该文件由构建生成）

udev 策略（常供电、不加 `uaccess`）不在此补丁里，由打包脚本安装。

## 重新生成补丁

补丁由驱动仓库生成，不要手工编辑：

```bash
git clone https://github.com/linruowuyin/OMS-fingerprint-linux
cd OMS-fingerprint-linux
# 在 libfprint v1.94.100 源码树中应用改动后：
git format-patch -1 --stdout > omsmoc-driver.patch
sha256sum omsmoc-driver.patch
```

改动补丁内容后需同步更新 tag 与 AUR 包里的 `sha256sums`。
