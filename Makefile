TARGET=theming
PREFIX ?= /usr/local

CFLAGS=-Wall -Wextra -pedantic -Wno-unused-parameter -Wno-unused-value -Wshadow -Wdouble-promotion -Wformat=2 -Wformat-overflow -Wformat-truncation=2 -Wundef -fno-common -Wconversion
LDFLAGS=-lm

ifdef DEBUG
CFLAGS+=-g3
endif

ifdef RELEASE
CFLAGS+=-O3 -ffunction-sections -fdata-sections
endif

BUILD_DIR=build
SRC_DIR=src
INCLUDES=-I$(BUILD_DIR) -Iinclude
SRC=$(wildcard $(SRC_DIR)/*.c)
OBJ=$(patsubst $(SRC_DIR)/%.c, $(BUILD_DIR)/%.o, $(SRC))

MKDIR_P=mkdir -p
RM_RF=rm -rf

$(BUILD_DIR)/$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

$(BUILD_DIR):
	$(MKDIR_P) $(BUILD_DIR)

.PHONY: clean
clean:
	$(RM_RF) $(BUILD_DIR)

.PHONY: debug
debug:
	$(MAKE) DEBUG=1

.PHONY: release
release:
	$(MAKE) RELEASE=1

.PHONY: compile_commands
compile_commands: clean | $(BUILD_DIR)
	bear -- make debug
	mv compile_commands.json $(BUILD_DIR)/compile_commands.json

install:
	$(MAKE) release
	mkdir -p ${DESTDIR}${PREFIX}/bin
	cp -f $(BUILD_DIR)/$(TARGET) ${DESTDIR}${PREFIX}/bin
	chmod 755 ${DESTDIR}${PREFIX}/bin/$(TARGET)

uninstall:
	rm -f ${DESTDIR}${PREFIX}/bin/$(TARGET)
