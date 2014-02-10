########################################################################
# Get Linux Build Enviornment:
include ../../build.env

ifndef KERNDIR
KERNDIR := /home/ray/devel/android/jb-mr1.1/kernel
endif

######################################################################
# Build options
INCLUDE += -I./
INCLUDE += -I$(KERNDIR)/include
INCLUDE += -I$(LIBSDIR)/src/libion
INCLUDE += -I$(LIBSDIR)/src/libnxv4l2
INCLUDE += -I../../library/include
LIBRARY	+= -L../../library/lib

LIBRARY	+= -L$(LIBSDIR)/src/libion
LIBRARY	+= -L$(LIBSDIR)/src/libnxv4l2

LIBRARY += -lion -lv4l2-nexell
LIBRARY	+= -lm -lstdc++

######################################################################
# Target
CSI_OBJS	:= test-csi.o

DECI_OBJS	:= test-decimator.o

HDMI_OBJS	:= test-hdmi.o

RESC_OBJS	:= 
#RESC_OBJS	:= test-resc.o

CAM_OBJS	:= test-cam.o

COBJS	:= 

CPPOBJS	:= $(CSI_OBJS) $(DECI_OBJS) $(HDMI_OBJS) $(RESC_OBJS) $(CAM_OBJS)

CFLAGS	+= -g

CAMAPP  := camera_test
DECIAPP := decimator_test
CSIAPP  := csi_test
HDMIAPP := hdmi_test
RESCAPP := resc_test

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(CSIAPP) $(DECIAPP) $(HDMIAPP) $(CAMAPP)

$(DECIAPP):	depend $(OBJS)
	$(CC) $(CPPFLAGS) $(DECI_OBJS) -o $@ $(LIBRARY)

$(HDMIAPP):	depend $(OBJS)
	$(CC) $(CPPFLAGS) $(HDMI_OBJS) -o $@ $(LIBRARY)

$(CSIAPP):	depend $(OBJS)
	$(CC) $(CPPFLAGS) $(CSI_OBJS) -o $@ $(LIBRARY)

$(RESCAPP):	depend $(OBJS)
	$(CC) $(CPPFLAGS) $(RESC_OBJS) -o $@ $(LIBRARY)

$(CAMAPP):	depend $(OBJS)
	$(CC) $(CPPFLAGS) $(CAM_OBJS) -o $@ $(LIBRARY)

clean:
	rm -f $(DECIAPP) $(HDMIAPP) $(CSIAPP) $(RESCAPP) *.o .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CPPFLAGS) $(INCS) $(SRCS) > .depend
