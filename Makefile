#
# Makefile for cmsis library
#
CROSS ?= $(CROSS_COMPILE)
CC := $(CROSS)gcc
C++ := $(CROSS)g++
AR := $(CROSS)ar

CFLAGS := -Wall -Wno-trigraphs -O2 \
	  	   -fno-strict-aliasing -fno-common
#CXXFLAGS = -Wall -std=c++11
#LDFLAGS += -Wl,-rpath ./lib
INCLUDES += -I./ -I./src -I./include -I./kernel_headers
LIBS +=  -L./ -L./lib
#CFLAGS	+= -g -I$(INCLUDES)
SRCDIR := src/
OBJDIR := obj/

COBJS_O := \
	uvc_api.o \
	util_api.o \
	video_api.o

CPPOBJS_O := \
	RGBDClass.o
#	application.o \
#	private_vector.o 
#	RGBDSensor.o	\
#	test.o




COBJS := $(addprefix $(SRCDIR), $(COBJS_O))
CPPOBJS := $(addprefix $(SRCDIR), $(CPPOBJS_O))


vpath %.c $(sort $(dir $(COBJS_O)))
vpath %.S $(sort $(dir $(SOBJS_O)))

all : obj lib/librgbdsensor.a test test1 rgbd uvc rgbd_uvc rgbd_uvc_main #rgbd_class #capture

clean :
	rm -rf $(COBJS) $(CPPOBJS) lib/librgbdsensor.a

obj :
	mkdir -p obj

	
rgbd_uvc_main : 	 src/rgbd_uvc_main.cpp lib/librgbdsensor.a
	$(C++) $(CFLAGS)  $(INCLUDES) $(LIBS) -o rgbd_uvc_main src/rgbd_uvc_main.cpp -lpthread -lrgbdsensor	
	
rgbd_class : lib/librgbdsensor.a src/RGBDClass.cpp
	$(C++) $(CFLAGS) $(INCLUDES) $(LIBS) -o rgbd_class src/RGBDClass.cpp -lpthread -lrgbdsensor	
	
capture : lib/librgbdsensor.a src/capture.cpp
	$(C++) $(CFLAGS) $(INCLUDES) $(LIBS) -o capture src/capture.cpp -lpthread -lrgbdsensor	-lopencv_videoio -lopencv_core -lopencv_imgproc -lopencv_highgui

rgbd_uvc : lib/librgbdsensor.a src/test_rgbd_uvc.cpp
	$(C++) $(CFLAGS) $(INCLUDES) $(LIBS) -o rgbd_uvc src/test_rgbd_uvc.cpp -lpthread -lrgbdsensor	
	
uvc : lib/librgbdsensor.a src/test_uvc.cpp
	$(C++) $(CFLAGS) $(INCLUDES) $(LIBS) -o uvc src/test_uvc.cpp -lpthread -lrgbdsensor	

rgbd : lib/librgbdsensor.a src/test_rgbd.cpp
	$(C++) $(CFLAGS) $(INCLUDES) $(LIBS) -o rgbd src/test_rgbd.cpp -lpthread -lrgbdsensor	
	
test1 : lib/librgbdsensor.a src/test_main1.cpp
	$(C++) $(CFLAGS) $(INCLUDES) $(LIBS) -o test1 src/test_main1.cpp -lpthread -lrgbdsensor
	
test : lib/librgbdsensor.a src/test_main.cpp
	$(C++) $(CFLAGS) $(INCLUDES) $(LIBS) -o test src/test_main.cpp -lpthread -lrgbdsensor
	
lib/librgbdsensor.a :$(OBJDIR)depend $(COBJS) $(CPPOBJS)
	$(AR) r $@ $(COBJS) $(CPPOBJS)

$(COBJS): %.o: %.c
	$(CC) $(CFLAGS) $(INCLUDES) $(LIBS) -c -o $@ $<

$(CPPOBJS): %.o: %.cpp
	$(C++) $(SFLAGS) $(INCLUDES) $(LIBS) -c -o $@ $<

###########
ifeq ($(OBJDIR).depend,$(wildcard $(OBJDIR).depend))
include $(OBJDIR).depend
endif

SRCS := $(addprefix $(SRCDIR),$(COBJS_O:.o=.c))
SRCS += $(addprefix $(SRCDIR),$(CPPOBJS_O:.o=.cpp))
INCS := $(INCLUDES)
$(OBJDIR)depend $(OBJDIR)dep:
	$(CC) -M $(CFLAGS) $(INCS) $(SRCS) > $(OBJDIR).depend
