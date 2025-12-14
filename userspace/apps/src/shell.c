#include "../../libc/include/kuser.h"
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define SH_MAX_LINE 1024
#define SH_MAX_TOKENS 256
#define SH_SCRATCH_SIZE 8192
#define SH_PERM_SIZE 16384

#define SH_MAX_WORDS 64
#define SH_MAX_ARGS 64
#define SH_MAX_REDIRS 8

#define SH_MAX_VARS 64
#define SH_MAX_ALIASES 32
#define SH_MAX_FUNCS 32

#define SH_MAX_JOBS 32
#define SH_HISTORY 32

#define SH_ALIAS_EXPANSION_LIMIT 8
#define SH_SUBSHELL_DEPTH_LIMIT 4

static void sh_write_all(int fd, const char *s, size_t n) {
    while (n) {
        long r = write(fd, s, n);
        if (r <= 0) {
            return;
        }
        s += (size_t)r;
        n -= (size_t)r;
    }
}

static void sh_puts(const char *s) {
    if (!s) {
        return;
    }
    sh_write_all(1, s, strlen(s));
}

static int sh_is_space(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static int sh_is_digit(char c) {
    return c >= '0' && c <= '9';
}

static int sh_atoi(const char *s) {
    if (!s) {
        return 0;
    }

    while (sh_is_space(*s)) {
        s++;
    }

    int sign = 1;
    if (*s == '-') {
        sign = -1;
        s++;
    } else if (*s == '+') {
        s++;
    }

    int v = 0;
    while (sh_is_digit(*s)) {
        v = v * 10 + (*s - '0');
        s++;
    }

    return v * sign;
}

static int sh_is_name_start(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
}

static int sh_is_name_char(char c) {
    return sh_is_name_start(c) || sh_is_digit(c);
}

typedef struct {
    char buf[SH_SCRATCH_SIZE];
    size_t pos;
} ShArena;

static void arena_reset(ShArena *a) {
    a->pos = 0;
}

static char *arena_alloc(ShArena *a, size_t n) {
    if (a->pos + n > sizeof(a->buf)) {
        return 0;
    }
    char *p = &a->buf[a->pos];
    a->pos += n;
    return p;
}

static char *arena_strdup(ShArena *a, const char *s, size_t n) {
    char *p = arena_alloc(a, n + 1);
    if (!p) {
        return 0;
    }
    memcpy(p, s, n);
    p[n] = '\0';
    return p;
}

static char *arena_strcat3(ShArena *a, const char *x, const char *y, const char *z) {
    size_t nx = strlen(x);
    size_t ny = strlen(y);
    size_t nz = strlen(z);
    char *p = arena_alloc(a, nx + ny + nz + 1);
    if (!p) {
        return 0;
    }
    memcpy(p, x, nx);
    memcpy(p + nx, y, ny);
    memcpy(p + nx + ny, z, nz);
    p[nx + ny + nz] = '\0';
    return p;
}

typedef struct {
    char buf[SH_PERM_SIZE];
    size_t pos;
} ShPermArena;

static char *perm_strdup(ShPermArena *a, const char *s) {
    size_t n = strlen(s);
    if (a->pos + n + 1 > sizeof(a->buf)) {
        return 0;
    }
    char *p = &a->buf[a->pos];
    memcpy(p, s, n);
    p[n] = '\0';
    a->pos += n + 1;
    return p;
}

typedef struct {
    const char *name;
    const char *value;
} ShKV;

typedef struct {
    const char *name;
    const char *body;
} ShFunc;

typedef struct {
    int pid;
    const char *cmd;
    int running;
} ShJob;

typedef struct {
    int last_status;
    int opt_errexit;
    int opt_xtrace;

    ShKV vars[SH_MAX_VARS];
    int var_count;

    ShKV aliases[SH_MAX_ALIASES];
    int alias_count;

    ShFunc funcs[SH_MAX_FUNCS];
    int func_count;

    ShJob jobs[SH_MAX_JOBS];

    const char *history[SH_HISTORY];
    int history_count;
    int history_pos;

    ShPermArena perm;
} Shell;

static int sh_streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static const char *shell_get_kv(ShKV *kvs, int n, const char *name) {
    for (int i = 0; i < n; ++i) {
        if (kvs[i].name && sh_streq(kvs[i].name, name)) {
            return kvs[i].value;
        }
    }
    return 0;
}

static void shell_set_kv(ShPermArena *perm, ShKV *kvs, int *n, int max, const char *name, const char *value) {
    const char *pname = perm_strdup(perm, name);
    const char *pval = perm_strdup(perm, value ? value : "");
    if (!pname || !pval) {
        return;
    }

    for (int i = 0; i < *n; ++i) {
        if (kvs[i].name && sh_streq(kvs[i].name, name)) {
            kvs[i].name = pname;
            kvs[i].value = pval;
            return;
        }
    }

    if (*n < max) {
        kvs[*n].name = pname;
        kvs[*n].value = pval;
        (*n)++;
    }
}

static void shell_unset_kv(ShKV *kvs, int *n, const char *name) {
    for (int i = 0; i < *n; ++i) {
        if (kvs[i].name && sh_streq(kvs[i].name, name)) {
            kvs[i] = kvs[*n - 1];
            (*n)--;
            return;
        }
    }
}

static const char *shell_get_var(Shell *sh, const char *name) {
    return shell_get_kv(sh->vars, sh->var_count, name);
}

static void shell_set_var(Shell *sh, const char *name, const char *value) {
    shell_set_kv(&sh->perm, sh->vars, &sh->var_count, SH_MAX_VARS, name, value);
}

static void shell_unset_var(Shell *sh, const char *name) {
    shell_unset_kv(sh->vars, &sh->var_count, name);
}

static const char *shell_get_alias(Shell *sh, const char *name) {
    return shell_get_kv(sh->aliases, sh->alias_count, name);
}

static void shell_set_alias(Shell *sh, const char *name, const char *value) {
    shell_set_kv(&sh->perm, sh->aliases, &sh->alias_count, SH_MAX_ALIASES, name, value);
}

static void shell_unset_alias(Shell *sh, const char *name) {
    shell_unset_kv(sh->aliases, &sh->alias_count, name);
}

static const char *shell_get_func(Shell *sh, const char *name) {
    for (int i = 0; i < sh->func_count; ++i) {
        if (sh->funcs[i].name && sh_streq(sh->funcs[i].name, name)) {
            return sh->funcs[i].body;
        }
    }
    return 0;
}

static void shell_set_func(Shell *sh, const char *name, const char *body) {
    const char *pname = perm_strdup(&sh->perm, name);
    const char *pbody = perm_strdup(&sh->perm, body ? body : "");
    if (!pname || !pbody) {
        return;
    }

    for (int i = 0; i < sh->func_count; ++i) {
        if (sh->funcs[i].name && sh_streq(sh->funcs[i].name, name)) {
            sh->funcs[i].name = pname;
            sh->funcs[i].body = pbody;
            return;
        }
    }

    if (sh->func_count < SH_MAX_FUNCS) {
        sh->funcs[sh->func_count].name = pname;
        sh->funcs[sh->func_count].body = pbody;
        sh->func_count++;
    }
}

static void shell_unset_func(Shell *sh, const char *name) {
    for (int i = 0; i < sh->func_count; ++i) {
        if (sh->funcs[i].name && sh_streq(sh->funcs[i].name, name)) {
            sh->funcs[i] = sh->funcs[sh->func_count - 1];
            sh->func_count--;
            return;
        }
    }
}

static void shell_history_add(Shell *sh, const char *line) {
    const char *p = perm_strdup(&sh->perm, line);
    if (!p) {
        return;
    }

    if (sh->history_count < SH_HISTORY) {
        sh->history[sh->history_count++] = p;
    } else {
        sh->history[sh->history_pos] = p;
        sh->history_pos = (sh->history_pos + 1) % SH_HISTORY;
    }
}

typedef enum {
    TOK_EOF = 0,
    TOK_WORD,
    TOK_NEWLINE,
    TOK_SEMI,
    TOK_AMP,
    TOK_PIPE,
    TOK_AND_IF,
    TOK_OR_IF,
    TOK_LPAREN,
    TOK_RPAREN,
    TOK_LBRACE,
    TOK_RBRACE,
    TOK_GT,
    TOK_GTGT,
    TOK_LT,
    TOK_DLT,
    TOK_DLT_DASH,
} TokenKind;

typedef struct {
    TokenKind kind;
    const char *text;
    int quoted;
} Token;

typedef struct {
    const char *src;
    size_t pos;
    size_t len;
    ShArena *arena;
} Lexer;

static int lex_peek(Lexer *lx) {
    if (lx->pos >= lx->len) {
        return -1;
    }
    return (unsigned char)lx->src[lx->pos];
}

static int lex_get(Lexer *lx) {
    if (lx->pos >= lx->len) {
        return -1;
    }
    return (unsigned char)lx->src[lx->pos++];
}

static Token lex_next(Lexer *lx) {
    while (1) {
        int c = lex_peek(lx);
        if (c < 0) {
            Token t = {TOK_EOF, 0, 0};
            return t;
        }

        if (c == '#') {
            size_t p = lx->pos;
            int prev_space = 1;
            if (p > 0) {
                char prev = lx->src[p - 1];
                prev_space = sh_is_space(prev) || prev == ';' || prev == '&' || prev == '|';
            }
            if (prev_space) {
                while (c >= 0 && c != '\n') {
                    lex_get(lx);
                    c = lex_peek(lx);
                }
                continue;
            }
        }

        if (c == ' ' || c == '\t' || c == '\r') {
            lex_get(lx);
            continue;
        }

        break;
    }

    int c = lex_get(lx);
    if (c < 0) {
        Token t = {TOK_EOF, 0, 0};
        return t;
    }

    if (c == '\n') {
        Token t = {TOK_NEWLINE, 0, 0};
        return t;
    }

    if (c == ';') {
        Token t = {TOK_SEMI, 0, 0};
        return t;
    }

    if (c == '&') {
        if (lex_peek(lx) == '&') {
            lex_get(lx);
            Token t = {TOK_AND_IF, 0, 0};
            return t;
        }
        Token t = {TOK_AMP, 0, 0};
        return t;
    }

    if (c == '|') {
        if (lex_peek(lx) == '|') {
            lex_get(lx);
            Token t = {TOK_OR_IF, 0, 0};
            return t;
        }
        Token t = {TOK_PIPE, 0, 0};
        return t;
    }

    if (c == '(') {
        Token t = {TOK_LPAREN, 0, 0};
        return t;
    }

    if (c == ')') {
        Token t = {TOK_RPAREN, 0, 0};
        return t;
    }

    if (c == '{') {
        Token t = {TOK_LBRACE, 0, 0};
        return t;
    }

    if (c == '}') {
        Token t = {TOK_RBRACE, 0, 0};
        return t;
    }

    if (c == '>') {
        if (lex_peek(lx) == '>') {
            lex_get(lx);
            Token t = {TOK_GTGT, 0, 0};
            return t;
        }
        Token t = {TOK_GT, 0, 0};
        return t;
    }

    if (c == '<') {
        if (lex_peek(lx) == '<') {
            lex_get(lx);
            if (lex_peek(lx) == '-') {
                lex_get(lx);
                Token t = {TOK_DLT_DASH, 0, 0};
                return t;
            }
            Token t = {TOK_DLT, 0, 0};
            return t;
        }
        Token t = {TOK_LT, 0, 0};
        return t;
    }

    lx->pos--;

    char tmp[256];
    size_t n = 0;
    int quoted = 0;
    int in_single = 0;
    int in_double = 0;

    while (1) {
        int p = lex_peek(lx);
        if (p < 0) {
            break;
        }
        if (!in_single && !in_double) {
            if (p == ' ' || p == '\t' || p == '\r' || p == '\n') {
                break;
            }
            if (p == ';' || p == '&' || p == '|' || p == '(' || p == ')' || p == '{' || p == '}' || p == '<' || p == '>') {
                break;
            }
        }

        int ch = lex_get(lx);
        if (ch < 0) {
            break;
        }

        if (!in_double && ch == '\'') {
            in_single = !in_single;
            quoted = 1;
            continue;
        }
        if (!in_single && ch == '"') {
            in_double = !in_double;
            quoted = 1;
            continue;
        }

        if (!in_single && ch == '\\') {
            int next = lex_peek(lx);
            if (next < 0) {
                break;
            }
            if (in_double) {
                if (next == '\\' || next == '"' || next == '$' || next == '`' || next == '\n') {
                    ch = lex_get(lx);
                }
            } else {
                ch = lex_get(lx);
            }
        }

        if (n + 1 < sizeof(tmp)) {
            tmp[n++] = (char)ch;
        }
    }

    char *word = arena_strdup(lx->arena, tmp, n);
    Token t = {TOK_WORD, word ? word : "", quoted};
    return t;
}

typedef struct {
    Token tokens[SH_MAX_TOKENS];
    int count;
} TokenStream;

static int tokenize(ShArena *arena, const char *src, TokenStream *out) {
    out->count = 0;

    Lexer lx;
    lx.src = src;
    lx.pos = 0;
    lx.len = strlen(src);
    lx.arena = arena;

    while (out->count < SH_MAX_TOKENS) {
        Token t = lex_next(&lx);
        out->tokens[out->count++] = t;
        if (t.kind == TOK_EOF) {
            return 0;
        }
    }

    return -1;
}

typedef enum {
    REDIR_IN,
    REDIR_OUT,
    REDIR_APPEND,
    REDIR_HEREDOC,
} RedirKind;

typedef struct {
    RedirKind kind;
    int fd;
    const char *target;
    const char *heredoc_body;
} Redirect;

typedef enum {
    AST_EMPTY = 0,
    AST_SIMPLE,
    AST_SEQ,
    AST_AND,
    AST_OR,
    AST_PIPE,
    AST_BG,
    AST_GROUP,
    AST_SUBSHELL,
    AST_IF,
    AST_WHILE,
    AST_FOR,
    AST_FUNCDEF,
} AstKind;

typedef struct Ast Ast;

struct Ast {
    AstKind kind;
    union {
        struct {
            const char *words[SH_MAX_WORDS];
            uint8_t quoted[SH_MAX_WORDS];
            int nwords;
            Redirect redirs[SH_MAX_REDIRS];
            int nredirs;
        } simple;
        struct {
            Ast *left;
            Ast *right;
        } bin;
        struct {
            Ast *child;
        } unary;
        struct {
            Ast *body;
        } group;
        struct {
            Ast *cond;
            Ast *then_part;
            Ast *else_part;
        } ifnode;
        struct {
            int until;
            Ast *cond;
            Ast *body;
        } whilenode;
        struct {
            const char *var;
            const char *items[SH_MAX_WORDS];
            int nitems;
            Ast *body;
        } fornode;
        struct {
            const char *name;
            const char *body;
        } funcdef;
    } u;
};

typedef struct {
    TokenStream *ts;
    int pos;
    ShArena *arena;
} Parser;

static Token *ps_peek(Parser *ps) {
    if (ps->pos >= ps->ts->count) {
        return 0;
    }
    return &ps->ts->tokens[ps->pos];
}

static Token *ps_peek_n(Parser *ps, int n) {
    int idx = ps->pos + n;
    if (idx >= ps->ts->count) {
        return 0;
    }
    return &ps->ts->tokens[idx];
}

static Token ps_get(Parser *ps) {
    Token *t = ps_peek(ps);
    if (!t) {
        Token z = {TOK_EOF, 0, 0};
        return z;
    }
    ps->pos++;
    return *t;
}

static int ps_accept(Parser *ps, TokenKind kind) {
    Token *t = ps_peek(ps);
    if (t && t->kind == kind) {
        ps->pos++;
        return 1;
    }
    return 0;
}

static Ast *ast_new(ShArena *arena) {
    Ast *n = (Ast *)arena_alloc(arena, sizeof(Ast));
    if (!n) {
        return 0;
    }
    memset(n, 0, sizeof(Ast));
    return n;
}

static int is_keyword(Token *t, const char *kw) {
    return t && t->kind == TOK_WORD && t->text && sh_streq(t->text, kw);
}

static Ast *parse_list(Parser *ps);
static Ast *parse_and_or(Parser *ps);
static Ast *parse_pipeline(Parser *ps);
static Ast *parse_command(Parser *ps);

static int parse_redir(Parser *ps, Redirect *out) {
    Token *t = ps_peek(ps);
    if (!t) {
        return 0;
    }

    int fd = -1;
    if (t->kind == TOK_WORD) {
        int all_digits = 1;
        for (const char *p = t->text; *p; ++p) {
            if (!sh_is_digit(*p)) {
                all_digits = 0;
                break;
            }
        }
        Token *op = ps_peek_n(ps, 1);
        if (all_digits && op && (op->kind == TOK_GT || op->kind == TOK_GTGT || op->kind == TOK_LT || op->kind == TOK_DLT || op->kind == TOK_DLT_DASH)) {
            fd = sh_atoi(t->text);
            ps_get(ps);
        }
    }

    Token op = ps_get(ps);
    if (op.kind != TOK_GT && op.kind != TOK_GTGT && op.kind != TOK_LT && op.kind != TOK_DLT && op.kind != TOK_DLT_DASH) {
        return 0;
    }

    Token target = ps_get(ps);
    if (target.kind != TOK_WORD) {
        return 0;
    }

    if (fd < 0) {
        if (op.kind == TOK_LT || op.kind == TOK_DLT || op.kind == TOK_DLT_DASH) {
            fd = 0;
        } else {
            fd = 1;
        }
    }

    out->fd = fd;
    out->target = target.text;
    out->heredoc_body = 0;

    if (op.kind == TOK_LT) {
        out->kind = REDIR_IN;
    } else if (op.kind == TOK_GT) {
        out->kind = REDIR_OUT;
    } else if (op.kind == TOK_GTGT) {
        out->kind = REDIR_APPEND;
    } else {
        out->kind = REDIR_HEREDOC;
    }

    return 1;
}

static Ast *parse_simple(Parser *ps) {
    Ast *node = ast_new(ps->arena);
    if (!node) {
        return 0;
    }
    node->kind = AST_SIMPLE;

    while (1) {
        Token *t = ps_peek(ps);
        if (!t || t->kind == TOK_EOF) {
            break;
        }

        if (t->kind == TOK_WORD) {
            Token *op = ps_peek_n(ps, 1);
            if (op && (op->kind == TOK_GT || op->kind == TOK_GTGT || op->kind == TOK_LT || op->kind == TOK_DLT || op->kind == TOK_DLT_DASH)) {
                int all_digits = 1;
                for (const char *p = t->text; *p; ++p) {
                    if (!sh_is_digit(*p)) {
                        all_digits = 0;
                        break;
                    }
                }
                if (all_digits) {
                    if (node->u.simple.nredirs >= SH_MAX_REDIRS) {
                        break;
                    }
                    Redirect r;
                    if (!parse_redir(ps, &r)) {
                        break;
                    }
                    node->u.simple.redirs[node->u.simple.nredirs++] = r;
                    continue;
                }
            }

            if (node->u.simple.nwords >= SH_MAX_WORDS) {
                break;
            }
            Token w = ps_get(ps);
            node->u.simple.words[node->u.simple.nwords] = w.text;
            node->u.simple.quoted[node->u.simple.nwords] = (uint8_t)(w.quoted ? 1 : 0);
            node->u.simple.nwords++;
            continue;
        }

        if (t->kind == TOK_GT || t->kind == TOK_GTGT || t->kind == TOK_LT || t->kind == TOK_DLT || t->kind == TOK_DLT_DASH) {
            if (node->u.simple.nredirs >= SH_MAX_REDIRS) {
                break;
            }
            Redirect r;
            if (!parse_redir(ps, &r)) {
                break;
            }
            node->u.simple.redirs[node->u.simple.nredirs++] = r;
            continue;
        }

        break;
    }

    return node;
}

static Ast *parse_group(Parser *ps, int subshell) {
    Ast *node = ast_new(ps->arena);
    if (!node) {
        return 0;
    }

    node->kind = subshell ? AST_SUBSHELL : AST_GROUP;
    node->u.group.body = parse_list(ps);

    if (subshell) {
        if (!ps_accept(ps, TOK_RPAREN)) {
            return 0;
        }
    } else {
        if (!ps_accept(ps, TOK_RBRACE)) {
            return 0;
        }
    }

    return node;
}

static char *join_tokens(ShArena *arena, TokenStream *ts, int start, int end) {
    char tmp[SH_MAX_LINE];
    size_t n = 0;

    for (int i = start; i < end; ++i) {
        Token *t = &ts->tokens[i];
        const char *s = 0;
        if (t->kind == TOK_WORD) {
            s = t->text;
        } else if (t->kind == TOK_SEMI) {
            s = ";";
        } else if (t->kind == TOK_NEWLINE) {
            s = ";";
        } else if (t->kind == TOK_PIPE) {
            s = "|";
        } else if (t->kind == TOK_AND_IF) {
            s = "&&";
        } else if (t->kind == TOK_OR_IF) {
            s = "||";
        } else if (t->kind == TOK_AMP) {
            s = "&";
        } else if (t->kind == TOK_LT) {
            s = "<";
        } else if (t->kind == TOK_GT) {
            s = ">";
        } else if (t->kind == TOK_GTGT) {
            s = ">>";
        } else if (t->kind == TOK_DLT) {
            s = "<<";
        } else if (t->kind == TOK_DLT_DASH) {
            s = "<<-";
        } else if (t->kind == TOK_LPAREN) {
            s = "(";
        } else if (t->kind == TOK_RPAREN) {
            s = ")";
        } else if (t->kind == TOK_LBRACE) {
            s = "{";
        } else if (t->kind == TOK_RBRACE) {
            s = "}";
        } else {
            continue;
        }

        if (!s) {
            continue;
        }

        if (n && n + 1 < sizeof(tmp)) {
            tmp[n++] = ' ';
        }

        size_t sl = strlen(s);
        if (n + sl >= sizeof(tmp)) {
            break;
        }
        memcpy(&tmp[n], s, sl);
        n += sl;
    }

    return arena_strdup(arena, tmp, n);
}

static Ast *parse_if(Parser *ps) {
    Ast *node = ast_new(ps->arena);
    if (!node) {
        return 0;
    }
    node->kind = AST_IF;
    Ast *root = node;

    node->u.ifnode.cond = parse_list(ps);

    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    if (!is_keyword(ps_peek(ps), "then")) {
        return 0;
    }
    ps_get(ps);

    node->u.ifnode.then_part = parse_list(ps);

    while (1) {
        while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
            ps_get(ps);
        }

        if (is_keyword(ps_peek(ps), "elif")) {
            ps_get(ps);
            Ast *elif_node = ast_new(ps->arena);
            if (!elif_node) {
                return 0;
            }
            elif_node->kind = AST_IF;
            elif_node->u.ifnode.cond = parse_list(ps);

            while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
                ps_get(ps);
            }

            if (!is_keyword(ps_peek(ps), "then")) {
                return 0;
            }
            ps_get(ps);
            elif_node->u.ifnode.then_part = parse_list(ps);

            node->u.ifnode.else_part = elif_node;
            node = elif_node;
            continue;
        }

        if (is_keyword(ps_peek(ps), "else")) {
            ps_get(ps);
            node->u.ifnode.else_part = parse_list(ps);
            break;
        }

        break;
    }

    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    if (!is_keyword(ps_peek(ps), "fi")) {
        return 0;
    }
    ps_get(ps);

    return root;
}

