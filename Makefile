.PHONY: all clean

INCDIRS := ./mattlib
CFLAGS  := -std=c11 -pedantic -Wall -Wextra -g $(addprefix -I, $(INCDIRS))

all: bin/wc bin/datediff

clean:
	rm -rf ./bin

bin:
	mkdir -p ./bin ./bin/mattlib

# '$<' = name of first prerequisite
# '$@' = name of target
# '$^' = names of all prerequisites, spaces in between.
# '$?' = names of all prerequisites which are newer than the target
# '|' = separates "order-only prerequisites"; ignore their mtime when considering rebuilds.

# ----------------------------------------------------------------------

bin/mattlib/%.o: mattlib/%.c | bin
	gcc $(CFLAGS) -c $< -o $@

bin/mattlib.a: $(addprefix bin/mattlib/, mattlib.o mattrdr.o mattstr.o)
	ar rcs $@ $^

# ----------------------------------------------------------------------

A2_SOURCES := $(shell find ./assn2 -iname '*.c')
L3_SOURCES := $(shell find ./lab3 -iname '*.c')

bin/wc: $(A2_SOURCES) bin/mattlib.a
	gcc $(CFLAGS) $(A2_SOURCES) bin/mattlib.a -o $@

bin/datediff: $(L3_SOURCES) bin/mattlib.a
	gcc $(CFLAGS) $(L3_SOURCES) bin/mattlib.a -o $@
