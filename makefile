TARGET?=main
SHELL=bash
SHELL?=bash
CC=clang
CC?=clang
CC_FLAGS=-fpic
CC_EXTRA?=
CTAGS_I_PATH?=./
LD_FLAGS= -lm -ldl -L ./lua/src/
EXTRA_LD_FLAGS?=
LIB_LUA=./lua/src/liblua.a
ADD_SANITIZERS_CC= -g -fsanitize=address -fno-omit-frame-pointer
ADD_SANITIZERS_LD= -g -fsanitize=address
MEM_SANITIZERS_CC= -g -fsanitize=memory -fno-omit-frame-pointer
MEM_SANITIZERS_LD= -g -fsanitize=memory
UB_SANITIZERS_CC= -g -fsanitize=undefined -fno-omit-frame-pointer
UB_SANITIZERS_LD= -g -fsanitize=undefined
FUZZ_SANITIZERS_CC= -fsanitize=fuzzer,address -g -fno-omit-frame-pointer
FUZZ_SANITIZERS_LD= -fsanitize=fuzzer,address -g -fno-omit-frame-pointer
COV_CC= -fprofile-instr-generate -fcoverage-mapping
COV_LD= -fprofile-instr-generate
# BUILD_MODES are=RELEASE(default), DEBUG,ADDSAN,MEMSAN,UBSAN,FUZZ
BUILD_MODE?=RELEASE
OBJ_LIST:=$(patsubst %.c, %.o, $(wildcard *.c))
ASM_LIST:=$(patsubst %.c, %.dis, $(wildcard *.c))
WASM_LIST:=$(patsubst %.c, %.wasm, $(wildcard *.c))
IR_LIST:=$(patsubst %.c, %.ir, $(wildcard *.c))
JS_LIST:=$(patsubst %.c, %.js, $(wildcard *.c))

ifeq ($(BUILD_MODE), ADDSAN)
ifeq ($(CC), gcc)
$(error This build mode is only useable with clang.)
endif
CC_EXTRA+=$(ADD_SANITIZERS_CC)
EXTRA_LD_FLAGS+=$(ADD_SANITIZERS_LD)
endif

ifeq ($(BUILD_MODE), MEMSAN)
ifeq ($(CC), gcc)
$(error This build mode is only useable with clang.)
endif
CC_EXTRA+=$(MEM_SANITIZERS_CC)
EXTRA_LD_FLAGS+=$(MEM_SANITIZERS_LD)
endif

ifeq ($(BUILD_MODE), UBSAN)
ifeq ($(CC), gcc)
$(error This build mode is only useable with clang.)
endif
CC_EXTRA+=$(UB_SANITIZERS_CC)
EXTRA_LD_FLAGS+=$(UB_SANITIZERS_LD)
endif

ifeq ($(BUILD_MODE), FUZZ)
ifeq ($(CXX), g++)
$(error This build mode is only useable with clang++.)
endif
CXX_EXTRA+=$(FUZZ_SANITIZERS_CC)
EXTRA_LD_FLAGS+=$(FUZZ_SANITIZERS_LD)
endif

SRCS:=$(wildcard *.c)
HDRS:=$(wildcard *.h)
CC_FLAGS+=$(CC_EXTRA)
LD_FLAGS+=$(EXTRA_LD_FLAGS)

.DEFAULT:all

.PHONY:all clean help ASM SO TAGS WASM JS IR

all:$(TARGET)

everything:$(TARGET) A ASM SO $(TARGET)-static $(TARGET)-dbg TAGS $(TARGET)-cov WASM JS IR

depend:.depend

.depend:$(SRCS)
	rm -rf .depend
	$(CC) -MM $(CC_FLAGS) $^ > ./.depend
	echo $(patsubst %.o:, %.odbg:, $(shell $(CC) -MM $(CC_FLAGS) $^)) | sed -r 's/[A-Za-z0-9\-\_]+\.odbg/\n&/g' >> ./.depend
	echo $(patsubst %.o:, %.ocov:, $(shell $(CC) -MM $(CC_FLAGS) $^)) | sed -r 's/[A-Za-z0-9\-\_]+\.ocov/\n&/g' >> ./.depend

-include ./.depend

