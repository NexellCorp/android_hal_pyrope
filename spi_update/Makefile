
######################################################################
# Build options
LIBRARY	+=-lpthread
CFLAGS  += -static -W -O2
######################################################################
# Target
COBJS  	:= spieeprom.o update.o 
CPPOBJ	:=
TARGET 	:= spi_update

CC		= $(CROSS_COMPILE)gcc
#CC :=gcc 

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET) 

$(TARGET): depend $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $@ $(LIBRARY)

clean:
	@find $(OBJTREE) -type f \
		\( -name '*.o'	-o -name $(TARGET) -o -name $(TARGET2) -o -name '*.map' \) \
		-print | xargs rm -f

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
SRCS += $(COBJS2:.o=.c) $(CPPOBJS2:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend
