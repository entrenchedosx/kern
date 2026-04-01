/* *
 * kern Code Generator Implementation
 */

#include "codegen.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>

namespace kern {

CodeGenerator::CodeGenerator() { beginScope(); }

size_t CodeGenerator::addConstant(const std::string& s) {
    auto it = std::find(stringConstants_.begin(), stringConstants_.end(), s);
    if (it != stringConstants_.end()) return it - stringConstants_.begin();
    stringConstants_.push_back(s);
    return stringConstants_.size() - 1;
}

size_t CodeGenerator::addValueConstant(Value v) {
    valueConstants_.push_back(std::move(v));
    return valueConstants_.size() - 1;
}

size_t CodeGenerator::emit(Opcode op) {
    code_.emplace_back(op);
    code_.back().line = currentLine_;
    return code_.size() - 1;
}

size_t CodeGenerator::emit(Opcode op, int64_t arg) {
    code_.emplace_back(op, arg);
    code_.back().line = currentLine_;
    return code_.size() - 1;
}

size_t CodeGenerator::emit(Opcode op, double arg) {
    code_.emplace_back(op, arg);
    code_.back().line = currentLine_;
    return code_.size() - 1;
}

size_t CodeGenerator::emit(Opcode op, const std::string& arg) {
    return emit(op, addConstant(arg));
}

size_t CodeGenerator::emit(Opcode op, size_t arg) {
    code_.emplace_back(op, arg);
    code_.back().line = currentLine_;
    return code_.size() - 1;
}

size_t CodeGenerator::emit(Opcode op, size_t a, size_t b) {
    code_.emplace_back(op, a, b);
    code_.back().line = currentLine_;
    return code_.size() - 1;
}

void CodeGenerator::patchJump(size_t at, size_t target) {
    if (at >= code_.size()) return;
    code_[at].operand = target;
}

size_t CodeGenerator::resolveLocal(const std::string& name) {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        auto it = scopes_[i].find(name);
        if (it != scopes_[i].end()) return it->second;
    }
    return static_cast<size_t>(-1);
}

size_t CodeGenerator::totalLocalCount() {
    size_t n = 0;
    for (const auto& scope : scopes_) n += scope.size();
    return n;
}

void CodeGenerator::declareLocal(const std::string& name) {
    size_t slot = totalLocalCount();
    scopes_.back()[name] = slot;
}

void CodeGenerator::beginScope() { scopes_.emplace_back(); }

void CodeGenerator::endScope() { scopes_.pop_back(); }

// constant folding: get numeric value from a literal, or NAN if not foldable.
static void getNumeric(const Expr* e, bool* outIsInt, int64_t* outI, double* outD) {
    *outIsInt = false;
    *outI = 0;
    *outD = std::nan("");
    if (auto* n = dynamic_cast<const IntLiteral*>(e)) { *outIsInt = true; *outI = n->value; *outD = static_cast<double>(n->value); return; }
    if (auto* n = dynamic_cast<const FloatLiteral*>(e)) { *outD = n->value; *outI = static_cast<int64_t>(n->value); return; }
}

