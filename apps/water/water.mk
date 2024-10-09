#*********************************************************
# Application specific rules and defines...
#*********************************************************

CPPFLAGS = -I../../../src/include -O2

OBJS 	= water.o bndry.o cnstnt.o cshift.o initia.o interf.o intraf.o kineti.o poteng.o predcor.o redsync.o syscns.o 
VPATH = ../src 
JIALIB = ../../../lib/$(ARCH)

%.d:%.c 
#	@echo "Creating $@..."
#	@$(SHELL) -ec "$(CC)  $(CPPFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

TARGET 	= ./water

$(TARGET):$(OBJS) $(JIALIB)/libjia.a
	$(CC) $(CFLAGS) -o $@ $(OBJS) -L$(JIALIB) -ljia $(LDFLAGS)

all:$(TARGET)

clean:
	rm -f *.[od] *.log *.err $(TARGET)

# include $(OBJS:.o=.d)
