# Review Workflow

本文件定义 Charm 项目中 review 类任务的标准执行路径。  
目标是让 AI 在面对代码审查、PR 审查、实现合规性检查时，采用一致、可重复的流程，而不是临时发挥。

相关文件：
- `../rules/README.md`
- `../rules/charm-architecture.md`
- `../rules/embedded-modern-cpp.md`
- `../rules/collaboration.md`
- `../skills/code-review/SKILL.md`
- `../skills/code-review/checklist.md`
- `../templates/review-output.md`

---

# 1. 适用范围

本 workflow 适用于以下任务：

- review 某段代码
- review 某个模块
- review 某个 PR
- 判断某个实现是否符合 Charm 项目规则
- 判断某个改动是否存在阻断问题、重要问题或优化空间

不适用于：

- 从零生成代码
- 讨论某个能力应放在哪一层
- 纯协作方式对齐
- 不针对具体实现的纯技术哲学讨论

---

# 2. 总体原则

review 任务必须遵守以下原则：

- 先判断任务对象，再展开检查
- 先查硬性违规，再查设计退化，再看优化空间
- 先使用项目规则，再使用经验判断
- 不因“常见写法”而放宽 Charm 项目要求
- 信息不足时要明确说明，而不是假装确认

---

# 3. 执行顺序

## 第一步：识别 review 对象
先判断当前任务属于哪一类：

- 单个函数
- 单个类 / 模块
- 某个适配层
- 某个初始化链路
- 某个 PR / 多文件改动
- 架构级改动

若对象本质上已经超出实现 review，而属于“能力归属 / 分层 / 装配”问题，应切换到 `architect-review`，而不是硬按 code-review 处理。

---

## 第二步：加载规则
默认按以下顺序理解规则：

1. `../rules/charm-architecture.md`
2. `../rules/embedded-modern-cpp.md`

若任务中还涉及：
- 设计未定
- 方案分歧
- 需要先讨论再判断

则再补充：

3. `../rules/collaboration.md`

说明：
- `charm-architecture.md` 用于项目具体约束判断
- `embedded-modern-cpp.md` 用于技术方向与设计退化判断
- `collaboration.md` 用于处理不确定性和沟通方式

---

## 第三步：读取技能说明
读取：

- `../skills/code-review/SKILL.md`

目的是明确：
- review 的目标
- 问题分级方式
- 输出组织方式
- 对妥协代码的处理方式

---

## 第四步：使用 checklist 做系统检查
读取并使用：

- `../skills/code-review/checklist.md`

检查顺序建议为：

### 4.1 阻断项
先查：
- 动态分配
- 异常 / RTTI
- 初始化绕过
- 分层破坏
- IO 非阻塞纪律破坏
- 错误模型破坏
- 时间源破坏

### 4.2 重要问题
再查：
- 类型语义弱
- 运行期分支代替编译期建模
- `void*` / 裸指针 / 隐式约定
- 模块组织退化
- 输出与日志路径不一致

### 4.3 优化项
最后再看：
- 命名
- 抽象边界
- 成本可解释性
- 隔离质量
- 一致性提升空间

---

## 第五步：判断是否信息不足
如果缺少关键上下文，应先停下来判断是否足以得出结论。

典型缺失信息包括：

- 所属层级不明
- 是否位于实时路径不明
- 是否通过 `init.graph` / `io.registry` / RuntimeContext 接入不明
- 返回类型上下文不明
- 时间源和错误模型上下文不明

若缺失信息会影响判断，应明确输出：

- 当前能判断什么
- 当前不能判断什么
- 还需要哪些上下文
- 暂时能给出的风险判断是什么

---

## 第六步：套用输出模板
使用：

- `../templates/review-output.md`

输出时默认结构为：

1. 总体结论
2. 阻断问题
3. 重要问题
4. 优化建议
5. 暂未确认但值得关注的点
6. 总结建议

---

# 4. 分流规则

## 应切换到 architect-review 的情况
若 review 过程中发现问题核心是：

- 这个能力应放哪一层
- 这个模块是否该存在
- 是否应该进入 `CoreSystemChain` / `BoardChain` / `extra nodes`
- 装配路径是否合理
- 系统边界是否被破坏

则应转到：

- `../skills/architect-review/SKILL.md`

而不是继续只做局部实现 review。

## 应补充 collaboration 的情况
若当前任务存在：

- 多方案未定
- 关键设计未对齐
- 人的偏好会影响方案选择

则应补充使用：

- `../rules/collaboration.md`

并先把讨论对齐，而不是急着下结论。

---

# 5. 输出风格要求

review 输出应保持：

- 直接
- 清晰
- 高标准
- 可执行
- 有依据

避免：

- 空泛评价
- “看起来还行”
- 没有规则依据的主观断言
- 只提问题不提方向

---

# 6. 最终检查

在输出前，AI 应自查：

- 是否先指出了最重要的问题
- 是否把阻断问题和优化建议混在一起
- 是否说明了为什么是问题
- 是否指出了推荐修正方向
- 是否在信息不足时保持诚实
- 是否错误地按通用 C++ 项目标准放宽了 Charm 约束