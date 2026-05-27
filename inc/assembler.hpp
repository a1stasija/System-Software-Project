#include <string>
#include <vector>
#include <map>
#include <iostream>

using namespace std;

enum AddressingMode
{
  IMMEDIATE_LITERAL = 0x00, // Neposredno adresiranje sa literalom ($<literal>), 0
  IMMEDIATE_SYMBOL = 0x01,  // Neposredno adresiranje sa simbolom ($<simbol>), 1

  MEMORY_DIRECT_LITERAL = 0x10, // Memorijsko direktno sa literalom (<literal>), 16
  MEMORY_DIRECT_SYMBOL = 0x11,  // Memorijsko direktno sa simbolom (<simbol>), 17

  REGISTER_DIRECT = 0x20,         // Registarsko direktno (%<reg>), 32
  REGISTER_INDIRECT = 0x21,       // Registarsko indirektno ([%<reg>]), 33
  REGISTER_OFFSET_LITERAL = 0x22, // Registarsko indirektno sa pomeranjem ([%<reg> + <literal>]), 34
  REGISTER_OFFSET_SYMBOL = 0x23,  // Registarsko indirektno sa simbolom ([%<reg> + <simbol>]), 35

  ADDRESS_LITERAL = 0x30, // JMP/CALL adresiranje sa literalom (<literal>), 48
  ADDRESS_SYMBOL = 0x31,  // JMP/CALL adresiranje sa simbolom (<simbol>), 49

  OTHER = 0xFF
};

struct Arguments
{
  vector<string *> *argName;
  vector<int> *argType; // 0 - literal ; 1 - simbol ; 2 - registar
  AddressingMode addrMode;

  Arguments(string *name, int type, AddressingMode addr)
  {
    this->argName = new vector<string *>();
    this->argName->push_back(name);
    this->argType = new vector<int>();
    this->argType->push_back(type);
    this->addrMode = addr;
  }
};

struct ForwardReferenceTableEntry
{
  int section;
  int offset;
  int type; //0 za pc rel i 1 za aps
  unsigned char patchCode1;
  unsigned char patchCode2;

  ForwardReferenceTableEntry(int section, int offset, int type)
  {
    this->section = section;
    this->offset = offset;
    this->type = type;
    this->patchCode1 = 0x00;
    //this->patchCode2 = 0x00;
  }

  void setPatchOpcode(unsigned char opcode){
    this->patchCode1 = opcode;
    //this->patchCode2 = gprAB;
  }

  void print()
  {
    cout << "ForwardReferenceTableEntry -> Section: " << section
         << ", Offset: " << offset << endl;
  }
};

struct LiteralPoolEntry
{
  vector<ForwardReferenceTableEntry *> *flink;
  bool isSymbol; // true ako je simbol, false ako je literal
  LiteralPoolEntry(int section, int offset, bool isSymbol){
    this->flink = new vector<ForwardReferenceTableEntry *>();
    this->flink->push_back(new ForwardReferenceTableEntry(section, offset, 0));
    this->isSymbol = isSymbol;
  }
};

struct SymbolTableEntry
{
  static int cnt;
  int id;
  int value; //-1 za undf
  int type; // 0 za sekciju, 1 za simbol tj labelu
  int bind; // 0 za globalan 1 za lokalan 2 za extern
  int ndx; // 0 za UNDF, ostalo je id sekcije
  string name;
  vector<ForwardReferenceTableEntry *> *flink;

  SymbolTableEntry(string name, int bind, int type, int ndx, int value, bool defined)
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
      if (defined)
      {
        this->ndx = ndx;
      }
      else
      {
        this->ndx = 0;
      }
    }
    this->value = value;
    flink = new vector<ForwardReferenceTableEntry *>();
  }

  void printSymbol() {
    cout << "-------------------------------------\n";
    cout << "ID: " << id << "\n";
    cout << "Name: " << name << "\n";
    cout << "Value: " << value << "\n";
    cout << "Type: " << (type == 0 ? "Section" : "NOTYP") << "\n";
    cout << "Bind: " << (bind == 0 ? "Global" : (bind == 1 ? "Local" : "Extern")) << "\n";
    cout << "Section Index: " << (ndx == 0 ? "UNDEFINED" : to_string(ndx)) << "\n";

    if (!flink->empty()) {
        cout << "Forward References:\n";
        for (const auto& ref : *flink) {
            cout << "  → Reference at offset: " << ref->offset << "\n";
        }
    } else {
        cout << "No forward references.\n";
    }
    cout << "-------------------------------------\n";
}


};

