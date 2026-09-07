CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -O2
BIN := bin

all: $(BIN)/sender $(BIN)/receiver $(BIN)/producer_consumer_pipe $(BIN)/producer_consumer_semaphore

$(BIN)/sender: src/sinais/sender.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN)/receiver: src/sinais/receiver.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN)/producer_consumer_pipe: src/pipes/producer_consumer_pipe.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN)/producer_consumer_semaphore: CXXFLAGS += -pthread
$(BIN)/producer_consumer_semaphore: src/produtor-consumidor/producer_consumer_semaphore.cpp | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $<

$(BIN):
	mkdir -p $@

clean:
	rm -rf $(BIN)

.PHONY: all clean
