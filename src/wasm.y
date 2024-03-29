%{
// Parser for a slightly limited subset of Why.js assembly.
// Some pseudoinstructions and the "&="-type shorthands aren't supported.

#include <cassert>
#include <cstdarg>
#include <initializer_list>

#define NO_YYPARSE
#include "Lexer.h"
#include "ASTNode.h"
#include "Parser.h"
#include "StringSet.h"
#include "wasm/Nodes.h"

// Disable PVS-Studio warnings about branches that do the same thing.
//-V::1037

template <typename ...Args>
void D(Args && ...args) {
	(void) std::initializer_list<int> {
		((void) delete args, 0)...
	};
}

template <typename T>
const ASTLocation & L(std::initializer_list<const T *> nodes) {
	for (const T *node: nodes)
		if (node)
			return node->location;
	throw std::runtime_error("Couldn't find location: all nodes are null");
}

using AN = ASTNode;

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
%}

%debug
%defines
%define parse.error verbose
%token-table
%verbose
%glr-parser

%define api.prefix {wasm}

%token WASMTOK_ROOT WASMTOK_IDENT WASMTOK_INT_TYPE WASMTOK_TYPE

%token WASMTOK_BANG "!"
%token WASMTOK_EQUALS "="
%token WASMTOK_DOT "."
%token WASMTOK_LCURLY "{"
%token WASMTOK_PERCENT "%"
%token WASMTOK_AT "@"
%token WASMTOK_LPAR "("
%token WASMTOK_RPAR ")"
%token WASMTOK_LSQUARE "["
%token WASMTOK_RSQUARE "]"
%token WASMTOK_X "x"
%token WASMTOK_LANGLE "<"
%token WASMTOK_RANGLE ">"
%token WASMTOK_MEMSET "memset"
%token WASMTOK_LUI "lui"
%token WASMTOK_ASTERISK "*"
%token WASMTOK_COLON ":"
%token WASMTOK_PLUS "+"
%token WASMTOK_MINUS "-"
%token WASMTOK_INTO "->"
%token WASMTOK_AND "&"
%token WASMTOK_NAND "~&"
%token WASMTOK_LAND "&&"
%token WASMTOK_LNAND "!&&"
%token WASMTOK_OR "|"
%token WASMTOK_NOR "~|"
%token WASMTOK_LOR "||"
%token WASMTOK_LNOR "!||"
%token WASMTOK_XNOR "~x"
%token WASMTOK_NOT "~"
%token WASMTOK_SLASH "/"
%token WASMTOK_LXNOR "!xx"
%token WASMTOK_LXOR "xx"
%token WASMTOK_SEMICOLON ";"
%token WASMTOK_LEQ "<="
%token WASMTOK_DEQ "=="
%token WASMTOK_GEQ ">="
%token WASMTOK_NEWLINE "\n"
%token WASMTOK_NOTEQUAL "!="
%token WASMTOK_IF "if"
%token WASMTOK_NOP "<>"
%token WASMTOK_INT "%int"
%token WASMTOK_RIT "%rit"
%token WASMTOK_TIME "%time"
%token WASMTOK_RING "%ring"
%token WASMTOK_LL "<<"
%token WASMTOK_RL ">>>"
%token WASMTOK_RA ">>"
%token WASMTOK_PRC "prc"
%token WASMTOK_PRINT "print"
%token WASMTOK_PRX "prx"
%token WASMTOK_PRD "prd"
%token WASMTOK_PRB "prb"
%token WASMTOK_HALT "halt"
%token WASMTOK_SLEEP "sleep"
%token WASMTOK_ON "on"
%token WASMTOK_OFF "off"
%token WASMTOK_PAGE "%page"
%token WASMTOK_SETPT "%setpt"
%token WASMTOK_QUESTION "?"
%token WASMTOK_MEM "mem"
%token WASMTOK_P "p"
%token WASMTOK_REG
%token WASMTOK_NUMBER
%token WASMTOK_CHAR
%token WASMTOK_STRING
%token WASMTOK_META_HEADER "#meta"
%token WASMTOK_INCLUDE_HEADER "#include"
%token WASMTOK_DEBUG_HEADER "#debug"
%token WASMTOK_TEXT_HEADER "#text"
%token WASMTOK_VERSION "version"
%token WASMTOK_AUTHOR "author"
%token WASMTOK_ORCID "orcid"
%token WASMTOK_NAME "name"
%token WASMTOK_INC "++"
%token WASMTOK_DEC "--"
%token WASMTOK_REST "rest"
%token WASMTOK_IO "io"
%token WASMTOK_DI "%di"
%token WASMTOK_EI "%ei"
%token WASMTOK_DIR_TYPE "%type"
%token WASMTOK_DIR_SIZE "%size"
%token WASMTOK_DIR_STRING "%string"
%token WASMTOK_DIR_STRINGZ "%stringz"
%token WASMTOK_DIR_1B "%1b"
%token WASMTOK_DIR_2B "%2b"
%token WASMTOK_DIR_4B "%4b"
%token WASMTOK_DIR_8B "%8b"
%token WASMTOK_DIR_ALIGN "%align"
%token WASMTOK_DIR_FILL "%fill"
%token WASMTOK_FUNCTION "function"
%token WASMTOK_OBJECT "object"
%token WASMTOK_DATA "%data"
%token WASMTOK_CODE "%code"
%token WASMTOK_INSTRUCTION "instruction"
%token WASMTOK_SEXT "sext"
%token WASMTOK_TRANSLATE "translate"
%token WASMTOK_BC "bc"
%token WASMTOK_FUNCTION_TYPE "#fn"

