#include "minigo/tac.h"
#include <sstream>
#include <stdexcept>

namespace minigo {

const char *tac_op_name(TACOp op) {
    switch (op) {
        case TACOp::ASSIGN:            return "=";
        case TACOp::ADD:               return "+";
        case TACOp::SUB:               return "-";
        case TACOp::MUL:               return "*";
        case TACOp::DIV:               return "/";
        case TACOp::MOD:               return "%";
        case TACOp::NEG:               return "neg";
        case TACOp::NOT:               return "not";
        case TACOp::EQ:                return "==";
        case TACOp::NE:                return "!=";
        case TACOp::LT:                return "<";
        case TACOp::LE:                return "<=";
        case TACOp::GT:                return ">";
        case TACOp::GE:                return ">=";
        case TACOp::AND:               return "&&";
        case TACOp::OR:                return "||";
        case TACOp::LABEL:             return "label";
        case TACOp::JUMP:              return "jmp";
        case TACOp::JUMP_IF_ZERO:      return "ifz";
        case TACOp::JUMP_IF_NOT_ZERO:  return "ifnz";
        case TACOp::CALL:              return "call";
        case TACOp::PRINT:             return "print";
        case TACOp::RET:               return "ret";
        case TACOp::PARAM:             return "param";
    }
    return "unknown";
}

std::string TACOperand::to_string() const {
    switch (kind) {
        case TACOperandKind::NONE:      return "";
        case TACOperandKind::CONST_INT: return std::to_string(int_val);
        case TACOperandKind::VAR:       return name;
        case TACOperandKind::TEMP:      return name;
        case TACOperandKind::LABEL:     return name;
    }
    return "";
}

bool TACOperand::operator==(const TACOperand &other) const {
    if (kind != other.kind) return false;
    if (kind == TACOperandKind::CONST_INT) return int_val == other.int_val;
    return name == other.name;
}

std::string TACInstr::to_string() const {
    std::ostringstream oss;
    switch (op) {
        case TACOp::LABEL:
            oss << dest.to_string() << ":";
            break;
        case TACOp::ASSIGN:
            oss << "    " << dest.to_string() << " = " << src1.to_string();
            break;
        case TACOp::ADD:
        case TACOp::SUB:
        case TACOp::MUL:
        case TACOp::DIV:
        case TACOp::MOD:
        case TACOp::EQ:
        case TACOp::NE:
        case TACOp::LT:
        case TACOp::LE:
        case TACOp::GT:
        case TACOp::GE:
        case TACOp::AND:
        case TACOp::OR:
            oss << "    " << dest.to_string() << " = " << src1.to_string() << " "
                << tac_op_name(op) << " " << src2.to_string();
            break;
        case TACOp::NEG:
            oss << "    " << dest.to_string() << " = -" << src1.to_string();
            break;
        case TACOp::NOT:
            oss << "    " << dest.to_string() << " = !" << src1.to_string();
            break;
        case TACOp::JUMP:
            oss << "    jmp " << dest.to_string();
            break;
        case TACOp::JUMP_IF_ZERO:
            oss << "    ifz " << src1.to_string() << " goto " << dest.to_string();
            break;
        case TACOp::JUMP_IF_NOT_ZERO:
            oss << "    ifnz " << src1.to_string() << " goto " << dest.to_string();
            break;
        case TACOp::CALL:
            oss << "    ";
            if (!dest.is_none()) {
                oss << dest.to_string() << " = ";
            }
            oss << "call " << callee << "(";
            for (size_t i = 0; i < args.size(); ++i) {
                if (i > 0) oss << ", ";
                oss << args[i].to_string();
            }
            oss << ")";
            break;
        case TACOp::PRINT:
            oss << "    " << (print_newline ? "println" : "print") << "(" << src1.to_string() << ")";
            break;
        case TACOp::RET:
            oss << "    ret";
            if (!src1.is_none()) {
                oss << " " << src1.to_string();
            }
            break;
        case TACOp::PARAM:
            oss << "    param " << dest.to_string();
            break;
    }
    return oss.str();
}

std::string TACFunc::to_string() const {
    std::ostringstream oss;
    oss << "func " << name << "(";
    for (size_t i = 0; i < params.size(); ++i) {
        if (i > 0) oss << ", ";
        oss << params[i];
    }
    oss << "):\n";
    for (const auto &instr : instrs) {
        oss << instr.to_string() << "\n";
    }
    return oss.str();
}

std::string TACProgram::to_string() const {
    std::ostringstream oss;
    for (const auto &fn : funcs) {
        oss << fn.to_string() << "\n";
    }
    return oss.str();
}

void TACProgram::dump(std::ostream &os) const {
    os << to_string();
}

namespace {

class TACBuilder {
public:
    TACFunc func;
    int temp_counter = 0;
    int label_counter = 0;

