#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>

#include <sndc.h>
#include <modules/utils.h>

#define FN_STACK_SIZE   128

static int func_process(struct Node* n);

/* DECLARE_MODULE(func) */
const struct Module func = {
    "func", "A generator for mathematical functions",
    {
        {"expr",        DATA_STRING,    REQUIRED,
                        "mathematical function in infixe notation\n"
                        " - use '$s' for position within buffer (0.0 to 1.0)\n"
                        " - use '$t' for time (0.0 to 'duration')\n"
                        " - use '$n' for sample number\n"
                        " - use '$<N>' for param<N>"},

        {"duration",    DATA_FLOAT,     REQUIRED,
                        "duration of resulting signal"},

        {"sampling",    DATA_FLOAT,     OPTIONAL,
                        "sampling rate of resulting signal, def 44100"},

        {"interp",      DATA_STRING,    OPTIONAL,
                        "interpolation of resulting buffer, def 'linear'"},

        {"param0",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 0 for mathematical function, '$0'"},
        {"param1",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 1 for mathematical function, '$1'"},
        {"param2",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 2 for mathematical function, '$2'"},
        {"param3",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 3 for mathematical function, '$3'"},
        {"param4",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 4 for mathematical function, '$4'"},
        {"param5",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 5 for mathematical function, '$5'"},
        {"param6",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 6 for mathematical function, '$6'"},
        {"param7",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 7 for mathematical function, '$7'"},
        {"param8",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 8 for mathematical function, '$8'"},
        {"param9",      DATA_FLOAT | DATA_BUFFER,     OPTIONAL,
                        "param 9 for mathematical function, '$9'"},
    },
    {
        {"out",         DATA_BUFFER,    REQUIRED, "resulting signal"}
    },
    NULL,
    func_process,
    NULL
};

enum InputType {
    FUN = 0,
    DUR,
    SPL,
    ITP,
    PM0,
    PM1,
    PM2,
    NUM_INPUTS
};

enum FnTokenType {
    FN_NONE = 0,

    FN_LIT,

    FN_PLUS,
    FN_MINUS,
    FN_MULT,
    FN_DIV,
    FN_NEG,
    FN_POW,
    FN_EQU,
    FN_NEQ,
    FN_LT,
    FN_GT,
    FN_LEQ,
    FN_GEQ,

    FN_FUN,

    FN_OPAR,
    FN_CPAR,

    FN_S,
    FN_T,
    FN_N,
    FN_I
};

static const char* tkname[] = {
    "<none>", "LIT", "PLUS", "MINUS", "MULT", "DIV", "NEG", "POW",
    "EQU", "NEQ", "LT", "GT", "LEQ", "GEQ",
    "FUN",
    "OPAR", "CPAR", "REG_S", "REG_T", "REG_N", "REG_I"
};

static const int tkprec[] = {
    0, 0, 4, 4, 3, 3, 2, 1, 7, 7, 6, 6, 6, 6, 0, 0, 0, 0, 0, 0, 0
};

struct FnMathFunc {
    const char* name;
    float (*func)(float);
};

static const struct FnMathFunc functions[] = {
    {"exp", expf},
    {"log", logf},
    {"sqrt", sqrtf},

    {"sin", sinf},
    {"cos", cosf},
    {"tan", tanf},
    {"asin", asinf},
    {"acos", acosf},
    {"atan", atanf},

    {NULL, NULL}
};

struct FnToken {
    enum FnTokenType type;
    union FnTokenValue {
        unsigned int n;
        float f;
        float (*func)(float);
    } val;
};

static int func_setup(struct Node* n) {
    struct Buffer* out;

    GENERIC_CHECK_INPUTS(n, func);

    n->outputs[0]->type = DATA_BUFFER;
    out = &n->outputs[0]->content.buf;

    if (n->inputs[SPL]) out->samplingRate = n->inputs[SPL]->content.f;
    else out->samplingRate = 44100;

    if (!n->inputs[ITP]) out->interp = INTERP_LINEAR;
    else if ((out->interp = data_parse_interp(n->inputs[ITP])) < 0) return 0;

    out->size = n->inputs[DUR]->content.f * out->samplingRate;
    out->data = NULL;
    return 1;
}

static void print_token(struct FnToken* tk) {
    fprintf(stderr, "%s", tkname[tk->type]);
    if (tk->type == FN_LIT) fprintf(stderr, " = %f", tk->val.f);
    if (tk->type == FN_I) fprintf(stderr, " [%d]", tk->val.n);
    fprintf(stderr, "\n");
}

static void print_stack(struct FnToken* stack, unsigned int len) {
    unsigned int i;
    for (i = 0 ; i < len; i++) print_token(stack + i);
}

static float (*get_func(const char* f, const char* (*end)))(float) {
    const char* cur = f;
    unsigned int i;

    while (*cur >= 'a' && *cur <= 'z') cur++;
    if (cur == f || *cur != '(') return NULL;
    for (i = 0; functions[i].name; i++) {
        if (!strncmp(functions[i].name, f, strlen(functions[i].name))
                && strlen(functions[i].name) == cur - f) {
            *end = cur;
            return functions[i].func;
        }
    }
    return NULL;
}

#define IS_OP(t) ((t).type > FN_LIT && (t).type < FN_FUN)
#define PREC(t) (tkprec[t.type])

static const char* next_token(const char* func,
                              struct FnToken* tk,
                              struct FnToken* prevtk,
                              int* err) {
    const char* end;

    *err = 0;
    while (func[0] == ' ' || func[0] == '\t') func++;
    switch (func[0]) {
        case '\0': return NULL;
        case '-':
            if (       prevtk->type != FN_NONE
                    && prevtk->type != FN_OPAR
                    && !IS_OP(*prevtk)) {
                tk->type = FN_MINUS;
                return func + 1;
            }
            tk->type = FN_NEG;
            return func + 1;
        case '+': tk->type = FN_PLUS;   return func + 1;
        case '*': tk->type = FN_MULT;   return func + 1;
        case '/': tk->type = FN_DIV;    return func + 1;
        case '^': tk->type = FN_POW;    return func + 1;
        case '(': tk->type = FN_OPAR;   return func + 1;
        case ')': tk->type = FN_CPAR;   return func + 1;
        case '=':
            if (func[1] == '=') {
                tk->type = FN_EQU;
                return func + 2;
            }
            fprintf(stderr, "Error: func: single '='\n");
            *err = 1;
            return NULL;
        case '!':
            if (func[1] == '=') {
                tk->type = FN_NEQ;
                return func + 2;
            }
            break;
        case '<':
            if (func[1] == '=') {
                tk->type = FN_LEQ;
                return func + 2;
            }
            tk->type = FN_LT;
            return func + 1;
        case '>':
            if (func[1] == '=') {
                tk->type = FN_GEQ;
                return func + 2;
            }
            tk->type = FN_GT;
            return func + 1;
        case '$':
            switch (func[1]) {
                case 's': tk->type = FN_S; return func + 2;
                case 't': tk->type = FN_T; return func + 2;
                case 'n': tk->type = FN_N; return func + 2;
                default: break;
            }
            if (func[1] >= '0' && func[1] <= '9') {
                tk->type = FN_I;
                tk->val.n = func[1] - '0';
                return func + 2;
            }
            fprintf(stderr, "Error: func: invalid register: %2s\n", func);
            break;
        default: break;
    }
    if (func[0] >= '0' && func[0] <= '9') {
        tk->type = FN_LIT;
        tk->val.f = strtof(func, (char**) &end);
        return end;
    }
    if ((tk->val.func = get_func(func, &end))) {
        tk->type = FN_FUN;
        return end;
    }
    fprintf(stderr, "Error: func: unknown token: %c\n", func[0]);
    *err = 1;
    return NULL;
}

#define STACK_PUSH(s, v, l) \
    if ((l) >= FN_STACK_SIZE ) { \
        fprintf(stderr, "Error: stack size reached: %d\n", FN_STACK_SIZE); \
        return 0; \
    } else { \
        (s)[(l)++] = (v); \
    }

static int eval_stack(struct FnToken* expr,
                      unsigned int len,
                      float s,
                      float t,
                      unsigned int n,
                      struct Data* params[10],
                      float* res) {
    unsigned int i = 0;
    float stack[FN_STACK_SIZE];
    unsigned int stackLen = 0;

    for (i = 0; i < len; i++) {
        switch (expr[i].type) {
            case FN_LIT:
                STACK_PUSH(stack, expr[i].val.f, stackLen);
                break;
            case FN_S:
                STACK_PUSH(stack, s, stackLen);
                break;
            case FN_T:
                STACK_PUSH(stack, t, stackLen);
                break;
            case FN_N:
                STACK_PUSH(stack, n, stackLen);
                break;
            case FN_I:
                STACK_PUSH(stack,
                           data_float(params[expr[i].val.n], s, 0),
                           stackLen);
                break;
            case FN_NEG:
                if (stackLen < 1) return 0;
                stack[stackLen - 1] *= -1;
                break;
            case FN_PLUS:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] += stack[stackLen - 1];
                stackLen--;
                break;
            case FN_MINUS:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] -= stack[stackLen - 1];
                stackLen--;
                break;
            case FN_MULT:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] *= stack[stackLen - 1];
                stackLen--;
                break;
            case FN_DIV:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] /= stack[stackLen - 1];
                stackLen--;
                break;
            case FN_POW:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] = pow(stack[stackLen - 2],
                                          stack[stackLen - 1]);
                stackLen--;
                break;
            case FN_EQU:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] =
                    (stack[stackLen - 2] == stack[stackLen - 1]);
                stackLen--;
                break;
            case FN_NEQ:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] =
                    (stack[stackLen - 2] != stack[stackLen - 1]);
                stackLen--;
                break;
            case FN_LT:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] =
                    (stack[stackLen - 2] < stack[stackLen - 1]);
                stackLen--;
                break;
            case FN_GT:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] =
                    (stack[stackLen - 2] > stack[stackLen - 1]);
                stackLen--;
                break;
            case FN_LEQ:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] =
                    (stack[stackLen - 2] <= stack[stackLen - 1]);
                stackLen--;
                break;
            case FN_GEQ:
                if (stackLen < 2) return 0;
                stack[stackLen - 2] =
                    (stack[stackLen - 2] >= stack[stackLen - 1]);
                stackLen--;
                break;
            case FN_FUN:
                stack[stackLen - 1] = expr[i].val.func(stack[stackLen - 1]);
                break;
            default:
                return 0;
        }
    }
    if (stackLen != 1) return 0;
    *res = stack[0];
    return 1;
}

