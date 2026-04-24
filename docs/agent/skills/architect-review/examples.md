# Architect Review Examples

本文件提供 Charm 项目架构评审任务的输出范式示例。
目标：体现“结构优先、边界优先、装配优先”的判断路径。

相关文件：
- `SKILL.md`
- `../../rules/charm-architecture.md`
- `../../rules/embedded-modern-cpp.md`

---

# 示例 1：能力归属判断

## 输入场景
用户提案：

> 把输入采样直接放进 UI 模块。

## 参考输出

### 当前问题
输入采样的能力归属与边界划分。

### 关键判断依据
- 输入采样是 IO 层职责
- UI 只消费语义事件
- 统一输入链路要求通过 `input.service` 装配

### 推荐方案
- 输入采样放 IO/Service 层
- UI 通过 RouterIntentQueue 获取语义事件

### 不推荐方案
- UI 直接轮询硬件：会破坏分层与装配纪律

---

# 示例 2：装配路径评审

## 输入场景
用户改动：

> 在 main.cpp 里手写初始化顺序。

## 参考输出

### 当前问题
绕过 `init.graph` 装配路径。

### 关键判断依据
- 统一装配是架构硬约束
- 入口拼装会导致隐式依赖

### 推荐方案
- 将能力注册为 `init.node`
- 通过 `CoreSystemChain + BoardChain` 装配

### 不推荐方案
- 手写入口顺序：会破坏可裁剪与可验证性
