#*********************************************************
# Application specific rules and defines...
#*********************************************************

CC	= gcc
CPPFLAGS = -I../../../src/include
OBJS 	= lu.o

VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

%.d:%.c 
	@echo "Creating $@..."
	@$(SHELL) -ec "gcc -MM $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

TARGET 	= ./lu

$(TARGET):$(OBJS) $(JIALIB)/libjia.a
	$(CC) $(CFLAGS) -o $@ $(OBJS) -L$(JIALIB) -ljia $(LDFLAGS)

all:$(TARGET)

clean:
	rm -f *.[od] $(TARGET)

include $(OBJS:.o=.d)
