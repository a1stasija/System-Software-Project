#include <iostream>
#include "../inc/assembler.hpp"

using namespace std;

void printArguments(Arguments* args) {
  if (!args) {
    cout << "(nema argumenata)" << endl;
    return;
  }
  cout << "Adresa mode: " << args->addrMode << " | Argumenti: ";
  for (size_t i = 0; i < args->argName->size(); i++) {
    cout << *(args->argName->at(i)) << " (" << args->argType->at(i) << ") ";
  }
  cout << endl;
}

void process_label(string* label){
  cout <<"Nadjena labela: " << *label<<endl;
}

/* DIREKTIVE */
void process_END_DIR(){
  cout << "Procesiranje END direktive" << endl;
}

void process_GLOBAL_DIR(Arguments* args){
  cout << "Procesiranje GLOBAL direktive: ";
  printArguments(args);
}

void process_EXTERN_DIR(Arguments* args){
  cout << "Procesiranje EXTERN direktive: ";
  printArguments(args);
}

void process_WORD_DIR(Arguments* args){
  cout << "Procesiranje WORD direktive: ";
  printArguments(args);
}

void process_SECTION_DIR(string* name){
  cout << "Procesiranje SECTION direktive: " << *name << endl;
}

void process_SKIP_DIR(string* val){
  cout << "Procesiranje SKIP direktive sa vrednoscu: " << *val << endl;
}

/* INSTRUKCIJE */

void process_HALT_INSTR(){
  cout << "Izvrsena HALT instrukcija" << endl;
}

void process_INT_INSTR(){
  cout << "Izvrsena INT instrukcija" << endl;
}

void process_IRET_INSTR(){
  cout << "Izvrsena IRET instrukcija" << endl;
}

void process_CALL_INSTR(Arguments* arg){
  cout << "Izvrsena CALL instrukcija sa argumentima: ";
  printArguments(arg);
}

void process_RET_INSTR(){
  cout << "Izvrsena RET instrukcija" << endl;
}

void process_JMP_INSTR(Arguments* arg){
  cout << "Izvrsena JMP instrukcija sa argumentima: ";
  printArguments(arg);
}

void process_BEQ_INSTR(string* gpr1, string* gpr2, Arguments* arg){
  cout << "Izvrsena BEQ instrukcija sa " << *gpr1 << ", " << *gpr2 << " i operandima: ";
  printArguments(arg);
}

void process_BNE_INSTR(string* gpr1, string* gpr2, Arguments* arg){
  cout << "Izvrsena BNE instrukcija sa " << *gpr1 << ", " << *gpr2 << " i operandima: ";
  printArguments(arg);
}

void process_BGT_INSTR(string* gpr1, string* gpr2, Arguments* arg){
  cout << "Izvrsena BGT instrukcija sa " << *gpr1 << ", " << *gpr2 << " i operandima: ";
  printArguments(arg);
}

void process_PUSH_INSTR(string* gpr){
  cout << "Izvrsena PUSH instrukcija sa " << *gpr << endl;
}

void process_POP_INSTR(string* gpr){
  cout << "Izvrsena POP instrukcija sa " << *gpr << endl;
}

void process_XCHG_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena XCHG instrukcija sa " << *gprS << " i " << *gprD << endl;
}

void process_ADD_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena ADD instrukcija sa " << *gprS << " i " << *gprD << endl;
}

void process_SUB_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena SUB instrukcija sa " << *gprS << " i " << *gprD << endl;
}
void process_MUL_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena MUL instrukcija sa " << *gprS << " i " << *gprD << endl;
}
void process_DIV_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena DIV instrukcija sa " << *gprS << " i " << *gprD << endl;
}

void process_NOT_INSTR(string* gpr){
  cout << "Izvrsena NOT instrukcija sa " << *gpr << endl;
}

void process_AND_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena AND instrukcija sa " << *gprS << " i " << *gprD << endl;
}
void process_OR_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena OR instrukcija sa " << *gprS << " i " << *gprD << endl;
}

void process_XOR_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena XOR instrukcija sa " << *gprS << " i " << *gprD << endl;
}

void process_SHL_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena SHL instrukcija sa " << *gprS << " i " << *gprD << endl;
}

void process_SHR_INSTR(string* gprS, string* gprD){
  cout << "Izvrsena SHR instrukcija sa " << *gprS << " i " << *gprD << endl;
}
void process_LD_INSTR(Arguments* arg, string* gpr){
  cout << "Izvrsena LD instrukcija sa registrima " << *gpr << " i operandima: ";
  printArguments(arg);
}

void process_ST_INSTR(string* gpr, Arguments* arg){
  cout << "Izvrsena ST instrukcija sa registrima " << *gpr << " i operandima: ";
  printArguments(arg);
}

void process_CSRRD_INSTR(string* csr, string* gpr){
  cout << "Izvrsena CSRRD instrukcija sa " << *csr << " i " << *gpr << endl;
}

void process_CSRWR_INSTR(string* gpr, string* csr){
  cout << "Izvrsena CSRWR instrukcija sa " << *gpr << " na " << *csr << endl;
}