struct RelocationTableEntry
{
  int offset;
  int type;     // 0-pc relativno ili 1-apsolutno
  int idSymbol; // ovde je id simbola ako je globalni ili id sekcije ako je lokalni
  int addend;
  int section;

  RelocationTableEntry(int offset, int type, int addend, int section, int idSymbol)
  {
    this->offset = offset;
    this->type = type;
    this->addend = addend;
    this->section = section;
    this->idSymbol = idSymbol;
  }

  void print()
  {
    cout << "RelocationTableEntry -> Offset: " << offset
         << ", Type: " << type
         << ", Symbol ID: " << idSymbol
         << ", Addend: " << addend << endl;
  }
};

struct Section
{
  string name;
  int idSymbolTable;
  int base;
  int size;
  int literalPoolBase;
  int literalPoolSize;
  vector<RelocationTableEntry *> *relocationTableForSection;
  vector<char> *code;

  Section(string name, int id)
  {
    this->name = name;
    this->idSymbolTable = id;
    this->base = 0;
    this->size = 0;
    this->literalPoolSize = 0;
    this->literalPoolBase = -1;
    this->relocationTableForSection = new vector<RelocationTableEntry *>();
    this->code = new vector<char>();
  }

  void print()
{
  cout << "Section -> Name: " << name
       << ", ID in Symbol Table: " << idSymbolTable
       << ", Base Address: " << base
       << ", Length: " << size
       << ", Literal Pool Size: " << literalPoolSize << endl;

  if (!relocationTableForSection->empty()) {
    for (const auto &entry : *relocationTableForSection) {
      cout << "  Relocation -> Offset: " << entry->offset << ", Type: " << entry->type << endl;
    }
  }

  if (!code->empty())
  {
    cout << "  Hex Dump:\n";

    int addr = base;
    for (int i = 0; i < code->size(); i += 4)
    {
      printf("  0x%04X: ", addr + i);
      for (int j = 0; j < 4 && i + j < code->size(); ++j)
      {
        printf("%02X ", (unsigned char)(*code)[i + j]);
      }
      cout << endl;
    }
  }
}

};

// pomocne f-je
void printArguments(Arguments *args);
void free_mem();

// LABELE
void process_label(string *label);

// DIREKTIVE
void process_END_DIR();
void process_GLOBAL_DIR(Arguments *args);
void process_EXTERN_DIR(Arguments *args);
void process_WORD_DIR(Arguments *args);
void process_SECTION_DIR(string *name);
void process_SKIP_DIR(string *val);

// INSTRUKCIJE
void process_HALT_INSTR();
void process_INT_INSTR();
void process_IRET_INSTR();
void process_CALL_INSTR(Arguments *arg);
void process_RET_INSTR();
void process_JMP_INSTR(Arguments *arg);
void process_BEQ_INSTR(string *gpr1, string *gpr2, Arguments *arg);
void process_BNE_INSTR(string *gpr1, string *gpr2, Arguments *arg);
void process_BGT_INSTR(string *gpr1, string *gpr2, Arguments *arg);
void process_PUSH_INSTR(string *gpr);
void process_POP_INSTR(string *gpr);
void process_XCHG_INSTR(string *gprS, string *gprD);
void process_ADD_INSTR(string *gprS, string *gprD);
void process_SUB_INSTR(string *gprS, string *gprD);
void process_MUL_INSTR(string *gprS, string *gprD);
void process_DIV_INSTR(string *gprS, string *gprD);
void process_NOT_INSTR(string *gpr);
void process_AND_INSTR(string *gprS, string *gprD);
void process_OR_INSTR(string *gprS, string *gprD);
void process_XOR_INSTR(string *gprS, string *gprD);
void process_SHL_INSTR(string *gprS, string *gprD);
void process_SHR_INSTR(string *gprS, string *gprD);
void process_LD_INSTR(Arguments *arg, string *gpr);
void process_ST_INSTR(string *gpr, Arguments *arg);
void process_CSRRD_INSTR(string *csr, string *gpr);
void process_CSRWR_INSTR(string *gpr, string *csr);
