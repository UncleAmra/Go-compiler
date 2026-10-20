#ifndef MINIGO_TAC_H
#define MINIGO_TAC_H

#include <string>
#include <vector>
#include <iostream>
#include <cstdint>

#include "minigo/ast.h"

namespace minigo {

enum class TACOp {
    ASSIGN,
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    NEG,
    NOT,
    EQ,
    NE,
    LT,
    LE,
    GT,
    GE,
    AND,
    OR,
    LABEL,
    JUMP,
    JUMP_IF_ZERO,
    JUMP_IF_NOT_ZERO,
    CALL,
    PRINT,    /* println / print */
    RET,
    PARAM
};

const char *tac_op_name(TACOp op);

enum class TACOperandKind {
    NONE,
    CONST_INT,
    VAR,
    TEMP,
    LABEL
};

struct TACOperand {
    TACOperandKind kind = TACOperandKind::NONE;
    int64_t int_val = 0;
    std::string name;

    static TACOperand make_none() {
        return TACOperand();
    }

    static TACOperand make_int(int64_t val) {
        TACOperand op;
        op.kind = TACOperandKind::CONST_INT;
        op.int_val = val;
        return op;
    }

    static TACOperand make_var(const std::string &var_name) {
        TACOperand op;
        op.kind = TACOperandKind::VAR;
        op.name = var_name;
        return op;
    }

    static TACOperand make_temp(const std::string &temp_name) {
        TACOperand op;
        op.kind = TACOperandKind::TEMP;
        op.name = temp_name;
        return op;
    }

    static TACOperand make_label(const std::string &lbl_name) {
        TACOperand op;
        op.kind = TACOperandKind::LABEL;
        op.name = lbl_name;
        return op;
    }

    bool is_none() const { return kind == TACOperandKind::NONE; }
    bool is_const() const { return kind == TACOperandKind::CONST_INT; }
    bool is_var() const { return kind == TACOperandKind::VAR; }
    bool is_temp() const { return kind == TACOperandKind::TEMP; }
    bool is_label() const { return kind == TACOperandKind::LABEL; }
    bool is_variable_or_temp() const { return is_var() || is_temp(); }

    std::string to_string() const;
    bool operator==(const TACOperand &other) const;
    bool operator!=(const TACOperand &other) const { return !(*this == other); }
};

struct TACInstr {
    TACOp op = TACOp::ASSIGN;
    TACOperand dest;
    TACOperand src1;
    TACOperand src2;
    std::vector<TACOperand> args; /* For CALL */
    std::string callee;           /* For CALL */
    bool print_newline = true;    /* For PRINT (println vs print) */

    std::string to_string() const;
};

struct TACFunc {
    std::string name;
    std::vector<std::string> params;
    std::vector<TACInstr> instrs;

    std::string to_string() const;
};

struct TACProgram {
    std::vector<TACFunc> funcs;

    std::string to_string() const;
    void dump(std::ostream &os) const;
};

/* Lowers an AST program to Three-Address Code (TAC) */
TACProgram lower_ast_to_tac(const ASTProgram *ast);

} // namespace minigo

#endif /* MINIGO_TAC_H */
