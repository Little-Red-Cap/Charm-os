# VSF USB 结构对照

> status: `reference`
>
> scope: historical VSF device/host/controller/class decomposition

This note records the VSF shapes that informed early Charm USB work. It does
not describe current Charm status or prescribe a migration roadmap. Current
contracts and tests start at [`docs/usb/README.md`](../../usb/README.md).

## VSF Decomposition

| VSF area | Responsibility |
|---|---|
| `usb/common` | descriptor and class protocol types |
| `usb/device` | device core, configuration/interface tables and class drivers |
| `usb/host` | enumeration, devices, pipes/URBs, hubs and host class drivers |
| `usb/driver` | DCD, HCD and OTG controller adaptation |
| `usb/utils` | optional proxy/diagnostic utilities |

### Device side

Historical VSF device objects separate:

- device configuration and descriptor tables;
- interface entries bound to class operations and class parameters;
- class callbacks for descriptor/setup/data/init/fini work;
- endpoint transfer state containing buffer, length/ZLP and completion path;
- the DCD-facing controller interface.

Descriptor construction relies heavily on VSF macros, and event-driven class
handling can integrate with VSF EDA. Those implementation choices are VSF-local.

### Host side

Historical VSF host objects separate:

- HCD operations such as init, URB allocation/submission and device reset;
- URB/pipe transfer state and completion;
- enumerated device/interface topology;
- class probe/disconnect behavior;
- root-hub and external-hub handling.

The host side is not merely the device stack reversed: enumeration ownership,
addressing, hub state and asynchronous transfer lifetime are independent
contracts.

## Transferable Observations

- Controller drivers, protocol core and class behavior should remain distinct
  ownership layers.
- A class interface needs explicit setup/data/lifecycle responsibilities.
- Transfer state must identify endpoint/pipe, buffer extent, completion and
  cancellation/failure ownership.
- Descriptor data is protocol input, not evidence that enumeration or class IO
  succeeded.
- Device and host paths may share wire types while retaining different runtime
  state machines.

## What Does Not Transfer Automatically

- VSF macro configuration and compile-time component selection;
- EDA-specific callback shape;
- VSF object names or struct layout;
- driver/platform directory layout;
- the assumption that every supported class should enter Charm;
- Host, hub or OTG scope without a current consumer and evidence.

Charm USB must justify its own public surface from current source, consumers,
failure semantics and Native/QEMU/real-device evidence. This reference cannot
be used as proof that a Charm device class, host stack or controller backend is
implemented.
