# Makefile for QR Decomposition Embedded Module

CC = gcc
CFLAGS = -Wall -Wextra -O3 -flto
LDFLAGS = -lm

TARGET = qr_demo

SRCS = $(wildcard *.c)
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
