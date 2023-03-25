CC := cc

OPT := -O3
SDLCFLAGS != sdl2-config --cflags
CFLAGS := -ansi -pedantic -Wall -Wextra ${OPT} ${SDLCFLAGS}
DBGFLAGS := -ansi -pedantic -Wall -Wextra -g -O0 ${SDLCFLAGS}

SDLLDFLAGS != sdl2-config --libs
LDFLAGS := ${SDLLDFLAGS}

SRCS := src/snake.c

EXE := snake

.PHONY: all

all: ${EXE}

${EXE}: ${SRCS}
	${CC} ${CFLAGS} ${LDFLAGS} ${SRCS} -o $@

debug: ${SRCS}
	${CC} ${DBGFLAGS} ${LDFLAGS} ${SRCS} -o ${EXE}
