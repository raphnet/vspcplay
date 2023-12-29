VERSION_STR := 1.5

OBJS := $(wildcard **.c*)               # Initial pass of OBJs
OBJS := $(subst .cpp,.o,$(OBJS))        # Filter C++ files.
                                       # This needs to be done first otherwise the next step will corrupt the OBJ list.
OBJS := $(subst .c,.o,$(OBJS))          # Filter C files.

OBJS := $(filter-out getopt1.o,$(OBJS)) # Compiling this specific file causes the build to fail.
