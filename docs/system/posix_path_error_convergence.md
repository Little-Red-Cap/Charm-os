# POSIX Path Error Convergence (v0)

状态：exploration snapshot。

本页记录 `notdir/isdir` 第一轮收敛目标。该映射已进入当前源码；现行错误规则见
[`posix_error_semantics.md`](posix_error_semantics.md)。下面的范围和 acceptance 仅供追溯。

## Goal
Start converging file-type and path-component errors away from the current coarse `inval` bucket, without pulling in the full POSIX path matrix at once.

## First Slice
- `open("/dir", O_WRONLY)` should fail with `EISDIR`
- `open("/file/child", O_RDONLY)` should fail with `ENOTDIR`

## Layers Touched
1. `util::Errc` grows typed path errors: `notdir`, `isdir`
2. `posix.errno` maps them to `ENOTDIR`, `EISDIR`
3. `fs_ramfs` returns typed errors from `open()`
4. smoke verifies the new behavior through `posix.api::open()`

## Not In Scope
- full `stat()` type matrix
- `mkdir` / `rename` / `unlink` path-type convergence
- `stat_probe` recovery
- `close(-1)` errno convergence

## Acceptance
- QEMU mainline smoke stays green
- directory open failure is stable and observable as `EISDIR`
- non-directory path-prefix failure is stable and observable as `ENOTDIR`
