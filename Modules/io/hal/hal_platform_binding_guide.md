# HAL ƽ̨��ָ�� (Draft)

## Ŀ��
Ϊÿ��ƽ̨�ṩ��С HAL �󶨣�ʵ�� hal_* �ӿڣ�����Ⱦ����ģ�顣

## ��λ��
- ƽ̨ʵ�ַ��� Draft/Examples/targets ��δ�� charm-hal �Ӳֿ��С�
- ���� Modules ��ֻ����ӿ�����

## ��Ҫʵ�ֵĽӿڣ�MVP��
- system.clock: TimeSource (board_caps)
- hal_irq: IrqGuard / IrqController
- hal_gpio: GpioDriver
- hal_uart: UartDriver
- hal_timer: TimerDriver

## Լ��
- ����/��ʱ�������ĵ�����
- ISR ���������߳������ĵĵ���Լ��������ȷ��
- ���֧�ֹ��ܣ��뷵�� Status::unsupported��

## ʾ��
- Windows stub: Draft/Examples/hal_demo/main.cpp
- STM32 stub: �Ƽ��� Draft/Examples/stm32f103c8/Core/Src/hal_*.cpp


