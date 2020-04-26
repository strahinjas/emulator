CC = g++
CFLAGS = -g -I $(SRCDIR) -std=c++11 -pthread

SRCDIR = ./src
OBJDIR = ./bin/obj
TARGET = ./bin/emulator

OBJ = $(patsubst $(SRCDIR)/%.cpp, $(OBJDIR)/%.o, $(wildcard $(SRCDIR)/*.cpp))

$(TARGET) : $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^

$(OBJDIR)/%.o : $(SRCDIR)/%.cpp
	$(CC) $(CFLAGS) -o $@ -c $<

.PHONY: clean

clean:
	rm -f $(OBJDIR)/*.o
	rm -f $(TARGET)
