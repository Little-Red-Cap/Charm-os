<div align="center">

# ? Charm ?

**C++26 Modules �� Zero-alloc �� constexpr config �� Type-level FSM**

[![License: MIT](https://img.shields.io/badge/License-MIT-green.svg?style=flat-square)](LICENSE)
[![C++26](https://img.shields.io/badge/C%2B%2B-26-blue.svg?style=flat-square)](https://en.cppreference.com/w/cpp)
<br>
[![CLang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-clang.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)
[![CLang Build Status](https://github.com/Little-Red-Cap/Charm-os/actions/workflows/build-arm-none-eabi.yml/badge.svg)](https://github.com/Little-Red-Cap/Charm-os/actions)

> ͳһ��ģ�黯�ܹ�ƴͼ������/ϵͳ/IO/ý��/UI һ�廯��֯����ȡ��塣

</div>

---

## ?? Ϊʲô�� Charm��
- **ģ�黯����**��ȫ�� C++ Modules���߽�����������ϡ��ɲü��
- **����ڴ�**��std::array/std::span + �����ڹ滮��Ƕ��ʽ�Ѻá�
- **��������**��constexpr/consteval + concepts�����ü�У�飬������Լ����
- **��ѡ��ǿ**���¼�ȥ��/����/�ϲ������ȼ�������trace/alert/stats�����������

## ?? Ŀ¼����
- `Modules/core/` ���� util/trace/service/alg
- `Modules/system/` ���� kernel/modulex/boot
- `Modules/io/` ���� hal/port/fs/shell/out
- `Modules/media/` ���� audio
- `Modules/ui/ink/` ���� Charm-ink UI
- `Modules/ui/vivid/` ���� Charm-vivid UI
- `Modules/platform/` ���� ƽ̨���䣨win/δ�� MCU��
- `Modules/io/usb/` ���� USB �豸�˹Ǽ�����ݰ�
- `Examples/` ���� ʾ�����̣��ں�/boot/audio/fs/shell/service/alg/hal��
- `docs/` ���� �ܹ���Э���ĵ�
- `Draft/` ���� �ƻ�/�ݰ����ɱ䶯��

## ?? ����ƴͼ�����㼶��

**Foundation**
- util/expected/units
- service��ring_buffer/pool/trace/stream/json
- out����ʽ������־ͳһͨ·

**IO / HAL**
- hal��UART/SPI/I2C/GPIO/IRQ/Timer/Clock
- input��RawInputEvent/Sampler/Intent/����
- fs��VFS + RamFs + BlockFs + FatFs + MAL
- usb��common/device/driver + CDC/UAC/MSC �Ǽ�
- proto��X/YModem������С demo��

**Kernel / System**
- EDA �¼���������ȹǼ�
- �豸ģ��/registry/driver ��������
- Power/Low-Power �������

**Domains**
- Audio��sink/pipeline/player + SDL3 ��֤��·
- UI��Ink/Vivid ����Ǩ����ͳһ������/trace/�㷨���ã�
- Bootloader���ֲ�/�׶��ĵ� + ����Э�����

## ?? ������ڣ�������������֣�

### Audio
1. ���ĵ���`Modules/media/audio/audio_design.md`
2. ��ʵ�֣�`Modules/media/audio/`��player/sink/decoder/fifo/src/convert��
3. ��ʾ����`Examples/audio/sdl3_wav_demo`

### Kernel
1. ���ĵ���`Modules/system/kernel/docs/`
2. ��ʵ�֣�`Modules/system/kernel/`
3. ��ʾ����`Examples/kernel/windows`

### FS/VFS
1. ���ĵ���`docs/fs_vfs_mount_rules.md`
2. ��ʵ�֣�`Modules/io/fs/`
3. ��ʾ����`Examples/fs/`

### Shell/Service
1. ���ĵ���`docs/architecture_overview.md`
2. ��ʵ�֣�`Modules/io/shell/`��`Modules/core/service/`
3. ��ʾ����`Examples/shell/`��`Examples/service/`

### ModuleX
1. ���ĵ���`Modules/system/modulex/ModuleX_��ʽ�ݰ�.md`
2. ��ʵ�֣�`Modules/system/modulex/`
3. ��ʾ����`Examples/shell/service_shell`

### USB
1. ���ĵ���`docs/usb_arch_plan.md`
2. ��ʵ�֣�`Modules/io/usb/`
3. ��ʾ����`Examples/usb/usb_cdc_minimal`

### UI/Vivid
1. ���ĵ���`Modules/ui/vivid/ARCHITECTURE.md`��`Modules/ui/vivid/FEATURES.md`
2. ��ʵ�֣�`Modules/ui/vivid/`
3. ��ʾ����`Examples/project/scope`

## ?? ���� Demos��Windows��
- **M0** `Examples/kernel/windows/main.cpp` ��kernel + timer + event queue
- **M1** `Examples/kernel/windows/main_m1.cpp` ��sync + IPC
- **M2** `Examples/kernel/windows/main_m2.cpp` ��thread + blocking
- **M3** `Examples/kernel/windows/main_m3.cpp` ��trace + stats
> ʵ���� demo ���ⲻ�����ߣ����ֺ��Ĵ�����

### ? ���ٹ�����Windows��
```bash
cmake -S Examples/kernel/windows -B Examples/kernel/windows/build -G Ninja
cmake --build Examples/kernel/windows/build
Examples/kernel/windows/build/os-example-win.exe
```

## ?? ��ѡģ�飨Ĭ�Ϲرգ�
- ��̬ע�᣺`kernel.dynamic_registry` / `kernel.task_pool` / `kernel.task_auto`
- ��̬���ȶ��У�`kernel.event_queue_list`
- �ɹ۲��ԣ�`kernel.trace`��alert/replay��JSON ���
- �¼����ԣ�dedup / debounce / coalesce / boost����������

## ?? MCU Demo
- λ�ã�`Draft/Examples/stm32f103c8`����Ǩ�ƣ�
- ���أ�`-DCHARM_MCU_KERNEL_DEMO=ON`��preset Ĭ�Ͽ����
- ��ڣ�`main_mcu_stub.cpp`��`application()` -> `run_auto`��
- ƽ̨�󶨣�`Core/Src/kernel.port.stm32.cpp`

### ?? MCU ����ʾ��
```bash
cmake --preset Release -S Draft/Examples/stm32f103c8 -B Draft/Examples/stm32f103c8/build
cmake --build Draft/Examples/stm32f103c8/build --target vivid-example-stm32
```
> ��¼���弶����������ִ�С�

## ?? ģ���嵥���ܹ��ࣩ
- HAL��`Modules/io/hal/*`
- Service��`Modules/core/service/*`
- Shell��`Modules/io/shell/*`
- Module/XIP��`Modules/system/modulex/*`
- FS��`Modules/io/fs/*`

����ʾ����Examples����
- `Examples/hal/hal_demo`��HAL �ӿ���Сʾ��
- `Examples/shell/service_shell`��Service + Shell + Module ʾ��
- `Examples/service/service_core`��Service ����ʾ��
- `Examples/service/service_ds_demo`��Service DS ʾ��
- `Examples/fs/fs_demo`��VFS + RAMFS ����
- `Examples/fs/fs_vfs_demo`��VFS ���ʾ��
- `Examples/alg/alg_demo`���㷨/ѹ��ʾ��
- `Examples/boot/bootloader_demo`��bootloader ʾ��
- `Examples/audio/sdl3_wav_demo`��SDL3 ��Ƶʾ��
- `Examples/usb/usb_cdc_minimal`��CDC ��Сö��ʾ��

### ʾ������
```bash
# HAL demo
cmake -S Examples/hal/hal_demo -B Examples/hal/hal_demo/build -G Ninja
cmake --build Examples/hal/hal_demo/build
Examples/hal/hal_demo/build/hal-demo

# Service/Shell/Module demo
cmake -S Examples/shell/service_shell -B Examples/shell/service_shell/build -G Ninja
cmake --build Examples/shell/service_shell/build
Examples/shell/service_shell/build/service-shell-demo

# Service core demo
cmake -S Examples/service/service_core -B Examples/service/service_core/build -G Ninja
cmake --build Examples/service/service_core/build
Examples/service/service_core/build/service-core-demo

# FS demo
cmake -S Examples/fs/fs_demo -B Examples/fs/fs_demo/build -G Ninja
cmake --build Examples/fs/fs_demo/build
Examples/fs/fs_demo/build/fs-demo

# Alg demo
cmake -S Examples/alg/alg_demo -B Examples/alg/alg_demo/build -G Ninja
cmake --build Examples/alg/alg_demo/build
Examples/alg/alg_demo/build/alg-demo

# Service DS demo
cmake -S Examples/service/service_ds_demo -B Examples/service/service_ds_demo/build -G Ninja
cmake --build Examples/service/service_ds_demo/build
Examples/service/service_ds_demo/build/service-ds-demo

# Bootloader demo
cmake -S Examples/boot/bootloader_demo -B Examples/boot/bootloader_demo/build -G Ninja
cmake --build Examples/boot/bootloader_demo/build
Examples/boot/bootloader_demo/build/bootloader-demo

# SDL3 WAV demo
cmake -S Examples/audio/sdl3_wav_demo -B Examples/audio/sdl3_wav_demo/build -G Ninja
cmake --build Examples/audio/sdl3_wav_demo/build
Examples/audio/sdl3_wav_demo/build/sdl3-wav-demo <file.wav>
```

## ? ����״̬
- Windows ���� M0�CM3����ͨ��
- HAL demo����ͨ����[hal_demo] ok��
- Service/Shell/Module demo����ͨ����[shell] / [shell_time] / [module_demo]��
- Service core demo����ͨ����[distbus] / [service_core] ok��
- FS demo���ѹ�������������֤��
- STM32������ͨ��������¼��֤��

## ?? �ĵ�
- �ܹ�������`docs/architecture_overview.md`
- ����ֲ㣺`docs/input_layering_decision.md`
- Э���淶��`docs/��Э���ڴ���淶��.md`
- Э����֪��`docs/���ִ� C++ ��Ƭ������Э����֪��.md`
- �ƽ���ֹ���`docs/�ƽ�TODO��ֹ�.md`��`docs/refactor_todo_ownership.md`
- ����ĵ���Audio=`Modules/media/audio/audio_design.md`��HAL=`Modules/io/hal/charm_hal_design.md`��FS=`docs/fs_vfs_mount_rules.md`��Shell=`Modules/io/shell/`��Service=`Modules/core/service/`��ModuleX=`Modules/system/modulex/ModuleX_��ʽ�ݰ�.md`��Kernel=`Modules/system/kernel/docs/`

## ?? ��ʾ��ģ�壨CMake��

���ٴ�����ʾ�����̣�
- ģ�壺`Examples/cmake/ExampleTemplate.cmake`
- �Ƽ��÷�������ʾ�� `CMakeLists.txt` �� `include(...)`��Ȼ����� `charm_example_*` ϵ�к���
- SDL3 ͳһ��ڣ�`cmake/SDL3.cmake`������ `find_package`�����˵� `Examples/ThirdParty/SDL3`��

## ?? ·��ͼ��ժҪ��
- **����**��MCU ������֤��HAL MVP��GPIO/UART/Timer����time/sleep facade ��Сʵ��
- **����**��module loader/XIP �ݰ���RT facade���߳� API ��װ����VFS ���ƣ�Ŀ¼/��飩
- **Զ��**��Facade/Arduino ���ḻ��ģ���Ȳ��/ǩ����USB/TCPIP/FS ����������롢FatFS/FlashFS ����

## ? δ�� TODO�����߱ջ���
- **�����ջ�**���̼�ǩ��/��Կ������汾������ع����ԡ�A/B �������ԡ�OTA/����ͨ��
- **ƽ̨�ջ�**��ͳһ����ģ�͡�BSP ��֯�ṹ���弶ģ�塢��ֲָ�ϡ���С�����嵥
- **���бջ�**�����/���/�쳣�ָ�������ת����������ָ������Ϲ���
- **�洢�ջ�**��VFS ���ز��ԡ�����/һ���ԡ�������־������
- **ͨ�űջ�**��ͳһ IO/transport ����RPC/��ϢЭ�顢����ͨ��
- **��ȫ�ջ�**��ֻ������֤����С����������Կ��װ�ӿ�
- **���̻�**�����ּ�/ģ�幤�̡�����ͼ�������ĵ���PC/MCU smoke ����

## ?? ���
MIT���� LICENSE����


