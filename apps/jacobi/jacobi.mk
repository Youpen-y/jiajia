#*********************************************************
# Application specific rules and defines...
#*********************************************************

CPPFLAGS = -I../../../src/include -O2

CFLAGS = -g 
OBJS1 	= jacobi_heat.o
OBJS2	= jacobi_equation.o
VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

%.d:%.c 
#	@echo "Creating $@..."
#	@$(SHELL) -ec "$(CC) $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

TARGET1 	= ./jacobi_heat
TARGET2		= ./jacobi_equation

$(TARGET1):$(OBJS1) $(JIALIB)/libjia.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS1) -L$(JIALIB) -ljia $(LDFLAGS)

$(TARGET2):$(OBJS2) $(JIALIB)/libjia.a
	$(CC) $(CPPFLAGS) $(CFLAGS) -o $@ $(OBJS2) -L$(JIALIB) -ljia $(LDFLAGS)

all: $(TARGET1) $(TARGET2)

clean:
	rm -f *.[od] *.log *.err $(TARGET1) $(TARGET2)

#include $(OBJS1:.o=.d)
