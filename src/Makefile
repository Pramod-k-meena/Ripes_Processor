CXX = g++
CXXFLAGS = -Wall -Wextra -O2 -std=c++17

HEADERS = Processor.hpp
SOURCES = Processor.cpp

TARGET_FORWARD = forward
TARGET_NOFORWARD = noforward

all: clean $(TARGET_FORWARD) $(TARGET_NOFORWARD)

$(TARGET_FORWARD): forward.cpp $(SOURCES) $(HEADERS)
	@$(CXX) $(CXXFLAGS) -o $@ forward.cpp $(SOURCES)

$(TARGET_NOFORWARD): noforward.cpp $(SOURCES) $(HEADERS)
	@$(CXX) $(CXXFLAGS) -o $@ noforward.cpp $(SOURCES)

clean:
	@rm -f $(TARGET_FORWARD) $(TARGET_NOFORWARD)