# fs-block-vfs-demo

Minimal verification chain:

block.device -> vfs -> out

Cases expected:

1) image not found
2) invalid MBR
3) no FAT partition
4) successful mount + read

Run:

```
./verify.ps1
```

Provide images:

- invalid_mbr.img (invalid signature)
- no_fat.img (valid MBR, no FAT partition)
- disk.img (valid FAT with /hello.txt)
