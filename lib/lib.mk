#*********************************************************
# This makefile is used to create libjia.a library
#*********************************************************

# define compilers
CC  = gcc
CCC = gcc
FC = gfortran	# gfortran is more common

# define dirs
SRCDIR	= ../../src
INCLUDEDIR = $(SRCDIR)/include
CFLAGS	+= -I./${INCLUDEDIR} $(ARCH_FLAGS) -DDOSTAT -g

# define files
SRCS 	:= $(wildcard $(SRCDIR)/*/*.c)
DIRS	:= $(filter-out %.h, $(wildcard $(SRCDIR)/*))
OBJS 	:= $(patsubst %.c, %.o, $(foreach file, $(SRCS), $(notdir $(file))))

# define VPATH to search dependent file
VPATH = $(DIRS)

TARGET 	= libjia.a

# 暂时没有.d文件时通过这个依赖关系来进行第一次创建
%.d: %.c
	@echo "Creating $@..."	# @command, ignore 'command' in terminal presentation
	@$(SHELL) -ec "$(CC) -MM $(CFLAGS) $< | sed ' s/$*\.o/& $@/g' > $@"

%.d: %.f
	@echo "Creating $@..."
	@$(SHELL) -ec "$(FC) -c -MM $< | sed 's/$*\.o/& $@/g' > $@"

$(TARGET):$(OBJS)
	$(info srcs : $(OBJS)) 
	ar rv $(TARGET) $?

all:$(TARGET)

clean:
	rm -f *.[od] $(TARGET)

debug:
	$(info srcs : $(SRCS)) 

.PHONY: clean debug

# .d文件只通过显式规则来表明.o文件与.d文件的依赖项，并不提供重新构建的规则
# 如果需要进行重新构建，需要调用本makefile中的隐式规则
# .h文件的修改触发.o文件的重构，.o文件的重构调用.o:.c的隐式规则
include $(OBJS:.o=.d) 
