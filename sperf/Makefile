NAME := $(shell basename $(PWD))
export MODULE := M3
all: $(NAME)-64 $(NAME)-32

include ../Makefile

hello_world:hello_world.cpp
	g++ -o hello_world hello_world.cpp