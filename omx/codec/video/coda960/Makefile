########################################################################
# Get Linux Build Enviornment:
include env.mk

KERNEL_DIR	:= /home/ray/devel/android/jb-mr1.1/kernel-svn

######################################################################
# Build options
INCLUDE += -I./ -I./driver/include -I../libion -I../v4l2 -I$(KERNEL_DIR)/include

######################################################################
# Target
COBJS  	:= 
COBJS  	+= nx_video_api.o
COBJS  	+= nx_alloc_mem_ion.o
CPPOBJS	:=  

CFLAGS	+= -g

LIBNAME	:= libnxvpu

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(LIBNAME)

$(LIBNAME):	depend $(OBJS)
	$(AR) $(ARFLAGS) $(LIBNAME).a $(OBJS)

clean:
	@find $(OBJTREE) -type f \
		\( -name '*.o'	-o -name $(LIBNAME).a -o -name '*.map' \
		-o -name .depend -o -name $(APPNAME) -o -name '.depend' \) -print \
		| xargs rm -f

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
