---
name: "weekly-video-script"
description: "Generates a weekly progress video script (~1 min) with Chinese/English voiceover and storyboard. Invoke when user asks for a video script for teacher/instructor progress reporting, or mentions filming a weekly report video."
---

# Weekly Video Script Generator

此 skill 用于基于项目 DailyProgress、GDD 和 TDD 文档，自动生成用于向老师/导师汇报工作进度的周报视频脚本。脚本包含中英文双语口播内容和分镜表，时长控制在 1 分钟以内。

## 适用场景

- 用户说 "生成视频脚本" 或 "给我一个汇报视频脚本"
- 用户需要拍摄周进度汇报视频向老师汇报
- 用户需要中英文双语的口播内容

## 工作流程

### 1. 收集信息

在生成脚本之前，需要读取以下文件：

- **`DailyProgress.md`** 和 **`DailyProgress_Chinese.md`**：获取近期的每日工作内容
- **`GDD.md`** 和 **`GDD_Chinese.md`**（可选）：了解游戏整体设计，用于脚本开头介绍项目背景
- **`TDD.md`** 和 **`TDD_Chinese.md`**（可选）：了解技术架构，补充技术亮点描述

### 2. 确定日期范围

- 从 `DailyProgress.md` 中读取所有已有记录的日期
- 自动选取最近 2-3 天的工作内容作为脚本主体
- 在脚本文件头部和末尾的"工作内容摘要"区域明确标明日期范围

### 3. 生成视频脚本

生成的内容写入 `weeklyVideoScript.md`（项目根目录），包含以下结构：

#### 文件头部

```markdown
# CityFlow 周进度汇报视频脚本

**涵盖工作日期：YYYY-MM-DD ~ YYYY-MM-DD**

**视频时长：约 55-60 秒**
```

#### 分镜脚本

使用表格格式，包含四列：

| 时间 | 画面内容 | 拍摄/演示建议 |
|---|---|---|
| 0:00-0:05 | 片头 | 标题卡 |
| 0:05-0:15 | 游戏概念介绍 | GDD 概念图 / 动画 |
| 0:15-0:30 | 编辑器实操展示 | 录制 UE5 编辑器视口 |
| 0:30-0:45 | 代码结构展示 | IDE 截图 |
| 0:45-0:55 | 蓝图/材质展示 | 蓝图编辑器截图 |
| 0:55-1:00 | 结尾 | PPT 文字页 |

#### 中文版口播

- 首先用 2-3 句话介绍游戏核心概念（从 GDD 提取）
- 然后按照日期分组，逐日介绍工作内容（从 DailyProgress 提取）
- 最后以"下一步计划"收尾
- 字数控制在 250-300 字（约 55 秒语速）

#### English Voiceover

- 与中文版内容对应，翻译为自然流畅的英文
- 字数控制在 160-180 词（约 55 秒语速）
- 保持相同的信息结构：游戏介绍 → 工作内容 → 下一步计划

#### 视频制作建议

- 录制工具推荐
- 剪辑流程建议
- 字幕建议

#### 工作内容摘要

- 列出脚本涵盖日期范围内的完整工作项（中英文对照）
- 按日期分组，方便拍摄时参考

### 4. 口播脚本编写原则

- **信息密度高**：每句话承载一个关键成果，避免废话
- **口语化**：使用自然的口语表达，适合朗读
- **技术术语适度**：保留核心技术名词（如 GridVisualizer、L-system、Enhanced Input），但用通俗语言解释其作用
- **双语一致性**：中英文版本信息结构完全对应，不是逐字翻译而是意译
- **节奏感**：脚本中自然分段，方便录制时换气停顿

## 示例

参考项目根目录中的 `weeklyVideoScript.md` 文件，该文件是根据 2026-05-26 ~ 2026-05-27 的工作内容生成的标准格式样例。