static Ast *parse_while(Parser *ps, int until) {
    Ast *node = ast_new(ps->arena);
    if (!node) {
        return 0;
    }
    node->kind = AST_WHILE;
    node->u.whilenode.until = until;

    node->u.whilenode.cond = parse_list(ps);

    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    if (!is_keyword(ps_peek(ps), "do")) {
        return 0;
    }
    ps_get(ps);

    node->u.whilenode.body = parse_list(ps);

    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    if (!is_keyword(ps_peek(ps), "done")) {
        return 0;
    }
    ps_get(ps);

    return node;
}

static Ast *parse_for(Parser *ps) {
    Ast *node = ast_new(ps->arena);
    if (!node) {
        return 0;
    }
    node->kind = AST_FOR;

    Token name = ps_get(ps);
    if (name.kind != TOK_WORD) {
        return 0;
    }
    node->u.fornode.var = name.text;

    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    if (is_keyword(ps_peek(ps), "in")) {
        ps_get(ps);
        while (ps_peek(ps) && ps_peek(ps)->kind == TOK_WORD) {
            if (node->u.fornode.nitems < SH_MAX_WORDS) {
                Token w = ps_get(ps);
                node->u.fornode.items[node->u.fornode.nitems++] = w.text;
            } else {
                ps_get(ps);
            }
        }
    }

    if (ps_peek(ps) && (ps_peek(ps)->kind == TOK_SEMI || ps_peek(ps)->kind == TOK_NEWLINE)) {
        ps_get(ps);
    }

    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    if (!is_keyword(ps_peek(ps), "do")) {
        return 0;
    }
    ps_get(ps);

    node->u.fornode.body = parse_list(ps);

    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    if (!is_keyword(ps_peek(ps), "done")) {
        return 0;
    }
    ps_get(ps);

    return node;
}

