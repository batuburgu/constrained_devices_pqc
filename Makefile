CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2 -I Inc
BUILD := build

COMMON_SRCS := Src/PacketTypes.cpp Src/Measurement.cpp \
               Src/TCProtocol.cpp Src/UDProtocol.cpp Src/TestOrchestrator.cpp \
               Src/EchoServer.cpp Src/UserInterface.cpp
COMMON_OBJS := $(patsubst Src/%.cpp,$(BUILD)/%.o,$(COMMON_SRCS))

all: $(BUILD) client server

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/%.o: Src/%.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

client: $(COMMON_OBJS) $(BUILD)/ClientMain.o
	$(CXX) $(CXXFLAGS) $^ -o $@

server: $(COMMON_OBJS) $(BUILD)/ServerMain.o
	$(CXX) $(CXXFLAGS) $^ -o $@

clean:
	rm -rf $(BUILD) client server

.PHONY: all clean
