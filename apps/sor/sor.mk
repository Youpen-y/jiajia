#*********************************************************
# Application specific rules and defines...
#*********************************************************

CPPFLAGS += -I../../../src/include -O0 -g
CFLAGS = -g
OBJS 	= sor.o
VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

#%.d:%.c 
#	@echo "Creating $@..."
#	@$(SHELL) -ec "$(CC) $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

$(OBJS):sor.c
	$(CC) $(CPPFLAGS) -o $@ -c $?

TARGET 	= ./sor

$(TARGET):$(OBJS) $(JIALIB)/libjia.a
	$(CC) $(CFLAGS) -o $@ $(OBJS) -L$(JIALIB) -ljia $(LDFLAGS)

all:$(TARGET)

clean:
	rm -f *.[od] *.log *.err $(TARGET)

# include $(OBJS:.o=.d)
 