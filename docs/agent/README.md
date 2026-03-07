# Agent 协作规范

本目录用于团队协作一致化：prompts/rules/skills 的统一版本。

## 使用方式
1) 在仓库内维护与评审（版本可追踪）
2) 需要在本机生效时，复制到个人配置目录：
   - Windows 示例：`C:\Users\<你>\.codex\skills\`（skills）
   - 其他规则/模板可按团队约定放入 `.codex` 对应位置

## 同步示例（Windows）
```powershell
# 同步 skills
Copy-Item -Recurse -Force docs\agent\skills\* $env:USERPROFILE\.codex\skills\

# 同步 prompts/rules（如有固定目录约定）
Copy-Item -Force docs\agent\prompts.md $env:USERPROFILE\.codex\prompts.md
Copy-Item -Force docs\agent\rules.md $env:USERPROFILE\.codex\rules.md
```

## 文件说明
- `prompts.md`：统一提示模板（协作对齐用）
- `rules.md`：硬规则/禁区（不可违反）
- `skills/`：现有技能与用途清单

## 推荐流程
1) 需求描述清楚“目标/约束/输入/输出”
2) 复杂改动先给方案再落地
3) 构建或验证规则提前说明
4) 分批提交，小步可回滚

## 维护原则
- 只保留“协作必须知道”的内容
- 不写大段空话，尽量是可执行条目
- 规则变更先在仓库 review，再同步到个人目录
