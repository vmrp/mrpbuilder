arm-none-eabi-gcc -o bin.elf demo.c -nostdlib -nostartfiles -pie -fPIC -marm -march=armv5te --entry=_start
echo "ok bin.elf"
