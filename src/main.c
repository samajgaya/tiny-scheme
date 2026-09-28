#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "util.h"

#define ENV_INIT_SIZE 64

typedef struct s S;
typedef struct e E;
typedef S *(*P)(E *env, S *args);
typedef struct { char *name; P p; } F;

typedef struct {
    S *params;
    S *body;
    E *env;
} C;

typedef struct {
    enum {
        NIL = 0,
        BOOL,
        NUM,
        SYM,
        PRIM,
        CLOS,
    } is;
    union {
        bool b;
        double num;
        char *sym;
        P prim;
        C clos;
    };
} A;

typedef struct s {
    enum {
        PAIR,
        ATOM,
    } is;
    union {
        struct {
            struct s *car;
            struct s *cdr;
        } pair;
        A atom;
    };

    int gcmark;
    S *gcnext;
} S;

struct entry {
    char *k;
    S *v;
};

typedef struct e {
    struct entry *entries;
    size_t count;
    size_t capacity;

    E *parent;

    int gcmark;
    E *gcnext;
} E;

#define mksym(s)  mkatom((A){ .is = SYM,  .sym  = s})
#define mknum(v)  mkatom((A){ .is = NUM,  .num  = v})
#define mkbool(v) mkatom((A){ .is = BOOL, .b    = v})
#define mkprim(p) mkatom((A){ .is = PRIM, .prim = p})
#define mkclos(c) mkatom((A){ .is = CLOS, .clos = c})
#define mknil()   mkatom((A){ .is = NIL})

#define atom(s) (assert((s)->is == ATOM), (s)->atom)
#define pair(s) (assert((s)->is == PAIR), (s)->pair)
#define clos(a) (assert((a).is == CLOS), (a).clos)
#define sym(a)  (assert((a).is == SYM), (a).sym)
#define num(a)  (assert((a).is == NUM), (a).num)
#define nil(a)  (assert((a).is == NIL))

/* use p_car and p_cdr to assert p->is == PAIR */
#define car(p) ((p)->pair.car)
#define cdr(p) ((p)->pair.cdr)

static S *allocated;
static size_t nobjs, gcmax = 1000;

static E *allocated_e;

static S *eval(E *e, S *s);
static void gcmark(S *s);

static void
pp(S *s) {
    switch (s->is) {
    case ATOM:
        switch (s->atom.is) {
        case NIL:  printf("()");               break;
        case NUM:  printf("%g", s->atom.num);  break;
        case SYM:  printf("%s", s->atom.sym);  break;
        case BOOL:
            if (s->atom.b) printf("#t");
            else           printf("#f");
            break;
        case PRIM: printf("#prim "); break;
        case CLOS: printf("#clos "); break;
        }
        break;
    case PAIR:
        printf("(");
        pp(s->pair.car);
        printf(" . ");
        pp(s->pair.cdr);
        printf(")");
        break;
    }
}

#define ppln(s) do { pp(s); putchar('\n'); } while (0)

static S *
salloc(void) {
    S *s = xcalloc(1, sizeof(S));
    s->gcnext = allocated;
    allocated = s;
    nobjs++;
    return s;
}

static E *
envalloc(void) {
    E *e = xcalloc(1, sizeof(E));
    e->gcnext = allocated_e;
    allocated_e = e;
    nobjs++;
    return e;
}

static void
gcmarkenv(E *e) {
    if (!e || e->gcmark)
        return;

    e->gcmark = 1;

    for (size_t i = 0; i < e->count; i++) {
        gcmark(e->entries[i].v);
    }

    gcmarkenv(e->parent);
}

static void
gcmark(S *s) {
    if (!s || s->gcmark)
        return;

    s->gcmark = 1;

    switch (s->is) {
    case PAIR:
        gcmark(car(s));
        gcmark(cdr(s));
        break;
    case ATOM:
        switch (atom(s).is) {
        case CLOS:
            gcmark(clos(atom(s)).params);
            gcmark(clos(atom(s)).body);
            gcmarkenv(clos(atom(s)).env);
            break;
        case NIL:
        case NUM:
        case SYM:
        case PRIM:
        case BOOL:
        break;
        }
    }
}

static void
gcsweep(void) {
    S **p = &allocated;
    while (*p) {
        if (!(*p)->gcmark) {
            S *dead = *p;
            *p = dead->gcnext;
            free(dead);
            nobjs--;
        } else {
            (*p)->gcmark = 0;
            p = &(*p)->gcnext;
        }
    }
}

static void
gcsweepenv(void) {
    E **p = &allocated_e;
    while (*p) {
        if (!(*p)->gcmark) {
            E *dead = *p;
            *p = dead->gcnext;
            free(dead);
            nobjs--;
        } else {
            (*p)->gcmark = 0;
            p = &(*p)->gcnext;
        }
    }
}

static void
gc(E *e, S *s) {
    if (nobjs < gcmax)
        return;

    gcmarkenv(e);
    gcmark(s);

    gcsweep();
    gcsweepenv();
}

static S *
mkatom(A a) {
    S *s = salloc();
    s->is = ATOM;
    s->atom = a;
    return s;
}

static S *
mkpair(S *car, S *cdr) {
    S *s = salloc();
    s->is = PAIR;
    s->pair.car = car;
    s->pair.cdr = cdr;
    return s;
}

static int
length(S *s) {
    int i = 0;
    while (s->is == PAIR) {
        s = s->pair.cdr;
        i++;
    }
    nil(atom(s));
    return i;
}

static void 
arity(S *s, int n) {
    assert(length(s) == n);
}

static bool
falsy(S *s) {
    /* only #f is falsy */
    return (s->is == ATOM && s->atom.is == BOOL && s->atom.b == false);
}

/* primitives */
S *
p_car(E *e, S *a) {
    UNUSED(e);
    arity(a, 1);
    return car(a);
}

S *
p_cdr(E *e, S *a) {
    UNUSED(e);
    arity(a, 1);
    return cdr(a);
}

S *
p_cons(E *e, S *a) {
    UNUSED(e);

    arity(a, 2);
    return mkpair(car(a), car(cdr(a)));
}

S *
p_nullp(E *e, S *a) {
    arity(a, 1);
    S *res = eval(e, car(a));
    bool isnil = res->is == ATOM && res->atom.is == NIL;
    return mkbool(isnil);
}

S *
p_show(E *e, S *a) {
    arity(a, 1);
    S *res = eval(e, car(a));
    ppln(res);

    /* TODO: unspecified return value */
    return mknil();
}

S *
p_add(E *e, S *a) {
    UNUSED(e);

    double acc = 0;
    while (a->is == PAIR) {
        acc += num(atom(car(a)));
        a = cdr(a);
    }
    nil(a->atom);
    return mknum(acc);
}

S *
p_sub(E *e, S *a) {
    UNUSED(e);

    if (length(a) == 1)
        return mknum(-num(atom(car(a))));

    double acc = num(atom(car(a)));
    a = cdr(a);
    while (a->is == PAIR) {
        acc -= num(atom(car(a)));
        a = cdr(a);
    }
    nil(a->atom);
    return mknum(acc);
}
S *
p_div(E *e, S *a) {
    UNUSED(e);

    double acc = num(atom(car(a)));
    a = cdr(a);
    while (a->is == PAIR) {
        acc /= num(atom(car(a)));
        a = cdr(a);
    }
    nil(a->atom);
    return mknum(acc);
}

S *
p_mul(E *e, S *a) {
    UNUSED(e);

    double acc = 1;
    while (a->is == PAIR) {
        acc *= num(atom(car(a)));
        a = cdr(a);
    }
    nil(a->atom);
    return mknum(acc);
}

S *
p_gt(E *e, S *a) {
    UNUSED(e);
    double prev = num(atom(car(a)));
    a = cdr(a);
    while (a->is == PAIR) {
        double cur = num(atom(car(a)));
        if (!(prev > cur))
            return mkbool(false);
        prev = cur;
        a = cdr(a);
    }
    return mkbool(true);
}

S *
p_lt(E *e, S *a) {
    UNUSED(e);
    double prev = num(atom(car(a)));
    a = cdr(a);
    while (a->is == PAIR) {
        double cur = num(atom(car(a)));
        if (!(prev < cur))
            return mkbool(false);
        prev = cur;
        a = cdr(a);
    }
    return mkbool(true);
}

S *
p_eq(E *e, S *a) {
    UNUSED(e);
    double n = num(atom(car(a)));
    while (a->is == PAIR) {
        if (n != num(atom(car(a))))
            return mkbool(false);
        a = cdr(a);
    }
    return mkbool(true);
}

static struct entry * 
envget(E *e, char *k) {
    for (size_t i = 0; i < e->count; i++) {
        if (!strcmp(e->entries[i].k, k))
            return &e->entries[i];
    }

    if (e->parent)
        return envget(e->parent, k);

    return NULL;
}

static E * 
envput(E *e, char *k, S *v) {
    struct entry *ey;
    E *p = e->parent;
    e->parent = NULL;
    ey = envget(e, k);
    if (ey) {
        ey->v = v;
        return e;
    }
    e->parent = p;

    if (e->count >= e->capacity) {
        e->capacity = (e->capacity == 0)
            ? ENV_INIT_SIZE
            : e->capacity * 2;
        e->entries = realloc(e->entries, e->capacity * sizeof(struct entry));
        if (!e->entries)
            die("realloc():");
    }

    e->entries[e->count++] = (struct entry){k, v};

    return e;
}

static E *
mkenv(E *parent) {
    E *e = envalloc();

    if (!parent) {
        e = envput(e, "+",     mkprim(p_add));
        e = envput(e, "-",     mkprim(p_sub));
        e = envput(e, "*",     mkprim(p_mul));
        e = envput(e, "/",     mkprim(p_div));
        e = envput(e, "<",     mkprim(p_lt));
        e = envput(e, ">",     mkprim(p_gt));
        e = envput(e, "=",     mkprim(p_eq));
        e = envput(e, "cdr",   mkprim(p_cdr));
        e = envput(e, "car",   mkprim(p_car));
        e = envput(e, "cons",  mkprim(p_cons));
        e = envput(e, "null?", mkprim(p_nullp));
        e = envput(e, "show",  mkprim(p_show));
    }

    e->parent = parent;
    return e;
}

static void
bindparams(E *env, S *params, S *args) {
    while (params->is == PAIR) {
        if (args->is != PAIR)
            die("too few arguments\n");

        envput(env, sym(atom(car(params))), car(args));

        params = cdr(params);
        args = cdr(args);
    }
    if (args->is == PAIR)
        die("too many arguments\n");
}

/* special forms */
static S *
f_quote(E *e, S *a) {
    UNUSED(e);
    arity(a, 1);
    return car(a);
}

static S *
f_and(E *e, S *a) {
    S *res;
    for (S *s = a; s->is == PAIR; s = cdr(s)) {
        res = eval(e, car(s));
        if (falsy(res))
            return mkbool(false);
    }

    return res;
}

static S *
f_or(E *e, S *a) {
    S *res;
    for (S *s = a; s->is == PAIR; s = cdr(s)) {
        res = eval(e, car(s));
        if (!falsy(res))
            return res;
    }
    return mkbool(false);
}

static S *
f_if(E *e, S *a) {
    /* condition, then, else */
    arity(a, 3);

    S *cond = eval(e, car(a));
    S *then = car(cdr(a));
    S *elsh = car(cdr(cdr(a)));

    if (!falsy(cond))
        return eval(e, then);
    else
        return eval(e, elsh);
}

static S *
f_cond(E *e, S *a) {
    if (length(a) < 1)
        die("cond requires atleast 1 clause");

    for (S *s = a; s->is == PAIR; s = cdr(s)) {
        S *test = eval(e, car(car(s)));
        S *action = car(cdr(car(s)));

        if (!falsy(test))
            return eval(e, action);
    }

    die("unspecified return value");
    UNREACHABLE();
}

static S *
f_begin(E *e, S *s) {
    /* required if s is NIL */
    S *res;
    if (length(s) < 1)
        die("unspecified return value\n");

    for (;s->is == PAIR; s = cdr(s))
        res = eval(e, car(s));
    return res;
}

static S *
f_lambda(E *e, S *a) {
    arity(a, 2);
    C c = {
        .params = car(a),
        .body = cdr(a),
        .env = e,
    };

    return mkclos(c);
}

static S *
f_define(E *e, S *a) {
    arity(a, 2);

    char *name = sym(atom(car(a)));
    S *val = eval(e, car(cdr(a)));

    envput(e, name, val);
    return val;
}

static S *
f_set(E *e, S *a) {
    arity(a, 2);
    char *name = sym(atom(car(a)));
    S *val = eval(e, car(cdr(a)));
    struct entry *ey;

    if (!(ey = envget(e, name)))
        die("set!: '%s' was never defined", name);
    return ey->v = val;
}

static const F forms[] = {
    { "quote", f_quote },
    { "and", f_and },
    { "or", f_or },
    { "if", f_if },
    { "cond", f_cond },
    { "begin", f_begin },
    { "lambda", f_lambda },
    { "define", f_define },
    { "set!", f_set },
};

static P
getform(const char *name) {
    for (size_t i = 0; i < LEN(forms); i++) {
        if (!strcmp(name, forms[i].name)) {
            return forms[i].p;
        }
    }

    return NULL;
}

static S *
evlist(E *e, S *s) {

    if (s->is == ATOM)
        return s;

    S *car = eval(e, car(s));
    S *cdr = evlist(e, cdr(s));

    return mkpair(car, cdr);
}

static S *
apply(E *e, S *p, S *a) {
    if (atom(p).is == PRIM)
        return p->atom.prim(e, a);
    if (atom(p).is == CLOS) {
        E *callenv = mkenv(p->atom.clos.env);
        bindparams(callenv, p->atom.clos.params, a);
        return f_begin(callenv, p->atom.clos.body);
    }

    die("not a procedure\n");
    return NULL; /* unreachable */
}

static S *
eval(E *e, S *s) {
    struct entry *ey;

    switch (s->is) {
    case ATOM:
        if (s->atom.is == SYM) {
            if ((ey = envget(e, s->atom.sym)))
                return ey->v;
            die("symbol '%s' wasn't defined\n", s->atom.sym);
        }
        return s;
    case PAIR: {
        P p;
        if (car(s)->is == ATOM && car(s)->atom.is == SYM
                && (p = getform(s->pair.car->atom.sym)))
            return p(e, s->pair.cdr);

        S *proc = eval(e, car(s));
        S *args = evlist(e, cdr(s));
        return apply(e, proc, args);
    }
    }

    UNREACHABLE();
}

/* parsing */
static S *parse(FILE *in);
static S *parselst(FILE *in);

static S *
parselst(FILE *in) {
    int c;
    S *car, *cdr;

    /* parse empty lists */
    while ((c = getc(in)) > 0 && isspace(c))
        ;

    if (c == ')')
        return mknil();

    ungetc(c, in);
    car = parse(in);
    cdr = parselst(in);

    return mkpair(car, cdr);
}

static S *
parse(FILE *in) {
    char sym[32];
    size_t i = 0;
    int c;

    while ((c = getc(in)) > 0 && isspace(c))
        ;

    if (c == EOF)
        return NULL;

    if (c == '(') {
        return parselst(in);
    }

    if (c == '-') {
        int c_ = getc(in);
        if (isdigit(c_)) {
            sym[i++] = '-';
            c = c_;
        } else {
            ungetc(c_, in);
        }
    }

    if (isdigit(c)) {
        sym[i++] = c;
        while ((c = getc(in)) && (isdigit(c) || c == '.'))
            if (i < sizeof(sym) - 1) sym[i++] = c;
        ungetc(c, in);
        sym[i] = '\0';
        return mknum(strtod(sym, NULL));
    }

    if (c == '#') {
        int c_ = getc(in);
        if (c_ == 't' || c_ == 'T')
            return mkbool(true);
        else if (c_ == 'f' || c_ == 'F')
            return mkbool(false);
        ungetc(c_, in);
    }

    if (isprint(c)) {
        sym[i++] = c;
        while ((c = getc(in)) && isprint(c) && !isspace(c)
                && c != '(' && c != ')')
            if (i < sizeof(sym) - 1) sym[i++] = c;

        ungetc(c, in);
        sym[i++] = '\0';
        return mksym(clone(sym, i));
    }

    die("unrecognised character '%d'\n", c);
    return NULL; /* unreachable */
}

int
main(void) {
    FILE *in = stdin;
    E *e = mkenv(NULL);
    S *s;

    while ((s = parse(in))) {
        s = eval(e, s);
        gc(e, s);
    }

    gc(e, s);

    return 0;
}
