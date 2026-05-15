### Compile

````
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- sun55iw6p1_t536_defconfig
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- -j$(nproc) boot-package-sun55iw6
```

### Flash

```
dd if=boot0_sdcard.fex of=/dev/sdc bs=8k seek=1 conv=fsync
dd if=boot_package.fex of=/dev/sdc bs=8k seek=2050 conv=fsync
```
