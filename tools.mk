.PHONY: flash
flash: all
	openocd -f openocd.cfg -c "program build/motor-control-firmware.elf verify reset"

bootloader: all
	python ./can-bootloader/client/bootloader_flash.py --tcp 192.168.2.20 -b ./build/motor-control-firmware.bin -a 0x08003800 -c "motor-board-v1" -r 10 11

.PHONY: r
r: reset
.PHONY: reset
reset:
	openocd -f openocd.cfg -c "init" -c "reset" -c "shutdown"
