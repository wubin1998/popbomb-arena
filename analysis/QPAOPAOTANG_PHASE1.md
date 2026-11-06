# Q版泡泡堂（Qpaopaotang.swf）阶段一逆向分析报告

> 目标文件：`Qpaopaotang.swf`（2,167,060 字节，压缩后）
> 分析日期：2026-11-06
> 工具链：REA（结构探查）+ FFDec CLI（脚本/精灵/音频导出）
> 结论：该 SWF 是 **AS2 编写的 4 人炸弹人对战游戏**，550 × 400 舞台、35 fps 固定帧逻辑、15 × 13 格 × 30 px 棋盘，支持单人（1 vs 3 AI）与双人同屏（2P + 2 AI），包含 3 档难度、5 套场景主题和 12 种道具。

---

## 0. 摘要

| 项目 | 结果 |
|---|---|
| 文件签名 | `CWS`（zlib 压缩） |
| SWF 版本 | 9 |
| 声明长度 / 压缩后 / 解压后 | 2,309,722 / 2,167,060 / 2,309,714 字节 |
| 舞台 | 550 × 400，背景色 `#009900` |
| 帧率 | 35 fps |
| 主时间轴 | 12 帧，86 个帧标签 |
| 标签总数 | 3,966 |
| 脚本 | AS2，45 个 `DoAction` |
| 精灵 | 77 个 `DefineSprite` |
| 位图 | 141 张（`DefineBitsLossless2` / `DefineBitsJPEG3`） |
| 声音 | 17 段（11,025 Hz / 16 bit 单声道为主） |
| 字体 | 5 个（Arial Black、Times New Roman 等） |
| 动态文本 | 28 个 `DefineEditText` |
| 导出资源 | 39 个 `ExportAssets` |
| 阶段二/三 | 已完成：`src/` 下为 Canvas 2D 原生重制版 |

---

## 1. 文件结构

### 1.1 标签分布（关键项）

| 标签 | 数量 | 用途 |
|---|---:|---|
| `PlaceObject2` | 1,658 | 时间轴/精灵内实例摆放 |
| `ShowFrame` | 1,147 | 帧显示 |
| `DefineShape` / `3` / `4` | 216 / 38 / 19 | 矢量 UI、按钮、图标 |
| `RemoveObject2` | 190 | 移除实例 |
| `FrameLabel` | 86 | 菜单、场景、角色、道具标签 |
| `SoundStreamHead2` | 77 | 流式音频头 |
| `DefineSprite` | 77 | 游戏对象与特效影片剪辑 |
| `DefineBitsJPEG3` | 68 | 带 alpha 的 JPEG 位图 |
| `DefineBitsLossless2` | 62 | 带 alpha 的无损位图 |
| `DoAction` | 45 | AS2 逻辑 |
| `DefineSound` | 17 | 内嵌音效/BGM |
| `DefineButton2` | 16 | 菜单按钮 |
| `DefineEditText` | 28 | 分数、提示等动态文本 |

### 1.2 主时间轴与界面流程

12 个根帧配合 86 个标签，覆盖完整流程：

- 载入：`loading`
- 主菜单：`menu`、`menu2`、`menu3`
- 玩法说明：`howtoplay`、`toolInstruction`
- 角色/难度选择：`select`、`easy`、`normal`、`hard`
- 对战：`game`
- 结算：`win`、`timeupwin`、`lose`、`timeuplose`、`score`
- 排行榜：`highscore`、`highscore2`
- 场景标签：`森林系`、`沙漠系`、`城市系`、`藏宝系`、`糖果屋系`，以及 `_大型` 变体
- 角色标签：`红`、`黄`、`蓝`、`绿`，以及 `1p红`…`2p绿`
- 道具标签：`生命加1`、`抢手`、`炸弹糖`、`盾牌`、`威力药`、`飞鞋`、`火箭`、`地雷`、`手榴弹`、`汽弹`、`冰箭`、`火焰箭`
- 方向标签：每套角色/炮台均有 `left`、`up`、`right`、`down`

重制版保留了这套流程的实质（标题 → 模式/难度/角色 → 对战 → 关卡结算 → 最终分数），并按现代 Web 交互重新设计了界面；原作中装饰性的多级菜单页被合并。

---

## 2. 脚本结构

### 2.1 核心控制器：`DefineSprite_554`

这是整局游戏的主控制器，3 帧：

