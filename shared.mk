VERSION_STR := 1.5

OBJS := $(wildcard *.c*)
OBJS := $(subst .cpp,.o,$(OBJS))
OBJS := $(subst .c,.o,$(OBJS))

OBJS := $(filter-out getopt1.o,$(OBJS))