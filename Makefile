# `make`      -> build for this machine      (output in bin/<arch>/)
# `make pi4`  -> cross-build for Raspberry Pi 4 (output in bin/aarch64/)
TARGET ?= native

# OPENSSL=0 uses liboqs's built-in AES/SHA code. Keep it identical on both machines so
# results are comparable; set OPENSSL=1 only if you need libcrypto (e.g. the AEAD helper).
OPENSSL ?= 0
ifeq ($(OPENSSL),1)
  OQS_OPENSSL  := ON
  OPENSSL_LIBS := -lcrypto
else
  OQS_OPENSSL  := OFF
  OPENSSL_LIBS :=
endif

ifeq ($(TARGET),pi4)
  SUB_DIR := pi4
  CXX  := aarch64-linux-gnu-g++
  ARCH := aarch64
  OQS_CMAKE_ARGS := -DCMAKE_SYSTEM_NAME=Linux -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
                    -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
                    -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
  LINK_EXTRA := -static-libstdc++ -static-libgcc
else
  SUB_DIR := pc
  ARCH := $(shell uname -m)
  OQS_CMAKE_ARGS :=
  LINK_EXTRA :=
endif

BUILD   := build/$(SUB_DIR)
BIN_DIR := bin/$(SUB_DIR)

# --- liboqs (submodule), built and installed once per architecture ---
OQS_SRC   := extern/liboqs
OQS_BUILD := build/oqs-$(ARCH)
OQS_DIR   := third_party/liboqs-$(ARCH)
OQS_LIB   := $(OQS_DIR)/lib/liboqs.a

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -MMD -MP -pthread -I Inc -isystem $(OQS_DIR)/include
LDFLAGS  := -L$(OQS_DIR)/lib $(LINK_EXTRA) -pthread
LDLIBS   := -loqs $(OPENSSL_LIBS)

COMMON_SRCS := Src/packet_types.cpp Src/measurement.cpp \
               Src/tcp.cpp Src/udp.cpp Src/test_orchestrator.cpp \
               Src/server.cpp Src/user_interface.cpp \
               Src/kem.cpp Src/frame.cpp
COMMON_OBJS := $(patsubst Src/%.cpp,$(BUILD)/%.o,$(COMMON_SRCS))

BINS := $(BIN_DIR)/client $(BIN_DIR)/server $(BIN_DIR)/kem_tester

all: $(BINS)

pi4:
	$(MAKE) TARGET=pi4 all

$(BUILD) $(BIN_DIR):
	mkdir -p $@

# Build and install liboqs once per architecture (takes a few minutes the first time)
$(OQS_LIB):
	cmake -S $(OQS_SRC) -B $(OQS_BUILD) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX=$(CURDIR)/$(OQS_DIR) \
	  -DOQS_BUILD_ONLY_LIB=ON \
	  -DOQS_USE_OPENSSL=$(OQS_OPENSSL) \
	  -DBUILD_SHARED_LIBS=OFF \
	  $(OQS_CMAKE_ARGS)
	cmake --build $(OQS_BUILD) -j$$(nproc)
	cmake --install $(OQS_BUILD)

# liboqs headers must exist before any object is compiled
$(BUILD)/%.o: Src/%.cpp | $(BUILD) $(OQS_LIB)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -loqs goes AFTER the objects, otherwise the linker reports undefined references
$(BIN_DIR)/client: $(COMMON_OBJS) $(BUILD)/client_main.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(BIN_DIR)/server: $(COMMON_OBJS) $(BUILD)/server_main.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

$(BIN_DIR)/kem_tester: $(BUILD)/kem.o $(BUILD)/kem_tester.o | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

# Header changes trigger rebuilds of the objects that include them
-include $(wildcard $(BUILD)/*.d)

clean:
	rm -rf build bin client server kem_tester

# Also removes the installed liboqs builds (the next make rebuilds them)
distclean: clean
	rm -rf third_party

.PHONY: all pi4 clean distclean