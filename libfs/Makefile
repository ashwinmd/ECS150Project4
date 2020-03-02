# Target library
lib := libfs.a

objs := disk.o fs.o
CC := gcc
CFLAGS := -Wall -Wextra -Werror

ifeq ($(D),1)
CFLAGS	+= -g
endif

CFLAGS += -c -o

all: $(lib)

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

%.a: $(objs)
	ar rcs $@ $^
	ranlib $@

%.o: %.c
	@echo "CC $@ $^"
	$(CC) $(CFLAGS) $@ $< $(DEPFLAGS)

clean:    
	rm -f $(lib) $(objs) $(deps)