    TACOperand new_temp() {
        return TACOperand::make_temp("t" + std::to_string(temp_counter++));
    }

    std::string new_label(const std::string &prefix = "L") {
        return prefix + "_" + std::to_string(label_counter++);
    }

    void emit(TACInstr instr) {
        func.instrs.push_back(instr);
    }

    void emit_label(const std::string &label_name) {
        TACInstr instr;
        instr.op = TACOp::LABEL;
        instr.dest = TACOperand::make_label(label_name);
        emit(instr);
    }

    void emit_jump(const std::string &target_label) {
        TACInstr instr;
        instr.op = TACOp::JUMP;
        instr.dest = TACOperand::make_label(target_label);
        emit(instr);
    }

    void emit_ifz(const TACOperand &cond, const std::string &target_label) {
        TACInstr instr;
        instr.op = TACOp::JUMP_IF_ZERO;
        instr.src1 = cond;
        instr.dest = TACOperand::make_label(target_label);
        emit(instr);
    }

    TACOperand lower_expr(const ASTExpr *e) {
        if (!e) return TACOperand::make_none();

        switch (e->kind) {
            case EXPR_INT_LIT:
                return TACOperand::make_int(e->int_val);

            case EXPR_STRING_LIT:
                /* String literal not supported in numeric SSA, wrap or handle as var */
                return TACOperand::make_var(e->str_val ? e->str_val : "");

            case EXPR_IDENT:
                return TACOperand::make_var(e->ident_name);

            case EXPR_BINARY: {
                TACOperand left = lower_expr(e->binary.left);
                TACOperand right = lower_expr(e->binary.right);
                TACOperand t = new_temp();

                TACOp op = TACOp::ADD;
                switch (e->binary.op) {
                    case TOKEN_PLUS:     op = TACOp::ADD; break;
                    case TOKEN_MINUS:    op = TACOp::SUB; break;
                    case TOKEN_STAR:     op = TACOp::MUL; break;
                    case TOKEN_SLASH:    op = TACOp::DIV; break;
                    case TOKEN_PERCENT:  op = TACOp::MOD; break;
                    case TOKEN_EQ_EQ:    op = TACOp::EQ;  break;
                    case TOKEN_NOT_EQ:   op = TACOp::NE;  break;
                    case TOKEN_LT:       op = TACOp::LT;  break;
                    case TOKEN_LE:       op = TACOp::LE;  break;
                    case TOKEN_GT:       op = TACOp::GT;  break;
                    case TOKEN_GE:       op = TACOp::GE;  break;
                    case TOKEN_AND_AND:  op = TACOp::AND; break;
                    case TOKEN_OR_OR:    op = TACOp::OR;  break;
                    default:             op = TACOp::ADD; break;
                }

                TACInstr instr;
                instr.op = op;
                instr.dest = t;
                instr.src1 = left;
                instr.src2 = right;
                emit(instr);
                return t;
            }

            case EXPR_UNARY: {
                TACOperand operand = lower_expr(e->unary.operand);
                TACOperand t = new_temp();

                TACInstr instr;
                if (e->unary.op == TOKEN_MINUS) {
                    instr.op = TACOp::NEG;
                } else if (e->unary.op == TOKEN_EXCLAMATION) {
                    instr.op = TACOp::NOT;
                } else {
                    instr.op = TACOp::ASSIGN;
                }
                instr.dest = t;
                instr.src1 = operand;
                emit(instr);
                return t;
            }

            case EXPR_CALL: {
                const char *name = e->call.callee_name;
                if (strcmp(name, "println") == 0 || strcmp(name, "print") == 0) {
                    TACOperand arg = TACOperand::make_int(0);
                    if (e->call.arg_count > 0 && e->call.args[0]) {
                        arg = lower_expr(e->call.args[0]);
                    }
                    TACInstr instr;
                    instr.op = TACOp::PRINT;
                    instr.src1 = arg;
                    instr.print_newline = (strcmp(name, "println") == 0);
                    emit(instr);
                    return TACOperand::make_none();
                } else {
                    std::vector<TACOperand> call_args;
                    for (size_t i = 0; i < e->call.arg_count; ++i) {
                        call_args.push_back(lower_expr(e->call.args[i]));
                    }
                    TACOperand t = new_temp();
                    TACInstr instr;
                    instr.op = TACOp::CALL;
                    instr.dest = t;
                    instr.callee = name;
                    instr.args = call_args;
                    emit(instr);
                    return t;
                }
            }

            case EXPR_FIELD_ACCESS:
                /* Struct field access placeholder */
                return TACOperand::make_var(std::string(e->field_access.field_name));
        }

        return TACOperand::make_none();
    }

