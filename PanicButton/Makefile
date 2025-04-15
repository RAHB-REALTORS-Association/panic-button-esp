# Project name
PROJECT_NAME = PanicAlarm

# Board settings
FQBN = esp32:esp32:dfrobot_beetle_esp32c6:PartitionScheme=min_spiffs
PORT = /dev/cu.usbserial-0001  # Change this to match your port

# Paths
SKETCH_DIR = .
BUILD_DIR = ./build
OUTPUT_DIR = ./output

# Build flags (similar to PlatformIO's build_flags)
BUILD_FLAGS = -DESP32_C6 -DHAS_BATTERY_MONITORING=1 -DENABLE_DEEP_SLEEP=false \
              -DCONFIG_IDF_TARGET_ESP32C6 -fexceptions

# Arduino CLI command
ARDUINO_CLI = arduino-cli
COMPILE_ARGS = --fqbn $(FQBN) --build-path $(BUILD_DIR) --output-dir $(OUTPUT_DIR) --build-properties "build.extra_flags=$(BUILD_FLAGS)"
UPLOAD_ARGS = --fqbn $(FQBN) --port $(PORT)

# Default target
all: compile

# Create directories
dirs:
	@mkdir -p $(BUILD_DIR) $(OUTPUT_DIR)

# Compile sketch
compile: dirs
	@echo "Compiling $(PROJECT_NAME)..."
	$(ARDUINO_CLI) compile $(COMPILE_ARGS) $(SKETCH_DIR)

# Upload sketch
upload: compile
	@echo "Uploading $(PROJECT_NAME)..."
	$(ARDUINO_CLI) upload $(UPLOAD_ARGS) $(SKETCH_DIR)

# Clean build files
clean:
	@echo "Cleaning build directories..."
	@rm -rf $(BUILD_DIR) $(OUTPUT_DIR)

# Monitor serial port
monitor:
	$(ARDUINO_CLI) monitor --port $(PORT) --config baudrate=115200

# Show board info
board-info:
	$(ARDUINO_CLI) board details -b $(FQBN)

# Check arduino-cli version
version:
	$(ARDUINO_CLI) version

# Help
help:
	@echo "Available targets:"
	@echo "  all       - Default target (same as compile)"
	@echo "  compile   - Compile the sketch"
	@echo "  upload    - Compile and upload the sketch"
	@echo "  clean     - Remove build files"
	@echo "  monitor   - Start serial monitor"
	@echo "  board-info - Display board information"
	@echo "  version   - Display arduino-cli version"
	@echo "  help      - Display this help message"

.PHONY: all dirs compile upload clean monitor board-info version help