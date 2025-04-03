#include <string>
#include <vector>

using namespace std;

struct LinkerRelocationTableEntry{
  string symbolName;
  unsigned long virtAddress;   // gde se vrši zamena
  int sectionId;  // adresa sekcije u kojoj se relokacija vrši
  int addend;
  int type; // 0: PC-relative, 1: absolute

  LinkerRelocationTableEntry( string symbolName, int virtAddress, int sectionId, int addend, int type){
    this->symbolName = symbolName;
    this->virtAddress = virtAddress;
    this->sectionId = sectionId;
    this->addend = addend;
    this->type = type;
  }
};

struct LinkerSection
{
  string name;
  int idSymbolTable;
  unsigned long base; // postavlja se na osnovu -place ili automatski
  unsigned long size;
  vector<LinkerRelocationTableEntry *> *relocationTableForSection;
  vector<char> *code;

  LinkerSection(string name, int id, unsigned long base,  unsigned long size)
  {
    this->name = name;
    this->idSymbolTable = id;
    this->base = base;
    this->size = size;
    this->relocationTableForSection = new vector<LinkerRelocationTableEntry *>();
    this->code = new vector<char>();
  }

};

struct AssemblerSectionInfo{
  string sectionName;
  int linkerIdSymbol;
  int asmIdFile;
  int asmIdSymb;
  unsigned long base;
  unsigned long size;
  bool placed;

  AssemblerSectionInfo(string sectionName, int asmIdSymbol, int asmIdFile, int base, int size, bool placed){
    this->sectionName = sectionName;
    this->asmIdFile = asmIdFile;
    this->asmIdSymb = asmIdSymbol;
    this->base = base;
    this->size = size;
    this->placed = placed;
  }


};

struct LinkerSymbolTableEntry
{
  static int cnt;
  int id;
  unsigned long value; //-1 za undf
  int type; // 0 za sekciju, 1 za simbol tj labelu
  int bind; // 0 za globalan 1 za lokalan 2 za extern
  int ndx; // 0 za UNDF, ostalo je id sekcije
  string name;

  LinkerSymbolTableEntry(string name, int bind, int type, int ndx, unsigned long value)
  {
    this->id = ++cnt;
    this->name = name;
    this->bind = bind;
    this->type = type;
    if (type == 0)
    {
      this->ndx = this->id;
    }
    else
    {

      this->ndx = ndx;

    }
    this->value = value;
  }

};