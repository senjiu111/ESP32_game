import path from "node:path";
import fs from "node:fs/promises";
import { fileURLToPath } from "node:url";

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);
const ROOT = path.resolve(__dirname, "..");
const PIC = path.join(ROOT, "report_pic");
const OUT = path.join(ROOT, "outputs", "report_ppt");
const FINAL = path.join(OUT, "简易游戏机课堂汇报.pptx");
const PREVIEW = path.join(OUT, "preview");

const W = 1280;
const H = 720;

const C = {
  paper: "#F8FAF7",
  ink: "#111111",
  muted: "#5F675F",
  faint: "#E2E7DF",
  panel: "#FFFFFF",
  red: "#D9472E",
  yellow: "#F3D36B",
  green: "#79B867",
  blue: "#5E8CC7",
  dark: "#1F2922",
};

const font = "Microsoft YaHei";

const artifact = await import(
  "file:///C:/Users/Asus/.cache/codex-runtimes/codex-primary-runtime/dependencies/node/node_modules/@oai/artifact-tool/dist/artifact_tool.mjs"
);
const { Presentation, PresentationFile } = artifact;

function img(name) {
  return path.join(PIC, name);
}

function noneLine() {
  return { fill: { type: "none" } };
}

function rect(slide, x, y, width, height, fill = C.panel, line = C.faint, radius = 14) {
  return slide.shapes.add({
    geometry: "rect",
    position: { x, y, width, height },
    width,
    height,
    fill,
    line: line === "none" ? noneLine() : { fill: line, width: 1 },
    borderRadius: radius,
  });
}

function text(slide, value, x, y, width, height, opts = {}) {
  return slide.shapes.add({
    geometry: "rect",
    position: { x, y },
    width,
    height,
    fill: { type: "none" },
    line: noneLine(),
    text: {
      value,
      style: {
        typeface: font,
        fontSize: opts.size ?? 24,
        color: opts.color ?? C.ink,
        bold: opts.bold ?? false,
        alignment: opts.align ?? "left",
        anchor: opts.anchor ?? 1,
        lineSpacing: opts.leading ?? 1.12,
        insets: opts.insets ?? { top: 0, right: 0, bottom: 0, left: 0 },
      },
    },
  });
}

function bullet(slide, items, x, y, width, opts = {}) {
  const gap = opts.gap ?? 44;
  items.forEach((item, index) => {
    const cy = y + index * gap;
    rect(slide, x, cy + 10, 9, 9, opts.color ?? C.red, "none", 4);
    text(slide, item, x + 24, cy, width - 24, 34, {
      size: opts.size ?? 24,
      color: opts.textColor ?? C.ink,
    });
  });
}

function image(slide, name, x, y, width, height, opts = {}) {
  return slide.images.add({
    path: img(name),
    position: { x, y, width, height },
    width,
    height,
    fit: opts.fit ?? "cover",
    borderRadius: opts.radius ?? 16,
  });
}

function title(slide, kicker, claim) {
  rect(slide, 52, 46, 8, 30, C.red, "none", 4);
  text(slide, kicker, 72, 43, 220, 32, {
    size: 15,
    color: C.red,
    bold: true,
    anchor: 2,
  });
  text(slide, claim, 72, 78, 930, 58, {
    size: 34,
    bold: true,
    leading: 1.06,
  });
}

function footer(slide, page) {
  slide.shapes.add({
    geometry: "rect",
    position: { x: 72, y: 664 },
    width: 1030,
    height: 1,
    fill: C.faint,
    line: noneLine(),
  });
  text(slide, "ESP32-S3 简易游戏机", 72, 678, 240, 20, {
    size: 12,
    color: C.muted,
  });
  text(slide, String(page).padStart(2, "0"), 1152, 674, 52, 24, {
    size: 14,
    color: C.muted,
    align: "right",
  });
}

function slideBase(presentation, kicker, claim, page) {
  const slide = presentation.slides.add();
  slide.shapes.add({
    geometry: "rect",
    position: { x: 0, y: 0 },
    width: W,
    height: H,
    fill: C.paper,
    line: noneLine(),
  });
  title(slide, kicker, claim);
  footer(slide, page);
  return slide;
}

function chip(slide, label, x, y, color) {
  rect(slide, x, y, 126, 34, color, "none", 17);
  text(slide, label, x, y + 5, 126, 22, {
    size: 15,
    color: "#FFFFFF",
    bold: true,
    align: "center",
  });
}

function sectionNumber(slide, n, label, x, y, color) {
  rect(slide, x, y, 64, 64, color, "none", 16);
  text(slide, n, x, y + 9, 64, 38, {
    size: 30,
    color: "#FFFFFF",
    bold: true,
    align: "center",
  });
  text(slide, label, x + 82, y + 11, 300, 34, { size: 25, bold: true });
}

