# Compilers
WIN_GCC = x86_64-w64-mingw32-gcc
LINUX_GCC = gcc-13

# Flags
CORE_FLAGS = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wstrict-prototypes -Wmissing-prototypes -std=c17 -Isrc
DEBUG_FLAGS = $(CORE_FLAGS) -fno-omit-frame-pointer -O0 -g
LINUX_DEBUG_FLAGS = $(DEBUG_FLAGS) -fsanitize=address,undefined
WIN_DEBUG_FLAGS = $(DEBUG_FLAGS) -fsanitize=undefined -fsanitize-undefined-trap-on-error
RELEASE_FLAGS = $(CORE_FLAGS) -DNDEBUG -O2

# Libraries
CORE_LIBS = -lm
COMMUNICATION_LIBS = 
WIN_LIBS = $(CORE_LIBS) $(COMMUNICATION_LIBS)
LINUX_LIBS = $(CORE_LIBS) $(COMMUNICATION_LIBS)

# Directories
SRC_DIR = src
BUILD_DIR = build
WIN_DEBUG_DIR = $(BUILD_DIR)/debug/windows
WIN_DEBUG_OBJ_DIR = $(WIN_DEBUG_DIR)/obj
WIN_RELEASE_DIR = $(BUILD_DIR)/release/windows
WIN_RELEASE_OBJ_DIR = $(WIN_RELEASE_DIR)/obj
LINUX_DEBUG_DIR = $(BUILD_DIR)/debug/linux
LINUX_DEBUG_OBJ_DIR = $(LINUX_DEBUG_DIR)/obj
LINUX_RELEASE_DIR = $(BUILD_DIR)/release/linux
LINUX_RELEASE_OBJ_DIR = $(LINUX_RELEASE_DIR)/obj

# Sources
CORE_SRCS = $(shell find $(SRC_DIR)/core -type f -name '*.c')
COMMUNICATION_SRCS = $(shell find $(SRC_DIR)/platform_communication -type f -name '*.c')
WIN_SRCS = $(CORE_SRCS) $(COMMUNICATION_SRCS) \
	$(shell find $(SRC_DIR)/platforms/windows -type f -name '*.c')
LINUX_SRCS = $(CORE_SRCS) $(COMMUNICATION_SRCS) \
	$(shell find $(SRC_DIR)/platforms/linux -type f -name '*.c')

# Objects
WIN_DEBUG_OBJS = $(patsubst $(SRC_DIR)/%.c, $(WIN_DEBUG_OBJ_DIR)/%.o, $(WIN_SRCS))
WIN_RELEASE_OBJS = $(patsubst $(SRC_DIR)/%.c, $(WIN_RELEASE_OBJ_DIR)/%.o, $(WIN_SRCS))
LINUX_DEBUG_OBJS = $(patsubst $(SRC_DIR)/%.c, $(LINUX_DEBUG_OBJ_DIR)/%.o, $(LINUX_SRCS))
LINUX_RELEASE_OBJS = $(patsubst $(SRC_DIR)/%.c, $(LINUX_RELEASE_OBJ_DIR)/%.o, $(LINUX_SRCS))

# Dependencies
WIN_DEBUG_DEPS = $(WIN_DEBUG_OBJS:%.o=%.d)
WIN_RELEASE_DEPS = $(WIN_RELEASE_OBJS:%.o=%.d)
LINUX_DEBUG_DEPS = $(LINUX_DEBUG_OBJS:%.o=%.d)
LINUX_RELEASE_DEPS = $(LINUX_RELEASE_OBJS:%.o=%.d)

# Binaries
WIN_DEBUG_BIN = $(WIN_DEBUG_DIR)/archent-engine.exe
WIN_RELEASE_BIN = $(WIN_RELEASE_DIR)/archent-engine.exe
LINUX_DEBUG_BIN = $(LINUX_DEBUG_DIR)/archent-engine
LINUX_RELEASE_BIN = $(LINUX_RELEASE_DIR)/archent-engine

# Targets
.PHONY: all run clean \
	win-debug win-debug-run win-release win-release-run win-clean \
	linux-debug linux-debug-run linux-release linux-release-run linux-clean

all: win-debug

run: win-debug-run

clean: win-clean linux-clean
	@echo "All build artifacts cleaned!"

win-debug: $(WIN_DEBUG_BIN)
	@echo "Windows debug build complete: $<"

win-debug-run: win-debug
	@wine "$(WIN_DEBUG_BIN)"

win-release: $(WIN_RELEASE_BIN)
	@echo "Windows release build complete: $<"

win-release-run: win-release
	@wine "$(WIN_RELEASE_BIN)"

win-clean:
	@rm -rf $(WIN_DEBUG_DIR) $(WIN_RELEASE_DIR)
	@echo "Windows build artifacts cleaned!"

linux-debug: $(LINUX_DEBUG_BIN)
	@echo "Linux debug build complete: $<"

linux-debug-run: linux-debug
	@"$(LINUX_DEBUG_BIN)"

linux-release: $(LINUX_RELEASE_BIN)
	@echo "Linux release build complete: $<"

linux-release-run: linux-release
	@"$(LINUX_RELEASE_BIN)"

linux-clean:
	@rm -rf $(LINUX_DEBUG_DIR) $(LINUX_RELEASE_DIR)
	@echo "Linux build artifacts cleaned!"

# Directory Creation
$(WIN_DEBUG_DIR) $(WIN_RELEASE_DIR) $(LINUX_DEBUG_DIR) $(LINUX_RELEASE_DIR):
	@mkdir -p $@

# Compilation
$(WIN_DEBUG_OBJS): $(WIN_DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(WIN_GCC) $(WIN_DEBUG_FLAGS) -MMD -MP -c $< -o $@

$(WIN_RELEASE_OBJS): $(WIN_RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(WIN_GCC) $(RELEASE_FLAGS) -MMD -MP -c $< -o $@

$(LINUX_DEBUG_OBJS): $(LINUX_DEBUG_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(LINUX_GCC) $(LINUX_DEBUG_FLAGS) -MMD -MP -c $< -o $@

$(LINUX_RELEASE_OBJS): $(LINUX_RELEASE_OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(@D)
	$(LINUX_GCC) $(RELEASE_FLAGS) -MMD -MP -c $< -o $@

# Linking
$(WIN_DEBUG_BIN): $(WIN_DEBUG_OBJS) | $(WIN_DEBUG_DIR)
	$(WIN_GCC) $(WIN_DEBUG_FLAGS) $^ $(WIN_LIBS) -o $@

$(WIN_RELEASE_BIN): $(WIN_RELEASE_OBJS) | $(WIN_RELEASE_DIR)
	$(WIN_GCC) $(RELEASE_FLAGS) $^ $(WIN_LIBS) -o $@

$(LINUX_DEBUG_BIN): $(LINUX_DEBUG_OBJS) | $(LINUX_DEBUG_DIR)
	$(LINUX_GCC) $(LINUX_DEBUG_FLAGS) $^ $(LINUX_LIBS) -o $@

$(LINUX_RELEASE_BIN): $(LINUX_RELEASE_OBJS) | $(LINUX_RELEASE_DIR)
	$(LINUX_GCC) $(RELEASE_FLAGS) $^ $(LINUX_LIBS) -o $@

# Includes
-include $(WIN_DEBUG_DEPS)
-include $(WIN_RELEASE_DEPS)
-include $(LINUX_DEBUG_DEPS)
-include $(LINUX_RELEASE_DEPS)