| 帧 | 行数 | 职责 |
|---|---:|---|
| frame 1 | 1,686 | 清场、场景/难度参数、棋盘生成、道具表、爆炸/伤害/AI 工具函数、关卡初始化 |
| frame 2 | 357 | 生成蘑菇砖、放置英雄、定义每帧 `onEnterFrame`（输入、AI、计时、胜负判定） |
| frame 3 | 1 | `stop()` |

主要函数（frame 1）：

| 函数 | 作用 |
|---|---|
| `gameover(msg)` | `win` / `lose` / `timeup` 结算、加分、复活、跳转 |
| `putBomb(hero)` | 放置炸弹，`delayTime = 60` |
| `bombExp(bg)` | 炸弹爆炸，逐帧向外扩张 |
| `putSpecial(hero)` | 发射特殊武器 |
| `hurtHero(hero)` | 无敌帧、盾牌、扣命、AI 续命 |
| `brickBreak(bg, heroNum)` | 炸砖、加分、65% 掉道具 |
| `pushBomb(bg, dx, dy)` | 抢手推炸弹（每 2 帧移动一格） |
| `movHero(hero, xyId)` | 角色移动（6 帧/格，减速时 20 帧/格） |
| `isSave(x0, y0)` / `getScore(x0, y0)` | AI 安全格评估 |
| `calDepth(x, y, margin)` | 伪 3D 深度排序 |
| `disorder(arr)` | Fisher–Yates 洗牌 |

### 2.2 特效与对象脚本

| 精灵 | 帧数 | 用途 |
|---|---:|---|
| `DefineSprite_196_brick` | 58 | 可破坏砖 + 不可破坏石 + 大型地标 |
| `DefineSprite_232_bg` | 12 | 地面格子 |
| `DefineSprite_294` | 12 | 12 种道具图标 |
| `DefineSprite_372_bomb` | 40 | 炸弹动画 |
| `DefineSprite_274_exp` | 12 | 爆炸中心/中段/末端帧 |
| `DefineSprite_249_brickExp` | 11 | 砖块爆裂 |
| `DefineSprite_238_heroExp` | 36 | 角色死亡爆炸 |
| `DefineSprite_280_heroDieSmoke` | 36 | 死亡烟雾 |
| `DefineSprite_306_rocket` | 4 | 火箭 |
| `DefineSprite_307_mine` | 60 | 地雷 |
| `DefineSprite_311_grenade` | 4 | 手榴弹 |
| `DefineSprite_316_gasBomb` | 101 | 汽弹 |
| `DefineSprite_320_fireEffect` | 40 | 火焰箭火焰 |
| `DefineSprite_326_freezeEffect` | 40 | 冰箭冻结 |
| `DefineSprite_211_p_pointer` | 60 | 玩家指示箭头 |

---

## 3. 核心常量

```as2
w = 15;  h = 13;  d = 30;
xy    = [[-1,0],[0,-1],[1,0],[0,1]];      // 左、上、右、下
heroXy = [[3,3],[11,3],[3,9],[11,9]];      // 4 个固定出生点
FPS   = 35;
计时  = 180 秒
```

深度公式：

```as2
calDepth(x, y, margin) = (y * w + (w - x)) * margin
```

同排越靠下、越靠左，深度越大；棋盘 `margin = 10`，地面 `margin = 1`，炸弹额外 `-5`，道具 `-6`，火焰 `-7`。

---

## 4. 角色与操作

| 参数 | 值 |
|---|---|
| 初始移动速度 | `vLevel = 6` 帧/格 |
| 减速后速度 | `slowVLevel = 20` 帧/格 |
| 初始炸弹数 | `totalBomb = 1` |
| 初始威力 | `bombLevel = 1`（上限 10） |
| 出生无敌 | `varShield = 60 + random(4)` 帧 |
| 简单难度人类 | 额外 `shield = true` |
| 冰面冻结 | `stopTime = 300` 帧 |
| 汽弹减速 | `slowTime = 600` 帧 |

按键（来自 `frame_8`）：

- 单人：方向键 + `Space`
- 双人 P1：`A / W / D / S` + `Space`
- 双人 P2：方向键 + `Enter`
- 暂停：`P`

移动是逐格插值；只有起点和终点都在格心时才允许转向/放弹，避免斜穿。

---

## 5. 炸弹与爆炸

