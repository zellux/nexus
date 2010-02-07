#!/bin/bash
bochs -q 'display_library: nogui' 
# qemu -hda obj/kern/bochs.img -hdb obj/fs/fs.img -no-kqemu -parallel /dev/stdout > qemu.out
# qemu -hda obj/kern/bochs.img -hdb obj/fs/fs.img -no-kqemu -parallel stdio -s -S
# qemu -hda obj/kern/bochs.img -nographic -parallel /dev/stdout
