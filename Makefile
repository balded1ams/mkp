CC = gcc
CFLAGS = -Wall -Wextra -Werror

TARGET = mkp
TEST_TARGET = test_mkp 

all: $(TARGET)

$(TARGET): main.c
	$(CC) $(CFLAGS) -o $(TARGET) main.c

test: main.c
	$(CC) $(CFLAGS) -DUNIT_TEST -o $(TEST_TARGET) main.c
	./$(TEST_TARGET)

clean:
	rm -f $(TARGET) $(TEST_TARGET)

