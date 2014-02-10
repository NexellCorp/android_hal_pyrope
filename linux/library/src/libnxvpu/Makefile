########################################################################
# Get Linux Build Enviornment:
include ../../../build.env

LIB_INSTALL	:=	../../lib
INC_INSTALL	:= 	../../include

######################################################################
# Build options
INCLUDE += -I./ -I../../include -I$(KERNDIR)/include

LIBRARY += -L../../lib/
LIBRARY += -lnxvip -lnxdsp -lnxv4l2 -lnxvmem

######################################################################
# Target
COBJS  	:= 
COBJS  	+= nx_video_api.o
CPPOBJS	:=  

CFLAGS	+= -g

LIBNAME	:= libnxvpu
TARGET  := libnxvpu.so

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET)

$(TARGET):	depend $(OBJS)
	$(AR) $(ARFLAGS) $(LIBNAME).a $(OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(TARGET) -o $@ $(OBJS)

install:
	install -m 755 -d $(LIB_INSTALL)
	install -m 755 -d $(INC_INSTALL)
	install -m 644 $(TARGET) $(LIB_INSTALL)
	install -m 644 $(LIBNAME).a $(LIB_INSTALL)
	install -m 644 nx_video_api.h $(INC_INSTALL)
	install -m 644 vpu_types.h $(INC_INSTALL)

clean:
	rm -f *.o *.so *.a .depend

distclean: clean
	rm -f $(LIB_INSTALL)/lib/$(TARGET)
	rm -f $(LIB_INSTALL)/lib/$(LIBNAME).a
	rm -f $(INC_INSTALL)/nx_video_api.h
	rm -f $(INC_INSTALL)/vpu_types.h


#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