- 引信：`delayTime = 60` 帧（约 1.71 秒）。
- 爆炸从中心开始，四个方向每帧扩张一格。
- 遇到不可破坏石停止；遇到蘑菇砖炸毁并停止该方向。
- 爆炸区域保持 10 帧，然后每帧 `alpha -= 20` 淡出（约 5 帧）。
- 爆炸中心帧号 = `heroId`，中段 = `heroId + 4`，末端 = `heroId + 8`，旋转 `(方向索引 + 2) * 90°`。
- 连锁：爆炸范围内的炸弹立即引爆。
- 同格/相邻安全判定 `isSave` 会从距离 1 开始扫描，遇到砖/石停止，并检查炸弹 `bombLevel` 是否覆盖该距离。

计分：

| 事件 | 分数 |
|---|---:|
| 炸毁砖块（简单/普通/困难） | 5 / 10 / 15 |
| 击倒对手 | +1000 |
| 拾取道具 | +100 |
| 过关奖励 | +1000 |
| 过关剩余时间 | `int(剩余秒数) * 10` |

时间耗尽时比较“存活人类最高分”与“存活 AI 最高分”，人类严格大于 AI 才算胜利。

---

## 6. 道具系统

### 6.1 掉落

炸砖后：

1. 65% 概率掉道具；
2. 掉落后 80% 为普通道具，20% 为特殊武器。

普通道具权重：

| 道具 | 权重 |
|---|---:|
| 生命加1 | 1 |
| 抢手 | 14 |
| 炸弹糖 | 25 |
| 盾牌 | 10 |
| 威力药 | 25 |
| 飞鞋 | 25 |

特殊武器按“当前关卡的两件套”随机：

```as2
specialToolName_array = ["火箭","地雷","手榴弹","汽弹","冰箭","火焰箭"];
```

- 每 3 关为一组：`[0,1]` → `[2,3]` → `[4,5]`；
- 进入下一组前重新洗牌；
- 因此一个 3 关周期内只会出现两种特殊武器。

### 6.2 效果

| 道具 | 效果 |
|---|---|
| 生命加1 | `life++` |
| 抢手 | 可推动自己放置的炸弹 |
| 炸弹糖 | `totalBomb++` |
| 盾牌 | 抵挡一次伤害，并获得 60 帧无敌 |
| 威力药 | `bombLevel++`，上限 10 |
| 飞鞋 | 移动速度 -1 帧/格，最低 3 |
| 火箭 | 直线飞行，命中目标/砖块后爆炸 |
| 地雷 | 敌人踩中即爆 |
| 手榴弹 | 向面朝方向投掷两格 |
| 汽弹 | 命中敌人后减速 600 帧 |
| 冰箭 | 生成冰墙，冻结附近角色 300 帧，1 次 |
| 火焰箭 | 贯穿三格火焰 |

AI 限制（原版）：

- 简单难度：AI 不拾取任何道具。
- 普通难度：AI 不拾取飞鞋和全部 6 种特殊武器。
- 困难难度：AI 可以使用全部道具。

---

## 7. AI

每个 AI 每帧：

1. 若当前格安全（`isSave` 为真）：
   - 20% 概率寻找高分相邻格（有道具 +5，再加 `random(3)`）；
   - 否则 5% 概率“放炸弹并逃跑”（目标格分数 ≥ 2）。
2. 若当前格不安全：
   - 以 `escapeRate` 概率向相邻安全格逃跑；
   - 无安全格时随机方向。

`escapeRate` 由难度决定：

| 难度 | 逃生率 |
|---|---:|
| 简单 | 20 |
| 普通 | 60 |
| 困难 | 100 |

AI 与人类共用同一套移动、炸弹、道具和受伤规则；区别仅在于道具拾取限制、受伤后是否可续命。

---

## 8. 地图生成（完全复刻）

1. 随机选择场景 `sceneStyle = random(5)`。
2. 洗牌砖块样式与不可破坏石样式：
   - 砖块只使用洗牌后的**前 4 种**；
   - 不可破坏石只使用洗牌后的**前 2 种**。
3. 中央 2 × 2 设为不可破坏，并放置一个大型地标精灵。
4. 生成候选格 `ranMap`：排除 4 个出生点及其上下左右邻格。
5. 洗牌前 `ranMapL = ranMap.length`。
6. 把每个出生点的 4 个**对角格** `unshift` 到 `ranMap` 前端，保证它们一定被处理。
7. 只遍历前 `ranMapL` 个候选格，用 8 邻域环形检测狭小空腔：
   - 若环上出现连续 3 个阻挡格，跳过；
   - 否则统计“空段 + 阻挡段”数量，若 ≤ 3，则放置不可破坏石。
