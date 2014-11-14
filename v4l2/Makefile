CROSS := arm-cortex_a9-linux-gnueabi-
CXX := $(CROSS)g++

KERNEL_SOURCE := /home/swpark/android-3.4/
CXXFLAGS := -O2 -g -fpic -I$(KERNEL_SOURCE)/include -I../include

SRCS := nxp-v4l2.cpp nxp-v4l2-dev.cpp
OBJS := $(SRCS:.cpp=.o)

LIBNAME := libv4l2-nexell
SO_MAJ := .1
SO_MIN := .1

SO_NAME := $(LIBNAME).so$(SO_MAJ) 
TARGET := $(LIBNAME).so$(SO_MAJ)$(SO_MIN)

all: $(TARGET)

$(TARGET) : $(OBJS)
	$(CXX) $(LDFLAGS) -shared -Wl,-soname,$(SO_NAME) -o $@ $(OBJS) $(LIBRARY)
	ln -s $@ $(LIBNAME).so

%o:%cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	-rm -rf $(OBJS)
	-rm -f $(TARGET)
	-rm -f *.so

depend: $(SRCS)
	$(CC) -M $(CXXFLAGS) $^ > $@
