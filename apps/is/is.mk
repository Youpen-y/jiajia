#*********************************************************
# Application specific rules and defines...
#*********************************************************

CPPFLAGS = -I../../../src/include -O2 -DMEDIUM

CFLAGS = -g
OBJS 	= is.o
VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

%.d:%.c 
#	@echo "Creating $@..."
#	@$(SHELL) -ec "$(CC) $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

TARGET 	= ./is

$(TARGET):$(OBJS) $(JIALIB)/libjia.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS)  -L$(JIALIB) -ljia $(LDFLAGS)

all:$(TARGET)

clean:
	rm -f *.[od] *.log *.err $(TARGET)

#include $(OBJS:.o=.d)
