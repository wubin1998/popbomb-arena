# PopBomb Arena — R36S / PortMaster

Q 版泡泡堂掌机版。游戏逻辑与渲染复用 [ports/3ds](../3ds) 的 C 代码，平台层用 SDL2 重写。

## 安装

把发行包里的内容解压到 `/roms/ports/`：

```
/roms/ports/PopBomb.sh
/roms/ports/popbomb/
```

从 EmulationStation 的 Ports 菜单启动。

## 操作

| 按键 | 功能 |
| --- | --- |
| 方向键 / 左摇杆 | 移动 |
| A / X | 放炸弹 |
| START | 暂停 |
| B / Y | 重新开始本关 |
| SELECT / BACK | 退出 |

## 说明

- 二进制为 aarch64，`libs.aarch64/` 里带上自编译的 SDL2（KMSDRM + GLES，无 X11 依赖）。
- 数据在 `popbomb/data/`：`gfx/` 是 tex3ds 描述符和 PNG，`romfs/audio/` 是 WAV。
- 环境变量：`POPBOMB_DATA`（数据根目录）、`POPBOMB_FULLSCREEN=1`（全屏）、
  `POPBOMB_RENDERER=software`（强制软件渲染）、`POPBOMB_NOAUDIO=1`（静音）。
