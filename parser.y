%code requires{
  #include <cstdio>
  #include <iostream>
  #include <string>
  #include "../inc/assembler.hpp"
  using namespace std;

  extern int yylex();
  extern int yyparse();
  extern FILE *yyin;

  void yyerror(const char *s);
  
  extern unsigned int lineNum;
}

%union{
  string* psval;
  Arguments* argsval;

}

%token LBRACKET RBRACKET COMMA COLON PLUS DOLLAR PERCENT NEWLINE END_DIR GLOBAL_DIR EXTERN_DIR WORD_DIR SECTION_DIR SKIP_DIR HALT_INSTR INT_INSTR IRET_INSTR CALL_INSTR RET_INSTR JMP_INSTR BEQ_INSTR BNE_INSTR BGT_INSTR PUSH_INSTR POP_INSTR XCHG_INSTR ADD_INSTR SUB_INSTR MUL_INSTR DIV_INSTR NOT_INSTR AND_INSTR OR_INSTR XOR_INSTR SHL_INSTR SHR_INSTR LD_INSTR ST_INSTR CSRRD_INSTR CSRWR_INSTR
%token <psval> SPEC_REGISTER LITERAL REGISTER SP_REGISTER PC_REGISTER SYMBOL

%type <argsval> list_of_symbols list_of_symbols_or_literals operandAddr operand
%type <psval> section_name skip_literal gpr csr

%%
program:
     statements opt_new_line END_DIR opt_new_line {process_END_DIR();}
;

statements:
    opt_new_line statement NEWLINE
    | statements opt_new_line statement NEWLINE
;

opt_new_line:
  opt_new_line NEWLINE
  | /*prazan*/
;

statement:
    label_optional statement_body
    | label_only
;

label_optional:
    SYMBOL COLON { process_label($1); delete $1; }
    | /* prazan */
;

label_only:
    SYMBOL COLON { process_label($1); delete $1; }
;

statement_body:
    directive
    | instruction
;

directive:
  GLOBAL_DIR list_of_symbols {process_GLOBAL_DIR($2); delete $2;}
  | EXTERN_DIR list_of_symbols {process_EXTERN_DIR($2); delete $2;}
  | SECTION_DIR section_name {process_SECTION_DIR($2); delete $2;}
  | WORD_DIR list_of_symbols_or_literals {process_WORD_DIR($2); delete $2;}
  | SKIP_DIR skip_literal {process_SKIP_DIR($2); delete $2;}
;

skip_literal: 
  LITERAL { $$ = $1; }
;

instruction:
  HALT_INSTR {process_HALT_INSTR();}
  | INT_INSTR {process_INT_INSTR();}
  | IRET_INSTR {process_IRET_INSTR();}
  | CALL_INSTR operandAddr {process_CALL_INSTR($2); delete $2;}
  | RET_INSTR {process_RET_INSTR();}
  | JMP_INSTR operandAddr {process_JMP_INSTR($2); delete $2;}
  | BEQ_INSTR gpr COMMA gpr COMMA operand {process_BEQ_INSTR($2,$4,$6); delete $2; delete $4; delete $6;}
  | BNE_INSTR gpr COMMA gpr COMMA operand {process_BNE_INSTR($2,$4,$6); delete $2; delete $4; delete $6;}
  | BGT_INSTR gpr COMMA gpr COMMA operand {process_BGT_INSTR($2,$4,$6); delete $2; delete $4; delete $6;}
  | PUSH_INSTR gpr {process_PUSH_INSTR($2); delete $2;}
  | POP_INSTR gpr {process_POP_INSTR($2); delete $2;}
  | XCHG_INSTR gpr COMMA gpr {process_XCHG_INSTR($2,$4); delete $2; delete $4;}
  | ADD_INSTR gpr COMMA gpr {process_ADD_INSTR($2,$4); delete $2; delete $4;}
  | SUB_INSTR gpr COMMA gpr {process_SUB_INSTR($2,$4); delete $2; delete $4;}
  | MUL_INSTR gpr COMMA gpr {process_MUL_INSTR($2,$4); delete $2; delete $4;}
  | DIV_INSTR gpr COMMA gpr {process_DIV_INSTR($2,$4); delete $2; delete $4;}
  | NOT_INSTR gpr {process_NOT_INSTR($2); delete $2;}
  | AND_INSTR gpr COMMA gpr {process_AND_INSTR($2,$4); delete $2; delete $4;}
  | OR_INSTR gpr COMMA gpr {process_OR_INSTR($2,$4); delete $2; delete $4;}
  | XOR_INSTR gpr COMMA gpr {process_XOR_INSTR($2,$4); delete $2; delete $4;}
  | SHL_INSTR gpr COMMA gpr {process_SHL_INSTR($2,$4); delete $2; delete $4;}
  | SHR_INSTR gpr COMMA gpr {process_SHR_INSTR($2,$4); delete $2; delete $4;}
  | LD_INSTR operand COMMA gpr {process_LD_INSTR($2,$4); delete $2; delete $4;}
  | ST_INSTR gpr COMMA operand {process_ST_INSTR($2,$4); delete $2; delete $4;}
  | CSRRD_INSTR csr COMMA gpr {process_CSRRD_INSTR($2,$4); delete $2; delete $4;}
  | CSRWR_INSTR gpr COMMA csr{process_CSRWR_INSTR($2,$4); delete $2; delete $4;}
