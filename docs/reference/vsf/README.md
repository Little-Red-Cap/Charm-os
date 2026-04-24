# VSF 参考入口

本目录收纳对 VSF 的结构、组件和专题映射整理，用来做对照和借鉴，不直接等同于 Charm 当前实现。

## 当前状态

- 这些笔记主要形成于 Charm 初期借鉴 VSF 的阶段。
- 当前保留它们，更多是为了说明某些架构概念的历史来源，而不是指导新功能按 VSF 路线继续扩张。
- 如果相同结论已经被 Charm 自己的契约、总览或专题入口吸收，这里的条目可以继续收缩、归档，甚至删除。

如果你是第一次进入参考资料目录，建议先回到：

- [`../README.md`](../README.md)

## 建议阅读顺序

1. [`vsf_comparison.md`](vsf_comparison.md)
2. [`vsf_component_scan.md`](vsf_component_scan.md)
3. [`vsf_storage_map.md`](vsf_storage_map.md)
4. [`vsf_tcpip_map.md`](vsf_tcpip_map.md)
5. [`vsf_usb_map.md`](vsf_usb_map.md)

## 使用提醒

- 这些材料的用途是“对照 / 借鉴 / 归纳”，不是“直接照搬”。
- 如果某条结论最终进入 Charm 的现行设计，应回写到上位架构或专题入口，而不是只停留在这里。
- 仓库内不再假定旧的 VSF 工作目录写法仍然存在；如果需要对照 VSF 源码，请按对应的 `source/...` 路径在本地镜像或上游仓库中查找。
