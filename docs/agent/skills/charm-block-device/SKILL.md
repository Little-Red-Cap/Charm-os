# charm-block-device

> status: `supporting`

Use this skill for block-media adapters, registry binding, cache wrappers or VFS
block mounts. Start from the
[`BlockDevice contract`](../../../storage/block_device_contract.md); do not
assume every media consumer needs registry or filesystem mounting.

## Determine The Boundary

1. Identify media geometry, supported read/write/erase/flush operations,
   alignment and partial-operation behavior.
2. Identify ownership and lifetime of media context, callbacks and buffers.
3. Decide which boundary the real consumer needs:
   - direct `block::Device` injection;
   - named/capability lookup through `block::Registry`;
   - a cache adapter;
   - VFS/filesystem mount;
   - a protocol bridge such as USB MSC.
4. Keep partition, filesystem, Store and protocol semantics outside the base
   block adapter unless that adapter explicitly owns them.

## Checks

- `Caps` and callbacks agree for operations used by the consumer.
- block count/size arithmetic, byte conversion, range and alignment are checked
  without overflow.
- timeout, short operation, media busy/error and post-failure state are explicit.
- registry names/caps are unique; hash collision and non-owning device lifetime
  are not hidden.
- replace/unregister cannot leave consumers with an undocumented stale pointer.
- cache ownership, dirty eviction and flush/failure policy match the cache
  contract.
- board/HAL handles remain below the adapter; upper layers see media semantics.

## Evidence

Use the smallest applicable positive and negative path. Registry/mount tests do
not prove raw media fault behavior; Host file/memory media does not prove SDMMC,
flash timing, detach or power-loss behavior. Report which layer and evidence
domain were actually exercised.
