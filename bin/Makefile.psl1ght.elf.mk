# OpenPS3FTP Makefile

include $(PSL1GHT)/ppu_rules

TARGET		:= openps3ftp.elf

# Libraries
LIBPATHS	:= -L../lib -L$(PORTLIBS)/lib $(LIBPSL1GHT_LIB)
LIBS		:= -lopenps3ftp -lNoRSX -lfreetype -lgcm_sys -lrsx -lnetctl -lnet -lsysutil -lsysmodule -lrt -lsysfs -llv2 -lm -lz

# Includes
INCLUDE		:= -I../include -I./helper -I./feat -I$(PORTLIBS)/include/freetype2 -I$(PORTLIBS)/include $(LIBPSL1GHT_INC)

# Source Files
SRCS		:= $(wildcard psl1ght/*.cpp) $(wildcard helper/*.cpp) $(wildcard feat/*/*.cpp)
OBJS		:= $(SRCS:.cpp=.ppu.o)

# Define compilation options
DEFINES		:= -DPSL1GHT_SDK
CXXFLAGS	= -O2 -mregnames -Wall -mcpu=cell $(MACHDEP) $(INCLUDE) $(DEFINES)
LDFLAGS		= -s $(MACHDEP) $(LIBPATHS) $(LIBS)

# Make rules
.PHONY: all clean

all: $(TARGET)

clean: 
	rm -f $(TARGET) $(OBJS)

$(TARGET): $(OBJS)
	$(CXX) $^ $(LDFLAGS) -o $@
	$(SPRX) $@

%.ppu.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@