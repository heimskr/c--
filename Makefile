COMPILER        ?= clang++
OPTIMIZATION    ?= -O0 -g
STANDARD        ?= c++20
WARNINGS        ?= -Wall -Wextra
CFLAGS          := -std=$(STANDARD) $(OPTIMIZATION) $(WARNINGS) -Iinclude
OUTPUT          ?= c--

LEXERCPP        := src/flex.cpp
PARSERCPP       := src/bison.cpp
PARSERHDR       := include/bison.h
LEXERSRC        := src/lexer.l
PARSERSRC       := src/parser.y
FLEXPREFIX      := cmm

LEXFLAGS        := -Wno-sign-compare -Wno-register
BISONFLAGS      := --color=always

SOURCES         := $(shell find src/**/*.cpp src/*.cpp) $(LEXERCPP) $(PARSERCPP)
OBJECTS         := $(SOURCES:.cpp=.o)

CLOC_OPTIONS    := --exclude-dir=.vscode,fixed_string --not-match-f='^(flex|bison|fixed_string)'

.PHONY: all test clean

all: $(OUTPUT)

$(OUTPUT): $(OBJECTS)
	$(COMPILER) -o $@ $^ $(LDFLAGS)

test: $(OUTPUT)
	./$(OUTPUT) arrays.c--

%.o: %.cpp $(PARSERHDR)
	$(COMPILER) $(CFLAGS) -c $< -o $@

$(LEXERCPP): $(LEXERSRC) $(PARSERHDR)
	flex --prefix=$(FLEXPREFIX) --outfile=$(LEXERCPP) $(LEXERSRC)

$(PARSERCPP) $(PARSERHDR): $(PARSERSRC)
	bison $(BISONFLAGS) --defines=$(PARSERHDR) --output=$(PARSERCPP) $(PARSERSRC)

counter:
	bison -Wcounterexamples $(BISONFLAGS) --defines=$(PARSERHDR) --output=$(PARSERCPP) $(PARSERSRC)

$(LEXERCPP:.cpp=.o): $(LEXERCPP)
	$(COMPILER) $(CFLAGS) $(LEXFLAGS) -c $< -o $@

$(PARSERCPP:.cpp=.o): $(PARSERCPP) $(PARSERHDR)
	$(COMPILER) $(CFLAGS) $(LEXFLAGS) -c $< -o $@

clean:
	rm -f $(LEXERCPP) $(PARSERCPP) $(PARSERHDR) src/*.o src/**/*.o $(OUTPUT) src/bison.output

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
