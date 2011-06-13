#
# $Header: /home/ben/.cvs_rep/nds/Debug/debug_lib_src/Makefile,v 1.1.1.1 2006/09/06 10:13:18 ben Exp $
#
# The Makefile for the Native NDS version
#
# This makefile builds a library of the wmbhost common code and the .nds
# file. The DS specific code is built by makefiles in the nds directory.
#

NDSTOOL = ndstool

PREFIX = arm-eabi-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
AR = $(PREFIX)ar
AS = $(PREFIX)as
OBJCOPY	= $(PREFIX)objcopy

ARCH = -mthumb-interwork -march=armv5te -mtune=arm946e-s

INC_DIRS = $(DEVKITPRO)/libnds/include

CFLAGS	= -g -W -Wall -O2 -DARM9\
	 -fomit-frame-pointer\
	-ffast-math\
	$(ARCH) $(addprefix -I,$(INC_DIRS))

ASFLAGS = -g $(ARCH)

# The library
lib_name_base = debugstub9
lib_name = lib$(lib_name_base).a

sources = debug_stub.c breakpoints.c opcode_decode.c arm_opcode.c thumb_opcode.c debug_utilities.c
asm_sources = debug_asm_fns.asm
objects = $(sources:.c=.o) $(asm_sources:.asm=.o)

.PHONY : all
all : $(lib_name) $(serial_lib_name) $(tcp_lib_name)
	$(MAKE) -C tcp_comms


# Make the libraries
$(lib_name): $(objects)
	$(AR) rcs $@ $^

# Make the object files from Asm source
%.o : %.asm
	$(CC) -x assembler-with-cpp $(ASFLAGS) -c $< -o $@

# Make the object files from C source
# Add -DDO_LOGGING and rebuild the sources to enable logging.
%.o : %.c
	@echo building $@ from $<
	$(CC) $(CFLAGS) $< -c -o $@

#	$(CC) $(CFLAGS) -DDO_LOGGING $< -c -o $@

.PHONY : clean
clean :
	-rm -f *.o
	-rm -f $(lib_name)
	$(MAKE) -C serial_comms clean
	$(MAKE) -C tcp_comms clean

# Generate the dependencies
%.d: %.c
	set -e; $(CC) -MM $(CFLAGS) $< \
		| sed 's|\($*\)\.o[ :]*|\1.o $@ : |g' > $@; \
		[ -s $@ ] || rm -f $@

# include the dependencies files
include $(sources:.c=.d)
