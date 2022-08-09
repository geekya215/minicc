#include "minicc.h"
#include <stdio.h>

static int depth;

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
      printf("  ld a0, 0(a0)\n");
      return;
    case ND_DEREF:
      gen_expr(node->lhs);
      printf("  ld a0, 0(a0)\n");
      return;
    case ND_ADDR:
      gen_addr(node->lhs);
      return;
    case ND_ASSIGN:
      gen_addr(node->lhs);
      push();
      gen_expr(node->rhs);
      pop("a1");
      printf("  sd a0, 0(a1)\n");
      return;
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
      printf("  j .L.return\n");
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
  int offset = 0;
  for (Obj *var = prog->locals; var; var = var->next) {
    offset += 8;
    var->offset = -offset;
  }
  prog->stack_size = align_to(offset, 16);
}

void codegen(Function *prog) {
  assign_lvar_offsets(prog);
  printf("  .globl main\n");
  printf("main:\n");

  printf("  addi sp, sp, -8\n");
  printf("  sd fp, 0(sp)\n");
  printf("  mv fp, sp\n");
  printf("  addi sp, sp, %d\n", -prog->stack_size);

  gen_stmt(prog->body);
  assert(depth == 0);

  printf(".L.return:\n");
  printf("  mv sp, fp\n");
  printf("  ld fp, 0(sp)\n");
  printf("  addi sp, sp, 8\n");

  printf("  ret\n");
}

