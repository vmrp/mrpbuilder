

.PHONY: demo
demo:
	arm-none-eabi-gcc -o bin.elf demo.c -nostdlib -nostartfiles -pie -fPIC -marm -march=armv5te --entry=_start

.PHONY: life
life:
	arm-none-eabi-gcc -o bin.elf life.c -nostdlib -nostartfiles -pie -fPIC -marm -march=armv5te --entry=_start