static Ast *parse_funcdef(Parser *ps) {
    Token name = ps_get(ps);
    if (name.kind != TOK_WORD) {
        return 0;
    }

    if (!ps_accept(ps, TOK_LPAREN) || !ps_accept(ps, TOK_RPAREN)) {
        return 0;
    }

    if (!ps_accept(ps, TOK_LBRACE)) {
        return 0;
    }

    int body_start = ps->pos;
    (void)parse_list(ps);

    if (!ps_accept(ps, TOK_RBRACE)) {
        return 0;
    }

    int body_end = ps->pos - 1;
    char *body = join_tokens(ps->arena, ps->ts, body_start, body_end);

    Ast *node = ast_new(ps->arena);
    if (!node) {
        return 0;
    }
    node->kind = AST_FUNCDEF;
    node->u.funcdef.name = name.text;
    node->u.funcdef.body = body ? body : "";
    return node;
}

static Ast *parse_command(Parser *ps) {
    Token *t = ps_peek(ps);
    if (!t) {
        return 0;
    }

    if (is_keyword(t, "if")) {
        ps_get(ps);
        return parse_if(ps);
    }

    if (is_keyword(t, "while")) {
        ps_get(ps);
        return parse_while(ps, 0);
    }

    if (is_keyword(t, "until")) {
        ps_get(ps);
        return parse_while(ps, 1);
    }

    if (is_keyword(t, "for")) {
        ps_get(ps);
        return parse_for(ps);
    }

    if (t->kind == TOK_LPAREN) {
        ps_get(ps);
        return parse_group(ps, 1);
    }

    if (t->kind == TOK_LBRACE) {
        ps_get(ps);
        return parse_group(ps, 0);
    }

    if (t->kind == TOK_WORD) {
        Token *t1 = ps_peek_n(ps, 1);
        Token *t2 = ps_peek_n(ps, 2);
        Token *t3 = ps_peek_n(ps, 3);
        if (t1 && t2 && t3 && t1->kind == TOK_LPAREN && t2->kind == TOK_RPAREN && t3->kind == TOK_LBRACE) {
            return parse_funcdef(ps);
        }
    }

    return parse_simple(ps);
}

