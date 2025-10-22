CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -g \
    -Itest/stubs \
    -I../chirpy_tx/test

TEST_BIN = test_fesk_tx
TEST_SOURCES = \
    test/test_fesk_tx.c \
    fesk_tx.c \
    ../chirpy_tx/test/unity.c

all: $(TEST_BIN)

$(TEST_BIN): $(TEST_SOURCES)
	$(CC) $(CFLAGS) $(TEST_SOURCES) -o $(TEST_BIN)

.PHONY: test

test: $(TEST_BIN)
	./$(TEST_BIN)

.PHONY: clean

clean:
	rm -f $(TEST_BIN)
