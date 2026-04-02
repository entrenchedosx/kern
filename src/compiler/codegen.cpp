/* *
 * kern Code Generator Implementation
 */

#include "codegen.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <variant>

namespace kern {

static bool stmtHasYield(const Stmt* s) {
    if (!s) return false;
    if (dynamic_cast<const YieldStmt*>(s)) return true;
    if (auto* b = dynamic_cast<const BlockStmt*>(s)) {
        for (const auto& st : b->statements)
            if (stmtHasYield(st.get())) return true;
        return false;
    }
    if (auto* x = dynamic_cast<const IfStmt*>(s)) {
        if (stmtHasYield(x->thenBranch.get())) return true;
        if (x->elseBranch && stmtHasYield(x->elseBranch.get())) return true;
        for (const auto& e : x->elifBranches)
            if (stmtHasYield(e.second.get())) return true;
        return false;
    }
    if (auto* x = dynamic_cast<const WhileStmt*>(s)) return stmtHasYield(x->body.get());
    if (auto* x = dynamic_cast<const RepeatStmt*>(s)) return stmtHasYield(x->body.get());
    if (auto* x = dynamic_cast<const RepeatWhileStmt*>(s)) return stmtHasYield(x->body.get());
    if (auto* x = dynamic_cast<const ForRangeStmt*>(s)) return stmtHasYield(x->body.get());
    if (auto* x = dynamic_cast<const ForInStmt*>(s)) return stmtHasYield(x->body.get());
    if (auto* x = dynamic_cast<const ForCStyleStmt*>(s))
        return stmtHasYield(x->init.get()) || stmtHasYield(x->body.get());
    if (auto* x = dynamic_cast<const TryStmt*>(s)) {
        if (stmtHasYield(x->tryBlock.get())) return true;
        if (x->catchBlock && stmtHasYield(x->catchBlock.get())) return true;
        if (x->elseBlock && stmtHasYield(x->elseBlock.get())) return true;
        if (x->finallyBlock && stmtHasYield(x->finallyBlock.get())) return true;
        return false;
    }
    if (auto* x = dynamic_cast<const MatchStmt*>(s)) {
        for (const auto& c : x->cases)
            if (stmtHasYield(c.body.get())) return true;
        return false;
    }
    if (auto* x = dynamic_cast<const FunctionDeclStmt*>(s)) return stmtHasYield(x->body.get());
    if (auto* x = dynamic_cast<const UnsafeBlockStmt*>(s)) return stmtHasYield(x->body.get());
    return false;
}

static int64_t approxTypeSize(const std::string& typeName) {
    if (typeName == "bool") return 1;
    if (typeName == "char") return 1;
    if (typeName == "u8" || typeName == "i8") return 1;
    if (typeName == "u16" || typeName == "i16" || typeName == "short") return 2;
    if (typeName == "u32" || typeName == "i32" || typeName == "f32") return 4;
    if (typeName == "int") return 8;
    if (typeName == "long") return 8;
    if (typeName == "u64" || typeName == "i64" || typeName == "usize" || typeName == "isize") return 8;
    if (typeName == "float") return 8;
    if (typeName == "double") return 8;
    if (typeName == "ptr" || typeName == "ref") return 8;
    if (!typeName.empty() && typeName[0] == '*') return 8;
    return 8;
}

static int64_t approxTypeAlign(const std::string& typeName) {
    if (typeName == "bool") return 1;
    if (typeName == "char") return 1;
    if (typeName == "i16" || typeName == "u16" || typeName == "short") return 2;
    if (typeName == "i32" || typeName == "u32" || typeName == "f32" || typeName == "int") return 4;
    return 8;
}

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

int CodeGenerator::findDefiningScopeIndex(const std::string& name) const {
    for (int i = static_cast<int>(scopes_.size()) - 1; i >= 0; --i) {
        if (scopes_[static_cast<size_t>(i)].count(name)) return i;
    }
    return -1;
}

bool CodeGenerator::tryResolveLocalSlot(const std::string& name, int64_t* outSlot) {
    if (!lambdaCtxStack_.empty()) {
        const auto& L = lambdaCtxStack_.back();
        int d = findDefiningScopeIndex(name);
        if (d >= 0 && static_cast<size_t>(d) < L.captureBoundary) {
            auto it = L.captureIndex.find(name);
            if (it == L.captureIndex.end())
                throw CodegenError("Internal: missing lambda capture index for '" + name + "'", currentLine_);
            *outSlot = static_cast<int64_t>(L.arity + it->second);
            return true;
        }
        if (d >= 0) {
            *outSlot = static_cast<int64_t>(scopes_[static_cast<size_t>(d)].at(name));
            return true;
        }
        return false;
    }
    size_t local = resolveLocal(name);
    if (local == static_cast<size_t>(-1)) return false;
    *outSlot = static_cast<int64_t>(local);
    return true;
}

void CodeGenerator::scanAssignTargetForCaptures(const Expr* t, size_t boundary, std::unordered_set<std::string>& seen,
                                                std::vector<std::string>& order) {
    if (!t) return;
    if (auto* id = dynamic_cast<const Identifier*>(t)) {
        int d = findDefiningScopeIndex(id->name);
        if (d >= 0 && static_cast<size_t>(d) < boundary) {
            if (seen.insert(id->name).second) order.push_back(id->name);
        }
        return;
    }
    if (auto* idx = dynamic_cast<const IndexExpr*>(t)) {
        scanLambdaCapturesExpr(idx->object.get(), boundary, seen, order);
        scanLambdaCapturesExpr(idx->index.get(), boundary, seen, order);
        return;
    }
    if (auto* mem = dynamic_cast<const MemberExpr*>(t)) {
        scanLambdaCapturesExpr(mem->object.get(), boundary, seen, order);
    }
}

void CodeGenerator::scanLambdaCapturesExpr(const Expr* e, size_t boundary, std::unordered_set<std::string>& seen,
                                           std::vector<std::string>& order) {
    if (!e) return;
    if (dynamic_cast<const LambdaExpr*>(e)) return;
    if (auto* id = dynamic_cast<const Identifier*>(e)) {
        int d = findDefiningScopeIndex(id->name);
        if (d >= 0 && static_cast<size_t>(d) < boundary) {
            if (seen.insert(id->name).second) order.push_back(id->name);
        }
        return;
    }
    if (auto* x = dynamic_cast<const BinaryExpr*>(e)) {
        scanLambdaCapturesExpr(x->left.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->right.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const UnaryExpr*>(e)) {
        scanLambdaCapturesExpr(x->operand.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const AwaitExpr*>(e)) {
        scanLambdaCapturesExpr(x->target.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const CallExpr*>(e)) {
        scanLambdaCapturesExpr(x->callee.get(), boundary, seen, order);
        for (const auto& a : x->args) scanLambdaCapturesExpr(a.expr.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const MemberExpr*>(e)) {
        scanLambdaCapturesExpr(x->object.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const IndexExpr*>(e)) {
        scanLambdaCapturesExpr(x->object.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->index.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const AssignExpr*>(e)) {
        scanLambdaCapturesExpr(x->value.get(), boundary, seen, order);
        scanAssignTargetForCaptures(x->target.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const CoalesceExpr*>(e)) {
        scanLambdaCapturesExpr(x->left.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->right.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const PipelineExpr*>(e)) {
        scanLambdaCapturesExpr(x->left.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->right.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const TernaryExpr*>(e)) {
        scanLambdaCapturesExpr(x->condition.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->thenExpr.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->elseExpr.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const ArrayComprehensionExpr*>(e)) {
        scanLambdaCapturesExpr(x->iterExpr.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->bodyExpr.get(), boundary, seen, order);
        if (x->filterExpr) scanLambdaCapturesExpr(x->filterExpr.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const MapComprehensionExpr*>(e)) {
        scanLambdaCapturesExpr(x->iterExpr.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->keyExpr.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->valExpr.get(), boundary, seen, order);
        if (x->filterExpr) scanLambdaCapturesExpr(x->filterExpr.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const ArrayLiteral*>(e)) {
        for (const auto& el : x->elements) scanLambdaCapturesExpr(el.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const MapLiteral*>(e)) {
        for (const auto& kv : x->entries) {
            scanLambdaCapturesExpr(kv.first.get(), boundary, seen, order);
            scanLambdaCapturesExpr(kv.second.get(), boundary, seen, order);
        }
        return;
    }
    if (auto* x = dynamic_cast<const RangeExpr*>(e)) {
        scanLambdaCapturesExpr(x->start.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->end.get(), boundary, seen, order);
        if (x->step) scanLambdaCapturesExpr(x->step.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const DurationExpr*>(e)) {
        scanLambdaCapturesExpr(x->amount.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const SliceExpr*>(e)) {
        scanLambdaCapturesExpr(x->object.get(), boundary, seen, order);
        if (x->start) scanLambdaCapturesExpr(x->start.get(), boundary, seen, order);
        if (x->end) scanLambdaCapturesExpr(x->end.get(), boundary, seen, order);
        if (x->step) scanLambdaCapturesExpr(x->step.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const FStringExpr*>(e)) {
        for (const auto& part : x->parts) {
            if (std::holds_alternative<ExprPtr>(part))
                scanLambdaCapturesExpr(std::get<ExprPtr>(part).get(), boundary, seen, order);
        }
        return;
    }
    if (auto* x = dynamic_cast<const OptionalChainExpr*>(e)) {
        scanLambdaCapturesExpr(x->object.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const OptionalIndexExpr*>(e)) {
        scanLambdaCapturesExpr(x->object.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->index.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const SpreadExpr*>(e)) {
        scanLambdaCapturesExpr(x->target.get(), boundary, seen, order);
    }
}

void CodeGenerator::scanLambdaCapturesStmt(const Stmt* s, size_t boundary, std::unordered_set<std::string>& seen,
                                           std::vector<std::string>& order) {
    if (!s) return;
    if (dynamic_cast<const FunctionDeclStmt*>(s)) return;
    if (dynamic_cast<const ClassDeclStmt*>(s)) return;
    if (auto* x = dynamic_cast<const ExprStmt*>(s)) {
        scanLambdaCapturesExpr(x->expr.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const VarDeclStmt*>(s)) {
        if (x->initializer) scanLambdaCapturesExpr(x->initializer.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const DestructureStmt*>(s)) {
        if (x->initializer) scanLambdaCapturesExpr(x->initializer.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const SequenceStmt*>(s)) {
        for (const auto& st : x->statements) scanLambdaCapturesStmt(st.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const BlockStmt*>(s)) {
        for (const auto& st : x->statements) scanLambdaCapturesStmt(st.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const UnsafeBlockStmt*>(s)) {
        scanLambdaCapturesStmt(x->body.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const IfStmt*>(s)) {
        scanLambdaCapturesExpr(x->condition.get(), boundary, seen, order);
        scanLambdaCapturesStmt(x->thenBranch.get(), boundary, seen, order);
        if (x->elseBranch) scanLambdaCapturesStmt(x->elseBranch.get(), boundary, seen, order);
        for (const auto& el : x->elifBranches) {
            scanLambdaCapturesExpr(el.first.get(), boundary, seen, order);
            scanLambdaCapturesStmt(el.second.get(), boundary, seen, order);
        }
        return;
    }
    if (auto* x = dynamic_cast<const ForRangeStmt*>(s)) {
        scanLambdaCapturesExpr(x->start.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->end.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->step.get(), boundary, seen, order);
        scanLambdaCapturesStmt(x->body.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const ForCStyleStmt*>(s)) {
        if (x->init) scanLambdaCapturesStmt(x->init.get(), boundary, seen, order);
        if (x->condition) scanLambdaCapturesExpr(x->condition.get(), boundary, seen, order);
        if (x->update) scanLambdaCapturesExpr(x->update.get(), boundary, seen, order);
        scanLambdaCapturesStmt(x->body.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const WhileStmt*>(s)) {
        scanLambdaCapturesExpr(x->condition.get(), boundary, seen, order);
        scanLambdaCapturesStmt(x->body.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const RepeatStmt*>(s)) {
        scanLambdaCapturesExpr(x->count.get(), boundary, seen, order);
        scanLambdaCapturesStmt(x->body.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const RepeatWhileStmt*>(s)) {
        scanLambdaCapturesStmt(x->body.get(), boundary, seen, order);
        scanLambdaCapturesExpr(x->condition.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const DeferStmt*>(s)) {
        scanLambdaCapturesExpr(x->expr.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const TryStmt*>(s)) {
        scanLambdaCapturesStmt(x->tryBlock.get(), boundary, seen, order);
        if (x->catchBlock) scanLambdaCapturesStmt(x->catchBlock.get(), boundary, seen, order);
        if (x->elseBlock) scanLambdaCapturesStmt(x->elseBlock.get(), boundary, seen, order);
        if (x->finallyBlock) scanLambdaCapturesStmt(x->finallyBlock.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const ThrowStmt*>(s)) {
        scanLambdaCapturesExpr(x->value.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const AssertStmt*>(s)) {
        scanLambdaCapturesExpr(x->condition.get(), boundary, seen, order);
        if (x->message) scanLambdaCapturesExpr(x->message.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const MatchStmt*>(s)) {
        scanLambdaCapturesExpr(x->value.get(), boundary, seen, order);
        for (const auto& c : x->cases) {
            if (c.pattern) scanLambdaCapturesExpr(c.pattern.get(), boundary, seen, order);
            if (c.guard) scanLambdaCapturesExpr(c.guard.get(), boundary, seen, order);
            scanLambdaCapturesStmt(c.body.get(), boundary, seen, order);
        }
        return;
    }
    if (auto* x = dynamic_cast<const ForInStmt*>(s)) {
        scanLambdaCapturesExpr(x->iterable.get(), boundary, seen, order);
        scanLambdaCapturesStmt(x->body.get(), boundary, seen, order);
        return;
    }
    if (auto* y = dynamic_cast<const YieldStmt*>(s)) {
        if (y->value) scanLambdaCapturesExpr(y->value.get(), boundary, seen, order);
        return;
    }
    if (auto* x = dynamic_cast<const ReturnStmt*>(s)) {
        for (const auto& v : x->values) scanLambdaCapturesExpr(v.get(), boundary, seen, order);
    }
}

size_t CodeGenerator::totalLocalCount() {
    size_t n = 0;
    for (const auto& scope : scopes_) n += scope.size();
    return n;
}

void CodeGenerator::declareLocal(const std::string& name) {
    if (!lambdaCtxStack_.empty()) {
        size_t slot = lambdaCtxStack_.back().nextInnerSlot++;
        scopes_.back()[name] = slot;
        return;
    }
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
        int64_t slot = 0;
        if (tryResolveLocalSlot(x->name, &slot))
            emit(Opcode::LOAD, slot);
        else
            emit(Opcode::LOAD_GLOBAL, addConstant(x->name));
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
            default:
                throw CodegenError("Unsupported binary operator in codegen", currentLine_);
        }
        return;
    }
    if (auto* x = dynamic_cast<const UnaryExpr*>(e)) {
        if (tryConstantFoldUnary(x)) return;
        if (x->op == TokenType::STAR) {
            // pointer dereference read: *ptr -> peek64(ptr)
            emit(Opcode::LOAD_GLOBAL, addConstant("peek64"));
            emitExpr(x->operand.get());
            emit(Opcode::CALL, static_cast<size_t>(1));
        } else {
            emitExpr(x->operand.get());
            if (x->op == TokenType::MINUS) emit(Opcode::NEG);
            else if (x->op == TokenType::NOT) emit(Opcode::NOT);
        }
        return;
    }
    if (auto* x = dynamic_cast<const AwaitExpr*>(e)) {
        emit(Opcode::LOAD_GLOBAL, addConstant("__await_task"));
        emitExpr(x->target.get());
        emit(Opcode::CALL, static_cast<size_t>(1));
        return;
    }
    if (auto* x = dynamic_cast<const CallExpr*>(e)) {
        bool anySpread = false;
        for (const auto& a : x->args)
            if (a.spread) { anySpread = true; break; }
        if (anySpread) {
            const std::string acc = "__invoke_args";
            emit(Opcode::BUILD_ARRAY, static_cast<size_t>(0));
            emit(Opcode::STORE_GLOBAL, addConstant(acc));
            for (const auto& a : x->args) {
                if (a.spread) {
                    emit(Opcode::LOAD_GLOBAL, addConstant("extend_array"));
                    emit(Opcode::LOAD_GLOBAL, addConstant(acc));
                    emitExpr(a.expr.get());
                    emit(Opcode::CALL, static_cast<size_t>(2));
                    emit(Opcode::POP);
                } else {
                    emit(Opcode::LOAD_GLOBAL, addConstant("push"));
                    emit(Opcode::LOAD_GLOBAL, addConstant(acc));
                    emitExpr(a.expr.get());
                    emit(Opcode::CALL, static_cast<size_t>(2));
                    emit(Opcode::POP);
                }
            }
            emit(Opcode::LOAD_GLOBAL, addConstant("invoke"));
            emitExpr(x->callee.get());
            emit(Opcode::LOAD_GLOBAL, addConstant(acc));
            emit(Opcode::CALL, static_cast<size_t>(2));
            return;
        }
        std::vector<const Expr*> ordered;
        const Identifier* id = dynamic_cast<const Identifier*>(x->callee.get());
        const MemberExpr* mem = dynamic_cast<const MemberExpr*>(x->callee.get());
        bool hasNamed = false;
        for (const auto& a : x->args)
            if (!a.name.empty()) { hasNamed = true; break; }
        if (id && hasNamed) {
            auto it = functionParams_.find(id->name);
            if (it != functionParams_.end() && !it->second.empty()) {
                const std::vector<std::string>& pnames = it->second;
                ordered.resize(pnames.size(), nullptr);
                size_t pos = 0;
                for (const auto& a : x->args) {
                    if (a.name.empty()) {
                        if (pos < pnames.size()) ordered[pos++] = a.expr.get();
                    } else {
                        for (size_t i = 0; i < pnames.size(); ++i)
                            if (pnames[i] == a.name) { ordered[i] = a.expr.get(); break; }
                    }
                }
            }
        }
        if (mem) {
            emitExpr(mem->object.get());
            emit(Opcode::DUP);
            emit(Opcode::STORE_GLOBAL, addConstant("this"));
            emit(Opcode::GET_FIELD, addConstant(mem->member));
            for (const auto& a : x->args) emitExpr(a.expr.get());
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
                for (const auto& a : x->args) emitExpr(a.expr.get());
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
        if (x->op == TokenType::COALESCE_EQ) {
            if (auto* id = dynamic_cast<Identifier*>(x->target.get())) {
                int64_t localSlot = 0;
                bool isLocal = tryResolveLocalSlot(id->name, &localSlot);
                if (isLocal)
                    emit(Opcode::LOAD, localSlot);
                else
                    emit(Opcode::LOAD_GLOBAL, addConstant(id->name));
                emit(Opcode::DUP);
                emit(Opcode::CONST_NULL);
                emit(Opcode::EQ);
                size_t keep = emit(Opcode::JMP_IF_FALSE, size_t(0));
                emit(Opcode::POP);
                emitExpr(x->value.get());
                emit(Opcode::DUP); // expression result (same as plain = assign)
                if (isLocal)
                    emit(Opcode::STORE, localSlot);
                else
                    emit(Opcode::STORE_GLOBAL, addConstant(id->name));
                size_t endJ = emit(Opcode::JMP, size_t(0));
                patchJump(keep, code_.size());
                emit(Opcode::POP); // skip path: drop value left from DUP/EQ (assign path jumps over this)
                if (isLocal)
                    emit(Opcode::LOAD, localSlot);
                else
                    emit(Opcode::LOAD_GLOBAL, addConstant(id->name));
                patchJump(endJ, code_.size());
                return;
            }
            if (auto* idx = dynamic_cast<IndexExpr*>(x->target.get())) {
                emitExpr(idx->object.get());
                emitExpr(idx->index.get());
                emit(Opcode::GET_INDEX);
                emit(Opcode::DUP);
                emit(Opcode::CONST_NULL);
                emit(Opcode::EQ);
                size_t keep = emit(Opcode::JMP_IF_FALSE, size_t(0));
                emit(Opcode::POP);
                emitExpr(idx->object.get());
                emitExpr(idx->index.get());
                emitExpr(x->value.get());
                emit(Opcode::SET_INDEX);
                size_t endJ = emit(Opcode::JMP, size_t(0));
                patchJump(keep, code_.size());
                emit(Opcode::POP);
                emitExpr(idx->object.get());
                emitExpr(idx->index.get());
                emit(Opcode::GET_INDEX);
                patchJump(endJ, code_.size());
                return;
            }
            if (auto* mem = dynamic_cast<MemberExpr*>(x->target.get())) {
                emitExpr(mem->object.get());
                emit(Opcode::GET_FIELD, addConstant(mem->member));
                emit(Opcode::DUP);
                emit(Opcode::CONST_NULL);
                emit(Opcode::EQ);
                size_t keep = emit(Opcode::JMP_IF_FALSE, size_t(0));
                emit(Opcode::POP);
                emitExpr(mem->object.get());
                emitExpr(x->value.get());
                emit(Opcode::SET_FIELD, addConstant(mem->member));
                size_t endJ = emit(Opcode::JMP, size_t(0));
                patchJump(keep, code_.size());
                emit(Opcode::POP);
                emitExpr(mem->object.get());
                emit(Opcode::GET_FIELD, addConstant(mem->member));
                patchJump(endJ, code_.size());
                return;
            }
        }
        if (auto* un = dynamic_cast<UnaryExpr*>(x->target.get())) {
            if (un->op == TokenType::STAR) {
                // pointer write: *ptr = value -> poke64(ptr, value)
                emit(Opcode::LOAD_GLOBAL, addConstant("poke64"));
                emitExpr(un->operand.get());
                emitExpr(x->value.get());
                emit(Opcode::CALL, static_cast<size_t>(2));
                emit(Opcode::POP); // pointer assignment expression yields null
                emit(Opcode::CONST_NULL);
                return;
            }
        }
        // vM SET_FIELD / SET_INDEX pop value first (top of stack), then object[/index].
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
            int64_t localSlot = 0;
            if (tryResolveLocalSlot(id->name, &localSlot))
                emit(Opcode::STORE, localSlot);
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
    if (auto* x = dynamic_cast<const MapComprehensionExpr*>(e)) {
        const std::string mapGlobal = "__map_comp";
        beginScope();
        declareLocal("__iter");
        declareLocal("__mk");
        declareLocal("__mv");
        declareLocal(x->varName);
        size_t iterSlot = scopes_.back()["__iter"];
        size_t mkSlot = scopes_.back()["__mk"];
        size_t mvSlot = scopes_.back()["__mv"];
        size_t varSlot = scopes_.back()[x->varName];
        emit(Opcode::NEW_OBJECT);
        emit(Opcode::STORE_GLOBAL, addConstant(mapGlobal));
        emitExpr(x->iterExpr.get());
        emit(Opcode::STORE, static_cast<int64_t>(iterSlot));
        emit(Opcode::LOAD, static_cast<int64_t>(iterSlot));
        emit(Opcode::FOR_IN_ITER);
        size_t loopStart = code_.size();
        emit(Opcode::FOR_IN_NEXT, varSlot);
        size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        if (x->filterExpr) {
            emitExpr(x->filterExpr.get());
            size_t skipToNext = emit(Opcode::JMP_IF_FALSE, size_t(0));
            emitExpr(x->keyExpr.get());
            emit(Opcode::STORE, static_cast<int64_t>(mkSlot));
            emitExpr(x->valExpr.get());
            emit(Opcode::STORE, static_cast<int64_t>(mvSlot));
            emit(Opcode::LOAD_GLOBAL, addConstant(mapGlobal));
            emit(Opcode::LOAD, static_cast<int64_t>(mkSlot));
            emit(Opcode::LOAD, static_cast<int64_t>(mvSlot));
            emit(Opcode::SET_INDEX);
            emit(Opcode::POP);
            patchJump(skipToNext, code_.size());
        } else {
            emitExpr(x->keyExpr.get());
            emit(Opcode::STORE, static_cast<int64_t>(mkSlot));
            emitExpr(x->valExpr.get());
            emit(Opcode::STORE, static_cast<int64_t>(mvSlot));
            emit(Opcode::LOAD_GLOBAL, addConstant(mapGlobal));
            emit(Opcode::LOAD, static_cast<int64_t>(mkSlot));
            emit(Opcode::LOAD, static_cast<int64_t>(mvSlot));
            emit(Opcode::SET_INDEX);
            emit(Opcode::POP);
        }
        emit(Opcode::JMP, loopStart);
        patchJump(exitJump, code_.size());
        emit(Opcode::LOAD_GLOBAL, addConstant(mapGlobal));
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
        size_t captureBoundary = scopes_.size();
        size_t skipBody = emit(Opcode::JMP, size_t(0));
        size_t entry = code_.size();
        beginScope();
        lambdaCtxStack_.push_back(LambdaCodegenLayer{});
        LambdaCodegenLayer& LC = lambdaCtxStack_.back();
        LC.captureBoundary = captureBoundary;
        LC.arity = lam->params.size();
        std::unordered_set<std::string> capSeen;
        if (auto* es = dynamic_cast<const ExprStmt*>(lam->body.get()))
            scanLambdaCapturesExpr(es->expr.get(), captureBoundary, capSeen, LC.captureOrder);
        else
            scanLambdaCapturesStmt(lam->body.get(), captureBoundary, capSeen, LC.captureOrder);
        for (size_t i = 0; i < LC.captureOrder.size(); ++i) LC.captureIndex[LC.captureOrder[i]] = i;
        LC.nextInnerSlot = LC.arity + LC.captureOrder.size();
        for (size_t pi = 0; pi < lam->params.size(); ++pi) scopes_.back()[lam->params[pi]] = pi;
        if (auto* es = dynamic_cast<const ExprStmt*>(lam->body.get())) {
            emitExpr(es->expr.get());
        } else {
            emitStmt(lam->body.get());
        }
        emit(Opcode::RETURN);
        std::vector<std::string> capOrder = std::move(LC.captureOrder);
        lambdaCtxStack_.pop_back();
        endScope();
        patchJump(skipBody, code_.size());
        for (const auto& capName : capOrder) {
            int64_t outerSlot = 0;
            if (!tryResolveLocalSlot(capName, &outerSlot))
                throw CodegenError("Lambda capture '" + capName + "' is not a local in the enclosing scope", currentLine_);
            emit(Opcode::LOAD, outerSlot);
        }
        emit(Opcode::BUILD_CLOSURE, entry, capOrder.size());
        emit(Opcode::SET_FUNC_ARITY, static_cast<size_t>(lam->params.size()));
        if (stmtHasYield(lam->body.get())) emit(Opcode::SET_FUNC_GENERATOR);
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
    throw CodegenError("Unsupported expression node in codegen", currentLine_);
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
    if (auto* x = dynamic_cast<const SequenceStmt*>(s)) {
        for (const auto& st : x->statements) emitStmt(st.get());
        return;
    }
    if (auto* x = dynamic_cast<const BlockStmt*>(s)) {
        beginScope();
        for (const auto& st : x->statements) emitStmt(st.get());
        endScope();
        return;
    }
    if (auto* x = dynamic_cast<const UnsafeBlockStmt*>(s)) {
        emit(Opcode::UNSAFE_BEGIN);
        emitStmt(x->body.get());
        emit(Opcode::UNSAFE_END);
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
    if (auto* x = dynamic_cast<const RepeatWhileStmt*>(s)) {
        breakPatches_.emplace_back();
        continuePatches_.emplace_back();
        loopLabels_.push_back("");
        loopStartStack_.push_back(0);
        loopEndStack_.push_back(0);
        size_t bodyEntry = code_.size();
        emitStmt(x->body.get());
        size_t continuePoint = code_.size();
        loopStartStack_.back() = continuePoint;
        emitExpr(x->condition.get());
        size_t exitJump = emit(Opcode::JMP_IF_FALSE, size_t(0));
        emit(Opcode::JMP, bodyEntry);
        for (size_t at : breakPatches_.back()) patchJump(at, code_.size());
        patchJump(exitJump, code_.size());
        breakPatches_.pop_back();
        continuePatches_.pop_back();
        loopLabels_.pop_back();
        loopStartStack_.pop_back();
        loopEndStack_.pop_back();
        return;
    }
    if (auto* x = dynamic_cast<const DeferStmt*>(s)) {
        const auto* call = dynamic_cast<const CallExpr*>(x->expr.get());
        if (call) {
            emitExpr(call->callee.get());
            for (const auto& a : call->args) emitExpr(a.expr.get());
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
        } else if (x->finallyBlock) {
            beginScope();
            declareLocal("__exc");
            size_t excSlot = scopes_.back()["__exc"];
            emit(Opcode::STORE, static_cast<int64_t>(excSlot));
            emitStmt(x->finallyBlock.get());
            emit(Opcode::LOAD, static_cast<int64_t>(excSlot));
            emit(Opcode::THROW);
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
                        continue;
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
                continue;
            }
            const auto* arrPattern = dynamic_cast<const ArrayLiteral*>(c.pattern.get());
            if (arrPattern) {
                emit(Opcode::DUP);
                emit(Opcode::ARRAY_LEN);
                emit(Opcode::CONST_I64, static_cast<int64_t>(arrPattern->elements.size()));
                emit(Opcode::EQ);
                nextCasePatches.push_back(emit(Opcode::JMP_IF_FALSE, size_t(0)));
                beginScope();
                for (const auto& el : arrPattern->elements) {
                    const auto* idBind = dynamic_cast<const Identifier*>(el.get());
                    if (idBind) declareLocal(idBind->name);
                }
                for (size_t ei = 0; ei < arrPattern->elements.size(); ++ei) {
                    const Expr* el = arrPattern->elements[ei].get();
                    emit(Opcode::DUP);
                    emit(Opcode::CONST_I64, static_cast<int64_t>(ei));
                    emit(Opcode::GET_INDEX);
                    if (const auto* idBind = dynamic_cast<const Identifier*>(el)) {
                        size_t slot = scopes_.back()[idBind->name];
                        emit(Opcode::STORE, static_cast<int64_t>(slot));
                    } else {
                        emitExpr(el);
                        emit(Opcode::EQ);
                        size_t nextCase = emit(Opcode::JMP_IF_FALSE, size_t(0));
                        emit(Opcode::POP);
                        emit(Opcode::POP);
                        nextCasePatches.push_back(nextCase);
                        endScope();
                        continue;
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
    if (auto* y = dynamic_cast<const YieldStmt*>(s)) {
        if (y->value) emitExpr(y->value.get());
        else emit(Opcode::CONST_NULL);
        emit(Opcode::YIELD);
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
        std::vector<LambdaCodegenLayer> savedLambdaCtx;
        savedLambdaCtx.swap(lambdaCtxStack_);
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
        if (stmtHasYield(x->body.get())) emit(Opcode::SET_FUNC_GENERATOR);
        functionParams_[x->name] = std::move(pnames);
        if (scopes_.size() == 1)
            emit(Opcode::STORE_GLOBAL, addConstant(x->name));
        else {
            declareLocal(x->name);
            emit(Opcode::STORE, static_cast<int64_t>(scopes_.back()[x->name]));
        }
        lambdaCtxStack_ = std::move(savedLambdaCtx);
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
    if (auto* x = dynamic_cast<const StructDeclStmt*>(s)) {
        emit(Opcode::LOAD_GLOBAL, addConstant("struct_define"));
        emit(Opcode::CONST_STR, addConstant(x->name));
        for (const auto& f : x->fields) {
            emit(Opcode::CONST_STR, addConstant(f.name));
            emit(Opcode::CONST_I64, approxTypeSize(f.typeName));
            emit(Opcode::CONST_I64, approxTypeAlign(f.typeName));
            emit(Opcode::BUILD_ARRAY, static_cast<size_t>(3));
        }
        emit(Opcode::BUILD_ARRAY, static_cast<size_t>(x->fields.size()));
        emit(Opcode::CALL, static_cast<size_t>(2));
        emit(Opcode::POP);

        emit(Opcode::NEW_OBJECT);
        emit(Opcode::DUP);
        emit(Opcode::CONST_STR, addConstant("struct"));
        emit(Opcode::SET_FIELD, addConstant("__kind"));
        emit(Opcode::POP);
        emit(Opcode::DUP);
        emit(Opcode::CONST_STR, addConstant(x->name));
        emit(Opcode::SET_FIELD, addConstant("__name"));
        emit(Opcode::POP);
        for (const auto& f : x->fields) {
            emit(Opcode::DUP);
            emit(Opcode::CONST_STR, addConstant(f.typeName));
            emit(Opcode::SET_FIELD, addConstant(f.name));
            emit(Opcode::POP);
        }
        emit(Opcode::STORE_GLOBAL, addConstant(x->name));
        return;
    }
    if (auto* x = dynamic_cast<const FfiDeclStmt*>(s)) {
        size_t skipBody = emit(Opcode::JMP, size_t(0));
        size_t entry = code_.size();
        beginScope();
        for (const auto& p : x->params) declareLocal(p.name);

        emit(Opcode::LOAD_GLOBAL, addConstant("ffi_call"));
        emit(Opcode::CONST_STR, addConstant(x->dllName));
        emit(Opcode::CONST_STR, addConstant(x->symbolName));
        emit(Opcode::CONST_STR, addConstant(x->returnType));
        emit(Opcode::CONST_STR, addConstant(x->abi));
        for (const auto& p : x->params) {
            emit(Opcode::CONST_STR, addConstant(p.typeName.empty() ? std::string("int") : p.typeName));
        }
        emit(Opcode::BUILD_ARRAY, static_cast<size_t>(x->params.size()));
        for (const auto& p : x->params) {
            size_t slot = scopes_.back()[p.name];
            emit(Opcode::LOAD, static_cast<int64_t>(slot));
        }
        emit(Opcode::CALL, static_cast<size_t>(5 + x->params.size()));
        emit(Opcode::RETURN);
        endScope();

        patchJump(skipBody, code_.size());
        emit(Opcode::BUILD_FUNC, entry);
        emit(Opcode::SET_FUNC_ARITY, static_cast<size_t>(x->params.size()));
        emit(Opcode::SET_FUNC_NAME, addConstant(x->name));
        std::string joined;
        for (size_t i = 0; i < x->params.size(); ++i) {
            if (i) joined += ",";
            joined += x->params[i].name;
        }
        if (!joined.empty()) emit(Opcode::SET_FUNC_PARAM_NAMES, addConstant(joined));
        emit(Opcode::STORE_GLOBAL, addConstant(x->name));
        return;
    }
    if (auto* x = dynamic_cast<const ImportStmt*>(s)) {
        std::string path = x->moduleName;
        // Leave the import string unchanged and let __import handle resolution:
        // - stdlib modules (e.g. "math")
        // - builtin module names (e.g. "g2d", "game")
        // - file paths (e.g. "lib/kern/foo.kn")
        std::string bindingName = x->hasAlias ? x->alias : x->moduleName;
        if (!x->hasAlias && bindingName.size() >= 4 && bindingName.compare(bindingName.size() - 4, 4, ".kn") == 0)
            bindingName = bindingName.substr(0, bindingName.size() - 4);
        emit(Opcode::LOAD_GLOBAL, addConstant("__import"));
        emit(Opcode::CONST_STR, addConstant(path));
        emit(Opcode::CALL, static_cast<size_t>(1));
        emit(Opcode::STORE_GLOBAL, addConstant(bindingName));
        return;
    }
    throw CodegenError("Unsupported statement node in codegen", currentLine_);
}

void CodeGenerator::emitProgram(const Program* p) {
    for (const auto& st : p->statements) emitStmt(st.get());
}

void CodeGenerator::emitFunctionOntoStack(const FunctionDeclStmt* x) {
    std::vector<LambdaCodegenLayer> savedLambdaCtx;
    savedLambdaCtx.swap(lambdaCtxStack_);
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
    if (stmtHasYield(x->body.get())) emit(Opcode::SET_FUNC_GENERATOR);
    functionParams_[x->name] = std::move(pnames);
    lambdaCtxStack_ = std::move(savedLambdaCtx);
}

Bytecode CodeGenerator::generate(std::unique_ptr<Program> program) {
    code_.clear();
    emitProgram(program.get());
    emit(Opcode::HALT);
    // peephole (NOP removal + jump remap) was removed: it caused stack underflow in smoke tests.
    // reintroduce as a separate pass only after fixing BUILD_FUNC / jump remapping.
    return std::move(code_);
}

} // namespace kern