    void lower_stmt(const ASTStmt *s) {
        if (!s) return;

        switch (s->kind) {
            case STMT_BLOCK:
                for (size_t i = 0; i < s->block.count; ++i) {
                    lower_stmt(s->block.stmts[i]);
                }
                break;

            case STMT_EXPR:
                lower_expr(s->expr_stmt);
                break;

            case STMT_VAR_DECL: {
                TACOperand init_op;
                if (s->var_decl.init) {
                    init_op = lower_expr(s->var_decl.init);
                } else {
                    init_op = TACOperand::make_int(0);
                }
                TACInstr instr;
                instr.op = TACOp::ASSIGN;
                instr.dest = TACOperand::make_var(s->var_decl.var_name);
                instr.src1 = init_op;
                emit(instr);
                break;
            }

            case STMT_SHORT_VAR_DECL: {
                TACOperand init_op = lower_expr(s->short_var_decl.init);
                TACInstr instr;
                instr.op = TACOp::ASSIGN;
                instr.dest = TACOperand::make_var(s->short_var_decl.var_name);
                instr.src1 = init_op;
                emit(instr);
                break;
            }

            case STMT_ASSIGN: {
                TACOperand val_op = lower_expr(s->assign.value);
                TACInstr instr;
                instr.op = TACOp::ASSIGN;
                instr.dest = TACOperand::make_var(s->assign.var_name);
                instr.src1 = val_op;
                emit(instr);
                break;
            }

            case STMT_RETURN: {
                TACOperand ret_op = TACOperand::make_none();
                if (s->ret.value) {
                    ret_op = lower_expr(s->ret.value);
                }
                TACInstr instr;
                instr.op = TACOp::RET;
                instr.src1 = ret_op;
                emit(instr);
                break;
            }

            case STMT_IF: {
                TACOperand cond_op = lower_expr(s->if_stmt.cond);
                std::string else_lbl = new_label("L_else");
                std::string end_lbl = new_label("L_endif");

                emit_ifz(cond_op, else_lbl);

                lower_stmt(s->if_stmt.then_branch);
                emit_jump(end_lbl);

                emit_label(else_lbl);
                if (s->if_stmt.else_branch) {
                    lower_stmt(s->if_stmt.else_branch);
                }
                emit_label(end_lbl);
                break;
            }

            case STMT_FOR: {
                std::string cond_lbl = new_label("L_loop_cond");
                std::string body_lbl = new_label("L_loop_body");
                std::string end_lbl  = new_label("L_loop_end");

                if (s->for_stmt.init) {
                    lower_stmt(s->for_stmt.init);
                }

                emit_label(cond_lbl);
                if (s->for_stmt.cond) {
                    TACOperand cond_op = lower_expr(s->for_stmt.cond);
                    emit_ifz(cond_op, end_lbl);
                }

                emit_label(body_lbl);
                lower_stmt(s->for_stmt.body);

                if (s->for_stmt.post) {
                    lower_stmt(s->for_stmt.post);
                }
                emit_jump(cond_lbl);

                emit_label(end_lbl);
                break;
            }
        }
    }
};

} // anonymous namespace

TACProgram lower_ast_to_tac(const ASTProgram *ast) {
    TACProgram prog;
    if (!ast) return prog;

    for (size_t i = 0; i < ast->decl_count; ++i) {
        const ASTDecl *decl = ast->decls[i];
        if (decl->kind == DECL_FUNC) {
            TACBuilder builder;
            builder.func.name = decl->func.name;

            for (size_t p = 0; p < decl->func.param_count; ++p) {
                std::string param_name = decl->func.params[p].name;
                builder.func.params.push_back(param_name);
                TACInstr pinstr;
                pinstr.op = TACOp::PARAM;
                pinstr.dest = TACOperand::make_var(param_name);
                builder.emit(pinstr);
            }

            builder.lower_stmt(decl->func.body);

            /* Ensure function ends with a return */
            bool has_terminal_ret = false;
            if (!builder.func.instrs.empty() && builder.func.instrs.back().op == TACOp::RET) {
                has_terminal_ret = true;
            }
            if (!has_terminal_ret) {
                TACInstr ret_instr;
                ret_instr.op = TACOp::RET;
                ret_instr.src1 = TACOperand::make_int(0);
                builder.emit(ret_instr);
            }

            prog.funcs.push_back(builder.func);
        }
    }

    return prog;
}

} // namespace minigo