%token WASM_RNODE WASM_STATEMENTS WASM_INODE WASM_COPYNODE WASM_LOADNODE WASM_STORENODE WASM_SETNODE WASM_LINODE
%token WASM_SINODE WASM_LNINODE WASM_CHNODE WASM_LHNODE WASM_SHNODE WASM_CMPNODE WASM_CMPINODE WASM_SELNODE WASM_JNODE
%token WASM_JCNODE WASM_JRNODE WASM_JRCNODE WASM_IMMEDIATE WASM_MULTRNODE WASM_MULTINODE WASM_DIVIINODE WASM_LUINODE
%token WASM_STACKNODE WASM_NOPNODE WASM_INTINODE WASM_RITINODE WASM_TIMEINODE WASM_TIMERNODE WASM_RINGINODE
%token WASM_RINGRNODE WASM_PRINTNODE WASM_HALTNODE WASM_SLEEPRNODE WASM_PAGENODE WASM_SETPTINODE WASM_MVNODE WASM_LABEL
%token WASM_SETPTRNODE WASM_SVPGNODE WASM_QUERYNODE WASM_PSEUDOPRINTNODE WASM_INCLUDES WASM_STATEMENT
%token WASM_ARGS WASM_STRINGPRINTNODE WASM_JEQNODE WASM_CSNODE WASM_LSNODE WASM_SSNODE WASM_SIZEDSTACKNODE WASM_RESTNODE
%token WASM_IONODE WASM_ARRAYVALUE WASM_INTVALUE WASM_STRUCTVALUE WASM_POINTERVALUE WASM_AGGREGATEVALUE WASM_ARRAYTYPE
%token WASM_STRUCTTYPE WASM_POINTERTYPE WASM_TYPELIST WASM_AGGREGATELIST WASM_INTERRUPTSNODE WASM_TYPEDIR WASM_SIZEDIR
%token WASM_STRINGDIR WASM_VALUEDIR WASM_ALIGNDIR WASM_FILLDIR WASM_CODEDIR WASM_DATADIR WASM_EXPRESSION
%token WASM_INVERSENODE WASM_TRANSNODE WASM_PAGESTACKNODE WASM_SVRINGNODE WASM_SVTIMENODE

