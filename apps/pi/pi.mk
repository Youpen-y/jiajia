#*********************************************************
# Application specific rules and defines...
#*********************************************************

CFLAGS += -I../../../src/include -O2 -g
CFLAGS = -g
OBJS 	= pi.o
VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

%.d:%.c 
#	@echo "Creating $@..."
#	@$(SHELL) -ec "$(CC)  $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

TARGET 	= ./pi

$(TARGET):$(OBJS) $(JIALIB)/libjia.a
	$(CC) $(CFLAGS) -o $@ $(OBJS) -L$(JIALIB) -ljia $(LDFLAGS)

all:$(TARGET)

clean:
	rm -f *.[od] *.log *.err $(TARGET)

# include $(OBJS:.o=.d)
