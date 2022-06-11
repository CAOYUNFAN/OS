NAME := $(shell basename $(PWD))
export MODULE := M5
all: $(NAME)-64 $(NAME)-32
LDFLAGS += -lm

CFLAGS+=-g #-DLOCAL
include ../Makefile

run:$(NAME)-32 $(NAME)-64 git
	@./frecov-64 ./M5-frecov.img

gdb:$(NAME)-32 $(NAME)-64 git
	gdb ./frecov-64 --args ./frecov-64 ./M5-frecov.img

inner-clear:
	rm bmp/*

.phony:run gdb inner-clear