static Ast *parse_pipeline(Parser *ps) {
    Ast *left = parse_command(ps);
    if (!left) {
        return 0;
    }

    while (ps_accept(ps, TOK_PIPE)) {
        Ast *right = parse_command(ps);
        if (!right) {
            return 0;
        }
        Ast *p = ast_new(ps->arena);
        if (!p) {
            return 0;
        }
        p->kind = AST_PIPE;
        p->u.bin.left = left;
        p->u.bin.right = right;
        left = p;
    }

    return left;
}

static Ast *parse_and_or(Parser *ps) {
    Ast *left = parse_pipeline(ps);
    if (!left) {
        return 0;
    }

    while (1) {
        if (ps_accept(ps, TOK_AND_IF)) {
            Ast *right = parse_pipeline(ps);
            if (!right) {
                return 0;
            }
            Ast *n = ast_new(ps->arena);
            if (!n) {
                return 0;
            }
            n->kind = AST_AND;
            n->u.bin.left = left;
            n->u.bin.right = right;
            left = n;
            continue;
        }

        if (ps_accept(ps, TOK_OR_IF)) {
            Ast *right = parse_pipeline(ps);
            if (!right) {
                return 0;
            }
            Ast *n = ast_new(ps->arena);
            if (!n) {
                return 0;
            }
            n->kind = AST_OR;
            n->u.bin.left = left;
            n->u.bin.right = right;
            left = n;
            continue;
        }

        break;
    }

    return left;
}

static Ast *parse_list(Parser *ps) {
    while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
        ps_get(ps);
    }

    Ast *left = parse_and_or(ps);
    if (!left) {
        Ast *e = ast_new(ps->arena);
        if (!e) {
            return 0;
        }
        e->kind = AST_EMPTY;
        return e;
    }

    while (1) {
        while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
            ps_get(ps);
        }

        TokenKind sep = TOK_EOF;
        if (ps_accept(ps, TOK_SEMI)) {
            sep = TOK_SEMI;
        } else if (ps_accept(ps, TOK_AMP)) {
            sep = TOK_AMP;
        } else {
            break;
        }

        while (ps_peek(ps) && ps_peek(ps)->kind == TOK_NEWLINE) {
            ps_get(ps);
        }

        Ast *right = parse_and_or(ps);
        if (!right) {
            break;
        }

        Ast *n = ast_new(ps->arena);
        if (!n) {
            return 0;
        }

        if (sep == TOK_AMP) {
            n->kind = AST_SEQ;
            Ast *bg = ast_new(ps->arena);
            if (!bg) {
                return 0;
            }
            bg->kind = AST_BG;
            bg->u.unary.child = right;
            n->u.bin.left = left;
            n->u.bin.right = bg;
        } else {
            n->kind = AST_SEQ;
            n->u.bin.left = left;
            n->u.bin.right = right;
        }

        left = n;
    }

    return left;
}

static int read_line(char *buf, size_t cap) {
    size_t n = 0;
    while (n + 1 < cap) {
        char ch;
        long r = read(0, &ch, 1);
        if (r <= 0) {
            return -1;
        }
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n') {
            buf[n++] = '\n';
            break;
        }
        if (ch == 0x7f || ch == 0x08) {
            if (n) {
                n--;
            }
            continue;
        }
        buf[n++] = ch;
    }
    buf[n] = '\0';
    return (int)n;
}

static int wait_status(int pid, int *status) {
    int st = 0;
    int r = waitpid(pid, &st);
    if (status) {
        *status = st;
    }
    return r;
}

static int eval_arith_expr(const char *s, int *ok) {
    int val = 0;
    int sign = 1;
    *ok = 1;

    while (sh_is_space(*s)) {
        s++;
    }

    if (*s == '+') {
        s++;
    } else if (*s == '-') {
        sign = -1;
        s++;
    }

    if (!sh_is_digit(*s)) {
        *ok = 0;
        return 0;
    }

    while (sh_is_digit(*s)) {
        val = val * 10 + (*s - '0');
        s++;
    }

    while (sh_is_space(*s)) {
        s++;
    }

    if (*s != '\0') {
        *ok = 0;
        return 0;
    }

    return val * sign;
}

