---
name: "daily-progress"
description: "基于聊天上下文和工作区代码/资产变更，总结当日工作进度并记录到 DailyProgress.md（英文），同时翻译为中文同步到 DailyProgress_Chinese.md。当用户说「记录工作进度」「更新进度」「daily progress」或完成一天的工作需要记录时调用。"
---

# 每日工作进度记录与同步

此 skill 用于在完成一天或一个阶段的工作后，根据聊天上下文和实际代码/资产变更，总结当日工作进度，并保持中英文版本同步。

## 适用场景

- 用户说 "记录工作进度" 或 "更新进度" 或 "daily progress"
- 完成了一天的工作，需要将今天的实现情况记录到 DailyProgress.md
- 需要同时维护中英文两份进度文档

## 工作流程

### 1. 收集信息

在记录进度之前，需要收集以下信息：

- **聊天上下文**：回顾本次会话中用户实现了哪些功能、讨论了哪些设计、做出了哪些决策
- **代码变更**：使用 `grep` / `SearchCodebase` / `LS` 等工具了解本次涉及的核心文件，了解：
  - 新增/修改了哪些类、函数、接口
  - 实现了哪些功能模块
  - 修复了哪些问题
- **资产变更**：查看是否新增或修改了 Blueprint、材质、网格等资产文件

### 2. 整理进度内容（英文）

根据收集到的信息，以简洁的要点形式总结今日工作：

- 每条要点一行，使用 `- ` 开头
- 每条描述一个独立的工作项（新增功能、修复问题、重构等）
- 内容简洁，每条不超过一句话
- 不包含与工作无关的聊天内容

### 3. 写入 DailyProgress.md

**格式规范：**

```markdown
## YYYY-MM-DD

- Work item 1
- Work item 2
- Work item 3
```

**规则：**
- 使用 `## YYYY-MM-DD` 作为日期标题（使用今天的日期）
- 如果是同一天再次更新，追加到同一天标题下的末尾，不重复创建标题
- 如果当天尚无记录，在文件末尾追加新的日期标题和内容
- 保持文件现有的所有历史记录不变

### 4. 翻译为中文

- 将整理好的英文进度逐条翻译为中文
- 保持同样的要点格式
- 翻译应准确、自然，不添加额外信息

### 5. 写入 DailyProgress_Chinese.md

- 使用与 `DailyProgress.md` 完全相同的格式和规则
- 写入中文版本的内容
- 确保两份文件的日期和条目一一对应

## 示例

假设今天实现了 GridVisualizer 模块并修复了一个渲染 Bug：

**DailyProgress.md：**

```markdown
## 2026-05-27

- Implemented GridVisualizer module for rendering grid lines in the editor
- Added configurable grid line thickness and color parameters
- Fixed rendering bug where grid lines disappeared at certain camera angles
```

**DailyProgress_Chinese.md：**

```markdown
## 2026-05-27

- 实现了 GridVisualizer 模块，用于在编辑器中渲染网格线
- 添加了可配置的网格线粗细和颜色参数
- 修复了特定摄像机角度下网格线消失的渲染问题
```
