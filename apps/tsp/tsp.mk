#*********************************************************
# Application specific rules and defines...
#*********************************************************

CPPFLAGS = -I../../../src/include -O2
CFLAGS = -Wunused-result -g
OBJS 	= pq.o solve.o tours.o tsp.o read_tsp.o
VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

%.d:%.c 
#	@echo "Creating $@..."
#	@$(SHELL) -ec "$(CC) $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

TARGET 	= ./tsp

$(TARGET):$(OBJS) $(JIALIB)/libjia.a
	$(CC) $(CFLAGS) -o $@ $(OBJS) -L$(JIALIB) -ljia $(LDFLAGS)

all:$(TARGET)

clean:
	rm -f *.[od] *.log *.err $(TARGET)

# include $(OBJS:.o=.d)
