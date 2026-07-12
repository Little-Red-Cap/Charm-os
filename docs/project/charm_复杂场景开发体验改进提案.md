# 复杂场景开发体验提案摘要

> **文档状态：`exploration`（未冻结）**

完整提案见 [`../archive/project-proposals-v0/charm_复杂场景开发体验改进提案.md`](../archive/project-proposals-v0/charm_复杂场景开发体验改进提案.md)。它不是当前系统契约或排期。

## 保留问题清单

- 入口、启动链和板级事实容易分散；
- init 装配粒度与复杂场景的复用粒度不总是一致；
- 实验代码、资源约束和可观测性缺少统一的项目工作流；
- Host、MCU 和真实板之间的验证边界需要更清楚。

## 当前状态

提案中出现的 `Scenario`、`Bundle`、`Resource Class`、`Bringup Recipe`、`Host Parity` 等仍是候选名词，没有共同实现或准入裁决。不能用提案中的对象层替换当前代码事实。

## 重新推进条件

从一个真实重复痛点提取最小改动，并用构建入口、源码和 smoke 证明收益；不能同时引入多个新对象层。
