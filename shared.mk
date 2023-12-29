VERSION_STR := 1.5

OBJS := $(subst .c,.o,$(subst .cpp,.o,$(wildcard **.c*)))

MAKEFLAGS += --jobs=$(words $(OBJS))