$(LIB_LUA):
	$(MAKE) -C lua/src linux

.c.o:
	$(CC) $(CC_FLAGS) -c $< -o $@

%.odbg:%.c
	$(CC) $(CC_FLAGS) -g -c $< -o $@

%.ocov:%.c
	$(CC) $(CC_FLAGS) $(COV_CC) -c $< -o $@

$(TARGET): $(TARGET).o test.o $(LIB_LUA)
	$(CC) $^ $(LD_FLAGS) -o $@

$(TARGET)-static: $(TARGET).o $(LIB_LUA)
	$(CC) $(LD_FLAGS) $^ -static -o $@

$(TARGET)-dbg: $(TARGET).odbg $(LIB_LUA)
	$(CC) $(LD_FLAGS) $^ -g -o $@

$(TARGET)-cov: $(TARGET).ocov $(LIB_LUA)
	$(CC) $(LD_FLAGS) $^ $(COV_LD) -o $@

cov: runcov
	@llvm-profdata merge -sparse ./default.profraw -o ./default.profdata
	@llvm-cov show $(TARGET)-cov -instr-profile=default.profdata

covrep: runcov
	@llvm-profdata merge -sparse ./default.profraw -o ./default.profdata
	@llvm-cov report $(TARGET)-cov -instr-profile=default.profdata

ASM:$(ASM_LIST)

SO:$(TARGET).so

A:$(TARGET).a

IR:$(IR_LIST)

WASM:$(WASM_LIST)

JS:$(JS_LIST)

TAGS:tags

#https://github.com/rizsotto/Bear
BEAR: clean
	bear make

tags:$(SRCS)
	$(shell $(CC) -c -I $(CTAGS_I_PATH) -M $(SRCS)|\
		sed -e 's/[\\ ]/\n/g'|sed -e '/^$$/d' -e '/\.o:[ \t]*$$/d'|\
		ctags -L - --c++-kinds=+p --fields=+iaS --extra=+q)

%.dis: %.o
	objdump -r -d -M intel -S $< > $@

%.ir: %.c
	$(CC) -emit-llvm -S -o $@ $<

%.wasm: %.c
	$(CC) --compile $< --target=wasm32-unknown-unknown-wasm --output $@

%.js: %.c
	emcc $< -o $@

$(TARGET).so: $(TARGET).o $(LIB_LUA)
	$(CC) $(LD_FLAGS) $^ -shared -o $@

$(TARGET).a: $(TARGET).o $(LIB_LUA)
	ar rcs $(TARGET).a $(TARGET).o

runcov: $(TARGET)-cov
	$(TARGET)-cov

test: $(TARGET)
	$(TARGET)

run: $(TARGET)
	$(TARGET)

valgrind: $(TARGET)
	- valgrind --track-origins=yes --leak-check=full --show-leak-kinds=all $(TARGET)

format:
	- clang-format -i $(SRCS) $(HDRS)

clean:
	- rm -f *.o *.dis *.odbg *.ocov *.js *.ir *~ $(TARGET) $(TARGET).so $(TARGET)-static $(TARGET)-dbg $(TARGET).a $(TARGET)-cov

deepclean: clean
	- rm tags
	- rm .depend
	- rm ./default.profraw ./default.profdata
	- rm vgcore.*
	- rm compile_commands.json
	- rm *.gch
	$(MAKE) -C lua clean

help:
	@echo "--all is the default target, runs $(TARGET) target"
	@echo "--everything will build everything"
	@echo "--SO will generate the so"
	@echo "--ASM will generate assembly files"
	@echo "--TAGS will generate tags file"
	@echo "--BEAR will generate a compilation database"
	@echo "--IR will generate llvm IR"
	@echo "--$(TARGET) builds the dynamically-linked executable"
	@echo "--$(TARGET)-dbg will generate the debug build. BUILD_MODE should be set to DEBUG to work"
	@echo "--$(TARGET)-static will statically link the executable to the libraries"
	@echo "--$(TARGET)-cov is the coverage build"
	@echo "--cov will print the coverage report"
	@echo "--covrep will print the line coverage report"
	@echo "--A will build the static library"
	@echo "--TAGS will build the tags file"
	@echo "--clean"
	@echo "--deepclean will clean almost everything"