;

gpr:
  PERCENT REGISTER { $$ = $2; }
  | PERCENT SP_REGISTER { $$ = $2; }
  | PERCENT PC_REGISTER { $$ = $2; }
;

csr:
  SPEC_REGISTER { $$ = $1; }
;

list_of_symbols:
  SYMBOL { $$ = new Arguments($1, 1, AddressingMode::OTHER);}
  | list_of_symbols COMMA SYMBOL {$1->argName->push_back($3); $1->argType->push_back(1); $$ = $1;}
;

section_name:
  SYMBOL { $$ = $1; }
;

list_of_symbols_or_literals:
  SYMBOL { $$ = new Arguments($1, 1, AddressingMode::OTHER);}
  | LITERAL {$$ = new Arguments($1, 0, AddressingMode::OTHER);}
  | list_of_symbols_or_literals COMMA SYMBOL {$1->argName->push_back($3); $1->argType->push_back(1); $$ = $1;}
  | list_of_symbols_or_literals COMMA LITERAL {$1->argName->push_back($3); $1->argType->push_back(0); $$ = $1;}
;

operand:
  DOLLAR LITERAL {$$ = new Arguments($2,0,AddressingMode::IMMEDIATE_LITERAL);}
  | DOLLAR SYMBOL {$$ = new Arguments($2,1,AddressingMode::IMMEDIATE_SYMBOL);}
  | LITERAL {$$ = new Arguments($1,0,AddressingMode::MEMORY_DIRECT_LITERAL);}
  | SYMBOL {$$ = new Arguments($1,1,AddressingMode::MEMORY_DIRECT_SYMBOL);}
  | PERCENT REGISTER {$$ = new Arguments($2,2,AddressingMode::REGISTER_DIRECT);}
  | LBRACKET gpr RBRACKET {$$ = new Arguments($2,2,AddressingMode::REGISTER_INDIRECT);}
  | LBRACKET gpr PLUS LITERAL RBRACKET {$$ = new Arguments($2,2,AddressingMode::REGISTER_OFFSET_LITERAL); $$->argName->push_back($4); $$->argType->push_back(0); }
  | LBRACKET gpr PLUS SYMBOL RBRACKET {$$ = new Arguments($2,2,AddressingMode::REGISTER_OFFSET_SYMBOL); $$->argName->push_back($4); $$->argType->push_back(1); }
;

operandAddr:
  LITERAL {$$ = new Arguments($1, 0, AddressingMode::ADDRESS_LITERAL);}
  | SYMBOL{$$ = new Arguments($1, 1, AddressingMode::ADDRESS_SYMBOL);}
;

%%


int main(int argc, char ** argv){
  FILE *myFile = NULL;
  myFile = fopen(argv[1], "r");

  if(myFile == NULL){
    perror("Nije uspelo otvaranje fajla");
    return -1;
  } else {
    yyin = myFile;
  }

  //while(yylex());
  yyparse();
  //free_mem();
}

void yyerror(const char *s){
  cout << "GRESKA u parseru na liniji: " << lineNum << " sa uzrokom: " << s << endl;
  exit(-1);
}