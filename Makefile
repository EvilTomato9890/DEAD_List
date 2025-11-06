CXX := g++

CXXFLAGS := -Iinclude -Wall -Wextra -DVERIFY_DEBUG
LDFLAGS := 

SRC_DIR := source
BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/target

SOURCES := $(wildcard $(SRC_DIR)/*.cpp)
OBJECTS := $(SOURCES:$(SRC_DIR)/%.cpp=$(BUILD_DIR)/%.o)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CXX) $(LDFLAGS) $(OBJECTS) $(LDLIBS) -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cpp | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) -c $< -o $@

$(BUILD_DIR):
	mkdir -p $@

$(BIN_DIR):
	mkdir -p $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: clean debug release vars

# Добавьте в makefile
check:
	@echo "Checking files..."
	@ls -la $(SRC_DIR)/
	@echo "Object files:"
	@ls -la $(BUILD_DIR)/ || echo "Build dir doesn't exist"
	@echo "Binary info:"
	@file $(TARGET) 2>/dev/null || echo "Target doesn't exist"

run: $(TARGET)
	@echo "Running $(TARGET)..."
	./$(TARGET)