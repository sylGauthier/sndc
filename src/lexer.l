%{
#include "tokens.h"

char strVal[MAX_IDENT];
uint64_t intVal;
double dblVal;
%}

digit           [0-9]
declit          {digit}(_?{digit})*
hexdigit        [a-fA-F0-9]
octdigit        [0-7]
bindigit        [0-1]
letter          [a-zA-Z_]
ident           {letter}({letter}|{digit})*

%x IN_COMMENT
%%
<INITIAL>{
"/*"      BEGIN(IN_COMMENT);
}
<IN_COMMENT>{
"*/"      BEGIN(INITIAL);
[^*\n]+   /* eat comment in chunks */
"*"       /* eat the lone star */
\n        yylineno++;
}

"{"                  { return OBRACE;       }
"}"                  { return CBRACE;       }
":"                  { return COLON;        }
";"                  { return SEMICOLON;    }
"."                  { return DOT;          }
"-"                  { return MINUS;        }

{declit}?\.{declit}((e|E)(\+|\-)?{declit})? { dblVal = strtod(yytext, NULL);
                                              return FLOAT_LIT;    }
{declit}\.                                  { dblVal = strtod(yytext, NULL);
                                              return FLOAT_LIT;    }
{declit}                                    { intVal = strtol(yytext, NULL, 10);
                                              return DEC_LIT;      }
("0x"|"0X"){hexdigit}(_?{hexdigit})*        { intVal = strtol(yytext+2, NULL, 16);
                                              return HEX_LIT;      }
("0o"|"0O"){octdigit}(_?{octdigit})*        { intVal = strtol(yytext+2, NULL, 8);
                                              return OCT_LIT;      }
("0b"|"0B"){bindigit}(_?{bindigit})*        { intVal = strtol(yytext+2, NULL, 2);
                                              return BIN_LIT;      }
\"(?:[^"\\]|\\.)*\"                         { return STRING_LIT;      }

"import"                                    { return IMPORT; }
"export"                                    { return EXPORT; }
"as"                                        { return AS; }
"input"                                     { return INPUT; }
"output"                                    { return OUTPUT; }

{ident}              { if (strlen(yytext) >= MAX_IDENT)
                           fprintf(stderr, "Warning: ident length %s exceeds MAX_IDENT (%d), truncating\n",
                                           yytext, MAX_IDENT);
                       strncpy(strVal, yytext, MAX_IDENT - 1);
                       strVal[MAX_IDENT - 1] = '\0';
                       return IDENT;      }
[\n\r]               { yylineno++;        } /* necessary for incrementing yylineno */
[ \t]                                       /* skip whitespace */

.                    { fprintf(stderr, "Unknown character [%c]\n",yytext[0]);
                       return UNKNOWN;    }
%%

int yywrap(void){return 1;}
