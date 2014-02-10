########################################################################
# Get Linux Build Enviornment:
ifndef TOPDIR
TOPDIR	:= ../..
endif
include $(TOPDIR)/build.env

######################################################################
# Build options
INCLUDE += -I./ -I$(LIBSDIR)/include

LIBRARY	+= -lstdc++ -lpthread -lgobject-2.0 -lm
LIBRARY += -lglib-2.0 -lgstreamer-0.10 
LIBRARY += -L$(LIBSDIR)/lib -lnxmovplayer

######################################################################
# Target
TARGET	:= movieplayer_app

######################################################################
# Build
COBJS  	:= main.o
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(TARGET) 

clean:
	rm -f $(TARGET) *.o .depend

$(TARGET): depend $(OBJS)
	$(CC) $(CPPFLAGS) $(OBJS) -o $(TARGET) $(LIBRARY)
	@echo "done"

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend
