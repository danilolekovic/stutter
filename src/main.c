#include <stdio.h>
#include <stdlib.h>
#include <editline/readline.h>
#include "mpc.h"

typedef struct sval {
    int type;
    long num;
    char* err;
    char* sym;
    int count;
    struct sval** cell;
} sval;

enum { SVAL_NUM, SVAL_ERR, SVAL_SYM, SVAL_SEXPR };
enum { SERR_DIV_ZERO, SERR_BAD_OP, SERR_BAD_NUM };

void sval_print(sval *v);

sval* sval_num(long x) {
    sval* v = malloc(sizeof(sval));
    v->type = SVAL_NUM;
    v->num = x;
    return v;
}

sval* sval_err(char* m) {
    sval* v = malloc(sizeof(sval));
    v->type = SVAL_ERR;
    v->err = malloc(strlen(m) + 1);
    strcpy(v->err, m);
    return v;
}

sval* sval_sym(char* s) {
    sval* v = malloc(sizeof(sval));
    v->type = SVAL_SYM;
    v->sym = malloc(strlen(s) + 1);
    strcpy(v->sym, s);
    return v;
}

sval* sval_sexpr(void) {
    sval* v = malloc(sizeof(sval));
    v->type = SVAL_SEXPR;
    v->count = 0;
    v->cell = NULL;
    return v;
}

void sval_del(sval* v) {
    switch (v->type) {
        case SVAL_NUM: break;
        case SVAL_ERR: free(v->err); break;
        case SVAL_SYM: free(v->sym); break;

        case SVAL_SEXPR:
            for (int i = 0; i < v->count; i++) {
                sval_del(v->cell[i]);
            }

            free(v->cell);
        break;
    }

    free(v);
}

sval* sval_read_num(mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ?
        sval_num(x) : sval_err("Invalid number");
}

sval *sval_add(sval *v, sval *x)
{
    v->count++;
    v->cell = realloc(v->cell, sizeof(sval *) * v->count);
    v->cell[v->count - 1] = x;
    return v;
}

sval* sval_read(mpc_ast_t* t) {
    if (strstr(t->tag, "number")) { return sval_read_num(t); }
    if (strstr(t->tag, "symbol")) { return sval_sym(t->contents); }

    sval* x = NULL;
    if (strcmp(t->tag, ">") == 0) { x = sval_sexpr(); }
    if (strstr(t->tag, "sexpr")) { x = sval_sexpr(); }

    for (int i = 0; i < t->children_num; i++) {
        if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
        if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
        if (strcmp(t->children[i]->tag, "regex") == 0) { continue; }
        x = sval_add(x, sval_read(t->children[i]));
    }

    return x;
}

void sval_expr_print(sval* v, char open, char close) {
    putchar(open);

    for (int i = 0; i < v->count; i++) {
        sval_print(v->cell[i]);

        if (i != (v->count - 1)) {
            putchar(' ');
        }
    }

    putchar(close);
}

void sval_print(sval *v) {
    switch (v->type) {
        case SVAL_NUM:
            printf("%li", v->num);
            break;
        case SVAL_ERR:
            printf("Error: %s", v->err);
            break;
        case SVAL_SYM:
            printf("%s", v->sym);
            break;
        case SVAL_SEXPR:
            sval_expr_print(v, '(', ')');
            break;
    }
}

void sval_println(sval* v) { sval_print(v); putchar('\n'); }

sval* sval_pop(sval* v, int i) {
    sval* x = v->cell[i];

    memmove(&v->cell[i], &v->cell[i + 1], sizeof(sval*) * (v->count - i - 1));

    v->count--;

    v->cell = realloc(v->cell, sizeof(sval*) * v->count);
    return x;
}

sval* builtin_op(sval* a, char* op) {
    for (int i = 0; i < a->count; i++) {
        if (a->cell[i]->type != SVAL_NUM) {
            sval_del(a);
            return sval_err("Cannot operate on non-number!");
        }
    }

    sval* x = sval_pop(a, 0);

    if ((strcmp(op, "-") == 0) && a->count == 0) {
        x->num = -x->num;
    }

    while (a->count > 0) {
        sval* y = sval_pop(a, 0);

        if (strcmp(op, "+") == 0) { x->num += y->num; }
        if (strcmp(op, "-") == 0) { x->num -= y->num; }
        if (strcmp(op, "*") == 0) { x->num *= y->num; }
        if (strcmp(op, "/") == 0) {
            if (y->num == 0) {
                sval_del(x);
                sval_del(y);
            }

            x->num /= y->num;
        }

        sval_del(y);
    }

    sval_del(a);
    return x;
}

sval* sval_take(sval* v, int i) {
    sval* x = sval_pop(v, i);
    sval_del(v);
    return x;
}

sval* sval_eval_sexr(sval* v);

sval* sval_eval(sval* v) {
    if (v->type == SVAL_SEXPR) { return sval_eval_sexr(v); }
    return v;
}

sval* sval_eval_sexr(sval* v) {
    for (int i = 0; i < v->count; i++) {
        v->cell[i] = sval_eval(v->cell[i]);
    }

    for (int i = 0; i < v->count; i++) {
        if (v->cell[i]->type == SVAL_ERR) { return sval_take(v, i); }
    }

    if (v->count == 0) { return v; }
    if (v->count == 1) { return sval_take(v, 0); }

    sval* f = sval_pop(v, 0);

    if (f->type != SVAL_SYM) {
        sval_del(f); sval_del(v);
        return sval_err("S-expression does not start with symbol!");
    }

    sval* result = builtin_op(v, f->sym);
    sval_del(f);
    return result;
}

int main(int argc, char** argv) {
    mpc_parser_t* Number = mpc_new("number");
    mpc_parser_t* Symbol = mpc_new("symbol");
    mpc_parser_t *Sexpr = mpc_new("sexpr");
    mpc_parser_t* Expr = mpc_new("expr");
    mpc_parser_t* Stutter = mpc_new("stutter");

    mpca_lang(MPCA_LANG_DEFAULT,
              "\
        number : /-?[0-9]+/ ; \
        symbol : '+' | '-' | '*' | '/' ; \
        sexpr : '(' | <expr>* | ')' ; \
        expr : <number> | <symbol> | <sexpr> ; \
        stutter : /^/ <expr>* /$/ ; \
    ",
    Number, Symbol, Sexpr, Expr, Stutter);

    puts("Stutter v0.0.1");
    puts("Press Ctrl+C to Exit\n");
    
    while (1) {
        char* input = readline("stutter> ");

        add_history(input);

        mpc_result_t r;

        if (mpc_parse("<stdin>", input, Stutter, &r)) {
            sval* x = sval_eval(sval_read(r.output));
            sval_println(x);
            sval_del(x);
        } else {
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input);
    }

    mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Stutter);

    return 0;
}