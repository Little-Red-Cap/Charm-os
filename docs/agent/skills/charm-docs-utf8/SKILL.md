# charm-docs-utf8

用途：
- 用于文档编辑与审阅时的编码一致性保障。
- 避免中文文档因 PowerShell 编码导致乱码。

适用场景：
- 修改/新增中文文档
- review 文档内容时出现乱码
- 需要在 PowerShell 中正确读写 UTF-8

不适用场景：
- 代码实现或构建相关问题

依赖规则：
- `../../rules/collaboration.md`

参考：
- `docs/project/tooling/Powershell设置utf8.md`

---

## 工作约定

- 读写中文文档时使用 UTF-8 编码
- PowerShell 读取文件时显式指定 `-Encoding utf8`
- 写入文件时显式指定 `-Encoding utf8`

---

## 输出要求

- 说明已采用 UTF-8 读取/写入
- 若发现乱码，提示按 `Powershell设置utf8.md` 修复环境
