# ======================================
# Forge Project – Cross-Platform Makefile (PRODUCTION)
# ======================================

CC := gcc
AR := ar
CFLAGS := -Wall -Wextra -O2 -std=c99 -D_FORTIFY_SOURCE=2

# ---------- OS Detection ----------
ifeq ($(OS),Windows_NT)
    IS_WINDOWS := 1
else
    IS_WINDOWS := 0
endif

# ---------- Platform Commands ----------
ifeq ($(IS_WINDOWS),1)
    MKDIR_P := powershell -Command "New-Item -ItemType Directory -Force -Path"
    RM_RF   := powershell -Command "Remove-Item -Recurse -Force -ErrorAction SilentlyContinue -Path"
    CP      := powershell -Command "Copy-Item -Force"
    COPY_HEADERS = powershell -Command "Copy-Item '$(CORE_DIR)/include/*' '$(VENDOR_DIR)/include/' -Force -ErrorAction SilentlyContinue"
    LDFLAGS := -lws2_32 -lwininet -mconsole
    EXE     := .exe
    SILENT  := >nul 2>&1
else
    MKDIR_P := mkdir -p
    RM_RF   := rm -rf
    CP      := cp -f
    COPY_HEADERS = cp -f $(CORE_DIR)/include/* $(VENDOR_DIR)/include/
    LDFLAGS := -pthread
    EXE     :=
    SILENT  := >/dev/null 2>&1
endif

# ---------- Directories ----------
BUILD_DIR := build
CORE_DIR  := framework-core
PM_DIR    := pm-tool
APP_DIR   := user-app
VENDOR_DIR := $(APP_DIR)/vendor/forge/0.1.0

# ---------- Framework Core ----------
CORE_INC := -I$(CORE_DIR)/include
CORE_SRC := \
    $(CORE_DIR)/src/forge_server.c \
    $(CORE_DIR)/src/forge_router.c \
    $(CORE_DIR)/src/forge_http.c

CORE_OBJ := $(CORE_SRC:%.c=$(BUILD_DIR)/%.o)
CORE_LIB := $(BUILD_DIR)/libforge.a

# ---------- Package Manager (PM) ----------
PM_INC := -I$(PM_DIR)/include $(CORE_INC)
# ✅ FIXED: EXCLUDE stubs.c from production build
PM_SRC := \
    $(PM_DIR)/forge_pm.c \
    $(PM_DIR)/downloader.c \
    $(PM_DIR)/extractor.c \
    $(PM_DIR)/sha256.c \
    $(PM_DIR)/forge_lock.c \
    $(PM_DIR)/forge_manifest.c

PM_OBJ := $(PM_SRC:%.c=$(BUILD_DIR)/%.o)
PM_BIN := $(BUILD_DIR)/forge-pm$(EXE)

# ---------- User App ----------
APP_INC := -I$(VENDOR_DIR)/include
APP_BIN := $(BUILD_DIR)/user-app$(EXE)

# ======================================
# Targets
# ======================================
all: framework pm app

# Build the static library
framework: $(CORE_LIB)

$(CORE_LIB): $(CORE_OBJ)
	@$(MKDIR_P) "$(BUILD_DIR)"
	$(AR) rcs $@ $^

# ✅ FIXED: Proper PM target (EXCLUDES stubs.c)
pm: framework $(PM_OBJ)
	$(CC) $(CFLAGS) $(PM_OBJ) $(CORE_LIB) -o $(PM_BIN) $(LDFLAGS)
	@echo "✅ Built $(PM_BIN)"

# ✅ FIXED: Universal compilation rule
$(BUILD_DIR)/%.o: %.c
	@$(MKDIR_P) "$(dir $@)"
	$(CC) $(CFLAGS) $(PM_INC) -c $< -o $@

# Install headers and lib to user-app vendor folder
install-framework: framework
	@$(MKDIR_P) "$(VENDOR_DIR)/include"
	@$(MKDIR_P) "$(VENDOR_DIR)/lib"
	@$(COPY_HEADERS)
	@$(CP) "$(CORE_LIB)" "$(VENDOR_DIR)/lib/"

# Build the final user application
app: install-framework
	$(CC) $(CFLAGS) $(APP_DIR)/src/main.c $(APP_INC) -L$(VENDOR_DIR)/lib -lforge -o $(APP_BIN) $(LDFLAGS)
	@echo "✅ Built $(APP_BIN)"



# ✅ FIXED: Windows-safe clean
clean:
ifeq ($(IS_WINDOWS),1)
	@powershell -Command "Remove-Item -Recurse -Force -Path '$(BUILD_DIR)' -ErrorAction SilentlyContinue; Write-Output '🧹 Cleaned build directory'"
else
	@$(RM_RF) "$(BUILD_DIR)"
	@echo "🧹 Cleaned build directory"
endif


# ---------- Tests ----------
TEST_DIR := tests
TEST_INC := -I$(TEST_DIR) -I$(PM_DIR)/include $(CORE_INC)
TEST_BIN := $(BUILD_DIR)/tests$(EXE)

test: framework
	@$(MKDIR_P) "$(TEST_DIR)"
	@$(CC) $(CFLAGS) -O0 $(TEST_INC) $(TEST_DIR)/test_pm.c $(CORE_LIB) -o $(TEST_BIN) $(LDFLAGS)
	@echo "🧪 Running tests..."
	@$(TEST_BIN)
	@echo "✅ All tests PASSED!"

# Update 'all' target
all: framework pm app test

# ---------- Integration Tests ----------
integration: framework pm
	@$(MKDIR_P) "$(TEST_DIR)"
	@$(CC) $(CFLAGS) -O0 -I$(TEST_DIR) $(TEST_DIR)/test_integration.c -o $(BUILD_DIR)/test_integration$(EXE) $(LDFLAGS)
	@echo "🔗 Running integration tests..."
	@$(BUILD_DIR)/test_integration$(EXE)
	@echo "✅ Integration tests PASSED!"


# Update 'all' target
all: framework pm app test integration


run-app: app
	./$(APP_BIN)

run-pm: pm
	./$(PM_BIN)

.PHONY: all framework pm app clean install-framework run-app run-pm