async function main() {
  await fs.mkdir(OUT, { recursive: true });
  await fs.mkdir(PREVIEW, { recursive: true });

  const presentation = Presentation.create({ slideSize: { width: W, height: H } });

  {
    const slide = presentation.slides.add();
    slide.shapes.add({ geometry: "rect", position: { x: 0, y: 0 }, width: W, height: H, fill: C.paper, line: noneLine() });
    image(slide, "全览图.jpg", 690, 55, 510, 610, { radius: 28 });
    rect(slide, 64, 58, 8, 88, C.red, "none", 4);
    text(slide, "基于 ESP32-S3 的", 92, 60, 480, 44, { size: 28, color: C.muted });
    text(slide, "简易游戏机\n设计与实现", 90, 115, 560, 170, { size: 58, bold: true, leading: 1.02 });
    text(slide, "课堂汇报 | 硬件设计、软件实现与 AI 辅助开发", 94, 326, 560, 36, { size: 22, color: C.muted });
    chip(slide, "Dino", 94, 426, C.red);
    chip(slide, "2048", 240, 426, C.yellow);
    chip(slide, "Snake", 386, 426, C.green);
    text(slide, "ESP32-S3 MCU / LCD 显示 / 按键交互 / 多游戏框架", 94, 608, 560, 28, { size: 18, color: C.muted });
    footer(slide, 1);
  }

  {
    const slide = slideBase(presentation, "项目目标", "用一块 MCU 搭建可扩展的掌上小游戏平台", 2);
    image(slide, "主界面.jpg", 742, 152, 400, 400, { radius: 22 });
    rect(slide, 83, 166, 540, 336, C.panel, C.faint, 18);
    bullet(slide, [
      "低成本、小型化：以 ESP32-S3 作为核心控制器",
      "可交互：通过实体按键完成菜单选择和游戏控制",
      "可扩展：主菜单 + 多游戏模块的整体结构",
      "可展示：完成三款经典小游戏和统一浅色界面",
    ], 120, 205, 470, { gap: 61 });
    text(slide, "当前成果：主菜单、Dino、2048、Snake 三款游戏均已实现基本逻辑与显示优化。", 118, 540, 960, 42, { size: 23, bold: true, color: C.dark });
  }

  {
    const slide = slideBase(presentation, "系统方案", "按键输入、游戏逻辑和 LCD 显示形成闭环", 3);
    const xs = [86, 333, 580, 827];
    const labels = ["按键输入", "ESP32-S3 主控", "游戏逻辑模块", "LCD 显示输出"];
    const colors = [C.blue, C.red, C.green, C.yellow];
    labels.forEach((label, i) => {
      rect(slide, xs[i], 268, 198, 108, colors[i], "none", 24);
      text(slide, label, xs[i], 300, 198, 34, { size: 26, color: "#FFFFFF", bold: true, align: "center" });
      if (i < labels.length - 1) {
        slide.shapes.add({ geometry: "rect", position: { x: xs[i] + 212, y: 318 }, width: 84, height: 4, fill: C.dark, line: noneLine() });
        slide.shapes.add({ geometry: "triangle", position: { x: xs[i] + 292, y: 307 }, width: 20, height: 26, fill: C.dark, line: noneLine(), rotation: 90 });
      }
    });
    rect(slide, 156, 455, 860, 70, "#FFF8E1", "#E8D894", 18);
    text(slide, "主程序负责菜单和游戏切换；各游戏模块独立维护状态、逻辑、绘制与按键响应。", 196, 477, 780, 30, { size: 24, color: C.dark, bold: true });
  }

  {
    const slide = slideBase(presentation, "硬件组成", "ESP32-S3 负责控制、刷新和输入采集", 4);
    image(slide, "全览图.jpg", 720, 148, 390, 390, { radius: 24 });
    sectionNumber(slide, "01", "主控：ESP32-S3", 100, 174, C.red);
    sectionNumber(slide, "02", "显示：LCD 彩屏", 100, 274, C.blue);
    sectionNumber(slide, "03", "输入：方向键 / OK / BACK", 100, 374, C.green);
    sectionNumber(slide, "04", "接口：电源、下载与调试", 100, 474, C.yellow);
  }

  {
    const slide = slideBase(presentation, "原理图", "硬件电路围绕最小系统、显示和按键展开", 5);
    image(slide, "原理图.png", 64, 150, 760, 470, { fit: "contain", radius: 18 });
    rect(slide, 870, 170, 275, 330, C.panel, C.faint, 18);
    bullet(slide, [
      "ESP32-S3 最小系统",
      "LCD 屏幕接口",
      "实体按键输入",
      "电源与下载调试电路",
      "后续可预留音频接口",
    ], 902, 206, 220, { size: 21, gap: 52 });
  }

  {
    const slide = slideBase(presentation, "PCB 设计", "布局围绕掌机形态组织屏幕、按键和主控区域", 6);
    image(slide, "PCB.png", 86, 148, 650, 460, { fit: "contain", radius: 18 });
    rect(slide, 785, 168, 330, 300, "#FFFFFF", C.faint, 18);
    text(slide, "布局关注点", 820, 204, 260, 34, { size: 29, bold: true });
    bullet(slide, [
      "屏幕与按键区域直观分离",
      "主控与接口集中布置",
      "控制整机尺寸，便于手持",
      "后续可扩展喇叭、电池和震动",
    ], 822, 264, 270, { size: 21, gap: 48, color: C.blue });
  }

  {
    const slide = slideBase(presentation, "软件架构", "以主菜单为入口，游戏模块分文件实现", 7);
    const rows = [
      ["main", "菜单显示 / 游戏切换 / 输入分发", C.red],
      ["display", "LCD 绘图封装 / 图片资源显示 / 局部刷新", C.blue],
      ["dino_game", "跳跃、下蹲、障碍、动画、计分", C.green],
      ["game_2048", "棋盘移动、合成、胜负判断、计分", C.yellow],
      ["snake_game", "移动、食物、增长、碰撞和信息栏", C.dark],
    ];
    rows.forEach((row, i) => {
      const y = 150 + i * 85;
      rect(slide, 115, y, 215, 54, row[2], "none", 12);
      text(slide, row[0], 115, y + 12, 215, 26, { size: 23, color: "#FFFFFF", bold: true, align: "center" });
      rect(slide, 355, y, 665, 54, C.panel, C.faint, 12);
      text(slide, row[1], 385, y + 12, 610, 26, { size: 22 });
    });
  }

  {
    const slide = slideBase(presentation, "主菜单", "封面式入口让三款游戏更容易识别", 8);
    image(slide, "主界面.jpg", 90, 148, 530, 430, { fit: "contain", radius: 24 });
    rect(slide, 690, 180, 390, 300, C.panel, C.faint, 18);
    bullet(slide, [
      "每个游戏对应独立封面",
      "方向键选择，OK 进入",
      "统一白底黑字浅色风格",
      "封面资源由 AI 生成后嵌入",
    ], 730, 225, 320, { size: 23, gap: 56 });
  }

  {
    const slide = slideBase(presentation, "Dino", "从基础跳跃扩展到下蹲、飞鸟和背景动画", 9);
    image(slide, "dino_1.jpg", 70, 152, 330, 220, { radius: 16 });
    image(slide, "dino_3.jpg", 430, 152, 330, 220, { radius: 16 });
    image(slide, "dino_5.jpg", 790, 152, 330, 220, { radius: 16 });
    rect(slide, 90, 430, 1000, 110, "#FFFFFF", C.faint, 18);
    bullet(slide, [
      "实现恐龙跳跃、下蹲、仙人掌、飞鸟、云朵和计分",
      "针对不同高度飞鸟设计跳跃 / 下蹲两种躲避方式",
      "通过局部刷新和碰撞范围调整改善游戏体验",
    ], 128, 455, 920, { size: 23, gap: 34 });
  }

  {
    const slide = slideBase(presentation, "2048", "4×4 棋盘在左，分数和状态信息在右", 10);
    image(slide, "2048.jpg", 84, 140, 520, 520, { fit: "contain", radius: 24 });
    rect(slide, 665, 160, 390, 340, C.panel, C.faint, 18);
    text(slide, "核心逻辑", 704, 198, 300, 34, { size: 30, bold: true });
    bullet(slide, [
      "滑动方向控制整行 / 整列移动",
      "相同数字合并并累计分数",
      "随机生成新方块",
      "合成到 2048 即完成游戏",
      "数字方块颜色随数值加深",
    ], 704, 256, 310, { size: 21, gap: 43, color: C.yellow });
  }

  {
    const slide = slideBase(presentation, "Snake", "蛇头、蛇身、蛇尾和苹果都做成图形化元素", 11);
    image(slide, "snake_1.jpg", 76, 148, 500, 390, { fit: "contain", radius: 22 });
    image(slide, "snake_2.jpg", 624, 148, 500, 390, { fit: "contain", radius: 22 });
    text(slide, "网格线淡化后，蛇和苹果的图形层次更明显；右侧信息栏保留分数与状态信息。", 120, 575, 940, 34, { size: 23, bold: true, align: "center" });
  }

  {
    const slide = slideBase(presentation, "显示优化", "从全屏重绘转向局部刷新，降低闪烁感", 12);
    rect(slide, 105, 168, 430, 280, "#FFFFFF", C.faint, 20);
    text(slide, "早期问题", 145, 208, 300, 34, { size: 31, bold: true, color: C.red });
    bullet(slide, ["动画或滑动时画面闪烁", "黑底白字整体观感偏重", "部分碰撞和素材位置需要实机调试"], 148, 278, 320, { size: 22, gap: 52 });
    rect(slide, 635, 168, 430, 280, "#FFFFFF", C.faint, 20);
    text(slide, "优化方式", 675, 208, 300, 34, { size: 31, bold: true, color: C.green });
    bullet(slide, ["改为白底黑字浅色主题", "只刷新变化区域", "游戏区和信息栏分区绘制", "图片资源数字化后直接绘制"], 678, 278, 330, { size: 22, gap: 45, color: C.green });
  }

  {
    const slide = slideBase(presentation, "AI 使用", "大模型参与了代码、调试和资源处理的多个环节", 13);
    const cards = [
      ["代码辅助", "拆分游戏模块\n实现游戏逻辑\n整理显示接口", C.red],
      ["调试辅助", "分析闪烁现象\n调整碰撞判定\n定位动画位置", C.blue],
      ["美术辅助", "生成游戏封面\n处理图片数字化\n统一视觉风格", C.green],
    ];
    cards.forEach((card, i) => {
      const x = 95 + i * 355;
      rect(slide, x, 180, 300, 330, C.panel, C.faint, 22);
      rect(slide, x + 28, 210, 54, 54, card[2], "none", 16);
      text(slide, String(i + 1), x + 28, 221, 54, 30, { size: 26, color: "#FFFFFF", bold: true, align: "center" });
      text(slide, card[0], x + 100, 220, 160, 34, { size: 29, bold: true });
      text(slide, card[1], x + 32, 310, 236, 130, { size: 25, color: C.dark, leading: 1.45 });
    });
  }

  {
    const slide = slideBase(presentation, "开发体会", "AI 能加快迭代，但最终效果仍依赖实机验证", 14);
    rect(slide, 110, 170, 455, 310, "#FFFFFF", C.faint, 22);
    text(slide, "优势", 150, 212, 220, 38, { size: 34, bold: true, color: C.green });
    bullet(slide, ["快速把想法转化为代码", "适合界面和逻辑快速试错", "能辅助整理结构和定位问题"], 152, 285, 350, { size: 23, gap: 54, color: C.green });
    rect(slide, 640, 170, 455, 310, "#FFFFFF", C.faint, 22);
    text(slide, "局限", 680, 212, 220, 38, { size: 34, bold: true, color: C.red });
    bullet(slide, ["引脚和屏幕表现必须实测", "操作手感需要反复调整", "AI 代码仍需编译和硬件验证"], 682, 285, 350, { size: 23, gap: 54, color: C.red });
  }

  {
    const slide = slideBase(presentation, "扩展方向", "硬件音频是最自然的下一步", 15);
    const items = [
      ["硬件音频", "PWM 蜂鸣器 / I2S DAC + 功放 + 喇叭", C.red],
      ["最高分保存", "使用 NVS 保存分数、设置和历史记录", C.blue],
      ["电池供电", "锂电池、电量采样和低电量提示", C.green],
      ["震动反馈", "碰撞、得分和失败时提供触觉提示", C.yellow],
      ["设置菜单", "音量、亮度、主题和清除存档", C.dark],
    ];
    items.forEach((item, i) => {
      const y = 154 + i * 78;
      rect(slide, 112, y, 156, 46, item[2], "none", 12);
      text(slide, item[0], 112, y + 10, 156, 24, { size: 20, color: "#FFFFFF", bold: true, align: "center" });
      text(slide, item[1], 305, y + 10, 730, 26, { size: 23 });
    });
  }

  {
    const slide = slideBase(presentation, "总结", "项目已形成可演示、可维护、可继续扩展的游戏机原型", 16);
    image(slide, "全览图.jpg", 690, 156, 410, 410, { radius: 28 });
    bullet(slide, [
      "完成 ESP32-S3 简易游戏机硬件与 PCB 设计",
      "实现主菜单和 Dino、2048、Snake 三款游戏",
      "优化显示刷新、界面风格和图形资源表现",
      "AI 大模型在代码实现、调试和素材处理上提供辅助",
      "后续可继续扩展音频、电池、震动和更多游戏",
    ], 108, 178, 490, { size: 24, gap: 63, color: C.red });
  }

  const pptx = await PresentationFile.exportPptx(presentation);
  await pptx.save(FINAL);

  for (let i = 0; i < presentation.slides.count; i += 1) {
    const slide = presentation.slides.getItem(i);
    const blob = await presentation.export({ slide, format: "png" });
    const bytes = Buffer.from(await blob.arrayBuffer());
    await fs.writeFile(path.join(PREVIEW, `slide-${String(i + 1).padStart(2, "0")}.png`), bytes);
  }

  console.log(FINAL);
}

main().catch((error) => {
  console.error(error);
  process.exit(1);
});
