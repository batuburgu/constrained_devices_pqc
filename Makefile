CXX := g++

# --- liboqs (built from the extern/liboqs submodule, installed locally) ---
OQS_SRC   := extern/liboqs
OQS_BUILD := $(OQS_SRC)/build
OQS_DIR   := third_party/liboqs
OQS_LIB   := $(OQS_DIR)/lib/liboqs.a

CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I Inc -isystem $(OQS_DIR)/include
LDFLAGS  := -L$(OQS_DIR)/lib
LDLIBS   := -loqs -lcrypto -lpthread

BUILD := build

COMMON_SRCS := Src/packet_types.cpp Src/measurement.cpp \
               Src/tcp.cpp Src/udp.cpp Src/test_orchestrator.cpp \
               Src/echo_server.cpp Src/user_interface.cpp \
			   Src/kem.cpp
COMMON_OBJS := $(patsubst Src/%.cpp,$(BUILD)/%.o,$(COMMON_SRCS))

all: client server kem_tester

$(BUILD):
	mkdir -p $(BUILD)

# Build and install liboqs once (takes a few minutes the first time)
$(OQS_LIB):
	cmake -S $(OQS_SRC) -B $(OQS_BUILD) \
	  -DCMAKE_BUILD_TYPE=Release \
	  -DCMAKE_INSTALL_PREFIX=$(CURDIR)/$(OQS_DIR) \
	  -DOQS_BUILD_ONLY_LIB=ON \
	  -DOQS_USE_OPENSSL=ON \
	  -DBUILD_SHARED_LIBS=OFF
	cmake --build $(OQS_BUILD) -j$$(nproc)
	cmake --install $(OQS_BUILD)

# liboqs headers must exist before any object is compiled
$(BUILD)/%.o: Src/%.cpp | $(BUILD) $(OQS_LIB)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# -loqs goes AFTER the objects, otherwise the linker reports undefined references
client: $(COMMON_OBJS) $(BUILD)/client_main.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

server: $(COMMON_OBJS) $(BUILD)/server_main.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

kem_tester: $(BUILD)/kem.o $(BUILD)/kem_tester.o
	$(CXX) $(CXXFLAGS) $^ -o $@ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(BUILD) client server kem_tester

# Removes the liboqs build too (the next make will rebuild it)
distclean: clean
	rm -rf $(OQS_BUILD) third_party

.PHONY: all clean distclean