static int expand_param(Shell *sh, const char *name, char *out, size_t cap) {
    if (name[0] == '?' && name[1] == '\0') {
        int v = sh->last_status;
        char tmp[16];
        int n = 0;
        if (v == 0) {
            tmp[n++] = '0';
        } else {
            int t = v;
            if (t < 0) {
                if (cap < 2) {
                    return 0;
                }
                out[0] = '-';
                out++;
                cap--;
                t = -t;
            }
            char rev[16];
            int rn = 0;
            while (t && rn < (int)sizeof(rev)) {
                rev[rn++] = (char)('0' + (t % 10));
                t /= 10;
            }
            while (rn) {
                tmp[n++] = rev[--rn];
            }
        }
        if ((size_t)n >= cap) {
            return 0;
        }
        memcpy(out, tmp, (size_t)n);
        out[n] = '\0';
        return n;
    }

    const char *v = shell_get_var(sh, name);
    if (!v) {
        v = "";
    }

    size_t n = strlen(v);
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(out, v, n);
    out[n] = '\0';
    return (int)n;
}

static int shell_eval_string(Shell *sh, const char *src, int depth);
static int exec_ast(Shell *sh, ShArena *arena, Ast *node, int depth);

static int capture_command_output(Shell *sh, const char *cmd, char *out, size_t cap, int depth) {
    if (depth > SH_SUBSHELL_DEPTH_LIMIT) {
        return 0;
    }

    int fds[2];
    if (pipe(fds) < 0) {
        return 0;
    }

    int pid = fork();
    if (pid == 0) {
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);

        const char *argv[4];
        argv[0] = "/bin/sh";
        argv[1] = "-c";
        argv[2] = cmd;
        argv[3] = 0;
        exec("/bin/sh", argv);
        exit(127);
    }

    close(fds[1]);

    size_t n = 0;
    while (n + 1 < cap) {
        long r = read(fds[0], &out[n], cap - n - 1);
        if (r <= 0) {
            break;
        }
        n += (size_t)r;
    }

    close(fds[0]);

    int st = 0;
    wait_status(pid, &st);
    sh->last_status = st;

    while (n && (out[n - 1] == '\n' || out[n - 1] == '\r')) {
        n--;
    }

    out[n] = '\0';
    return (int)n;
}

static char *expand_word(Shell *sh, ShArena *arena, const char *in, int quoted, int depth) {
    char tmp[SH_MAX_LINE];
    size_t outn = 0;

    for (size_t i = 0; in[i] && outn + 1 < sizeof(tmp);) {
        if (in[i] == '$' && in[i + 1] == '(' && in[i + 2] == '(') {
            size_t j = i + 3;
            while (in[j] && !(in[j] == ')' && in[j + 1] == ')')) {
                j++;
            }
            if (in[j] == ')' && in[j + 1] == ')') {
                size_t inner_len = j - (i + 3);
                char inner[SH_MAX_LINE];
                if (inner_len >= sizeof(inner)) {
                    inner_len = sizeof(inner) - 1;
                }
                memcpy(inner, &in[i + 3], inner_len);
                inner[inner_len] = '\0';

                int ok = 0;
                int v = eval_arith_expr(inner, &ok);
                if (ok) {
                    char num[16];
                    int nn = 0;
                    int t = v;
                    if (t == 0) {
                        num[nn++] = '0';
                    } else {
                        if (t < 0) {
                            num[nn++] = '-';
                            t = -t;
                        }
                        char rev[16];
                        int rn = 0;
                        while (t && rn < (int)sizeof(rev)) {
                            rev[rn++] = (char)('0' + (t % 10));
                            t /= 10;
                        }
                        while (rn) {
                            num[nn++] = rev[--rn];
                        }
                    }
                    for (int k = 0; k < nn && outn + 1 < sizeof(tmp); ++k) {
                        tmp[outn++] = num[k];
                    }
                    i = j + 2;
                    continue;
                }
            }
        }

        if (in[i] == '$' && in[i + 1] == '(' && in[i + 2] != '(') {
            size_t j = i + 2;
            int level = 1;
            while (in[j] && level) {
                if (in[j] == '(') {
                    level++;
                } else if (in[j] == ')') {
                    level--;
                }
                j++;
            }
            if (level == 0) {
                size_t inner_len = (j - 1) - (i + 2);
                char inner[SH_MAX_LINE];
                if (inner_len >= sizeof(inner)) {
                    inner_len = sizeof(inner) - 1;
                }
                memcpy(inner, &in[i + 2], inner_len);
                inner[inner_len] = '\0';

                char outbuf[SH_MAX_LINE];
                int n = capture_command_output(sh, inner, outbuf, sizeof(outbuf), depth + 1);
                for (int k = 0; k < n && outn + 1 < sizeof(tmp); ++k) {
                    tmp[outn++] = outbuf[k];
                }
                i = j;
                continue;
            }
        }

        if (in[i] == '`') {
            size_t j = i + 1;
            while (in[j] && in[j] != '`') {
                if (in[j] == '\\' && in[j + 1]) {
                    j += 2;
                } else {
                    j++;
                }
            }
            if (in[j] == '`') {
                size_t inner_len = j - (i + 1);
                char inner[SH_MAX_LINE];
                if (inner_len >= sizeof(inner)) {
                    inner_len = sizeof(inner) - 1;
                }
                memcpy(inner, &in[i + 1], inner_len);
                inner[inner_len] = '\0';

                char outbuf[SH_MAX_LINE];
                int n = capture_command_output(sh, inner, outbuf, sizeof(outbuf), depth + 1);
                for (int k = 0; k < n && outn + 1 < sizeof(tmp); ++k) {
                    tmp[outn++] = outbuf[k];
                }
                i = j + 1;
                continue;
            }
        }

        if (in[i] == '$') {
            if (in[i + 1] == '{') {
                size_t j = i + 2;
                while (in[j] && in[j] != '}') {
                    j++;
                }
                if (in[j] == '}') {
                    size_t nlen = j - (i + 2);
                    char name[64];
                    if (nlen >= sizeof(name)) {
                        nlen = sizeof(name) - 1;
                    }
                    memcpy(name, &in[i + 2], nlen);
                    name[nlen] = '\0';
                    char val[SH_MAX_LINE];
                    int vn = expand_param(sh, name, val, sizeof(val));
                    for (int k = 0; k < vn && outn + 1 < sizeof(tmp); ++k) {
                        tmp[outn++] = val[k];
                    }
                    i = j + 1;
                    continue;
                }
            }

            size_t j = i + 1;
            if (in[j] == '?') {
                char val[16];
                int vn = expand_param(sh, "?", val, sizeof(val));
                for (int k = 0; k < vn && outn + 1 < sizeof(tmp); ++k) {
                    tmp[outn++] = val[k];
                }
                i += 2;
                continue;
            }

            if (sh_is_name_start(in[j])) {
                size_t start = j;
                j++;
                while (sh_is_name_char(in[j])) {
                    j++;
                }
                size_t nlen = j - start;
                char name[64];
                if (nlen >= sizeof(name)) {
                    nlen = sizeof(name) - 1;
                }
                memcpy(name, &in[start], nlen);
                name[nlen] = '\0';
                char val[SH_MAX_LINE];
                int vn = expand_param(sh, name, val, sizeof(val));
                for (int k = 0; k < vn && outn + 1 < sizeof(tmp); ++k) {
                    tmp[outn++] = val[k];
                }
                i = j;
                continue;
            }
        }

        tmp[outn++] = in[i++];
    }

    tmp[outn] = '\0';
    (void)quoted;

    return arena_strdup(arena, tmp, outn);
}

