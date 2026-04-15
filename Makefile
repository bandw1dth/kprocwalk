SHELL := cmd.exe
.SHELLFLAGS := /c

DRIVER_NAME := driver
ENTRY_POINT := DriverEntry

WDK_DIR = C:/Program Files (x86)/Windows Kits/10/
WDK_VER := 10.0.26100.0
WDK_LIB := $(WDK_DIR)/Lib/$(WDK_VER)/km/x64
WDK_INC := $(WDK_DIR)/Include/$(WDK_VER)

SRC_DIR := src
OUT_DIR := out

SRCS := $(wildcard $(SRC_DIR)/*.cpp)
OBJS := $(patsubst $(SRC_DIR)/%.cpp,$(OUT_DIR)/%.obj,$(SRCS))

INCLUDES := -isystem "$(WDK_INC)/km" \
            -isystem "$(WDK_INC)/km/crt" \
            -isystem "$(WDK_INC)/shared" \
            -I"$(SRC_DIR)"

CXXFLAGS := $(INCLUDES) \
            -target x86_64-pc-windows-msvc \
            -D_AMD64_ \
            -D_WIN64 \
            -std=c++23 \
            -ffreestanding \
            -fno-stack-protector \
            -fno-exceptions \
            -fno-rtti \
            -fno-threadsafe-statics \
            -fno-asynchronous-unwind-tables \
            -fno-unwind-tables \
            -fno-builtin \
            -ffunction-sections \
            -fdata-sections \
            -Wall -Wextra -Werror

LDFLAGS := /SUBSYSTEM:NATIVE \
           /ENTRY:$(ENTRY_POINT) \
           /NODEFAULTLIB \
           /MERGE:.rdata=.text

ifeq ($(DEBUG), 1)
    CXXFLAGS += -O0 -gcodeview -D_DEBUG -DDBG=1
    LDFLAGS += /DEBUG:FULL /PDBALTPATH:%_PDB% /PDB:"$(OUT_DIR)/$(DRIVER_NAME).pdb"
else
    CXXFLAGS += -O2 -flto=thin
    LDFLAGS += /OPT:REF /OPT:ICF
endif

all: $(OUT_DIR)/$(DRIVER_NAME).sys

$(OUT_DIR)/$(DRIVER_NAME).sys: $(OBJS) | $(OUT_DIR)
	lld-link $(LDFLAGS) /LIBPATH:"$(WDK_LIB)" $^ ntoskrnl.lib hal.lib /OUT:$@

$(OUT_DIR)/%.obj: $(SRC_DIR)/%.cpp | $(OUT_DIR)
	clang++ $(CXXFLAGS) -c $< -o $@

$(OUT_DIR):
	@if not exist $(OUT_DIR) mkdir $(OUT_DIR)

clean:
	@if exist $(OUT_DIR) rmdir /s /q $(OUT_DIR)

.PHONY: all clean