bool CodeGenerator::tryConstantFoldBinary(const BinaryExpr* x) {
    bool li, ri;
    int64_t lI, rI;
    double lD, rD;
    getNumeric(x->left.get(), &li, &lI, &lD);
    getNumeric(x->right.get(), &ri, &rI, &rD);
    if (std::isnan(lD) || std::isnan(rD)) return false;
    double resD = 0;
    int64_t resI = 0;
    bool useInt = false;
    switch (x->op) {
        case TokenType::PLUS:  resD = lD + rD; break;
        case TokenType::MINUS: resD = lD - rD; break;
        case TokenType::STAR:  resD = lD * rD; break;
        case TokenType::SLASH: if (rD == 0) return false; resD = lD / rD; break;
        case TokenType::PERCENT: if (rI == 0) return false; resI = lI % rI; useInt = true; break;
        case TokenType::STAR_STAR: resD = std::pow(lD, rD); break;
        case TokenType::EQ:  emit(lD == rD ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::NEQ: emit(lD != rD ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::LT:  emit(lD < rD ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::LE:  emit(lD <= rD ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::GT:  emit(lD > rD ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::GE:  emit(lD >= rD ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::AND: emit((lD != 0 && rD != 0) ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::OR:  emit((lD != 0 || rD != 0) ? Opcode::CONST_TRUE : Opcode::CONST_FALSE); return true;
        case TokenType::BIT_AND: resI = lI & rI; useInt = true; break;
        case TokenType::BIT_OR:  resI = lI | rI; useInt = true; break;
        case TokenType::BIT_XOR: resI = lI ^ rI; useInt = true; break;
        case TokenType::SHL: resI = lI << (rI & 63); useInt = true; break;
        case TokenType::SHR: resI = lI >> (rI & 63); useInt = true; break;
        default: return false;
    }
    if (useInt) {
        emit(Opcode::CONST_I64, resI);
    } else if (std::floor(resD) == resD && resD >= -9007199254740992.0 && resD <= 9007199254740992.0) {
        emit(Opcode::CONST_I64, static_cast<int64_t>(resD));
    } else {
        emit(Opcode::CONST_F64, resD);
    }
    return true;
}

bool CodeGenerator::tryConstantFoldUnary(const UnaryExpr* x) {
    bool isInt;
    int64_t vI;
    double vD;
    getNumeric(x->operand.get(), &isInt, &vI, &vD);
    if (std::isnan(vD)) return false;
    if (x->op == TokenType::MINUS) {
        if (isInt && vI != INT64_MIN) { emit(Opcode::CONST_I64, -vI); return true; }
        emit(Opcode::CONST_F64, -vD);
        return true;
    }
    if (x->op == TokenType::NOT) {
        emit(vD != 0 && vD == vD ? Opcode::CONST_FALSE : Opcode::CONST_TRUE);
        return true;
    }
    return false;
}

void CodeGenerator::emitExpr(const Expr* e) {
    if (!e) return;
    currentLine_ = e->line;
    if (auto* x = dynamic_cast<const IntLiteral*>(e)) {
        emit(Opcode::CONST_I64, x->value);
        return;
    }
    if (auto* x = dynamic_cast<const FloatLiteral*>(e)) {
        emit(Opcode::CONST_F64, x->value);
        return;
    }
    if (auto* x = dynamic_cast<const StringLiteral*>(e)) {
        emit(Opcode::CONST_STR, addConstant(x->value));
        return;
    }
    if (auto* x = dynamic_cast<const BoolLiteral*>(e)) {
        emit(x->value ? Opcode::CONST_TRUE : Opcode::CONST_FALSE);
        return;
    }
    if (dynamic_cast<const NullLiteral*>(e)) {
        emit(Opcode::CONST_NULL);
        return;
    }
    if (auto* x = dynamic_cast<const Identifier*>(e)) {
        size_t local = resolveLocal(x->name);
        if (local != static_cast<size_t>(-1)) {
            emit(Opcode::LOAD, static_cast<int64_t>(local));
        } else {
            emit(Opcode::LOAD_GLOBAL, addConstant(x->name));
        }
        return;
    }
    if (auto* x = dynamic_cast<const BinaryExpr*>(e)) {
        if (tryConstantFoldBinary(x)) return;
        emitExpr(x->left.get());
        emitExpr(x->right.get());
        switch (x->op) {
            case TokenType::PLUS: emit(Opcode::ADD); break;
            case TokenType::MINUS: emit(Opcode::SUB); break;
            case TokenType::STAR: emit(Opcode::MUL); break;
            case TokenType::SLASH: emit(Opcode::DIV); break;
            case TokenType::PERCENT: emit(Opcode::MOD); break;
            case TokenType::STAR_STAR: emit(Opcode::POW); break;
            case TokenType::EQ: emit(Opcode::EQ); break;
            case TokenType::NEQ: emit(Opcode::NE); break;
            case TokenType::LT: emit(Opcode::LT); break;
            case TokenType::LE: emit(Opcode::LE); break;
            case TokenType::GT: emit(Opcode::GT); break;
            case TokenType::GE: emit(Opcode::GE); break;
            case TokenType::AND: emit(Opcode::AND); break;
            case TokenType::OR: emit(Opcode::OR); break;
            case TokenType::BIT_AND: emit(Opcode::BIT_AND); break;
            case TokenType::BIT_OR: emit(Opcode::BIT_OR); break;
            case TokenType::BIT_XOR: emit(Opcode::BIT_XOR); break;
            case TokenType::SHL: emit(Opcode::SHL); break;
            case TokenType::SHR: emit(Opcode::SHR); break;
            default: emit(Opcode::ADD); break;
        }
        return;
    }
    if (auto* x = dynamic_cast<const UnaryExpr*>(e)) {
        if (tryConstantFoldUnary(x)) return;
        emitExpr(x->operand.get());
        if (x->op == TokenType::MINUS) emit(Opcode::NEG);
        else if (x->op == TokenType::NOT) emit(Opcode::NOT);
        return;
    }
    if (auto* x = dynamic_cast<const CallExpr*>(e)) {
        std::vector<const Expr*> ordered;
        const Identifier* id = dynamic_cast<const Identifier*>(x->callee.get());
        const MemberExpr* mem = dynamic_cast<const MemberExpr*>(x->callee.get());
        bool hasNamed = false;
        for (const auto& a : x->args) if (!a.first.empty()) { hasNamed = true; break; }
        if (id && hasNamed) {
            auto it = functionParams_.find(id->name);
            if (it != functionParams_.end() && !it->second.empty()) {
                const std::vector<std::string>& pnames = it->second;
                ordered.resize(pnames.size(), nullptr);
                size_t pos = 0;
                for (const auto& a : x->args) {
                    if (a.first.empty()) {
                        if (pos < pnames.size()) ordered[pos++] = a.second.get();
                    } else {
                        for (size_t i = 0; i < pnames.size(); ++i)
                            if (pnames[i] == a.first) { ordered[i] = a.second.get(); break; }
                    }
                }
            }
        }
        if (mem) {
            emitExpr(mem->object.get());
            emit(Opcode::DUP);
            emit(Opcode::STORE_GLOBAL, addConstant("this"));
            emit(Opcode::GET_FIELD, addConstant(mem->member));
            for (const auto& a : x->args) emitExpr(a.second.get());
            emit(Opcode::CALL, static_cast<size_t>(x->args.size()));
        } else {
            emitExpr(x->callee.get());
            if (!ordered.empty()) {
                for (const Expr* arg : ordered) {
                    if (arg) emitExpr(arg);
                    else emit(Opcode::CONST_NULL);
                }
                emit(Opcode::CALL, static_cast<size_t>(ordered.size()));
            } else {
                for (const auto& a : x->args) emitExpr(a.second.get());
                emit(Opcode::CALL, static_cast<size_t>(x->args.size()));
            }
        }
        return;
    }
    if (auto* x = dynamic_cast<const MemberExpr*>(e)) {
        emitExpr(x->object.get());
        emit(Opcode::GET_FIELD, addConstant(x->member));
        return;
    }
    if (auto* x = dynamic_cast<const IndexExpr*>(e)) {
        emitExpr(x->object.get());
        emitExpr(x->index.get());
        emit(Opcode::GET_INDEX);
        return;
    }
    if (auto* x = dynamic_cast<const AssignExpr*>(e)) {
        if (auto* idx = dynamic_cast<IndexExpr*>(x->target.get())) {
            emitExpr(idx->object.get());
            emitExpr(idx->index.get());
            emitExpr(x->value.get());
            emit(Opcode::SET_INDEX);
            return;
        }
        if (auto* mem = dynamic_cast<MemberExpr*>(x->target.get())) {
            emitExpr(mem->object.get());
            emitExpr(x->value.get());
            emit(Opcode::SET_FIELD, addConstant(mem->member));
            return;
        }
        emitExpr(x->value.get());
        bool leaveOnStack = dynamic_cast<Identifier*>(x->target.get()) != nullptr;
        if (leaveOnStack) emit(Opcode::DUP);  // chained assignment: a = b = c = 0
        if (auto* id = dynamic_cast<Identifier*>(x->target.get())) {
            size_t local = resolveLocal(id->name);
            if (local != static_cast<size_t>(-1))
                emit(Opcode::STORE, static_cast<int64_t>(local));
            else
                emit(Opcode::STORE_GLOBAL, addConstant(id->name));
        }
        return;
    }
    if (auto* x = dynamic_cast<const CoalesceExpr*>(e)) {
        emitExpr(x->left.get());
        emit(Opcode::DUP);
        emit(Opcode::CONST_NULL);
        emit(Opcode::EQ);  // stack: [left, eq]; JMP_IF_FALSE pops eq
        size_t skipRight = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emit(Opcode::POP);  // drop left (was null)
        emitExpr(x->right.get());
        emit(Opcode::NOP);  // jump target when left != null (keep [left] on stack)
        patchJump(skipRight, code_.size());
        return;
    }
    if (auto* x = dynamic_cast<const PipelineExpr*>(e)) {
        // vM CALL pops args first (top of stack), then callee. So we need [arg, callee] = [left, right].
        emitExpr(x->right.get());   // callee (will be popped last)
        emitExpr(x->left.get());    // single argument (popped first)
        emit(Opcode::CALL, static_cast<size_t>(1));
        return;
    }
    if (auto* x = dynamic_cast<const ArrayComprehensionExpr*>(e)) {
        const std::string accGlobal = "__comp_acc";
        beginScope();
        declareLocal("__iter");
        declareLocal("__tmp");
        declareLocal(x->varName);
        size_t iterSlot = scopes_.back()["__iter"];
        size_t tmpSlot = scopes_.back()["__tmp"];
        size_t varSlot = scopes_.back()[x->varName];
        emitExpr(x->iterExpr.get());
        emit(Opcode::STORE, static_cast<int64_t>(iterSlot));
        emit(Opcode::BUILD_ARRAY, static_cast<size_t>(0));
        emit(Opcode::STORE_GLOBAL, addConstant(accGlobal));
        emit(Opcode::LOAD, static_cast<int64_t>(iterSlot));
        emit(Opcode::FOR_IN_ITER);
        size_t loopStart = code_.size();
        emit(Opcode::FOR_IN_NEXT, varSlot);
        size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        if (x->filterExpr) {
            emitExpr(x->filterExpr.get());
            size_t skipToNext = emit(Opcode::JMP_IF_FALSE, size_t(0));
            emitExpr(x->bodyExpr.get());
            emit(Opcode::STORE, static_cast<int64_t>(tmpSlot));
            emit(Opcode::LOAD_GLOBAL, addConstant(accGlobal));
            emit(Opcode::LOAD, static_cast<int64_t>(tmpSlot));
            emit(Opcode::LOAD_GLOBAL, addConstant("push"));
            emit(Opcode::CALL, static_cast<size_t>(2));
            emit(Opcode::POP);
            patchJump(skipToNext, code_.size());
        } else {
            emitExpr(x->bodyExpr.get());
            emit(Opcode::STORE, static_cast<int64_t>(tmpSlot));
            emit(Opcode::LOAD_GLOBAL, addConstant(accGlobal));
            emit(Opcode::LOAD, static_cast<int64_t>(tmpSlot));
            emit(Opcode::LOAD_GLOBAL, addConstant("push"));
            emit(Opcode::CALL, static_cast<size_t>(2));
            emit(Opcode::POP);
        }
        emit(Opcode::JMP, loopStart);
        patchJump(exitJump, code_.size());
        emit(Opcode::LOAD_GLOBAL, addConstant(accGlobal));
        endScope();
        return;
    }
    if (auto* x = dynamic_cast<const TernaryExpr*>(e)) {
        emitExpr(x->condition.get());
        size_t skipElse = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emitExpr(x->thenExpr.get());
        size_t skipEnd = emit(Opcode::JMP, size_t(0));
        patchJump(skipElse, code_.size());
        emitExpr(x->elseExpr.get());
        patchJump(skipEnd, code_.size());
        return;
    }
    if (auto* lam = dynamic_cast<const LambdaExpr*>(e)) {
        size_t skipBody = emit(Opcode::JMP, size_t(0));
        size_t entry = code_.size();
        beginScope();
        for (const auto& p : lam->params) declareLocal(p);
        // single expression body: leave value on stack for RETURN (don't POP)
        if (auto* es = dynamic_cast<const ExprStmt*>(lam->body.get())) {
            emitExpr(es->expr.get());
        } else {
            emitStmt(lam->body.get());
        }
        emit(Opcode::RETURN);
        endScope();
        patchJump(skipBody, code_.size());
        emit(Opcode::BUILD_FUNC, entry);
        emit(Opcode::SET_FUNC_ARITY, static_cast<size_t>(lam->params.size()));
        emit(Opcode::SET_FUNC_NAME, addConstant("<lambda>"));
        return;
    }
    if (auto* x = dynamic_cast<const ArrayLiteral*>(e)) {
        bool hasSpread = false;
        for (const auto& el : x->elements) { if (dynamic_cast<const SpreadExpr*>(el.get())) { hasSpread = true; break; } }
        if (!hasSpread) {
            for (const auto& el : x->elements) emitExpr(el.get());
            emit(Opcode::BUILD_ARRAY, static_cast<size_t>(x->elements.size()));
        } else {
            emit(Opcode::BUILD_ARRAY, static_cast<size_t>(0));
            for (const auto& el : x->elements) {
                emit(Opcode::DUP);
                if (auto* sp = dynamic_cast<const SpreadExpr*>(el.get())) {
                    emitExpr(sp->target.get());
                    emit(Opcode::SPREAD);
                } else {
                    emitExpr(el.get());
                    emit(Opcode::BUILD_ARRAY, static_cast<size_t>(1));
                    emit(Opcode::SPREAD);
                }
            }
        }
        return;
    }
    if (auto* x = dynamic_cast<const RangeExpr*>(e)) {
        emit(Opcode::LOAD_GLOBAL, addConstant("range"));
        emitExpr(x->start.get());
        emitExpr(x->end.get());
        if (x->step) emitExpr(x->step.get()); else emit(Opcode::CONST_I64, static_cast<int64_t>(1));
        emit(Opcode::CALL, static_cast<size_t>(3));
        return;
    }
    if (auto* x = dynamic_cast<const DurationExpr*>(e)) {
        emitExpr(x->amount.get());
        double mult = 1.0;
        if (x->unit == "ms") mult = 0.001;
        else if (x->unit == "s") mult = 1.0;
        else if (x->unit == "m") mult = 60.0;
        else if (x->unit == "h") mult = 3600.0;
        emit(Opcode::CONST_F64, mult);
        emit(Opcode::MUL);
        return;
    }
    if (auto* x = dynamic_cast<const MapLiteral*>(e)) {
        emit(Opcode::NEW_OBJECT);
        for (const auto& kv : x->entries) {
            const auto* keyLit = dynamic_cast<const StringLiteral*>(kv.first.get());
            std::string keyStr = keyLit ? keyLit->value : (kv.first ? "?" : "");
            if (!keyLit && kv.first) { emitExpr(kv.first.get()); emit(Opcode::POP); continue; }
            emit(Opcode::DUP);
            emitExpr(kv.second.get());
            emit(Opcode::SET_FIELD, addConstant(keyStr));
            emit(Opcode::POP);  // leave only the map on stack, not the stored value
        }
        return;
    }
    if (auto* x = dynamic_cast<const SliceExpr*>(e)) {
        emitExpr(x->object.get());
        if (x->start) emitExpr(x->start.get()); else emit(Opcode::CONST_NULL);
        if (x->end) emitExpr(x->end.get()); else emit(Opcode::CONST_NULL);
        if (x->step) emitExpr(x->step.get()); else emit(Opcode::CONST_NULL);
        emit(Opcode::SLICE);
        return;
    }
    if (auto* x = dynamic_cast<const FStringExpr*>(e)) {
        if (x->parts.empty()) {
            emit(Opcode::CONST_STR, addConstant(""));
            return;
        }
        bool first = true;
        for (const auto& part : x->parts) {
            if (std::holds_alternative<std::string>(part)) {
                emit(Opcode::CONST_STR, addConstant(std::get<std::string>(part)));
            } else {
                emitExpr(std::get<ExprPtr>(part).get());
            }
            if (!first) emit(Opcode::ADD);  // string concat
            first = false;
        }
        return;
    }
    if (auto* x = dynamic_cast<const OptionalChainExpr*>(e)) {
        emitExpr(x->object.get());
        emit(Opcode::DUP);
        emit(Opcode::CONST_NULL);
        emit(Opcode::EQ);
        size_t skipJump = emit(Opcode::JMP_IF_FALSE, size_t(0));  // when obj==null jump; else stack [obj]
        size_t endJump = emit(Opcode::JMP, size_t(0));
        patchJump(skipJump, code_.size());  // null path: land here, GET_FIELD(null) -> nil
        emit(Opcode::GET_FIELD, addConstant(x->member));
        patchJump(endJump, code_.size());
        return;
    }
    if (auto* x = dynamic_cast<const OptionalIndexExpr*>(e)) {
        emitExpr(x->object.get());
        emit(Opcode::DUP);
        emit(Opcode::CONST_NULL);
        emit(Opcode::EQ);
        size_t nonNullPath = emit(Opcode::JMP_IF_FALSE, size_t(0));  // jump when obj != null
        emit(Opcode::POP);
        emit(Opcode::CONST_NULL);
        size_t endJump = emit(Opcode::JMP, size_t(0));
        patchJump(nonNullPath, code_.size());
        emitExpr(x->index.get());
        emit(Opcode::GET_INDEX);
        patchJump(endJump, code_.size());
        return;
    }
    emit(Opcode::CONST_NULL);
}

void CodeGenerator::emitStmt(const Stmt* s) {
    if (!s) return;
    currentLine_ = s->line;
    if (auto* x = dynamic_cast<const ExprStmt*>(s)) {
        emitExpr(x->expr.get());
        emit(Opcode::POP);
        return;
    }
    if (auto* x = dynamic_cast<const VarDeclStmt*>(s)) {
        if (x->initializer) emitExpr(x->initializer.get());
        else emit(Opcode::CONST_NULL);
        if (scopes_.size() == 1) {
            globals_[x->name] = 0;
            emit(Opcode::STORE_GLOBAL, addConstant(x->name));
        } else {
            declareLocal(x->name);
            emit(Opcode::STORE, static_cast<int64_t>(scopes_.back()[x->name]));
        }
        return;
    }
    if (auto* x = dynamic_cast<const DestructureStmt*>(s)) {
        if (x->initializer) emitExpr(x->initializer.get());
        else emit(Opcode::CONST_NULL);
        for (size_t i = 0; i < x->names.size(); ++i) {
            emit(Opcode::DUP);
            if (x->isArray) {
                emit(Opcode::CONST_I64, static_cast<int64_t>(i));
                emit(Opcode::GET_INDEX);
            } else {
                emit(Opcode::GET_FIELD, addConstant(x->names[i]));
            }
            if (scopes_.size() == 1) {
                globals_[x->names[i]] = 0;
                emit(Opcode::STORE_GLOBAL, addConstant(x->names[i]));
            } else {
                declareLocal(x->names[i]);
                emit(Opcode::STORE, static_cast<int64_t>(scopes_.back()[x->names[i]]));
            }
        }
        emit(Opcode::POP);
        return;
    }
    if (auto* x = dynamic_cast<const BlockStmt*>(s)) {
        beginScope();
        for (const auto& st : x->statements) emitStmt(st.get());
        endScope();
        return;
    }
    if (auto* x = dynamic_cast<const IfStmt*>(s)) {
        emitExpr(x->condition.get());
        size_t elseJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emitStmt(x->thenBranch.get());
        size_t endJump = emit(Opcode::JMP, size_t(0));
        patchJump(elseJump, code_.size());
        for (const auto& el : x->elifBranches) {
            emitExpr(el.first.get());
            size_t nextElse = emit(Opcode::JMP_IF_FALSE, size_t(0));
            emitStmt(el.second.get());
            endJump = emit(Opcode::JMP, size_t(0));
            patchJump(nextElse, code_.size());
        }
        if (x->elseBranch) emitStmt(x->elseBranch.get());
        patchJump(endJump, code_.size());
        return;
    }
    if (auto* x = dynamic_cast<const ForRangeStmt*>(s)) {
        beginScope();
        declareLocal(x->varName);
        size_t slot = scopes_.back()[x->varName];
        breakPatches_.emplace_back();
        continuePatches_.emplace_back();
        loopLabels_.push_back(x->label);
        loopStartStack_.push_back(0);
        loopEndStack_.push_back(0);
        emitExpr(x->start.get());
        emit(Opcode::STORE, static_cast<int64_t>(slot));
        size_t loopStart = code_.size();
        loopStartStack_.back() = loopStart;
        emit(Opcode::LOAD, static_cast<int64_t>(slot));
        emitExpr(x->end.get());
        emit(Opcode::LT);
        size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emitStmt(x->body.get());
        emit(Opcode::LOAD, static_cast<int64_t>(slot));
        emitExpr(x->step.get());
        emit(Opcode::ADD);
        emit(Opcode::STORE, static_cast<int64_t>(slot));
        emit(Opcode::JMP, loopStart);
        for (size_t at : breakPatches_.back()) patchJump(at, code_.size());
        patchJump(exitJump, code_.size());
        breakPatches_.pop_back();
        continuePatches_.pop_back();
        loopStartStack_.pop_back();
        loopEndStack_.pop_back();
        endScope();
        return;
    }
    if (auto* x = dynamic_cast<const ForCStyleStmt*>(s)) {
        beginScope();
        breakPatches_.emplace_back();
        continuePatches_.emplace_back();
        loopLabels_.push_back(x->label);
        loopStartStack_.push_back(0);
        loopEndStack_.push_back(0);
        if (x->init) emitStmt(x->init.get());
        size_t loopStart = code_.size();
        loopStartStack_.back() = loopStart;
        if (x->condition) {
            emitExpr(x->condition.get());
            size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
            emitStmt(x->body.get());
            if (x->update) { emitExpr(x->update.get()); emit(Opcode::POP); }
            emit(Opcode::JMP, loopStart);
            for (size_t at : breakPatches_.back()) patchJump(at, code_.size());
            patchJump(exitJump, code_.size());
        } else {
            emitStmt(x->body.get());
            if (x->update) { emitExpr(x->update.get()); emit(Opcode::POP); }
            emit(Opcode::JMP, loopStart);
            for (size_t at : breakPatches_.back()) patchJump(at, code_.size());
        }
        breakPatches_.pop_back();
        continuePatches_.pop_back();
        loopLabels_.pop_back();
        loopStartStack_.pop_back();
        loopEndStack_.pop_back();
        endScope();
        return;
    }
    if (auto* x = dynamic_cast<const WhileStmt*>(s)) {
        breakPatches_.emplace_back();
        continuePatches_.emplace_back();
        loopLabels_.push_back(x->label);
        loopStartStack_.push_back(0);
        loopEndStack_.push_back(0);
        size_t loopStart = code_.size();
        loopStartStack_.back() = loopStart;
        emitExpr(x->condition.get());
        size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emitStmt(x->body.get());
        emit(Opcode::JMP, loopStart);
        for (size_t at : breakPatches_.back()) patchJump(at, code_.size());
        patchJump(exitJump, code_.size());
        breakPatches_.pop_back();
        continuePatches_.pop_back();
        loopLabels_.pop_back();
        loopStartStack_.pop_back();
        loopEndStack_.pop_back();
        return;
    }
    if (auto* x = dynamic_cast<const RepeatStmt*>(s)) {
        beginScope();
        declareLocal("__repeat_n");
        size_t slot = scopes_.back()["__repeat_n"];
        emitExpr(x->count.get());
        emit(Opcode::STORE, static_cast<int64_t>(slot));
        size_t loopStart = code_.size();
        emit(Opcode::LOAD, static_cast<int64_t>(slot));
        emit(Opcode::CONST_I64, static_cast<int64_t>(0));
        emit(Opcode::GT);
        size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emitStmt(x->body.get());
        emit(Opcode::LOAD, static_cast<int64_t>(slot));
        emit(Opcode::CONST_I64, static_cast<int64_t>(1));
        emit(Opcode::SUB);
        emit(Opcode::STORE, static_cast<int64_t>(slot));
        emit(Opcode::JMP, loopStart);
        patchJump(exitJump, code_.size());
        endScope();
        return;
    }
    if (auto* x = dynamic_cast<const DeferStmt*>(s)) {
        const auto* call = dynamic_cast<const CallExpr*>(x->expr.get());
        if (call) {
            emitExpr(call->callee.get());
            for (const auto& a : call->args) emitExpr(a.second.get());
            emit(Opcode::DEFER, static_cast<size_t>(call->args.size() + 1));
        } else {
            emitExpr(x->expr.get());
            emit(Opcode::POP);
        }
        return;
    }
    if (auto* x = dynamic_cast<const TryStmt*>(s)) {
        size_t tryBegin = emit(Opcode::TRY_BEGIN, size_t(0));
        emitStmt(x->tryBlock.get());
        emit(Opcode::TRY_END);
        if (x->elseBlock) emitStmt(x->elseBlock.get());
        size_t jmpOverCatch = emit(Opcode::JMP, size_t(0));
        size_t catchEntry = code_.size();
        patchJump(tryBegin, catchEntry);
        if (x->catchBlock) {
            beginScope();
            size_t typeFilterJmp = 0;
            if (!x->catchVar.empty()) {
                declareLocal(x->catchVar);
                emit(Opcode::STORE, static_cast<int64_t>(scopes_.back()[x->catchVar]));
                if (!x->catchTypeName.empty()) {
                    emit(Opcode::LOAD_GLOBAL, addConstant("error_name"));
                    emit(Opcode::LOAD, static_cast<int64_t>(scopes_.back()[x->catchVar]));
                    emit(Opcode::CALL, static_cast<size_t>(1));
                    emit(Opcode::CONST_STR, addConstant(x->catchTypeName));
                    emit(Opcode::EQ);
                    typeFilterJmp = emit(Opcode::JMP_IF_FALSE, size_t(0));
                }
            } else {
                emit(Opcode::POP);
            }
            emitStmt(x->catchBlock.get());
            size_t jmpAfterCatch = emit(Opcode::JMP, size_t(0));
            if (!x->catchTypeName.empty() && !x->catchVar.empty()) {
                patchJump(typeFilterJmp, code_.size());
                emit(Opcode::RETHROW);
            }
            patchJump(jmpAfterCatch, code_.size());
            endScope();
        } else emit(Opcode::POP);
        patchJump(jmpOverCatch, code_.size());
        if (x->finallyBlock) emitStmt(x->finallyBlock.get());
        return;
    }
    if (dynamic_cast<const RethrowStmt*>(s)) {
        emit(Opcode::RETHROW);
        return;
    }
    if (auto* x = dynamic_cast<const ThrowStmt*>(s)) {
        if (x->value) emitExpr(x->value.get()); else emit(Opcode::CONST_NULL);
        emit(Opcode::THROW);
        return;
    }
    if (auto* x = dynamic_cast<const AssertStmt*>(s)) {
        emitExpr(x->condition.get());
        size_t skip = emit(Opcode::JMP_IF_TRUE, size_t(0));
        if (x->message) emitExpr(x->message.get());
        else emit(Opcode::CONST_STR, addConstant("Assertion failed"));
        emit(Opcode::THROW);
        patchJump(skip, code_.size());
        return;
    }
    if (auto* x = dynamic_cast<const MatchStmt*>(s)) {
        emitExpr(x->value.get());
        size_t endMatch = emit(Opcode::JMP, size_t(0));
        std::vector<size_t> nextCasePatches;
        for (size_t i = 0; i < x->cases.size(); ++i) {
            for (size_t p : nextCasePatches) patchJump(p, code_.size());
            nextCasePatches.clear();
            const auto& c = x->cases[i];
            if (c.isDefault) {
                emit(Opcode::POP);
                emitStmt(c.body.get());
                break;
            }
            const auto* objPattern = dynamic_cast<const MapLiteral*>(c.pattern.get());
            if (objPattern) {
                // destructuring match: { key: literal_or_bind, ... }
                emit(Opcode::DUP);
                beginScope();
                for (const auto& kv : objPattern->entries) {
                    const auto* idVal = dynamic_cast<const Identifier*>(kv.second.get());
                    if (idVal) declareLocal(idVal->name);
                }
                for (const auto& kv : objPattern->entries) {
                    std::string keyStr;
                    if (const auto* idKey = dynamic_cast<const Identifier*>(kv.first.get())) keyStr = idKey->name;
                    else if (const auto* strKey = dynamic_cast<const StringLiteral*>(kv.first.get())) keyStr = strKey->value;
                    else continue;
                    emit(Opcode::DUP);
                    emit(Opcode::GET_FIELD, addConstant(keyStr));
                    if (const auto* idVal = dynamic_cast<const Identifier*>(kv.second.get())) {
                        size_t slot = scopes_.back()[idVal->name];
                        emit(Opcode::STORE, static_cast<int64_t>(slot));
                    } else {
                        emitExpr(kv.second.get());
                        emit(Opcode::EQ);
                        size_t nextCase = emit(Opcode::JMP_IF_FALSE, size_t(0));
                        emit(Opcode::POP);
                        emit(Opcode::POP);  // drop duplicated object
                        nextCasePatches.push_back(nextCase);
                        endScope();
                        goto next_case;
                    }
                }
                emit(Opcode::POP);
                if (c.guard) {
                    emitExpr(c.guard.get());
                    nextCasePatches.push_back(emit(Opcode::JMP_IF_FALSE, size_t(0)));
                    emit(Opcode::POP);
                }
                emitStmt(c.body.get());
                emit(Opcode::JMP, endMatch);
                endScope();
            next_case:
                continue;
            }
            emit(Opcode::DUP);
            emitExpr(c.pattern.get());
            emit(Opcode::EQ);
            nextCasePatches.push_back(emit(Opcode::JMP_IF_FALSE, size_t(0)));
            emit(Opcode::POP);
            if (c.guard) {
                emitExpr(c.guard.get());
                nextCasePatches.push_back(emit(Opcode::JMP_IF_FALSE, size_t(0)));
                emit(Opcode::POP);
            }
            emitStmt(c.body.get());
            emit(Opcode::JMP, endMatch);
        }
        for (size_t p : nextCasePatches) patchJump(p, code_.size());
        patchJump(endMatch, code_.size());
        return;
    }
    if (auto* x = dynamic_cast<const ForInStmt*>(s)) {
        emitExpr(x->iterable.get());
        emit(Opcode::FOR_IN_ITER);
        beginScope();
        declareLocal(x->varName);
        size_t keySlot = scopes_.back()[x->varName];
        size_t valueSlot = static_cast<size_t>(-1);
        if (!x->valueVarName.empty()) {
            declareLocal(x->valueVarName);
            valueSlot = scopes_.back()[x->valueVarName];
        }
        breakPatches_.emplace_back();
        continuePatches_.emplace_back();
        loopLabels_.push_back(x->label);
        size_t loopStart = code_.size();
        loopStartStack_.push_back(loopStart);
        loopEndStack_.push_back(0);
        emit(Opcode::FOR_IN_NEXT, keySlot, valueSlot);
        size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emitStmt(x->body.get());
        emit(Opcode::JMP, loopStart);
        for (size_t at : breakPatches_.back()) patchJump(at, code_.size());
        patchJump(exitJump, code_.size());
        breakPatches_.pop_back();
        continuePatches_.pop_back();
        loopLabels_.pop_back();
        loopStartStack_.pop_back();
        loopEndStack_.pop_back();
        endScope();
        return;
    }
    if (dynamic_cast<const ReturnStmt*>(s)) {
        auto* r = static_cast<const ReturnStmt*>(s);
        if (r->values.empty()) {
            emit(Opcode::CONST_NULL);
        } else if (r->values.size() == 1) {
            emitExpr(r->values[0].get());
        } else {
            for (const auto& v : r->values) emitExpr(v.get());
            emit(Opcode::BUILD_ARRAY, static_cast<size_t>(r->values.size()));
        }
        emit(Opcode::RETURN);
        return;
    }
    if (auto* x = dynamic_cast<const BreakStmt*>(s)) {
        if (breakPatches_.empty()) return;
        size_t k = breakPatches_.size() - 1;
        if (!x->label.empty()) {
            for (size_t i = loopLabels_.size(); i > 0; --i) {
                if (loopLabels_[i - 1] == x->label) { k = i - 1; break; }
            }
        }
        breakPatches_[k].push_back(emit(Opcode::JMP, size_t(0)));
        return;
    }
    if (auto* x = dynamic_cast<const ContinueStmt*>(s)) {
        if (continuePatches_.empty() || loopStartStack_.empty()) return;
        size_t k = continuePatches_.size() - 1;
        if (!x->label.empty()) {
            for (size_t i = loopLabels_.size(); i > 0; --i) {
                if (loopLabels_[i - 1] == x->label) { k = i - 1; break; }
            }
        }
        continuePatches_[k].push_back(emit(Opcode::JMP, loopStartStack_[k]));
        return;
    }
    if (auto* x = dynamic_cast<const FunctionDeclStmt*>(s)) {
        size_t skipBody = emit(Opcode::JMP, size_t(0));
        size_t entry = code_.size();
        beginScope();
        for (const auto& p : x->params) declareLocal(p.name);
        for (size_t i = 0; i < x->params.size(); ++i) {
            const auto& p = x->params[i];
            if (p.defaultExpr) {
                size_t slot = scopes_.back()[p.name];
                emit(Opcode::LOAD, static_cast<int64_t>(slot));
                emit(Opcode::CONST_NULL);
                emit(Opcode::EQ);
                size_t skip = emit(Opcode::JMP_IF_FALSE, size_t(0));
                emitExpr(p.defaultExpr.get());
                emit(Opcode::STORE, static_cast<int64_t>(slot));  // sTORE pops value
                size_t endDefault = emit(Opcode::JMP, size_t(0));
                emit(Opcode::POP);  // skip path: drop the loaded param value (must emit before patching skip so jump lands on POP)
                patchJump(skip, code_.size());  // skip -> POP (VM does ip_ = target-1, so target = address of POP)
                emit(Opcode::NOP);  // endDefault lands here
                patchJump(endDefault, code_.size());
            }
        }
        emitStmt(x->body.get());
        emit(Opcode::RETURN);
        endScope();
        patchJump(skipBody, code_.size());
        emit(Opcode::BUILD_FUNC, entry);
        emit(Opcode::SET_FUNC_ARITY, static_cast<size_t>(x->params.size()));
        emit(Opcode::SET_FUNC_NAME, addConstant(x->name));
        std::vector<std::string> pnames;
        for (const auto& p : x->params) if (p.name != "...") pnames.push_back(p.name);
        if (!pnames.empty()) {
            std::string joined;
            for (size_t i = 0; i < pnames.size(); ++i) joined += (i ? "," : "") + pnames[i];
            emit(Opcode::SET_FUNC_PARAM_NAMES, addConstant(joined));
        }
        functionParams_[x->name] = std::move(pnames);
        if (scopes_.size() == 1)
            emit(Opcode::STORE_GLOBAL, addConstant(x->name));
        else {
            declareLocal(x->name);
            emit(Opcode::STORE, static_cast<int64_t>(scopes_.back()[x->name]));
        }
        return;
    }
    if (auto* x = dynamic_cast<const ClassDeclStmt*>(s)) {
        emit(Opcode::NEW_OBJECT);
        for (const auto& stmt : x->methods) {
            const FunctionDeclStmt* method = dynamic_cast<const FunctionDeclStmt*>(stmt.get());
            if (!method) continue;
            emit(Opcode::DUP);
            emitFunctionOntoStack(method);
            emit(Opcode::SET_FIELD, addConstant(method->name));
            emit(Opcode::POP);
        }
        emit(Opcode::STORE_GLOBAL, addConstant(x->name));
        return;
    }
    if (auto* x = dynamic_cast<const ImportStmt*>(s)) {
        std::string path = x->moduleName;
        if (path.find('.') == std::string::npos) path += ".kn";
        std::string bindingName = x->hasAlias ? x->alias : x->moduleName;
        if (!x->hasAlias && bindingName.size() >= 4 && bindingName.compare(bindingName.size() - 4, 4, ".kn") == 0)
            bindingName = bindingName.substr(0, bindingName.size() - 4);
        emit(Opcode::LOAD_GLOBAL, addConstant("__import"));
        emit(Opcode::CONST_STR, addConstant(path));
        emit(Opcode::CALL, static_cast<size_t>(1));
        emit(Opcode::STORE_GLOBAL, addConstant(bindingName));
        return;
    }
}

void CodeGenerator::emitProgram(const Program* p) {
    for (const auto& st : p->statements) emitStmt(st.get());
}

void CodeGenerator::emitFunctionOntoStack(const FunctionDeclStmt* x) {
    size_t skipBody = emit(Opcode::JMP, size_t(0));
    size_t entry = code_.size();
    beginScope();
    for (const auto& p : x->params) declareLocal(p.name);
    for (size_t i = 0; i < x->params.size(); ++i) {
        const auto& p = x->params[i];
        if (p.defaultExpr) {
            size_t slot = scopes_.back()[p.name];
            emit(Opcode::LOAD, static_cast<int64_t>(slot));
            emit(Opcode::CONST_NULL);
            emit(Opcode::EQ);
            size_t skip = emit(Opcode::JMP_IF_FALSE, size_t(0));
            emitExpr(p.defaultExpr.get());
            emit(Opcode::STORE, static_cast<int64_t>(slot));
            size_t endDefault = emit(Opcode::JMP, size_t(0));
            emit(Opcode::POP);
            patchJump(skip, code_.size());
            emit(Opcode::NOP);
            patchJump(endDefault, code_.size());
        }
    }
    emitStmt(x->body.get());
    emit(Opcode::RETURN);
    endScope();
    patchJump(skipBody, code_.size());
    emit(Opcode::BUILD_FUNC, entry);
    emit(Opcode::SET_FUNC_ARITY, static_cast<size_t>(x->params.size()));
    emit(Opcode::SET_FUNC_NAME, addConstant(x->name));
    std::vector<std::string> pnames;
    for (const auto& p : x->params) if (p.name != "...") pnames.push_back(p.name);
    if (!pnames.empty()) {
        std::string joined;
        for (size_t i = 0; i < pnames.size(); ++i) joined += (i ? "," : "") + pnames[i];
        emit(Opcode::SET_FUNC_PARAM_NAMES, addConstant(joined));
    }
    functionParams_[x->name] = std::move(pnames);
}

// peephole: remove NOPs and remap jump targets for smaller, faster bytecode.
static void peepholeOptimize(Bytecode& code) {
    if (code.empty()) return;
    std::vector<size_t> oldToNew(code.size(), static_cast<size_t>(-1));
    Bytecode out;
    out.reserve(code.size());
    for (size_t i = 0; i < code.size(); ++i) {
        if (code[i].op == Opcode::NOP) continue;
        oldToNew[i] = out.size();
        out.push_back(std::move(code[i]));
    }
    if (out.size() == code.size()) return;  // no NOPs removed
    code = std::move(out);
    const auto isJumpTargetOp = [](Opcode op) {
        return op == Opcode::JMP || op == Opcode::JMP_IF_FALSE || op == Opcode::JMP_IF_TRUE || op == Opcode::TRY_BEGIN;
    };
    for (size_t i = 0; i < code.size(); ++i) {
        Instruction& inst = code[i];
        if (inst.operand.index() != 4) continue;
        size_t target = std::get<size_t>(inst.operand);
        if (inst.op == Opcode::BUILD_FUNC) {
            if (target < oldToNew.size() && oldToNew[target] != static_cast<size_t>(-1))
                inst.operand = oldToNew[target] + 1;  // vM expects 1-based (ip_ = entry - 1)
            continue;
        }
        if (target >= oldToNew.size() || oldToNew[target] == static_cast<size_t>(-1)) continue;
        if (isJumpTargetOp(inst.op))
            inst.operand = oldToNew[target] + 1;
    }
}

Bytecode CodeGenerator::generate(std::unique_ptr<Program> program) {
    code_.clear();
    emitProgram(program.get());
    emit(Opcode::HALT);
    // peephole disabled: NOP removal + remap can cause stack underflow (e.g. after for-loop in smoke test).
    // to re-enable: fix BUILD_FUNC and jump target remap so 00_full_feature_smoke.kn and default-arg
    // examples pass with NOP removal; then uncomment below.
    // peepholeOptimize(code_);
    return std::move(code_);
}

} // namespace kern