static int split_fields(ShArena *arena, const char *s, const char **out, int max) {
    int n = 0;
    size_t i = 0;
    while (s[i]) {
        while (sh_is_space(s[i])) {
            i++;
        }
        if (!s[i]) {
            break;
        }
        size_t start = i;
        while (s[i] && !sh_is_space(s[i])) {
            i++;
        }
        if (n < max) {
            out[n++] = arena_strdup(arena, &s[start], i - start);
        }
    }
    return n;
}

static int brace_expand_one(ShArena *arena, const char *s, const char **out, int max) {
    const char *l = 0;
    const char *r = 0;
    for (const char *p = s; *p; ++p) {
        if (*p == '{') {
            l = p;
            break;
        }
    }
    if (!l) {
        out[0] = s;
        return 1;
    }

    int depth = 0;
    for (const char *p = l; *p; ++p) {
        if (*p == '{') {
            depth++;
        } else if (*p == '}') {
            depth--;
            if (depth == 0) {
                r = p;
                break;
            }
        }
    }
    if (!r) {
        out[0] = s;
        return 1;
    }

    const char *comma = 0;
    for (const char *p = l + 1; p < r; ++p) {
        if (*p == ',') {
            comma = p;
            break;
        }
    }
    if (!comma) {
        out[0] = s;
        return 1;
    }

    const char *prefix = s;
    size_t prefix_len = (size_t)(l - s);
    const char *suffix = r + 1;

    int n = 0;
    const char *p = l + 1;
    while (p < r && n < max) {
        const char *seg = p;
        while (p < r && *p != ',') {
            p++;
        }
        size_t seg_len = (size_t)(p - seg);
        if (p < r && *p == ',') {
            p++;
        }

        char *left = arena_strdup(arena, prefix, prefix_len);
        char *mid = arena_strdup(arena, seg, seg_len);
        char *res = arena_strcat3(arena, left ? left : "", mid ? mid : "", suffix);
        out[n++] = res ? res : "";
    }

    return n ? n : 1;
}

static int builtin_cd(Shell *sh, int argc, const char **argv) {
    (void)sh;
    (void)argc;
    (void)argv;
    sh_puts("cd: unsupported (no chdir syscall)\n");
    return 1;
}

static int builtin_export(Shell *sh, int argc, const char **argv) {
    if (argc == 1) {
        for (int i = 0; i < sh->var_count; ++i) {
            sh_puts(sh->vars[i].name);
            sh_puts("=");
            sh_puts(sh->vars[i].value);
            sh_puts("\n");
        }
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        const char *eq = 0;
        for (const char *p = argv[i]; *p; ++p) {
            if (*p == '=') {
                eq = p;
                break;
            }
        }
        if (!eq) {
            shell_set_var(sh, argv[i], "");
        } else {
            size_t n = (size_t)(eq - argv[i]);
            char name[64];
            if (n >= sizeof(name)) {
                n = sizeof(name) - 1;
            }
            memcpy(name, argv[i], n);
            name[n] = '\0';
            shell_set_var(sh, name, eq + 1);
        }
    }

    return 0;
}

static int builtin_unset(Shell *sh, int argc, const char **argv) {
    for (int i = 1; i < argc; ++i) {
        shell_unset_var(sh, argv[i]);
        shell_unset_alias(sh, argv[i]);
        shell_unset_func(sh, argv[i]);
    }
    return 0;
}

static int builtin_alias(Shell *sh, int argc, const char **argv) {
    if (argc == 1) {
        for (int i = 0; i < sh->alias_count; ++i) {
            sh_puts(sh->aliases[i].name);
            sh_puts("='");
            sh_puts(sh->aliases[i].value);
            sh_puts("'\n");
        }
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        const char *eq = 0;
        for (const char *p = argv[i]; *p; ++p) {
            if (*p == '=') {
                eq = p;
                break;
            }
        }
        if (!eq) {
            const char *v = shell_get_alias(sh, argv[i]);
            if (v) {
                sh_puts(argv[i]);
                sh_puts("='");
                sh_puts(v);
                sh_puts("'\n");
            }
            continue;
        }

        size_t n = (size_t)(eq - argv[i]);
        char name[64];
        if (n >= sizeof(name)) {
            n = sizeof(name) - 1;
        }
        memcpy(name, argv[i], n);
        name[n] = '\0';
        shell_set_alias(sh, name, eq + 1);
    }

    return 0;
}

static int builtin_history(Shell *sh, int argc, const char **argv) {
    (void)argc;
    (void)argv;
    for (int i = 0; i < sh->history_count; ++i) {
        sh_puts(sh->history[i]);
        size_t n = strlen(sh->history[i]);
        if (n == 0 || sh->history[i][n - 1] != '\n') {
            sh_puts("\n");
        }
    }
    return 0;
}

static int builtin_set(Shell *sh, int argc, const char **argv) {
    if (argc == 1) {
        sh_puts("set: -e ");
        sh_puts(sh->opt_errexit ? "on" : "off");
        sh_puts(", -x ");
        sh_puts(sh->opt_xtrace ? "on" : "off");
        sh_puts("\n");
        return 0;
    }

    for (int i = 1; i < argc; ++i) {
        if (sh_streq(argv[i], "-e")) {
            sh->opt_errexit = 1;
        } else if (sh_streq(argv[i], "+e")) {
            sh->opt_errexit = 0;
        } else if (sh_streq(argv[i], "-x")) {
            sh->opt_xtrace = 1;
        } else if (sh_streq(argv[i], "+x")) {
            sh->opt_xtrace = 0;
        }
    }

    return 0;
}

static int builtin_jobs(Shell *sh, int argc, const char **argv) {
    (void)argc;
    (void)argv;
    for (int i = 0; i < SH_MAX_JOBS; ++i) {
        if (sh->jobs[i].running) {
            sh_puts("[");
            char num[16];
            int nn = 0;
            int t = sh->jobs[i].pid;
            if (t == 0) {
                num[nn++] = '0';
            } else {
                char rev[16];
                int rn = 0;
                while (t && rn < (int)sizeof(rev)) {
                    rev[rn++] = (char)('0' + (t % 10));
                    t /= 10;
                }
                while (rn) {
                    num[nn++] = rev[--rn];
                }
            }
            num[nn] = '\0';
            sh_puts(num);
            sh_puts("] running ");
            sh_puts(sh->jobs[i].cmd ? sh->jobs[i].cmd : "");
            sh_puts("\n");
        }
    }
    return 0;
}

static int builtin_fg(Shell *sh, int argc, const char **argv) {
    int job_index = -1;
    for (int i = 0; i < SH_MAX_JOBS; ++i) {
        if (sh->jobs[i].running) {
            job_index = i;
            break;
        }
    }

    if (argc > 1) {
        int pid = sh_atoi(argv[1]);
        for (int i = 0; i < SH_MAX_JOBS; ++i) {
            if (sh->jobs[i].running && sh->jobs[i].pid == pid) {
                job_index = i;
                break;
            }
        }
    }

    if (job_index < 0) {
        sh_puts("fg: no jobs\n");
        return 1;
    }

    int st = 0;
    wait_status(sh->jobs[job_index].pid, &st);
    sh->jobs[job_index].running = 0;
    sh->last_status = st;
    return st;
}

static int builtin_disown(Shell *sh, int argc, const char **argv) {
    if (argc == 1) {
        for (int i = 0; i < SH_MAX_JOBS; ++i) {
            sh->jobs[i].running = 0;
        }
        return 0;
    }

    int pid = sh_atoi(argv[1]);
    for (int i = 0; i < SH_MAX_JOBS; ++i) {
        if (sh->jobs[i].running && sh->jobs[i].pid == pid) {
            sh->jobs[i].running = 0;
            return 0;
        }
    }

    return 1;
}

