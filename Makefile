CC = gcc
CFLAGS += -g -Wall
CPPFLAGS += -MMD
LDFLAGS += 

PP_BOLD := $(shell tput bold)
PP_RESET := $(shell tput sgr0)
PP_CC := $(PP_BOLD)$(shell tput setf 6)CC$(PP_RESET)
PP_LD := $(PP_BOLD)$(shell tput setf 2)LD$(PP_RESET)
PP_RM := $(PP_BOLD)$(shell tput setf 4)RM$(PP_RESET)

SRC := src/common.c \
       src/lexer.c \
       src/main.c \
       src/mapcat.c
OBJ := $(SRC:src/%.c=obj/%.o)
OUT := mapcat
INSTALL := /usr/local/bin/mapcat

all: $(OUT)

-include $(OBJ:.o=.d)

obj/%.o : src/%.c
	@echo "$(PP_CC) src/$*.c"
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(CPPFLAGS) -c src/$*.c -o obj/$*.o

$(OUT): $(OBJ)
	@echo "$(PP_LD) $(OUT)"
	@$(CC) $(OBJ) -o $(OUT) $(LDFLAGS)

clean:
	@echo "${PP_RM} obj"
	@rm -rf obj
	@echo "${PP_RM} ${OUT}"
	@rm -rf ${OUT}

install:
	install $(OUT) $(INSTALL)

uninstall:
	rm $(INSTALL)

.PHONY: clean install uninstall

