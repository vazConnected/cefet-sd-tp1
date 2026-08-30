CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
BIN := bin

all: $(BIN)/sender

$(BIN)/sender: src/signals/sender.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN):
	mkdir -p $@

clean:
	rm -rf $(BIN)

.PHONY: all clean
