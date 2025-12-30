# =========================================================
# Forge – Cross-Platform Production Makefile (FINAL v1.0)
# =========================================================

# ---------------- Compiler ----------------
CC      := gcc
AR      := ar
STD 		:= -std=c11
WARN    := -Wall -Wextra
OPT     := -O2
CFLAGS  := $(STD) $(WARN) $(OPT)

# ---------------- OS Detection ----------------
ifeq ($(OS),Windows_NT)
    IS_WINDOWS := 1
else
    IS_WINDOWS := 0
endif

# ---------------- Platform Flags ----------------
ifeq ($(IS_WINDOWS),1)
    EXE     := .exe
    LDFLAGS := -lws2_32 -lwininet
    MKDIR_P := powershell -Command "New-Item -ItemType Directory -Force -Path"
    RM_RF   := powershell -Command "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -Path"
else
    EXE     :=
    LDFLAGS := -pthread
    MKDIR_P := mkdir -p
    RM_RF   := rm -rf
    CFLAGS  += -D_FORTIFY_SOURCE=2
endif

# ---------------- Directories ----------------
BUILD_DIR  := build
CORE_DIR   := framework-core
PM_DIR     := pm-tool
APP_DIR    := user-app
TEST_DIR   := tests
VENDOR_DIR := $(APP_DIR)/vendor/forge/0.1.0

# =========================================================
# Framework Core
# =========================================================
CORE_INC := -I$(CORE_DIR)/include
CORE_SRC := \
    $(CORE_DIR)/src/forge_server.c \
    $(CORE_DIR)/src/forge_router.c \
    $(CORE_DIR)/src/forge_http.c

CORE_OBJ := $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)
CORE_LIB := $(BUILD_DIR)/libforge.a

# =========================================================
# Package Manager - FIXED PATHS
# =========================================================
PM_INC := -I$(PM_DIR)/include $(CORE_INC)
PM_SRC := \
    $(PM_DIR)/src/forge_pm.c \
    $(PM_DIR)/src/downloader.c \
    $(PM_DIR)/src/extractor.c \
    $(PM_DIR)/src/sha256.c \
    $(PM_DIR)/src/forge_lock.c \
    $(PM_DIR)/src/forge_manifest.c

# ✅ FIXED: Direct object paths (no substitution)
PM_OBJ := \
    $(BUILD_DIR)/pm-tool/src/forge_pm.o \
    $(BUILD_DIR)/pm-tool/src/downloader.o \
    $(BUILD_DIR)/pm-tool/src/extractor.o \
    $(BUILD_DIR)/pm-tool/src/sha256.o \
    $(BUILD_DIR)/pm-tool/src/forge_lock.o \
    $(BUILD_DIR)/pm-tool/src/forge_manifest.o

PM_BIN := $(BUILD_DIR)/forge-pm$(EXE)

# =========================================================
# User Application
# =========================================================
APP_INC := -I$(VENDOR_DIR)/include
APP_BIN := $(BUILD_DIR)/user-app$(EXE)

# =========================================================
# Targets
# =========================================================
all: framework pm app test integration

# Framework Core Library
framework: $(CORE_LIB)
$(CORE_LIB): $(CORE_OBJ)
	@$(MKDIR_P) "$(BUILD_DIR)"
	$(AR) rcs $@ $^
	@echo "✅ $(CORE_LIB) built"

# Package Manager ✅ FIXED
pm: framework $(PM_OBJ)
	@$(MKDIR_P) "$(BUILD_DIR)/pm-tool"
	$(CC) $(CFLAGS) $(PM_INC) $(PM_OBJ) $(CORE_LIB) -o "$(PM_BIN)" $(LDFLAGS)
	@echo "✅ $(PM_BIN) built"

# User Application ✅ SINGLE + CLEAN
app: framework
	@$(MKDIR_P) "$(BUILD_DIR)/user-app"
	$(CC) $(CFLAGS) $(APP_INC) "$(APP_DIR)/src/main.c" "$(CORE_LIB)" \
		-o "$(APP_BIN)" $(LDFLAGS)
	@echo "✅ $(APP_BIN) built"

# ---------------- Object Compilation ----------------
$(BUILD_DIR)/framework-core/src/%.o: $(CORE_DIR)/src/%.c
	@$(MKDIR_P) "$(dir $@)"
	$(CC) $(CFLAGS) $(CORE_INC) -c $< -o $@

$(BUILD_DIR)/pm-tool/src/%.o: $(PM_DIR)/src/%.c
	@$(MKDIR_P) "$(dir $@)"
	$(CC) $(CFLAGS) $(PM_INC) -c $< -o $@

# ---------------- Tests ----------------
test: framework
	$(CC) $(CFLAGS) -O0 $(TEST_INC) \
		$(TEST_DIR)/test_pm.c \
		$(CORE_LIB) \
		-o $(TEST_BIN) $(LDFLAGS)
	@$(TEST_BIN)

integration: framework pm
	$(CC) $(CFLAGS) -O0 -I$(TEST_DIR) \
		$(TEST_DIR)/test_integration.c \
		$(CORE_LIB) \
		-o $(INT_BIN) $(LDFLAGS)
	@$(INT_BIN)

# ---------------- Utility ----------------
stop:
	taskkill /IM user-app.exe /F || exit 0

clean:
	@$(RM_RF) "$(BUILD_DIR)"
	@echo "🧹 Cleaned"

run-app: app
ifeq ($(IS_WINDOWS),1)
	$(APP_BIN)
else
	./$(APP_BIN)
endif

run-pm: pm
ifeq ($(IS_WINDOWS),1)
	$(PM_BIN)
else
	./$(PM_BIN)
endif

.PHONY: all framework pm app test integration clean run-app run-pm
