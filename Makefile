CC=g++
CXXFLAGS=-std=c++11 -c -Wall

ifeq ($(DEBUG),1)
CXXFLAGS+=-g
endif

%.o: %.cpp
	$(CC) $(CXXFLAGS) $<

EXE=ladjust
OBJ=main.o

.PHONY: all clean rebuild

all: $(EXE)

$(EXE): $(OBJ)
	$(CC) -o $@ $(OBJ)

clean:
	rm -f $(OBJ) $(EXE)

rebuild: clean all
