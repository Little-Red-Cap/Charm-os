# daplink tools

## cmsis_dap_smoke.py

最小 CMSIS-DAP HID 联调脚本，覆盖：

1. `DAP_Info`（Vendor/Product/Serial/ProtocolVersion/FirmwareVersion）
2. `DAP_Connect`
3. `DAP_HostStatus`（Connected/Running LED）
4. `DAP_SWJ_Clock`
5. `DAP_Transfer` 读取 `DP_IDCODE`
6. `DAP_TransferBlock` 读取 `DP_IDCODE`
7. `DAP_ResetTarget`
8. `DAP_Disconnect`

### 依赖

```bash
pip install hidapi
```

### 用法

```bash
python Examples/project/daplink/tools/cmsis_dap_smoke.py
```

可选参数：

```bash
python Examples/project/daplink/tools/cmsis_dap_smoke.py --vid 0xCAFE --pid 0x4001 --timeout-ms 1000 --swj-clock-hz 1000000
```

跳过复位：

```bash
python Examples/project/daplink/tools/cmsis_dap_smoke.py --skip-reset
```