static int run_builtin(Shell *sh, int argc, const char **argv, int *handled) {
    *handled = 1;

    if (argc == 0) {
        return 0;
    }

    if (sh_streq(argv[0], "cd")) {
        return builtin_cd(sh, argc, argv);
    }
    if (sh_streq(argv[0], "export")) {
        return builtin_export(sh, argc, argv);
    }
    if (sh_streq(argv[0], "unset")) {
        return builtin_unset(sh, argc, argv);
    }
    if (sh_streq(argv[0], "alias")) {
        return builtin_alias(sh, argc, argv);
    }
    if (sh_streq(argv[0], "history")) {
        return builtin_history(sh, argc, argv);
    }
    if (sh_streq(argv[0], "set")) {
        return builtin_set(sh, argc, argv);
    }
    if (sh_streq(argv[0], "jobs")) {
        return builtin_jobs(sh, argc, argv);
    }
    if (sh_streq(argv[0], "fg")) {
        return builtin_fg(sh, argc, argv);
    }
    if (sh_streq(argv[0], "disown")) {
        return builtin_disown(sh, argc, argv);
    }
    if (sh_streq(argv[0], ":") || sh_streq(argv[0], "true")) {
        return 0;
    }
    if (sh_streq(argv[0], "false")) {
        return 1;
    }
    if (sh_streq(argv[0], "exit")) {
        int code = sh->last_status;
        if (argc > 1) {
            code = sh_atoi(argv[1]);
        }
        exit(code);
    }

    *handled = 0;
    return 0;
}

static int find_job_slot(Shell *sh) {
    for (int i = 0; i < SH_MAX_JOBS; ++i) {
        if (!sh->jobs[i].running) {
            return i;
        }
    }
    return -1;
}

static int exec_external(const char **argv) {
    const char *cmd = argv[0];

    if (!cmd || !cmd[0]) {
        exit(127);
    }

    int has_slash = 0;
    for (const char *p = cmd; *p; ++p) {
        if (*p == '/') {
            has_slash = 1;
            break;
        }
    }

    if (has_slash) {
        exec(cmd, argv);
        exit(127);
    }

    const char *paths[] = {"/bin/", "/usr/bin/", "/sbin/", 0};
    for (int i = 0; paths[i]; ++i) {
        char full[128];
        size_t pn = strlen(paths[i]);
        size_t cn = strlen(cmd);
        if (pn + cn + 1 >= sizeof(full)) {
            continue;
        }
        memcpy(full, paths[i], pn);
        memcpy(full + pn, cmd, cn);
        full[pn + cn] = '\0';
        exec(full, argv);
    }

    exit(127);
}

static int apply_redirs(Redirect *redirs, int nredirs) {
    for (int i = 0; i < nredirs; ++i) {
        Redirect *r = &redirs[i];
        if (r->kind == REDIR_HEREDOC) {
            int fds[2];
            if (pipe(fds) < 0) {
                return 1;
            }
            if (r->heredoc_body) {
                write(fds[1], r->heredoc_body, strlen(r->heredoc_body));
            }
            close(fds[1]);
            dup2(fds[0], r->fd);
            close(fds[0]);
            continue;
        }

        sh_puts("redirection to files unsupported\n");
        return 1;
    }

    return 0;
}

static int expand_command_words(Shell *sh, ShArena *arena, const char **in_words, const uint8_t *quoted, int nwords, const char **argv, int max, int depth) {
    int argc = 0;
    for (int i = 0; i < nwords && argc < max - 1; ++i) {
        char *x = expand_word(sh, arena, in_words[i], quoted[i] ? 1 : 0, depth);
        if (!x) {
            continue;
        }

        const char *fields[SH_MAX_ARGS];
        int nf = 1;
        if (!quoted[i]) {
            nf = split_fields(arena, x, fields, SH_MAX_ARGS);
        } else {
            fields[0] = x;
        }

        for (int k = 0; k < nf && argc < max - 1; ++k) {
            const char *expanded[SH_MAX_ARGS];
            int nb = brace_expand_one(arena, fields[k], expanded, SH_MAX_ARGS);
            for (int bi = 0; bi < nb && argc < max - 1; ++bi) {
                argv[argc++] = expanded[bi];
            }
        }
    }

    argv[argc] = 0;
    return argc;
}

static int exec_simple(Shell *sh, ShArena *arena, Ast *node, int in_child, int depth) {
    const char *argv[SH_MAX_ARGS];
    int argc = expand_command_words(
        sh,
        arena,
        node->u.simple.words,
        node->u.simple.quoted,
        node->u.simple.nwords,
        argv,
        SH_MAX_ARGS,
        depth);

    if (argc == 0) {
        return 0;
    }

    if (!in_child && depth < SH_ALIAS_EXPANSION_LIMIT) {
        const char *aval = shell_get_alias(sh, argv[0]);
        if (aval) {
            char buf[SH_MAX_LINE];
            size_t n = 0;
            size_t al = strlen(aval);
            if (al >= sizeof(buf) - 1) {
                al = sizeof(buf) - 1;
            }
            memcpy(buf, aval, al);
            n += al;

            for (int i = 1; i < argc && n + 2 < sizeof(buf); ++i) {
                buf[n++] = ' ';
                size_t wl = strlen(argv[i]);
                if (n + wl >= sizeof(buf) - 1) {
                    wl = (sizeof(buf) - 1) - n;
                }
                memcpy(&buf[n], argv[i], wl);
                n += wl;
            }
            buf[n] = '\0';
            return shell_eval_string(sh, buf, depth + 1);
        }
    }

    if (sh->opt_xtrace) {
        sh_puts("+");
        for (int i = 0; i < argc; ++i) {
            sh_puts(" ");
            sh_puts(argv[i]);
        }
        sh_puts("\n");
    }

    if (node->kind == AST_FUNCDEF) {
        shell_set_func(sh, node->u.funcdef.name, node->u.funcdef.body);
        return 0;
    }

    const char *fn = shell_get_func(sh, argv[0]);
    if (fn && !in_child) {
        return shell_eval_string(sh, fn, depth + 1);
    }

    if (!in_child) {
        int handled = 0;
        int st = run_builtin(sh, argc, argv, &handled);
        if (handled) {
            sh->last_status = st;
            return st;
        }
    }

    int pid = fork();
    if (pid == 0) {
        if (apply_redirs(node->u.simple.redirs, node->u.simple.nredirs)) {
            exit(1);
        }

        int handled = 0;
        int st = run_builtin(sh, argc, argv, &handled);
        if (handled) {
            exit(st);
        }

        if (fn) {
            int rst = shell_eval_string(sh, fn, depth + 1);
            exit(rst);
        }

        exec_external(argv);
        exit(127);
    }

    int st = 0;
    wait_status(pid, &st);
    sh->last_status = st;
    return st;
}

static int collect_pipeline(Ast *node, Ast **out, int max) {
    if (!node) {
        return 0;
    }
    if (node->kind == AST_PIPE) {
        int n = collect_pipeline(node->u.bin.left, out, max);
        if (n < max) {
            n += collect_pipeline(node->u.bin.right, out + n, max - n);
        }
        return n;
    }
    out[0] = node;
    return 1;
}

static int exec_pipeline(Shell *sh, ShArena *arena, Ast *pipe_node, int depth) {
    Ast *cmds[16];
    int ncmd = collect_pipeline(pipe_node, cmds, 16);
    if (ncmd <= 0) {
        return 0;
    }

    int prev_read = -1;
    int pids[16];

    for (int i = 0; i < ncmd; ++i) {
        int fds[2] = {-1, -1};
        if (i + 1 < ncmd) {
            if (pipe(fds) < 0) {
                return 1;
            }
        }

        int pid = fork();
        if (pid == 0) {
            if (prev_read >= 0) {
                dup2(prev_read, 0);
            }
            if (fds[1] >= 0) {
                dup2(fds[1], 1);
            }

            if (prev_read >= 0) {
                close(prev_read);
            }
            if (fds[0] >= 0) {
                close(fds[0]);
            }
            if (fds[1] >= 0) {
                close(fds[1]);
            }

            int st = exec_ast(sh, arena, cmds[i], depth + 1);
            exit(st);
        }

        pids[i] = pid;

        if (prev_read >= 0) {
            close(prev_read);
        }
        if (fds[1] >= 0) {
            close(fds[1]);
        }
        prev_read = fds[0];
    }

    if (prev_read >= 0) {
        close(prev_read);
    }

    int last = 0;
    for (int i = 0; i < ncmd; ++i) {
        int st = 0;
        wait_status(pids[i], &st);
        if (i == ncmd - 1) {
            last = st;
        }
    }

    sh->last_status = last;
    return last;
}

