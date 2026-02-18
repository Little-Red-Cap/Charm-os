VSF scan summary:

component/av/audio
component/crypto/hash/crc
component/debugger/{nulink,segger}
component/fs/driver
component/input/{driver,protocol}
component/mal/driver
component/misc/led_scan
component/scsi/driver
component/tcpip/{netdrv,protocol,socket}
component/ui/{disp,menusys,tgui}
component/usb/{common,device,driver,host,utils}
component/script/python

迁移优先级建议（从高到低）：
1) component/input（对齐 RawInputEvent/协议层）
2) component/mal + component/fs + component/scsi（块设备/文件系统桥接）
3) component/usb（Device 优先：CDC/MSC/UAC）
4) component/ui（结构参考，不必直接迁移）
5) component/tcpip（参考为主，复杂度高）
