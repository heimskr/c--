COMPILER        ?= clang++
OPTIMIZATION    ?= -O0 -g
STANDARD        ?= c++20
WARNINGS        ?= -Wall -Wextra
CFLAGS          := -std=$(STANDARD) $(OPTIMIZATION) $(WARNINGS) -Iinclude
BCFLAGS         := -std=$(STANDARD) $(WARNINGS) -Iinclude
OUTPUT          ?= c+-

LEXERCPP        := src/flex.cpp
PARSERCPP       := src/bison.cpp
PARSERHDR       := include/bison.h
LEXERSRC        := src/lexer.l
PARSERSRC       := src/parser.y

WASMLEXERCPP    := src/wasmflex.cpp
WASMPARSERCPP   := src/wasmbison.cpp
WASMPARSERHDR   := include/wasmbison.h
WASMLEXERSRC    := src/wasm.l
WASMPARSERSRC   := src/wasm.y

LEXFLAGS        := -Wno-sign-compare -Wno-register
BISONFLAGS      := --color=always

SOURCES         := $(shell find src/**/*.cpp src/*.cpp) $(LEXERCPP) $(PARSERCPP) $(WASMLEXERCPP) $(WASMPARSERCPP)
OBJECTS         := $(SOURCES:.cpp=.o)
BITCODE         := $(SOURCES:.cpp=.bc)

CLOC_OPTIONS    := --exclude-dir=.vscode,fixed_string --not-match-f='^((wasm)?flex|(wasm)?bison|fixed_string)'

.PHONY: all test clean

all: $(OUTPUT)

bc: $(BITCODE)

$(OUTPUT): $(OBJECTS)
	$(COMPILER) -o $@ $^ $(LDFLAGS)

test: $(OUTPUT)
	./$(OUTPUT) examples/why.c+- -d >/dev/null

%.o: %.cpp $(PARSERHDR) $(WASMPARSERHDR)
	$(COMPILER) $(CFLAGS) -c $< -o $@

%.bc: %.cpp $(PARSERHDR) $(WASMPARSERHDR)
	$(COMPILER) $(BCFLAGS) -emit-llvm -c -g -O0 -Xclang -disable-O0-optnone $< -o $@

$(LEXERCPP): $(LEXERSRC) $(PARSERHDR)
	flex --prefix=cpm --outfile=$(LEXERCPP) $(LEXERSRC)

$(PARSERCPP) $(PARSERHDR): $(PARSERSRC)
	bison $(BISONFLAGS) --defines=$(PARSERHDR) --output=$(PARSERCPP) $(PARSERSRC)

counter:
	bison -Wcounterexamples $(BISONFLAGS) --defines=$(PARSERHDR) --output=$(PARSERCPP) $(PARSERSRC)

$(LEXERCPP:.cpp=.o): $(LEXERCPP)
	$(COMPILER) $(CFLAGS) $(LEXFLAGS) -c $< -o $@

$(PARSERCPP:.cpp=.o): $(PARSERCPP) $(PARSERHDR)
	$(COMPILER) $(CFLAGS) $(LEXFLAGS) -c $< -o $@

$(WASMLEXERCPP): $(WASMLEXERSRC) $(WASMPARSERHDR)
	flex --prefix=wasm --outfile=$(WASMLEXERCPP) $(WASMLEXERSRC)

$(WASMPARSERCPP) $(WASMPARSERHDR): $(WASMPARSERSRC)
	bison $(BISONFLAGS) --defines=$(WASMPARSERHDR) --output=$(WASMPARSERCPP) $<

wcounter:
	bison -Wcounterexamples $(BISONFLAGS) --defines=$(WASMPARSERHDR) --output=$(WASMPARSERCPP) $(WASMPARSERSRC)

$(WASMLEXERCPP:.cpp=.o): $(WASMLEXERCPP)
	$(COMPILER) $(CFLAGS) $(LEXFLAGS) -c $< -o $@

$(WASMPARSERCPP:.cpp=.o): $(WASMPARSERCPP) $(WASMPARSERHDR)
	$(COMPILER) $(CFLAGS) $(LEXFLAGS) -c $< -o $@

clean:
	rm -f $(LEXERCPP) $(PARSERCPP) $(PARSERHDR) $(WASMLEXERCPP) $(WASMPARSERCPP) $(WASMPARSERHDR) src/*.o src/**/*.o $(OUTPUT) src/bison.output src/wasmbison.output strace_out pvs.log pvs.tasks

count:
	cloc . $(CLOC_OPTIONS)

countbf:
	cloc --by-file . $(CLOC_OPTIONS)

DEPFILE  = .dep
DEPTOKEN = "\# MAKEDEPENDS"
DEPFLAGS = -f $(DEPFILE) -s $(DEPTOKEN)

depend:
	@ echo $(DEPTOKEN) > $(DEPFILE)
	makedepend $(DEPFLAGS) -- $(COMPILER) $(CFLAGS) -- $(SOURCES) 2>/dev/null
	@ rm $(DEPFILE).bak

sinclude $(DEPFILE)
