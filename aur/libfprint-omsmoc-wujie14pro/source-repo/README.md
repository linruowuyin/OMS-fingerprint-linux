# omsmoc-driver patches

`omsmoc-driver.patch` — libfprint 补丁，为 OMS(欧菲光/江西欧迈斯) match-on-chip
指纹传感器 `33a7:2388` 添加驱动。

## 来源与许可

驱动本体来自 [BlueApple233/OMS-fingerprint-linux](https://github.com/BlueApple233/OMS-fingerprint-linux)，
许可 LGPL-2.1-or-later（见 `LICENSE`）。本补丁是它的修改版，改动如下：

1. 协议层拆到 `libfprint/drivers/omsmoc/omsmoc_proto.c`（布局与 `goodixmoc` 等一致）
2. 新增 `tests/test-omsmoc-proto.c`（14 个用例），注册进 meson 的 `unit-tests`
3. 补上 `tests/omsmoc/` 回放文件（原发布版缺失，会导致 meson 断言失败）
4. 移除对 `data/autosuspend.hwdb` 的手工修改（该文件由构建生成）

udev 策略（常供电、不加 uaccess）不在此补丁里，由打包脚本安装。

## 用于

Arch Linux AUR 包 `libfprint-omsmoc-wujie14pro`。
