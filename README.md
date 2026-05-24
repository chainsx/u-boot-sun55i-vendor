### Compile

```
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- sun55iw3_defconfig
```

A527:

```
make ARCH=arm CROSS_COMPILE=arm-linux-gnueabi- boot-package-a527 -j$(nproc)
```

### Flash

```
dd if=/dev/zero of=$1 bs=1M count=20
dd if=boot0_sdcard.fex of=/dev/sdc bs=8k seek=1 conv=fsync
dd if=boot_package.fex of=/dev/sdc bs=8k seek=2050 conv=fsync
```
