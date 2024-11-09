CC = gcc

GIT_HASH=$(shell git rev-parse --short HEAD)
COMPILE_TIME=$(shell date -u +'%Y-%m-%d %H:%M:%S UTC')
GIT_BRANCH=$(shell git branch --show-current)
export VERSION_FLAGS=-DGIT_HASH="\"$(GIT_HASH)\"" -DCOMPILE_TIME="\"$(COMPILE_TIME)\"" -DGIT_BRANCH="\"$(GIT_BRANCH)\""

zdostool: zdostool.c
	$(CC)  $(VERSION_FLAGS) zdostool.c -o zdostool

install: zdostool
	cp zdostool ~/bin

clean:
	rm -f *.o zdostool
