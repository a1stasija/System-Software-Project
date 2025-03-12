#include <string>
#include <vector>

using namespace std;

enum AddressingMode {
    IMMEDIATE_LITERAL = 0x00,   // Neposredno adresiranje sa literalom ($<literal>), 0
    IMMEDIATE_SYMBOL = 0x01,    // Neposredno adresiranje sa simbolom ($<simbol>), 1

    MEMORY_DIRECT_LITERAL = 0x10,  // Memorijsko direktno sa literalom (<literal>), 16
    MEMORY_DIRECT_SYMBOL = 0x11,   // Memorijsko direktno sa simbolom (<simbol>), 17

    REGISTER_DIRECT = 0x20,        // Registarsko direktno (%<reg>), 32
    REGISTER_INDIRECT = 0x21,      // Registarsko indirektno ([%<reg>]), 33
    REGISTER_OFFSET_LITERAL = 0x22, // Registarsko indirektno sa pomeranjem ([%<reg> + <literal>]), 34
    REGISTER_OFFSET_SYMBOL = 0x23,  // Registarsko indirektno sa simbolom ([%<reg> + <simbol>]), 35

    ADDRESS_LITERAL = 0x30,    // JMP/CALL adresiranje sa literalom (<literal>), 48
    ADDRESS_SYMBOL = 0x31,     // JMP/CALL adresiranje sa simbolom (<simbol>), 49

    OTHER = 0xFF
};

struct Arguments{
  vector<string*> *argName;
  vector<int> *argType; // 0 - literal ; 1 - simbol ; 2 - registar
  AddressingMode addrMode; 

  Arguments(string *name, int type, AddressingMode addr){
    this->argName = new vector<string*>();
    this->argName->push_back(name);
    this->argType = new vector<int>();
    this->argType->push_back(type);
    this->addrMode = addr;
  }
};

//pomocne f-je
void printArguments(Arguments* args);
void free_mem();

//LABELE
void process_label(string* label);

//DIREKTIVE
void process_END_DIR();
void process_GLOBAL_DIR(Arguments* args);
void process_EXTERN_DIR(Arguments* args);
void process_WORD_DIR(Arguments* args);
void process_SECTION_DIR(string* name);
void process_SKIP_DIR(string* val);

//INSTRUKCIJE
void process_HALT_INSTR();
void process_INT_INSTR();
void process_IRET_INSTR();
void process_CALL_INSTR(Arguments* arg);
void process_RET_INSTR();
void process_JMP_INSTR(Arguments* arg);
void process_BEQ_INSTR(string* gpr1, string* gpr2, Arguments* arg);
void process_BNE_INSTR(string* gpr1, string* gpr2, Arguments* arg);
void process_BGT_INSTR(string* gpr1, string* gpr2, Arguments* arg);
void process_PUSH_INSTR(string* gpr);
void process_POP_INSTR(string* gpr);
void process_XCHG_INSTR(string* gprS, string* gprD);
void process_ADD_INSTR(string* gprS, string* gprD);
void process_SUB_INSTR(string* gprS, string* gprD);
void process_MUL_INSTR(string* gprS, string* gprD);
void process_DIV_INSTR(string* gprS, string* gprD);
void process_NOT_INSTR(string* gpr);
void process_AND_INSTR(string* gprS, string* gprD);
void process_OR_INSTR(string* gprS, string* gprD);
void process_XOR_INSTR(string* gprS, string* gprD);
void process_SHL_INSTR(string* gprS, string* gprD);
void process_SHR_INSTR(string* gprS, string* gprD);
void process_LD_INSTR(Arguments* arg, string* gpr);
void process_ST_INSTR(string* gpr, Arguments* arg);
void process_CSRRD_INSTR(string* csr, string* gpr);
void process_CSRWR_INSTR(string* gpr, string* csr);