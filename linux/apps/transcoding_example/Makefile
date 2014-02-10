########################################################################
# Get Linux Build Enviornment:
include ../../build.env

######################################################################
# Build options
INCLUDE += -I./
LIBRARY	+= -L$(LIBSDIR)/lib 
LIBRARY += -lnxvpu -lnxdsp -lnxv4l2 -lnxgraphictools -lnxvmem -lnxscaler
LIBRARY += -lEGL -lGLESv2 -lVR
LIBRARY	+= -lavcodec -lavformat -lpthread -lstdc++

######################################################################
# Target
TRNAS_OBJS	:= codec_info.o
TRNAS_OBJS	+= queue.o
TRNAS_OBJS	+= NX_Semaphore.o
TRNAS_OBJS	+= trans_main.o

COBJS	:= queue.o codec_info.o NX_Semaphore.o trans_main.o

CPPOBJS	:=  

CFLAGS	+= -g

TRNASAPP := trans_test2

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TRNASAPP)

$(TRNASAPP):	depend $(OBJS)
	$(CC) $(CFLAGS) $(TRNAS_OBJS) -o $@ $(LIBRARY)

clean:
	rm -f $(TRNASAPP) *.o .depend

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp) $(APPOBJS:.o=.c)
INCS := $(INCLUDE)
depend dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > .depend
