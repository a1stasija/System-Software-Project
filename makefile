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
LINKER_SRC = src/linker.cpp

# Izvršni fajlovi
ASSEMBLER_EXEC = assembler
LINKER_EXEC = linker

# Podrazumevana meta
all: build-assembler build-linker

# Metoda za build asemblera
build-assembler: $(LEX_OUTPUT) $(YACC_OUTPUT_C) $(ASM_SRC)
	$(CC) $(CXXFLAGS) $^ -o $(ASSEMBLER_EXEC)

# Metoda za build linkera
build-linker: $(LINKER_SRC)
	$(CC) $(CXXFLAGS) $^ -o $(LINKER_EXEC)

# Generiši parser
$(YACC_OUTPUT_C): $(YACC_FILE)
	$(BISON) -d $< -o $@

# Generiši lexer
$(LEX_OUTPUT): $(LEX_FILE)
	$(FLEX) -o $@ $<

# Čišćenje svega
clean:
	rm -f $(LEX_OUTPUT) $(YACC_OUTPUT_C) $(YACC_OUTPUT_H)
	rm -f $(ASSEMBLER_EXEC) $(LINKER_EXEC)
	rm -f *.o *.hex *.txt




