NAME := $(shell basename $(PWD))
export MODULE := M4
all: $(NAME)-64 $(NAME)-32
LDFLAGS += -ldl

include ../Makefile

.PHONY:run-32 run-64
run-32: $(NAME)-32
	./$(NAME)-32 < input.txt
	

run-64: $(NAME)-64
	./$(NAME)-64 < input.txt
	