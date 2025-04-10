# Alati
CC = g++
FLEX = flex
BISON = bison

# Putanje i opcije
CXXFLAGS = -Iinc -g

# Fajlovi za lexer i parser
LEX_FILE = misc/lexer.l
YACC_FILE = misc/parser.y

LEX_OUTPUT = misc/lex.yy.c
YACC_OUTPUT_C = misc/parser.tab.c
YACC_OUTPUT_H = misc/parser.tab.h

# Izvori za asembler i linker
ASM_SRC = src/assembler.cpp
ASM_MAIN_SRC = src/main.cpp
LINKER_SRC = src/linker.cpp
EMULATOR_SRC = src/emulator.cpp

# Izvršni fajlovi
ASSEMBLER_EXEC = assembler
LINKER_EXEC = linker
EMULATOR_EXEC = emulator

# Podrazumevana meta
all: build-assembler build-linker build-emulator

# Metoda za build asemblera
build-assembler: $(LEX_OUTPUT) $(YACC_OUTPUT_C) $(ASM_SRC) $(ASM_MAIN_SRC)
	$(CC) $(CXXFLAGS) $^ -o $(ASSEMBLER_EXEC)


# Metoda za build linkera
build-linker: $(LINKER_SRC)
	$(CC) $(CXXFLAGS) $^ -o $(LINKER_EXEC)

build-emulator: $(EMULATOR_SRC)
	$(CC) $(CXXFLAGS) $^ -o $(EMULATOR_EXEC)

# Generiši parser
$(YACC_OUTPUT_C): $(YACC_FILE)
	$(BISON) -d $< -o $@

# Generiši lexer
$(LEX_OUTPUT): $(LEX_FILE)
	$(FLEX) -o $@ $<

# Čišćenje svega
clean:
	rm -f $(LEX_OUTPUT) $(YACC_OUTPUT_C) $(YACC_OUTPUT_H)
	rm -f $(ASSEMBLER_EXEC) $(LINKER_EXEC) $(EMULATOR_EXEC)
	rm -f *.o *.hex *.txt




