

.PHONY: all bin
all: bin
	../../mrp ./pack.json

bin:
	arm-none-eabi-gcc -o bin.elf extCallMr.c -nostdlib -nostartfiles -pie -fPIC -marm -march=armv5te --entry=_start


