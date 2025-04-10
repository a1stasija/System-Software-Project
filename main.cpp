#include <iostream>
#include <vector>
#include "assembler.hpp"
#include <map>
#include <string.h>
#include <regex>

using namespace std;

/* Globalni podaci */
string outputName = "output.o";

extern int parserMain(int argc, char *argv[]);

int main(int argc, char *argv[])
{
  if (argc < 2)
  {
    cerr << "Greska: Nisu prosledjeni argumenti.\n";
    cerr << "Koriscenje: ./assembler [-o output.o] input.s\n";
    return -1;
  }
  if (strcmp(argv[1], "-o") == 0)
    outputName = argv[2];
  else
  {
    outputName = argv[1];
    outputName = regex_replace(outputName, regex("\\.s"), ".o");
    outputName = outputName.substr(outputName.find_last_of("/") + 1);
  }
  return parserMain(argc, argv);
}