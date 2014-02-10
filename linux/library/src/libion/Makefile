########################################################################
# Get Linux Build Enviornment:
include ../../../build.env

LIB_INSTALL	:=	../../lib
INC_INSTALL	:= 	../../include

######################################################################
# Build options
CFLAGS		+= -fpic
INCLUDE		+= -I./ -I../../include -I$(KERNDIR)/include
LIBRARY		+= -L./

######################################################################
# Target
COBJS			:= ion.o nx_alloc_mem_ion.o
CPPOBJS			:= 

ION_OBJS		:= ion.o
ION_LIBNAME		:= libion
ION_TARGET		:= $(ION_LIBNAME).so

# VMEM = ION + Nexell Video Memory Allocator
VMEM_OBJS		:= nx_alloc_mem_ion.o
VMEM_OBJS		+= ion.o
VMEM_LIBNAME	:= libnxvmem
VMEM_TARGET		:= $(VMEM_LIBNAME).so

######################################################################
# Build
OBJS	:= $(COBJS) $(CPPOBJS)

all: $(ION_TARGET) $(VMEM_TARGET)

$(ION_TARGET): depend $(ION_OBJS)
	$(AR) $(ARFLAGS) $(ION_LIBNAME).a $(ION_OBJS)

$(VMEM_TARGET): depend $(VMEM_OBJS)
	$(AR) $(ARFLAGS) $(VMEM_LIBNAME).a $(VMEM_OBJS)
	$(CC) $(LDFLAGS) -shared -Wl,-soname,$(VMEM_SONAME) -o $@ $(VMEM_OBJS) $(LIBRARY)

clean:
	rm -f *.o *.a *.so .depend

install:
	install -m 755 -d $(LIB_INSTALL)
	install -m 755 -d $(INC_INSTALL)
	install -m 644 $(VMEM_TARGET) $(LIB_INSTALL)
	install -m 644 $(VMEM_LIBNAME).a $(LIB_INSTALL)
	install -m 644 nx_alloc_mem.h $(INC_INSTALL)
	install -m 644 nx_fourcc.h $(INC_INSTALL)

distclean: clean
	rm -f $(LIB_INSTALL)/$(VMEM_TARGET)
	rm -f $(LIB_INSTALL)/$(VMEM_LIBNAME).a 
	rm -f $(INC_INSTALL)/nx_alloc_mem.h

#########################################################################
# Dependency
ifeq (.depend,$(wildcard .depend))
include .depend
endif

SRCS := $(COBJS:.o=.c) $(CPPOBJS:.o=.cpp)
INCS := $(INCLUDE)
depend dep:
	$(CC)  -M  $(CFLAGS)   $(INCS) $(SRCS) > .depend