%start start

%define api.value.type {ASTNode *}

%initial-action {
    wasmParser.root = new ASTNode(wasmParser, WASMTOK_ROOT, ASTLocation(), "");
}

%%

start: program;

program: program statement { $$ = $1->adopt($2); }
       | program label { $$ = $1->adopt($2); }
       | program endop { $$ = $1; D($2); }
       | { $$ = wasmParser.root; };

statement: operation;
endop: "\n" | ";";

type: WASMTOK_TYPE;
typed_reg: reg type { $$ = $2->adopt($1); };
typed_imm: immediate type { $$ = $2->adopt($1); };
address: immediate { $$ = (new ASTNode(wasmParser, WASMTOK_TYPE, "{uv*}"))->adopt($1); };

operation: op_r     | op_mult | op_multi | op_lui   | op_i     | op_c      | op_l      | op_s    | op_set   | op_divii
         | op_li    | op_si   | op_ms    | op_lni   | op_cmp   | op_cmpi   | op_sel    | op_j    | op_jc    | op_jr
         | op_jrc   | op_mv   | op_spush | op_spop  | op_nop   | op_int    | op_rit    | op_time | op_timei | op_ext
         | op_ringi | op_ring | op_page  | op_setpt | op_svpg  | op_qmem   | op_sprint | op_inc  | op_dec   | op_di
         | op_ei    | op_inv  | op_trans | op_ppush | op_ppop  | op_svring | op_svtime;

label: "@" ident          { $$ = new WASMLabelNode($2); D($1); }
     | "@" WASMTOK_STRING { $$ = new WASMLabelNode($2->extracted()); D($1); };

op_r: typed_reg basic_oper_r typed_reg "->" typed_reg { $$ = new RNode($1, $2, $3, $5); D($4); }
    | typed_reg shorthandable_r "=" typed_reg         { $$ = new RNode($1, $2, $4, $1); D($3); }
    | "~" typed_reg "->" typed_reg { $$ = new RNode($2, $1, nullptr, $4); D($3); }
    | "!" typed_reg "->" typed_reg { $$ = new RNode($2, $1, nullptr, $4); D($3); }
    | "sext" typed_reg "->" typed_reg { $$ = new RNode($2, $1, nullptr, $4); D($3); }
    | "bc"   typed_reg "->" typed_reg { $$ = new RNode($2, $1, nullptr, $4); D($3); };
basic_oper_r: shorthandable_r | "<" | "<=" | "==" | ">" | ">=" | "!";
logical: "&&" | "||" | "!&&" | "!||" | "!xx" | "xx";
shorthandable_r: logical | shorthandable_i;

op_mult: typed_reg "*" typed_reg { $$ = new WASMMultRNode($1, $3); D($2); };

op_multi: typed_reg "*" typed_imm { $$ = new WASMMultINode($1, $3); D($2); };

op_lui: "lui" ":" typed_imm "->" typed_reg { $$ = new WASMLuiNode($3, $5); D($1, $2, $4); };

op_i: typed_reg basic_oper_i typed_imm "->" typed_reg { $$ = new INode($1, $2, $3, $5); D($4); }
    | typed_reg shorthandable_i "=" typed_imm         { $$ = new INode($1, $2, $4, $1); D($3); };
basic_oper_i: shorthandable_i | "<" | "<=" | "==" | ">" | ">=" | "!";
shorthandable_i: "+" | "-" | "&" | "|" | "x" | "~x" | "~&" | "~|" | "/" | "%" | "<<" | ">>>" | ">>";

