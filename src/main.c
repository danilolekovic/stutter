#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include "mpc.h"

typedef struct {
    int type;
    long num;
    int err;
} sval;

enum { SVAL_NUM, SVAL_ERR };
enum { SERR_DIV_ZERO, SERR_BAD_OP, SERR_BAD_NUM };

sval sval_num(long x) {
    sval v;
    v.type = SVAL_NUM;
    v.num = x;
    return v;
}

sval sval_err(int x) {
    sval v;
    v.type = SVAL_ERR;
    v.err = x;
    return v;
}

void sval_print(sval v) {
    switch (v.type) {
        case SVAL_NUM: printf("%li", v.num); break;

        case SVAL_ERR:
            if (v.err == SERR_DIV_ZERO) {
                printf("Error: Division by zero!");
            }
            if (v.err == SERR_BAD_OP) {
                printf("Error: Invalid operator!");
            }
            if (v.err == SERR_BAD_NUM) {
                printf("Error: Invalid number!");
            }
        break;
    }
}

void sval_println(sval v) { sval_print(v); putchar('\n'); }

sval eval_op(sval x, char* op, sval y) {

    if (x.type == SVAL_ERR) { return x; }
    if (y.type == SVAL_ERR) { return y; }

    if (strcmp(op, "+") == 0) { return sval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return sval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return sval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) {
        return y.num == 0 ? sval_err(SERR_DIV_ZERO) : sval_num(x.num / y.num);
    }

    return sval_err(SERR_BAD_OP);
}

sval eval(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) {
        errno = 0;

        long x = strtol(t->contents, NULL, 10);
        return errno != ERANGE ? sval_num(x) : sval_err(SERR_BAD_NUM);
    }

    char* op = t->children[1]->contents;

    sval x = eval(t->children[2]);

    int i = 3;

    while (strstr(t->children[i]->tag, "expr")) {
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }

    return x;
}

int main(int argc, char** argv) {
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Operator = mpc_new("operator");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Stutter = mpc_new("stutter");

    mpca_lang(MPCA_LANG_DEFAULT,
    "\
        number : /-?[0-9]+/ ; \
        operator : '+' | '-' | '*' | '/' ; \
        expr : <number> | '(' <operator> <expr>+ ')' ; \
        stutter : /^/ <operator> <expr>+ /$/ ; \
    ",
    Number, Operator, Expr, Stutter);

    puts("Stutter v0.0.1");
    puts("Press Ctrl+C to Exit\n");
    
    while (1) {
        char* input = readline("stutter> ");

        add_history(input);

        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Stutter, &r)) {
            sval result = eval(r.output);
            sval_println(result);
            mpc_ast_delete(r.output);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(4, Number, Operator, Expr, Stutter);

    return 0;
}