CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g
TARGET = test_fesk
DEMO_TARGET = fesk_demo
SOURCES = fesk_tx.c test_fesk.c
DEMO_SOURCES = fesk_tx.c fesk_demo.c

all: $(TARGET) $(DEMO_TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCES)

$(DEMO_TARGET): $(DEMO_SOURCES)
	$(CC) $(CFLAGS) -o $(DEMO_TARGET) $(DEMO_SOURCES)

test: $(TARGET)
	./$(TARGET)

# Usage: make demo TEXT="hello"
demo: $(DEMO_TARGET)
	./$(DEMO_TARGET) "$(TEXT)"

# Shortcut for "test"
demo-test: $(DEMO_TARGET)
	./$(DEMO_TARGET) "test"

clean:
	rm -f $(TARGET) $(DEMO_TARGET)

.PHONY: all test demo demo-test clean