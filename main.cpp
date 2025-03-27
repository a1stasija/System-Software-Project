#include <iostream>
#include <vector>
#include "assembler.hpp"
#include <map>
#include <string.h>
#include <regex>


using namespace std;


/* Globalni podaci */
int SymbolTableEntry::cnt = 0;
map<int, Section*> *allSections = new map<int, Section*>();
map<string, SymbolTableEntry*> *symbolTable = new map<string, SymbolTableEntry*>();
map<int, LiteralPoolEntry *> *literalPool = new map<int, LiteralPoolEntry *> ();

Section* currSection = nullptr;
int locationCounter = 0;
string outputName;



extern int parser_main(int argc, char* argv[]);

int main(int argc, char *argv[]){
  //dohvatamo ime izlaznog fajla
  outputName = argv[1]; //ako nije korisceno -o onda je ime isto kao ulaznog fajla samo sa .o
  string input_file_name(argv[1]);
  regex pattern("^.*/(.*)\\.s$");
  smatch match;

  if (regex_search(input_file_name, match, pattern))
  {
      outputName = match[1];
      outputName = outputName + + ".o";
  }
  
  for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
          outputName = argv[i + 1];
        }
  }

  //return parserMain(argc,argv);
}