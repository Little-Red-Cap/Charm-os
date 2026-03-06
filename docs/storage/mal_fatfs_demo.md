# MAL + FatFs 最小挂载示例

目标：用 MAL 统一后端（文件/块设备），然后通过 FatFs 挂载到 VFS。

## 1. 文件后端（VHD/镜像文件）

```cpp
import charm.io;
import charm.runtime;

using namespace fs;

int main() {
    MalFile mal_file{};
    auto st = mal_file.open("G:/Project/dev.vhd", 512);
    if (!st) return -1;

    FatFsMount fat{};
    st = fat.mount(mal_file.device(), false, 0);
    if (!st) return -2;

    vfs::clear_mounts();
    (void)vfs::add_mount("/", fat.mount_point());
    return 0;
}
```

## 2. 块设备后端（直接 BlockDevice）

```cpp
import charm.io;

using namespace fs;

int main() {
    BlockDevice dev = {/* ctx + read/write/erase/flush */};
    MalBlock mal_block{};
    (void)mal_block.bind(dev);

    FatFsMount fat{};
    auto st = fat.mount(mal_block.device(), false, 0);
    if (!st) return -1;

    vfs::clear_mounts();
    (void)vfs::add_mount("/", fat.mount_point());
    return 0;
}
```

## 3. 注意事项

- `MalFile` 仅用于 PC 验证或文件镜像后端。
- `FatFsMount::mount(MalDevice&)` 仍会走 BlockDevice 形状（内部转换），保证兼容。
- `vfs::clear_mounts()` + `vfs::add_mount("/", mount)` 用于显式建立根挂载。
