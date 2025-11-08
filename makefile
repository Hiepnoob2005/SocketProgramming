# Makefile for Remote Control Socket Programming Project

# Detect OS
ifeq ($(OS),Windows_NT)
    DETECTED_OS := Windows
    EXE_EXT := .exe
    RM := del /Q
    SEP := \\
else
    DETECTED_OS := $(shell uname -s)
    EXE_EXT :=
    RM := rm -f
    SEP := /
endif

# Compiler
CXX := g++

# Compiler flags
CXXFLAGS := -std=c++11 -Wall -Wextra -pthread

# Platform-specific flags
ifeq ($(DETECTED_OS),Windows)
    LDFLAGS := -lws2_32 -lpsapi -lgdiplus -lgdi32
else
    LDFLAGS := -pthread
    # Add X11 for Linux screenshot support
    LDFLAGS += -lX11
endif

# Target executables
SERVER := server$(EXE_EXT)
CLIENT := client$(EXE_EXT)
CLIENT_GUI := client_gui$(EXE_EXT)

# Source files
SERVER_SRC := server.cpp
CLIENT_SRC := client.cpp
CLIENT_GUI_SRC := client_gui.cpp

# Header files
HEADERS := common.h

# Default target
all: $(SERVER) $(CLIENT) $(CLIENT_GUI)
	@echo "Build complete!"
	@echo "OS detected: $(DETECTED_OS)"
	@echo "Executables created:"
	@echo "  - $(SERVER) : Remote control server (run on target machine)"
	@echo "  - $(CLIENT) : Console client (run on control machine)"
	@echo "  - $(CLIENT_GUI) : Web interface client (run on control machine)"

# Build server
$(SERVER): $(SERVER_SRC) $(HEADERS)
	@echo "Building server..."
	$(CXX) $(CXXFLAGS) $(SERVER_SRC) -o $(SERVER) $(LDFLAGS)
	@echo "Server built successfully!"

# Build console client
$(CLIENT): $(CLIENT_SRC) $(HEADERS)
	@echo "Building console client..."
	$(CXX) $(CXXFLAGS) $(CLIENT_SRC) -o $(CLIENT) $(LDFLAGS)
	@echo "Console client built successfully!"

# Build GUI client
$(CLIENT_GUI): $(CLIENT_GUI_SRC) $(HEADERS)
	@echo "Building web interface client..."
	$(CXX) $(CXXFLAGS) $(CLIENT_GUI_SRC) -o $(CLIENT_GUI) $(LDFLAGS)
	@echo "Web interface client built successfully!"

# Clean build artifacts
clean:
	@echo "Cleaning..."
ifeq ($(DETECTED_OS),Windows)
	-$(RM) $(SERVER) $(CLIENT) $(CLIENT_GUI) *.o 2>NUL
else
	-$(RM) $(SERVER) $(CLIENT) $(CLIENT_GUI) *.o
endif
	@echo "Clean complete!"

# Run server
run-server: $(SERVER)
	@echo "Starting server on port 8888..."
	./$(SERVER)

# Run console client
run-client: $(CLIENT)
	@echo "Starting console client..."
	./$(CLIENT)

# Run web client
run-web: $(CLIENT_GUI)
	@echo "Starting web interface client..."
	@echo "Open browser at http://localhost:8080"
	./$(CLIENT_GUI)

# Install dependencies (Linux only)
install-deps:
ifeq ($(DETECTED_OS),Linux)
	@echo "Installing dependencies for Linux..."
	sudo apt-get update
	sudo apt-get install -y build-essential libx11-dev imagemagick
else ifeq ($(DETECTED_OS),Darwin)
	@echo "Installing dependencies for macOS..."
	brew install imagemagick
else
	@echo "Windows: Please ensure MinGW-w64 is installed"
endif

# Help target
help:
	@echo "==============================================="
	@echo "Remote Control Socket Programming - Makefile"
	@echo "==============================================="
	@echo "Available targets:"
	@echo "  make all         - Build all executables"
	@echo "  make server      - Build only server"
	@echo "  make client      - Build only console client"
	@echo "  make client_gui  - Build only web interface client"
	@echo "  make clean       - Remove all built files"
	@echo "  make run-server  - Build and run server"
	@echo "  make run-client  - Build and run console client"
	@echo "  make run-web     - Build and run web interface"
	@echo "  make install-deps- Install required dependencies"
	@echo "  make help        - Show this help message"
	@echo ""
	@echo "Usage:"
	@echo "  1. On target machine: make run-server"
	@echo "  2. On control machine: make run-client or make run-web"
	@echo "==============================================="

.PHONY: all clean run-server run-client run-web install-deps help