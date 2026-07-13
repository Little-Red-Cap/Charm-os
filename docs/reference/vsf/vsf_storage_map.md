# VSF Storage Structure Reference

> status: `reference`
>
> scope: historical VSF MAL, filesystem and SCSI decomposition

This note preserves third-party structure observations only. Current Charm
block/VFS behavior starts at
[`docs/storage/README.md`](../../storage/README.md).

## VSF MAL

VSF's memory abstraction layer historically separates:

- a block-media object from its driver operation table;
- block-size/buffer queries from init/fini/read/write/erase operations;
- media features such as read/write/erase and non-uniform block geometry;
- wrappers such as memory, file, flash, SD/MMC, SCSI, cache and synthetic FAT;
- optional serialization of a shared media object through a reentrant wrapper.

This is useful evidence that media geometry, operation support and concurrency
policy are separate concerns. It does not determine Charm's interface names or
where a cache belongs.

## VSF Filesystem

The historical VSF filesystem layer separates mount/unmount/rename from file
and directory operations. File objects carry position, size, attributes and
parent/mount relationships. Drivers include FAT, littlefs, memory/ROM, host and
MAL-backed variants.

VSF integrates these operations with its EDA sub-call model. Charm must derive
sync/async behavior from its own consumers and IO contracts; the VSF execution
model does not transfer automatically.

## VSF SCSI

VSF includes both MAL-backed SCSI targets and SCSI devices adapted back into
MAL. The reusable observation is directional: a protocol endpoint and a block
media interface need an explicit bridge with clear command, geometry and error
translation.

## What Does Not Transfer Automatically

- VSF object names, driver tables or struct layout;
- EDA call shape and locking policy;
- a generic feature bitmap without current consumer semantics;
- cache placement or filesystem layering;
- SCSI/MSC/network-storage scope without an implemented boundary and evidence;
- historical directory and migration plans.

This reference cannot prove that a Charm block backend, filesystem, cache or
SCSI bridge is implemented or safe under detach/fault conditions.
