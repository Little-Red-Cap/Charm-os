# VSF迁移：Service/Shell/Module 规划草案 (Draft)

## Service 选型（MVP）
- service_ring_buffer: 固定容量 ring buffer
- service_stream: 统一流接口 + write(string_view)
- service_trace: 轻量 trace buffer
- service_json: 最小 JSON writer

## Shell 选型（MVP）
- shell_core: Errc/Result + Console
- shell_time: time/delay 接口
- shell_stdio: 最小 write 接口

## Module/XIP（MVP）
- module_core: 镜像头/段描述
- module_loader: 加载与定位（stub）
- module_link: 符号表与解析（stub）

## 示例
- Draft/Examples/vsf_service_shell: Service + Shell 最小用法示例

## 约定
- 目录命名: service_*, shell_*, module_*。
- 示例不混主线, 统一放 Draft/Examples/vsf_* 或 Draft/Examples/hal_*。

