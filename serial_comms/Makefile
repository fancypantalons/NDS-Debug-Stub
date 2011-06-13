#
# $Header: /home/ben/.cvs_rep/nds/Debug/debug_lib_src/serial_comms/Makefile,v 1.1.1.1 2006/09/06 10:13:19 ben Exp $
#

NDSTOOL = ndstool

PREFIX = arm-eabi-
CC = $(PREFIX)gcc
LD = $(PREFIX)ld
AR = $(PREFIX)ar
AS = $(PREFIX)as
OBJCOPY	= $(PREFIX)objcopy

ARCH = -mthumb-interwork -march=armv5te -mtune=arm946e-s

INC_DIRS = $(DEVKITPRO)/libnds/include .. ../../include

CFLAGS	= -g -W -Wall -O2 -DARM9\
	 -fomit-frame-pointer\
	-ffast-math\
	$(ARCH) $(addprefix -I,$(INC_DIRS))

ASFLAGS = -g $(ARCH)

# The serial comms library
serial_lib_name_base = debugserial9
serial_lib_name = lib$(serial_lib_name_base).a

serial_sources = debug_serial.c
serial_objects = $(serial_sources:.c=.o)

.PHONY : all
all : $(serial_lib_name)


# Make the libraries
$(serial_lib_name): $(serial_objects)
	$(AR) rcs $@ $^


# Make the object files from C source
# Add -DDO_LOGGING and rebuild the sources to enable logging.
%.o : %.c
	@echo building $@ from $<
	$(CC) $(CFLAGS) $< -c -o $@


.PHONY : clean
clean :
	-rm -f *.o
	-rm -f $(serial_lib_name)

# Generate the dependencies
%.d: %.c
	set -e; $(CC) -MM $(CFLAGS) $< \
		| sed 's|\($*\)\.o[ :]*|\1.o $@ : |g' > $@; \
		[ -s $@ ] || rm -f $@

# include the dependencies files
include $(serial_sources:.c=.d)