op_inv: op_sllii | op_srlii | op_sraii;
op_sllii: typed_imm "<<"  typed_reg "->" typed_reg { $$ = new WASMInverseShiftNode($3, $2, $1, $5); D($4); };
op_srlii: typed_imm ">>>" typed_reg "->" typed_reg { $$ = new WASMInverseShiftNode($3, $2, $1, $5); D($4); };
op_sraii: typed_imm ">>"  typed_reg "->" typed_reg { $$ = new WASMInverseShiftNode($3, $2, $1, $5); D($4); };

op_inc: typed_reg "++" { $$ = new INode($1->text, StringSet::intern("+"), TypedImmediate(OperandType($1), 1), $1->text, WASMTOK_PLUS); D($2); };

op_dec: typed_reg "--" { $$ = new INode($1->text, StringSet::intern("-"), TypedImmediate(OperandType($1), 1), $1->text, WASMTOK_MINUS); D($2); };

op_c: "[" typed_reg "]" "->" "[" typed_reg "]" { $$ = new WASMCopyNode($2, $6); D($1, $3, $4, $5, $7); };

op_l: "[" typed_reg "]" "->" typed_reg { $$ = new WASMLoadNode($2, $5); D($1, $3, $4); };

op_s: typed_reg "->" "[" typed_reg "]" { $$ = new WASMStoreNode($1, $4); D($2, $3, $5); };

op_set: typed_imm "->" typed_reg { $$ = new WASMSetNode($1, $3); D($2); };

op_divii: typed_imm "/" typed_reg "->" typed_reg { $$ = new WASMDiviINode($1, $3, $5); D($2, $4); };

op_li: "[" typed_imm "]" "->" typed_reg { $$ = new WASMLiNode($2, $5); D($1, $3, $4); };

op_si: typed_reg "->" "[" typed_imm "]" { $$ = new WASMSiNode($1, $4); D($2, $3, $5); };

op_ms: "memset" typed_reg "x" typed_reg "->" typed_reg { $$ = new RNode($2, $1, $4, $6); D($5); };

op_lni: "[" typed_imm "]" "->" "[" typed_reg "]" { $$ = new WASMLniNode($2, $6); D($1, $3, $4, $5, $7); };

op_trans: "translate" typed_reg "->" typed_reg { $$ = new WASMTransNode($2, $4); D($1, $3); };

op_cmp: typed_reg "~" typed_reg { $$ = new WASMCmpNode($1, $3); D($2); };

op_cmpi: typed_reg "~" typed_imm { $$ = new WASMCmpiNode($1, $3); D($2); };

op_sel: "[" typed_reg selop typed_reg "]" "->" typed_reg { $$ = new WASMSelNode($2, $3, $4, $7); D($1, $5, $6); };
selop: "=" | "<" | ">" | "!=";

op_j: _jcond colons address { $$ = new WASMJNode($1, $2, $3); };
_jcond: jcond | { $$ = nullptr; };
jcond: zero | "+" | "-" | "*";
colons: ":" ":" { $$ = $1->adopt($2); } | ":";

op_jc: op_j "if" typed_reg { $$ = new WASMJcNode(dynamic_cast<WASMJNode *>($1), $3); D($2); };

op_jr: _jcond colons typed_reg { $$ = new WASMJrNode($1, $2, $3); };

op_jrc: op_jr "if" typed_reg { $$ = new WASMJrcNode(dynamic_cast<WASMJrNode *>($1), $3); D($2); };

op_mv: typed_reg "->" typed_reg { $$ = new WASMMvNode($1, $3); D($2); };

op_spush: "[" typed_reg { $$ = new WASMStackNode($2, true); D($1); };

op_spop: "]" typed_reg { $$ = new WASMStackNode($2, false); D($1); };

op_nop: "<>" { $$ = new WASMNopNode(); D($1); };

op_int: "%int" typed_imm { $$ = new WASMIntINode($2); D($1); };

op_rit: "%rit" typed_imm { $$ = new WASMRitINode($2); D($1); };

op_time: "%time" typed_reg { $$ = new WASMTimeRNode($2); D($1); };

