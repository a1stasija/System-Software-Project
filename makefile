# Definicija kompajlera i alata
CC = g++
BISON = bison
FLEX = flex

# Opcije kompajliranja
CXXFLAGS = -Iinc -g  # -Iinc dodaje "inc" folder u include putanju

# Izlazni fajlovi
LEX_FILE = misc/lexer.l
YACC_FILE = misc/parser.y
SRC_FILES = src/assembler.cpp
EXECUTABLE = proba

# Generisani fajlovi
LEX_OUTPUT = misc/lex.yy.c
YACC_OUTPUT_C = misc/parser.tab.c
YACC_OUTPUT_H = misc/parser.tab.h

# Glavni build proces
all: $(EXECUTABLE)

# Generisanje izvršnog fajla
$(EXECUTABLE): $(LEX_OUTPUT) $(YACC_OUTPUT_C) $(SRC_FILES)
	$(CC) $(CXXFLAGS) $^ -o $@

# Generisanje parsera
$(YACC_OUTPUT_C): $(YACC_FILE)
	$(BISON) -d $< -o $(YACC_OUTPUT_C)

# Generisanje leksera
$(LEX_OUTPUT): $(LEX_FILE)
	$(FLEX) -o $@ $<

# Čišćenje svih generisanih fajlova
clean:
	rm -f $(LEX_OUTPUT) $(YACC_OUTPUT_C) $(YACC_OUTPUT_H) $(EXECUTABLE) *.o
