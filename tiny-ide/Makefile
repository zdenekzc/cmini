CC ?= cc
PKG_CONFIG ?= pkg-config
CFLAGS ?= -O2 -g
CFLAGS += -std=c11 -Wall -Wextra -Wpedantic
NCURSES_CFLAGS := $(filter-out -D_XOPEN_SOURCE=%,$(shell $(PKG_CONFIG) --cflags ncursesw 2>/dev/null))
NCURSES_LIBS := $(shell $(PKG_CONFIG) --libs ncursesw 2>/dev/null)
ifeq ($(strip $(NCURSES_LIBS)),)
NCURSES_LIBS := -lncursesw
endif
LDLIBS += $(NCURSES_LIBS)

TARGET := tiny-ide.bin
SOURCES := main.c app.c editor.c runner.c ui.c dialogs.c
HEADERS := tiny_ide.h editor.h
# The IDE compiles C and C++ programs with cmini (cmini/cmini.bin).
CMINI ?= ../cmini/cmini.bin

.PHONY: all run cmini check check-unit check-smoke clean

all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS) Makefile
	$(CC) $(CFLAGS) $(NCURSES_CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)

run: $(TARGET) cmini
	./$(TARGET) demo/hello.c

cmini:
	@test -x "$(CMINI)" || $(MAKE) -C ../cmini/cmini cmini.bin
	@test -x "$(CMINI)" && echo "using cmini: $(CMINI)"

check-unit: tests/test_editor.c editor.c editor.h
	mkdir -p ../_build/tiny-ide-tests
	$(CC) $(CFLAGS) -I. -o ../_build/tiny-ide-tests/test_editor tests/test_editor.c editor.c $(LDFLAGS)
	../_build/tiny-ide-tests/test_editor

check-smoke: $(TARGET) cmini
	CMINI="$(abspath $(CMINI))" python3 -I tests/smoke.py ./$(TARGET)

check: check-unit check-smoke

clean:
	rm -f $(TARGET)
