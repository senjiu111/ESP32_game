# ESP32-S3 Game Console

一个基于 ESP32-S3 和 ST7789 LCD 的简易掌机工程，目前包含菜单封面、Dino、2048 和 Snake 三个游戏。工程使用 Arduino + PlatformIO 构建，代码按游戏模块拆分，图片素材会被转换为头文件后直接编译进固件。

## 当前功能

- 主菜单
  - 三个游戏封面卡片：Dino Jump、2048、Snake。
  - 支持方向键切换，OK 进入游戏，BACK 返回菜单。
  - 整体为浅色界面，白底黑字，红色作为强调色。

- Dino Jump
  - 支持奔跑、跳跃、下蹲动画。
  - 障碍包含仙人掌和飞鸟。
  - 飞鸟有两种高度：低空需要跳跃躲避，高空需要下蹲躲避。
  - 天空云朵为无碰撞装饰。
  - 支持 4 位分数显示和 HI 标识。
  - 针对 ST7789 局部刷新做了优化，减少画面闪烁。

- 2048
  - 4 x 4 棋盘。
  - 方块随数字增大由浅黄色逐渐过渡到红色。
  - 支持方向键滑动、数字合成、分数统计。
  - 合成到 2048 或无法继续移动时结束游戏。

- Snake
  - 左侧游戏区，右侧信息栏。
  - 支持方向控制、吃苹果、增长、碰撞死亡和重新开始。
  - 蛇头、蛇身、蛇尾和苹果使用绘制图形表现，风格与菜单封面接近。
  - 网格线为浅色，减少对主体图形的干扰。

## 硬件连接

当前代码中的引脚定义在 `src/main.cpp` 和 `include/key_input.h`。

### LCD

使用 SPI 驱动 ST7789，逻辑分辨率为 320 x 240。

| 功能 | ESP32-S3 GPIO |
| --- | --- |
| MOSI | GPIO11 |
| SCLK | GPIO12 |
| CS | GPIO10 |
| DC | GPIO9 |
| RST | GPIO8 |

### 按键

按键使用 `INPUT_PULLDOWN`，按下时输入应为高电平。

| 功能 | ESP32-S3 GPIO |
| --- | --- |
| UP | GPIO4 |
| DOWN | GPIO5 |
| LEFT | GPIO6 |
| RIGHT | GPIO7 |
| OK | GPIO15 |
| BACK | GPIO16 |

## 使用办法

### 1. 安装环境

需要安装：

- VS Code
- PlatformIO 扩展，或 PlatformIO Core 命令行工具
- ESP32-S3 对应的 USB 串口驱动

### 2. 编译

在工程根目录执行：

```powershell
pio run
```

如果本机 PlatformIO 缓存目录权限受限，可以指定工程内的 core 目录：

```powershell
$env:PLATFORMIO_CORE_DIR = Join-Path (Get-Location) '.pio\core'
pio run
```

### 3. 烧录

连接 ESP32-S3 开发板后执行：

```powershell
pio run -t upload
```

如果有多个串口设备，可以在 `platformio.ini` 中增加 `upload_port`，或在命令中指定对应串口。

### 4. 操作

- 菜单：UP/DOWN 或 LEFT/RIGHT 选择游戏。
- OK：进入游戏或重新开始。
- BACK：返回主菜单。
- Dino：UP/OK 跳跃，DOWN 下蹲。
- 2048：方向键移动方块。
- Snake：方向键控制移动方向。

## 项目结构

```text
include/
  dino_game.h         Dino 游戏接口
  game2048.h          2048 游戏接口
  snake_game.h        Snake 游戏接口
  key_input.h         按键输入接口
  game_sprites.h      Dino 相关 1bpp 图片数据
  menu_covers.h       菜单封面 RGB565 图片数据

src/
  main.cpp            菜单、任务调度和应用状态切换
  esp32_lcd.cpp/.h    ST7789 基础绘图封装
  dino_game.cpp       Dino 游戏逻辑和渲染
  game2048.cpp        2048 游戏逻辑和渲染
  snake_game.cpp      Snake 游戏逻辑和渲染
  key_input.cpp       按键扫描和消抖

pic/
  原始 PNG 素材和菜单封面图片

tools/
  图片素材转头文件、封面裁剪和报告生成脚本
```

## 图片资源生成

工程中的游戏素材位于 `pic/`，头文件位于 `include/`。常用脚本：

```powershell
python tools/pngs_to_header.py
python tools/generate_sprite_meta.py
python tools/menu_covers_to_header.py
```

如果替换了 PNG 素材，需要重新运行对应脚本生成头文件，然后再执行 `pio run` 编译。

## 后续可扩展方向

- 增加硬件音频：蜂鸣器、PWM 音效或 I2S DAC。
- 使用 NVS 保存各游戏最高分和设置。
- 增加亮度调节、电池电量显示和低电量提示。
- 增加震动反馈。
- 新增设置菜单，统一管理音量、亮度、震动和存档。
