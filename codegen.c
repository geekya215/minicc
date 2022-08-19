#include "minicc.h"
#include <stdio.h>

static int depth;
static char *argreg[] = {"a0", "a1", "a2", "a3", "a4", "a5"};
static Function *current_fn;

static void gen_expr(Node *node);

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  printf("  addi sp, sp, -8\n");
  printf("  sd a0, 0(sp)\n");
  depth++;
}

static void pop(char *reg) {
  printf("  ld %s, 0(sp)\n", reg);
  printf("  addi sp, sp, 8\n");
  depth--;
}

static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

static void gen_addr(Node *node) {
  switch (node->kind) {
    case ND_VAR:
      printf("  addi a0, fp, %d\n", node->var->offset);
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      return;
    default:
      break;
  }
  error_tok(node->tok, "not an lvalue");
}

// Load a value from where %rax is pointing to.
static void load(Type *ty) {
  if (ty->kind == TY_ARRAY) {
    // If it is an array, do not attempt to load a value to the
    // register because in general we can't load an entire array to a
    // register. As a result, the result of an evaluation of an array
    // becomes not the array itself but the address of the array.
    // This is where "array is automatically converted to a pointer to
    // the first element of the array in C" occurs.
    return;
  }

  printf("  ld a0, 0(a0)\n");
}

// Store %rax to an address that the stack top is pointing to.
static void store(void) {
  pop("a1");
  printf("  sd a0, 0(a1)\n");
}

static void gen_expr(Node *node) {
  switch (node->kind) {
    case ND_NUM:
      printf("  li a0, %d\n", node->val);
      return;
    case ND_NEG:
      gen_expr(node->lhs);
      printf("  neg a0, a0\n");
      return;
    case ND_VAR:
      gen_addr(node);
      load(node->ty);
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      load(node->ty);
      return;
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_ASSIGN:
      gen_addr(node->lhs);
      push();
      gen_expr(node->rhs);
      store();
      return;
    case ND_FUNCALL: {
      int nargs = 0;
      for (Node *arg = node->args; arg; arg = arg->next) {
        gen_expr(arg);
        push();
        nargs++;
      }

      for (int i = nargs - 1; i >= 0; i--)
        pop(argreg[i]);

      printf("  call %s\n", node->funcname);
      return;
    }
    default:
      break;
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("a1");

  switch (node->kind) {
    case ND_ADD:
      printf("  add a0, a0, a1\n");
      return;
    case ND_SUB:
      printf("  sub a0, a0, a1\n");
      return;
    case ND_MUL:
      printf("  mul a0, a0, a1\n");
      return;
    case ND_DIV:
      printf("  div a0, a0, a1\n");
      return;
    case ND_EQ:
    case ND_NE:
      printf("  xor a0, a0, a1\n");
      if (node->kind == ND_EQ)
        printf("  seqz a0, a0\n");
      else
        printf("  snez a0, a0\n");
      return;
    case ND_LT:
      printf("  slt a0, a0, a1\n");
      return;
    case ND_LE:
      printf("  slt a0, a1, a0\n");
      printf("  xori a0, a0, 1\n");
      return;
    default:
      break;
  }

  error_tok(node->tok, "invalid expression");
}

static void gen_stmt(Node *node) {
  switch (node->kind) {
    case ND_IF: {
      int c = count();
      gen_expr(node->cond);
      printf("  beqz a0, .L.else.%d\n", c);
      gen_stmt(node->then);
      printf("  j .L.end.%d\n", c);
      printf(".L.else.%d:\n", c);
      if (node->els)
        gen_stmt(node->els);
      printf(".L.end.%d:\n", c);
      return;
    }
    case ND_FOR: {
      int c = count();
      if (node->init)
        gen_stmt(node->init);
      printf(".L.begin.%d:\n", c);
      if (node->cond) {
        gen_expr(node->cond);
        printf("  beqz a0, .L.end.%d\n", c);
      }
      gen_stmt(node->then);
      if (node->inc)
        gen_expr(node->inc);
      printf("  j .L.begin.%d\n", c);
      printf(".L.end.%d:\n", c);
      return;
    }
    case ND_BLOCK:
      for (Node *n = node->body; n; n = n->next)
        gen_stmt(n);
      return;
    case ND_RETURN:
      gen_expr(node->lhs);
      printf("  j .L.return.%s\n", current_fn->name);
      return;
    case ND_EXPR_STMT:
      gen_expr(node->lhs);
      return;
    default:
      break;
  }

  error_tok(node->tok, "invalid statement");
}

static void assign_lvar_offsets(Function *prog) {
  for (Function *fn = prog; fn; fn = fn->next) {
    int offset = 0;
    for (Obj *var = fn->locals; var; var = var->next) {
      offset += var->ty->size;
      var->offset = -offset;
    }
    fn->stack_size = align_to(offset, 16);
  }
}

void codegen(Function *prog) {
  assign_lvar_offsets(prog);

  for (Function *fn = prog; fn; fn = fn->next) {
    printf("  .globl %s\n", fn->name);
    printf("%s:\n", fn->name);
    current_fn = fn;

    printf("  addi sp, sp, -16\n");
    printf("  sd ra, 8(sp)\n");
    printf("  sd fp, 0(sp)\n");
    printf("  mv fp, sp\n");
    printf("  addi sp, sp, %d\n", -fn->stack_size);

    // Save passed-by-register arguments to the stack
    int i = 0;
    for (Obj *var = fn->params; var; var = var->next)
      printf("  sd %s, %d(fp)\n", argreg[i++], var->offset);

    gen_stmt(fn->body);
    assert(depth == 0);

    printf(".L.return.%s:\n", fn->name);
    printf("  mv sp, fp\n");
    printf("  ld fp, 0(sp)\n");
    printf("  ld ra, 8(sp)\n");
    printf("  addi sp, sp, 16\n");

    printf("  ret\n");
  }
}

