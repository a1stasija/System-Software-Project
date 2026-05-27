%option noyywrap

%{
  #include <string>
  using namespace std;
  #include "parser.tab.h"
  unsigned int lineNum = 1;
%}

%%


"[" { return LBRACKET; }
"]" { return RBRACKET; }
"," { return COMMA; }
"+" { return PLUS; }
":" { return COLON; }
"$" { return DOLLAR; }
"%" { return PERCENT; }

".global"|".GLOBAL" { return GLOBAL_DIR; }
".extern"|".EXTERN" { return EXTERN_DIR;}
".section"|".SECTION" { return SECTION_DIR;}
".word"|".WORD" { return WORD_DIR; }
".skip"|".SKIP" { return SKIP_DIR; }
".end"|".END" { return END_DIR; }

"halt"|"HALT" { return HALT_INSTR; }
"int"|"INT" { return INT_INSTR; }
"iret"|"IRET" { return IRET_INSTR; }
"call"|"CALL" { return CALL_INSTR; }
"ret"|"RET" { return RET_INSTR; }
"jmp"|"JMP" { return JMP_INSTR; }
"beq"|"BEQ" { return BEQ_INSTR; }
"bne"|"BNE" { return BNE_INSTR; }
"bgt"|"BGT" { return BGT_INSTR; }
"push"|"PUSH" { return PUSH_INSTR; }
"pop"|"POP" { return POP_INSTR; }
"xchg"|"XCHG" { return XCHG_INSTR; }
"add"|"ADD" { return ADD_INSTR; }
"sub"|"SUB" { return SUB_INSTR; }
"mul"|"MUL" { return MUL_INSTR; }
"div"|"DIV" { return DIV_INSTR; }
"not"|"NOT" { return NOT_INSTR; }
"and"|"AND" { return AND_INSTR; }
"or"|"OR" { return OR_INSTR; }
"xor"|"xor" { return XOR_INSTR; }
"shl"|"SHL" { return SHL_INSTR; }
"shr"|"SHR" { return SHR_INSTR; }
"ld"|"LD" { return LD_INSTR; }
"st"|"ST" { return ST_INSTR; }
"csrrd"|"CSRRD" {return CSRRD_INSTR; }
"csrwr"|"CSRWR" { return CSRWR_INSTR; }

"r0"|"r1"|"r2"|"r3"|"r4"|"r5"|"r6"|"r7"|"r8"|"r9"|"r10"|"r11"|"r12"|"r13" { yylval.psval = new string(yytext); return REGISTER; }

"sp"|"r14" { yylval.psval = new string("r14"); return SP_REGISTER; }

"pc"|"r15" { yylval.psval = new string("r15"); return PC_REGISTER; }

"%status"|"%handler"|"%cause" { yylval.psval = new string(yytext); return SPEC_REGISTER;}

(([-]?[0-9])|([-]?[0-9]+[\.]?[0-9]+)|([-]?0x[0-9a-fA-F]+)|([-]?0[0-7]+)) { yylval.psval = new string(yytext); return LITERAL;}

[a-zA-Z][a-zA-Z0-9_]* { yylval.psval = new string(yytext); return SYMBOL;}

\n  {lineNum++; return NEWLINE;}

[\t]      ; // Ignoriši beline

#.*      {return NEWLINE;} // Ignoriši komentare (jednolinijski, završava se sa \n)

. ;


%%
