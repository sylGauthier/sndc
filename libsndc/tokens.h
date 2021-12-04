#ifndef TOKENS_H
#define TOKENS_H

#include <stdio.h>
#include <stdint.h>

#define MAX_IDENT 128

extern FILE* yyin;
extern char strVal[];
extern uint64_t intVal;
extern double dblVal;
extern int yylineno;
extern char* yytext;

enum Token {
    END = 0,

    /* Symbols */
    OBRACE,
    CBRACE,
    COLON,
    SEMICOLON,
    DOT,
    MINUS,

    /* Literals */
    DEC_LIT,
    HEX_LIT,
    OCT_LIT,
    BIN_LIT,
    FLOAT_LIT,
    STRING_LIT,
    IMPORT_LIT,

    /* Keywords */
    IMPORT,
    EXPORT,
    AS,
    INPUT,
    OUTPUT,

    IDENT,
    UNKNOWN
};

int yylex();
int yylex_destroy();

#endif