static int read_heredoc_body(const char *delim, ShArena *arena, const char **out_body) {
    char line[SH_MAX_LINE];
    size_t total = 0;

    char *buf = arena_alloc(arena, SH_SCRATCH_SIZE / 2);
    if (!buf) {
        return -1;
    }

    size_t dlen = strlen(delim);

    while (1) {
        sh_puts("> ");
        int n = read_line(line, sizeof(line));
        if (n < 0) {
            break;
        }

        if (strncmp(line, delim, dlen) == 0 && (line[dlen] == '\n' || line[dlen] == '\0')) {
            break;
        }

        if (total + (size_t)n >= SH_SCRATCH_SIZE / 2 - 1) {
            break;
        }
        memcpy(&buf[total], line, (size_t)n);
        total += (size_t)n;
    }

    buf[total] = '\0';
    *out_body = buf;
    return 0;
}

static void attach_heredocs(Ast *node, ShArena *arena) {
    if (!node) {
        return;
    }

    if (node->kind == AST_SIMPLE) {
        for (int i = 0; i < node->u.simple.nredirs; ++i) {
            Redirect *r = &node->u.simple.redirs[i];
            if (r->kind == REDIR_HEREDOC && !r->heredoc_body) {
                const char *body = 0;
                if (read_heredoc_body(r->target, arena, &body) == 0) {
                    r->heredoc_body = body;
                }
            }
        }
        return;
    }

    if (node->kind == AST_PIPE || node->kind == AST_SEQ || node->kind == AST_AND || node->kind == AST_OR) {
        attach_heredocs(node->u.bin.left, arena);
        attach_heredocs(node->u.bin.right, arena);
        return;
    }

    if (node->kind == AST_BG) {
        attach_heredocs(node->u.unary.child, arena);
        return;
    }

    if (node->kind == AST_GROUP || node->kind == AST_SUBSHELL) {
        attach_heredocs(node->u.group.body, arena);
        return;
    }

    if (node->kind == AST_IF) {
        attach_heredocs(node->u.ifnode.cond, arena);
        attach_heredocs(node->u.ifnode.then_part, arena);
        attach_heredocs(node->u.ifnode.else_part, arena);
        return;
    }

    if (node->kind == AST_WHILE) {
        attach_heredocs(node->u.whilenode.cond, arena);
        attach_heredocs(node->u.whilenode.body, arena);
        return;
    }

    if (node->kind == AST_FOR) {
        attach_heredocs(node->u.fornode.body, arena);
        return;
    }
}

static int exec_ast(Shell *sh, ShArena *arena, Ast *node, int depth) {
    if (!node) {
        return 0;
    }

    switch (node->kind) {
    case AST_EMPTY:
        return 0;
    case AST_SIMPLE:
        return exec_simple(sh, arena, node, 0, depth);
    case AST_FUNCDEF:
        shell_set_func(sh, node->u.funcdef.name, node->u.funcdef.body);
        return 0;
    case AST_SEQ: {
        int st = exec_ast(sh, arena, node->u.bin.left, depth);
        if (sh->opt_errexit && st != 0) {
            return st;
        }
        return exec_ast(sh, arena, node->u.bin.right, depth);
    }
    case AST_AND: {
        int st = exec_ast(sh, arena, node->u.bin.left, depth);
        if (st == 0) {
            return exec_ast(sh, arena, node->u.bin.right, depth);
        }
        return st;
    }
    case AST_OR: {
        int st = exec_ast(sh, arena, node->u.bin.left, depth);
        if (st != 0) {
            return exec_ast(sh, arena, node->u.bin.right, depth);
        }
        return st;
    }
    case AST_PIPE:
        return exec_pipeline(sh, arena, node, depth);
    case AST_BG: {
        int slot = find_job_slot(sh);
        int pid = fork();
        if (pid == 0) {
            int st = exec_ast(sh, arena, node->u.unary.child, depth);
            exit(st);
        }
        if (slot >= 0) {
            sh->jobs[slot].pid = pid;
            sh->jobs[slot].cmd = "(background)";
            sh->jobs[slot].running = 1;
        }
        return 0;
    }
    case AST_GROUP:
        return exec_ast(sh, arena, node->u.group.body, depth);
    case AST_SUBSHELL: {
        int pid = fork();
        if (pid == 0) {
            int st = exec_ast(sh, arena, node->u.group.body, depth + 1);
            exit(st);
        }
        int st = 0;
        wait_status(pid, &st);
        sh->last_status = st;
        return st;
    }
    case AST_IF: {
        int st = exec_ast(sh, arena, node->u.ifnode.cond, depth);
        if (st == 0) {
            return exec_ast(sh, arena, node->u.ifnode.then_part, depth);
        }
        if (node->u.ifnode.else_part) {
            return exec_ast(sh, arena, node->u.ifnode.else_part, depth);
        }
        return st;
    }
    case AST_WHILE: {
        int last = 0;
        while (1) {
            int st = exec_ast(sh, arena, node->u.whilenode.cond, depth);
            int cond_true = node->u.whilenode.until ? (st != 0) : (st == 0);
            if (!cond_true) {
                break;
            }
            last = exec_ast(sh, arena, node->u.whilenode.body, depth);
            if (sh->opt_errexit && last != 0) {
                return last;
            }
        }
        return last;
    }
    case AST_FOR: {
        int last = 0;
        for (int i = 0; i < node->u.fornode.nitems; ++i) {
            shell_set_var(sh, node->u.fornode.var, node->u.fornode.items[i]);
            last = exec_ast(sh, arena, node->u.fornode.body, depth);
            if (sh->opt_errexit && last != 0) {
                return last;
            }
        }
        return last;
    }
    default:
        return 0;
    }
}

static int shell_eval_string(Shell *sh, const char *src, int depth) {
    if (depth > SH_ALIAS_EXPANSION_LIMIT) {
        return 1;
    }

    ShArena arena;
    arena_reset(&arena);

    TokenStream ts;
    if (tokenize(&arena, src, &ts) != 0) {
        sh_puts("tokenize: too many tokens\n");
        return 1;
    }

    Parser ps;
    ps.ts = &ts;
    ps.pos = 0;
    ps.arena = &arena;

    Ast *ast = parse_list(&ps);
    if (!ast) {
        sh_puts("parse error\n");
        return 2;
    }

    attach_heredocs(ast, &arena);

    int st = exec_ast(sh, &arena, ast, depth);
    sh->last_status = st;
    return st;
}

static void shell_init(Shell *sh) {
    memset(sh, 0, sizeof(*sh));
    sh->last_status = 0;
    sh->opt_errexit = 0;
    sh->opt_xtrace = 0;
    sh->perm.pos = 0;

    shell_set_var(sh, "PATH", "/bin:/usr/bin:/sbin");
}

int main(int argc, char **argv) {
    Shell sh;
    shell_init(&sh);

    if (argc >= 3 && argv[1] && sh_streq(argv[1], "-c")) {
        return shell_eval_string(&sh, argv[2], 0);
    }

    if (argc > 1) {
        sh_puts("sh: script execution unsupported (no filesystem); use: sh -c '...'\n");
    }

    sh_puts("sh: ready\n");

    char line[SH_MAX_LINE];
    while (1) {
        sh_puts("sh$ ");
        int n = read_line(line, sizeof(line));
        if (n < 0) {
            break;
        }
        if (n == 0) {
            continue;
        }

        shell_history_add(&sh, line);
        shell_eval_string(&sh, line, 0);
    }

    return 0;
}
