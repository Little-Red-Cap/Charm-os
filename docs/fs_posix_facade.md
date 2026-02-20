# POSIX facade（VFS 薄封装）

目标：提供最小 POSIX 风格的文件 API，内部全部走 `fs_vfs`，避免业务层直接依赖底层细节。

## 模块位置

- 入口模块：`fs_posix`
- 文件路径：`Modules/io/fs/fs_posix.cppm`

## 能力覆盖

- `open/read/write/close`
- `lseek`
- `unlink/rename/truncate`

说明：
- 只做“薄封装”，不引入线程/同步语义。
- 仍使用 `fs::Err` 作为错误码（负值返回）。

## 使用示例（最小）

```cpp
import fs_posix;
import fs_vfs;
import fs_ramfs;

using Posix = fs_posix::PosixApi<8>;

void demo_posix() {
    fs::RamFs<512, 8, 32> ramfs{};
    fs::set_mount(ramfs.mount_point());

    int fd = Posix::open("/hello.txt", fs_posix::O_CREAT | fs_posix::O_TRUNC | fs_posix::O_RDWR);
    if (fd < 0) return;

    const char msg[] = "hello";
    (void)Posix::write(fd, msg, sizeof(msg) - 1);

    (void)Posix::lseek(fd, 0, fs_posix::SeekSet);

    char buf[8]{};
    (void)Posix::read(fd, buf, sizeof(buf));

    (void)Posix::close(fd);
}
```

## 使用示例（VFS 前缀挂载）

```cpp
import fs_posix;
import fs_vfs;
import fs_fatfs;

using Posix = fs_posix::PosixApi<8>;

void demo_posix_fatfs(fs::FatFsMount& fatfs) {
    (void)fs::add_mount("/sd", fatfs.mount_point());
    int fd = Posix::open("/sd/hello.txt", fs_posix::O_RDONLY);
    if (fd < 0) return;
    char buf[32]{};
    (void)Posix::read(fd, buf, sizeof(buf));
    (void)Posix::close(fd);
}
```

## 约束与注意事项

- `MaxFd` 为编译期上限，超出返回 `-Err::busy`。
- `O_TRUNC` 会调用 `vfs_truncate(path, 0)`。
- `lseek` 目前返回新的绝对偏移（或负错误码）。

## 后续可扩展

- `mkdir/stat/readdir`
- `dup/pipe`
- errno 对接

