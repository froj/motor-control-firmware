##############################################################################
# Build global options
# NOTE: Can be overridden externally.
#

# Compiler options here.
ifeq ($(USE_OPT),)
  USE_OPT = -O2 -ggdb -fomit-frame-pointer -falign-functions=16
  USE_OPT += -fno-stack-protector -ftracer -ftree-loop-distribute-patterns
  USE_OPT += -frename-registers -freorder-blocks -fconserve-stack -g3
endif

# C specific options here (added to USE_OPT).
ifeq ($(USE_COPT),)
  USE_COPT = 
endif

# C++ specific options here (added to USE_OPT).
ifeq ($(USE_CPPOPT),)
  USE_CPPOPT = -fno-rtti
  USE_CPPOPT +=  -std=gnu++11
  USE_CPPOPT += -fno-exceptions -fno-unwind-tables -fno-threadsafe-statics
endif

# Enable this if you want the linker to remove unused code and data
ifeq ($(USE_LINK_GC),)
  USE_LINK_GC = yes
endif

# Linker extra options here.
ifeq ($(USE_LDOPT),)
  USE_LDOPT = 
endif

# Enable this if you want link time optimizations (LTO)
ifeq ($(USE_LTO),)
  USE_LTO = no
endif

# If enabled, this option allows to compile the application in THUMB mode.
ifeq ($(USE_THUMB),)
  USE_THUMB = yes
endif

# Enable this if you want to see the full log while compiling.
ifeq ($(USE_VERBOSE_COMPILE),)
  USE_VERBOSE_COMPILE = no
endif

#
# Build global options
##############################################################################

##############################################################################
# Architecture or project specific options
#

# Stack size to be allocated to the Cortex-M process stack. This stack is
# the stack used by the main() thread.
ifeq ($(USE_PROCESS_STACKSIZE),)
  USE_PROCESS_STACKSIZE = 0x400
endif

# Stack size to the allocated to the Cortex-M main/exceptions stack. This
# stack is used for processing interrupts and exceptions.
ifeq ($(USE_EXCEPTIONS_STACKSIZE),)
  USE_EXCEPTIONS_STACKSIZE = 0x400
endif

# Enables the use of FPU on Cortex-M4 (no, softfp, hard).
ifeq ($(USE_FPU),)
  USE_FPU = hard
endif

#
# Architecture or project specific options
##############################################################################

##############################################################################
# Project, sources and paths
#

# Define project name here
PROJECT = motor-control-firmware

# Imported source files and paths
CHIBIOS = ChibiOS
include $(CHIBIOS)/os/hal/hal.mk
include $(CHIBIOS)/os/hal/ports/STM32/STM32F3xx/platform.mk
include $(CHIBIOS)/os/hal/osal/rt/osal.mk
include $(CHIBIOS)/os/rt/rt.mk
include $(CHIBIOS)/os/rt/ports/ARMCMx/compilers/GCC/mk/port_stm32f3xx.mk
include $(CHIBIOS)/test/rt/test.mk
include $(CHIBIOS)/os/various/cpp_wrappers/chcpp.mk

# must be defined before board.mk include
# USE_BOOTLOADER = yes
include board/board.mk
include src/src.mk

# Define linker script file here
LDSCRIPT = $(BOARDLDSCRIPT)

# C sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CSRC = $(PORTSRC) \
       $(KERNSRC) \
       $(TESTSRC) \
       $(HALSRC) \
       $(OSALSRC) \
       $(PLATFORMSRC) \
       $(CHIBIOS)/os/various/chprintf.c \
       $(CHIBIOS)/os/various/memstreams.c \
       $(CHIBIOS)/os/various/shell.c \
       $(BOARDSRC) \
       $(PROJCSRC) \
       $(CHIBIOS)/os/various/syscalls.c

# C++ sources that can be compiled in ARM or THUMB mode depending on the global
# setting.
CPPSRC = $(CHCPPSRC) $(PROJCPPSRC)

# C sources to be compiled in ARM mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
ACSRC =

# C++ sources to be compiled in ARM mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
ACPPSRC =

# C sources to be compiled in THUMB mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
TCSRC =

# C sources to be compiled in THUMB mode regardless of the global setting.
# NOTE: Mixing ARM and THUMB mode enables the -mthumb-interwork compiler
#       option that results in lower performance and larger code size.
TCPPSRC =

# List ASM source files here
ASMSRC = $(PORTASM)

INCDIR = $(PORTINC) $(KERNINC) $(TESTINC) \
         $(HALINC) $(OSALINC) $(PLATFORMINC) $(BOARDINC) \
         $(CHCPPINC) $(CHIBIOS)/os/various

#
# Project, sources and paths
##############################################################################

##############################################################################
# Compiler settings
#

MCU  = cortex-m4

#TRGT = arm-elf-
TRGT = arm-none-eabi-
CC   = $(TRGT)gcc
CPPC = $(TRGT)g++
# Enable loading with g++ only if you need C++ runtime support.
# NOTE: You can use C++ even without C++ support if you are careful. C++
#       runtime support makes code size explode.
# LD   = $(TRGT)gcc
LD   = $(TRGT)g++
CP   = $(TRGT)objcopy
AS   = $(TRGT)gcc -x assembler-with-cpp
AR   = $(TRGT)ar
OD   = $(TRGT)objdump
SZ   = $(TRGT)size
HEX  = $(CP) -O ihex
BIN  = $(CP) -O binary -j startup -j constructors -j destructors -j .text -j .ARM.extab -j .ARM.exidx -j .eh_frame_hdr -j .eh_frame -j .textalign -j .data

# ARM-specific options here
AOPT =

# THUMB-specific options here
TOPT = -mthumb -DTHUMB

# Define C warning options here
CWARN = -Wall -Wextra -Wstrict-prototypes

# Define C++ warning options here
CPPWARN = -Wall -Wextra

#
# Compiler settings
##############################################################################

##############################################################################
# Start of user section
#

# List all user C define here, like -D_DEBUG=1
UDEFS  = $(BOARDDEFS)
UDEFS += -DUAVCAN_STM32_CHIBIOS=1 \
		 -DUAVCAN_STM32_TIMER_NUMBER=7 \
		 -DUAVCAN_STM32_NUM_IFACES=1

# Define ASM defines here
UADEFS =

# List all user directories here
UINCDIR = ./src

# List the user directory to look for the libraries here
ULIBDIR =

# List all user libraries here
ULIBS =

#
# UAVCAN
#
include uavcan/libuavcan/include.mk
include src/can-driver/include.mk

CPPSRC += $(LIBUAVCAN_SRC) $(LIBUAVCAN_STM32_SRC)
UINCDIR += $(LIBUAVCAN_INC) $(LIBUAVCAN_STM32_INC) ./dsdlc_generated

# run uavcan dsdl compiler
$(info $(shell $(LIBUAVCAN_DSDLC) $(UAVCAN_DSDL_DIR)))

#
# End of user defines
##############################################################################

RULESPATH = $(CHIBIOS)/os/common/ports/ARMCMx/compilers/GCC
include $(RULESPATH)/rules.mk
-include tools.mk

.PHONY: packager
packager:
	python packager/packager.py

CMakeLists.txt: package.yml
	python packager/packager.py

src/src.mk: package.yml
	python packager/packager.py

.PHONY: tests
tests: CMakeLists.txt
	@mkdir -p build/tests
	@cd build/tests; \
	cmake ../..; \
	make ; \
	./tests;
