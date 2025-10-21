CXX = clang++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -I.
LDFLAGS = 

TARGET = aritest
SOURCES = AriTest.cpp HNSW.cpp
OBJECTS = $(SOURCES:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CXX) $(OBJECTS) -o $(TARGET) $(LDFLAGS)

%.o: %.cpp HNSW.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run