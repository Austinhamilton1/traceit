CXX=g++
INC=./include
SRC=./src
BIN=./bin
LIB=./lib
OBJ=./obj

OBJS=$(patsubst $(SRC)/%.cpp, $(OBJ)/%.o, $(wildcard $(SRC)/*.cpp))

CXXFLAGS=-I$(INC) -g

.PHONY: all clean

all: $(BIN)/memrace-benchmark

$(BIN)/memrace-benchmark: $(OBJ)/benchmark.o $(OBJS) | $(BIN)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJ)/benchmark.o $(OBJS)

$(OBJ)/benchmark.o: benchmark.cpp | $(OBJ)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
$(OBJ)/memrace.o: $(SRC)/memrace.cpp | $(OBJ)
	$(CXX) $(CXXFLAGS) -c -o $@ $<
	
$(BIN):
	mkdir -p $@
$(LIB):
	mkdir -p $@
$(OBJ):
	mkdir -p $@

clean:
	rm -rf $(BIN)
	rm -rf $(LIB)
	rm -rf $(OBJ)