8. 对除石头外的所有格子做 60% 砖块判定。
9. 最后清除每个出生点自身的砖块，供英雄出生。

结果特征：

- 每个出生点周围 4 个对角格必定是不可破坏石（形成 X 形保护）；
- 出生点上下左右可能有砖，需要炸开；
- 额外石头只出现在狭窄空腔附近，不是随机撒点。

---

## 9. 资产清单

`src/assets` 共 546 张 PNG、17 段 MP3、约 4.0 MB：

| 资源集 | 帧数 | 来源精灵 |
|---|---:|---|
| `bg` | 12 | `DefineSprite_232_bg` |
| `brick` | 58 | `DefineSprite_196_brick` |
| `tool` | 12 | `DefineSprite_294` |
| `bomb` | 40 | `DefineSprite_372_bomb` |
| `exp` | 12 | `DefineSprite_274_exp` |
| `brickExp` | 11 | `DefineSprite_249_brickExp` |
| `heroExp` | 36 | `DefineSprite_238_heroExp` |
| `dieSmoke` | 36 | `DefineSprite_280_heroDieSmoke` |
| `rocket` | 4 | `DefineSprite_306_rocket` |
| `rocketSmoke` | 4 | `DefineSprite_314_rocketSmoke` |
| `mine` | 60 | `DefineSprite_307_mine` |
| `grenade` | 4 | `DefineSprite_311_grenade` |
| `gas` | 101 | `DefineSprite_316_gasBomb` |
| `fireEffect` | 40 | `DefineSprite_320_fireEffect` |
| `freezeEffect` | 40 | `DefineSprite_326_freezeEffect` |
| `pointer` | 60 | `DefineSprite_211_p_pointer` |
| `hero1`–`hero4` | 各 4 | 4 个角色精灵 |

音效：

- BGM：`bgm1`–`bgm4`、`menu`
- 事件：`place`、`explode`、`push`、`pickup`、`rocket`、`grenade`、`mine`、`stageWin`、`stageLose`
- 原始编号音效：`sfx315`、`sfx319`、`sfx325`

PNG 由 FFDec 导出精灵帧后，用纯标准库脚本 `tools/build_qpaopao_assets.py` 裁掉透明边距并统一命名。

---

## 10. 与重制版的对应关系

| AS2 模块 | HTML5 模块 |
|---|---|
| `DefineSprite_554` frame 1 | `src/js/game/game.js`、`src/js/game/map.js` |
| frame 2 每帧循环 | `src/js/game/game.js#step` |
| `putBomb` / `bombExp` | `src/js/game/bomb.js` |
| `movHero` / `hurtHero` / AI | `src/js/game/hero.js` |
| 道具表与掉落 | `src/js/game/tools.js` |
| 菜单/结算/输入 | `src/js/main.js`、`src/js/engine.js` |
| 时间轴帧标签 | DOM 屏幕状态（标题/设置/说明/暂停/结算） |

固定 35 Hz 逻辑由 `BI.Loop` 的累加器保证；渲染层按行做深度排序，移动层用格内插值，保持原作的“逐格移动 + 连续动画”手感。

---

## 11. 已知差异与说明

1. **界面重绘**：原作菜单是 Flash 时间轴动画，重制版改为响应式 DOM/canvas 布局，信息结构一致。
2. **素材裁剪**：PNG 去掉了透明边距，绘制时按原比例缩放。
3. **声音**：浏览器自动播放策略要求首次点击/按键后才能播放 BGM。
4. **输入**：仅键盘操作；移动端可以浏览界面，但无法进行对战。
5. **未复刻内容**：原作排行榜（`highscore`）与部分装饰性过场未实现；核心规则、数值、AI、地图生成和关卡流程均已复刻。
6. **反编译产物**：`analysis/` 为逆向工作区；运行游戏只需要 `index.html`、`style.css`、`src/`。

---

## 12. 复现命令

```bash
# 结构探查（REA 未随仓库分发，需另行安装）
node /path/to/rea ...

# FFDec 导出脚本/精灵/音频
/Applications/FFDec.app/Contents/MacOS/ffdec -export script,image,sound ...

# 资源整理
python3 -I tools/build_qpaopao_assets.py

# 本地运行
python3 -m http.server 8000
```

> 具体的 FFDec 路径与参数按本机安装位置调整；`analysis/` 中已保留本次导出的完整结果，可直接用于二次分析。
