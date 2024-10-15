
# Compiler and linker settings
INCLUDES += -Ilib -I$(STAGING_DIR)/usr/include/gstreamer-1.0 -I$(STAGING_DIR)/usr/include/glib-2.0
CXXFLAGS += -std=c++11 
LDLIBS = -lgstreamer-1.0 -lgobject-2.0 -lglib-2.0 -lpthread
# Source files
SRCS = $(wildcard *.cpp)

# Object files
OBJS = $(SRCS:.cpp=.o)

# Build rules
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^ $(LDLIBS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)