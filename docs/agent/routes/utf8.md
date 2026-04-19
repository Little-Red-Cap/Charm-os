# UTF-8 Route

## 适用场景

- 文档乱码修复
- 编码检查
- 需要判断“先标记还是先修复”的文档损坏问题

## 最短阅读顺序

1. [`../../documentation_maintenance.md`](../../documentation_maintenance.md)
2. [`../skills/charm-docs-utf8/SKILL.md`](../skills/charm-docs-utf8/SKILL.md)
3. 目标目录下的 `README.md`

## 先不要做什么

- 不要对语义不确定的乱码正文做大面积猜写。
- 不要把损坏文档继续放在推荐入口里。
- 不要只修文件编码，不验证链接和文义。

## 完成前自检

- UTF-8 严格解码通过。
- 本地链接有效。
- 如果暂时无法恢复，入口层已明确标注“待恢复 / 不作为首选入口”。
