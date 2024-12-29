#*********************************************************
# Application specific rules and defines...
#*********************************************************

CPPFLAGS = -I../../../src/include -O2
CFLAGS = -g
OBJS 	= mm.o
VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

%.d:%.c 
#	@echo "Creating $@..."
#	@$(SHELL) -ec "$(CC) $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

TARGET 	= ./mm

$(TARGET):$(OBJS) $(JIALIB)/libjia.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS) -L$(JIALIB) -ljia $(LDFLAGS)

all:$(TARGET)

clean:
	rm -f *.[od] *.log *.err $(TARGET)

#include $(OBJS:.o=.d)