op_timei: "%time" typed_imm { $$ = new WASMTimeINode($2); D($1); };

op_svtime: "%time" "->" typed_reg { $$ = new WASMSvtimeNode($3); D($1, $2); };

op_ring: "%ring" typed_reg { $$ = new WASMRingRNode($2); D($1); };

op_svring: "%ring" "->" typed_reg { $$ = new WASMSvringNode($3); D($1, $2); };

op_ringi: "%ring" typed_imm { $$ = new WASMRingINode($2); D($1); };

op_di: "%di" { $$ = new WASMInterruptsNode(false); D($1); };

op_ei: "%ei" { $$ = new WASMInterruptsNode(true); D($1); };

op_ext: op_print | op_pprint | op_sleep | op_halt | op_rest | op_io;

op_sleep: "<" "sleep" typed_reg ">" { $$ = new WASMSleepRNode($3); D($1, $2, $4); };

op_rest: "<" "rest" ">" { $$ = new WASMRestNode; D($1, $2, $3); };

op_io: "<" "io" ident ">" { $$ = new WASMIONode($3->text); D($1, $2, $3, $4); };

op_print: "<" printop typed_reg ">" { $$ = new WASMPrintNode($3, $2); D($1, $4); };
printop: "print" | "prx" | "prd" | "prc" | "prb" | "p";

op_pprint: "<" "prc" character ">" { $$ = new WASMPseudoPrintNode($3); D($1, $2, $4); }
         | "<" "p"   character ">" { $$ = new WASMPseudoPrintNode($3); D($1, $2, $4); };

op_sprint: "<" "p" string ">" { $$ = new WASMPseudoPrintNode($3->text); D($1, $2, $3, $4); };

op_halt: "<" "halt" ">" { $$ = new WASMHaltNode(); D($1, $2, $3); };

op_page: "%page" "on"  { $$ = new WASMPageNode(true);  D($1, $2); };
       | "%page" "off" { $$ = new WASMPageNode(false); D($1, $2); };

op_setpt: "%setpt" typed_reg               { $$ = new WASMSetptRNode($2); D($1); }
        | ":" "%setpt" typed_reg typed_reg { $$ = new WASMSetptRNode($3, $4); D($1, $2); };

op_svpg: "%page" "->" typed_reg { $$ = new WASMSvpgNode($3); D($1, $2); };

op_ppush: "[" "%page" { $$ = new WASMPageStackNode(true); D($1, $2); };

op_ppop:     "]" "%page"           { $$ = new WASMPageStackNode(false);     D($1, $2);     }
       | ":" "]" "%page" typed_reg { $$ = new WASMPageStackNode(false, $4); D($1, $2, $3); };

op_qmem: "?" "mem" "->" typed_reg { $$ = new WASMQueryNode(QueryType::Memory, $4); D($1, $2, $3); };

immediate: "&" ident { $$ = $2; D($1); }
         | "&" WASMTOK_STRING { $$ = $2; D($1); }
         | ident
         | number
         | character
         | WASMTOK_STRING;

ident: ident_option { $1->symbol = WASMTOK_IDENT; } | WASMTOK_IDENT;
ident_option: "memset" | "lui" | "if" | "halt" | "on" | "off" | "sleep" | "io" | "version" | "author" | "orcid" | "name"
            | "sext" | printop | "translate";

zero: number { if (*$1->text != "0") { wasmerror("Invalid number in jump condition: " + *$1->text); } };

reg: WASMTOK_REG;
number: WASMTOK_NUMBER | "-" WASMTOK_NUMBER { $$ = $2; $$->text = StringSet::intern("-" + *$$->text); D($1); };
character: WASMTOK_CHAR;
string: WASMTOK_STRING;

%%

#pragma GCC diagnostic pop

const char * Parser::getNameWASM(int symbol) {
    return yytname[YYTRANSLATE(symbol)];
}