static int func_process(struct Node* n) {
    struct Buffer* out = &n->outputs[0]->content.buf;
    struct FnToken token, prevToken = {0};
    struct FnToken queue[FN_STACK_SIZE], opStack[FN_STACK_SIZE];
    unsigned int queueLen = 0, opStackLen = 0, i;
    int err;
    const char* cur;

    if (!func_setup(n)) return 0;

    if (!(out->data = malloc(out->size * sizeof(float)))) {
        return 0;
    }
    cur = n->inputs[FUN]->content.str;

    /* shunting yard algorithm, see: 
     * https://en.wikipedia.org/wiki/Shunting-yard_algorithm
     */
    while ((cur = next_token(cur, &token, &prevToken, &err))) {
        switch (token.type) {
            case FN_LIT:
            case FN_S:
            case FN_T:
            case FN_N:
            case FN_I:
                STACK_PUSH(queue, token, queueLen);
                break;
            case FN_FUN:
                STACK_PUSH(opStack, token, opStackLen);
                break;
            case FN_PLUS:
            case FN_MINUS:
            case FN_MULT:
            case FN_DIV:
            case FN_POW:
            case FN_EQU:
            case FN_NEQ:
            case FN_LT:
            case FN_GT:
            case FN_LEQ:
            case FN_GEQ:
                while (    opStackLen
                        && IS_OP(opStack[opStackLen - 1])
                        && (PREC(opStack[opStackLen - 1]) < PREC(token)
                            || (PREC(opStack[opStackLen - 1]) == PREC(token)
                                && opStack[opStackLen - 1].type != FN_POW))) {
                    STACK_PUSH(queue, opStack[--opStackLen], queueLen);
                }
            case FN_NEG:
            case FN_OPAR:
                STACK_PUSH(opStack, token, opStackLen);
                break;
            case FN_CPAR:
                while (opStackLen && opStack[opStackLen - 1].type != FN_OPAR) {
                    STACK_PUSH(queue, opStack[--opStackLen], queueLen);
                }
                if (!opStackLen) {
                    fprintf(stderr, "Error: %s: function: "
                                    "')' has no matching '('\n",
                                    n->name);
                    return 0;
                }
                opStackLen--;
                if (opStackLen && opStack[opStackLen - 1].type == FN_FUN) {
                    STACK_PUSH(queue, opStack[--opStackLen], queueLen);
                }
                break;
            default:
                fprintf(stderr, "Error: %s: invalid token: %d\n",
                        n->name, token.type);
                return 0;
        }
        prevToken = token;
    }
    if (err) {
        fprintf(stderr, "Error: %s: token error\n", n->name);
        return 0;
    }
    for (; opStackLen; opStackLen--) {
        if (opStack[opStackLen - 1].type == FN_OPAR) {
            fprintf(stderr, "Error: %s: function: '(' has no matching ')'\n",
                    n->name);
            return 0;
        }
        STACK_PUSH(queue, opStack[opStackLen - 1], queueLen);
    }
    for (i = 0; i < out->size; i++) {
        float s = (float) i / (float) out->size;
        float t = (float) i / (float) out->samplingRate;

        if (!eval_stack(queue, queueLen,
                        s, t, i, n->inputs + PM0,
                        out->data + i)) {
            fprintf(stderr, "Error: %s: "
                    "stack error, expression might be incorrect\n",
                    n->name);
            return 0;
        }
    }
    return 1;
}
