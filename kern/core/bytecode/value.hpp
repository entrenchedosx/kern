/* *
 * kern Value - Runtime value representation for the VM
 */

#ifndef KERN_VALUE_HPP
#define KERN_VALUE_HPP

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>

namespace kern {

struct Value;
struct ScriptCode;  // script bytecode + constants (see script_code.hpp)
struct FunctionObject;
struct ClassObject;
struct InstanceObject;
struct GeneratorObject;
struct Vec3Object;

using ValuePtr = std::shared_ptr<Value>;
using FunctionPtr = std::shared_ptr<FunctionObject>;
using ClassPtr = std::shared_ptr<ClassObject>;
using InstancePtr = std::shared_ptr<InstanceObject>;
using GeneratorPtr = std::shared_ptr<GeneratorObject>;
using Vec3Ptr = std::shared_ptr<Vec3Object>;

struct Vec3Object {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct Value {
    enum class Type { NIL, BOOL, INT, FLOAT, STRING, ARRAY, MAP, FUNCTION, CLASS, INSTANCE, GENERATOR, PTR, VEC3 };
    Type type = Type::NIL;
    std::variant<
        std::monostate,
        bool,
        int64_t,
        double,
        std::string,
        std::vector<ValuePtr>,
        std::unordered_map<std::string, ValuePtr>,
        FunctionPtr,
        ClassPtr,
        InstancePtr,
        GeneratorPtr,
        void*,
        Vec3Ptr
    > data;

    Value() : type(Type::NIL), data(std::monostate{}) {}
    static Value nil() { return Value(); }
    static Value fromBool(bool b) { Value v; v.type = Type::BOOL; v.data = b; return v; }
    static Value fromInt(int64_t i) { Value v; v.type = Type::INT; v.data = i; return v; }
    static Value fromFloat(double d) { Value v; v.type = Type::FLOAT; v.data = d; return v; }
    static Value fromString(std::string s) { Value v; v.type = Type::STRING; v.data = std::move(s); return v; }
    static Value fromArray(std::vector<ValuePtr> a) {
        for (auto& x : a) {
            if (!x) x = std::make_shared<Value>(Value::nil());
        }
        Value v;
        v.type = Type::ARRAY;
        v.data = std::move(a);
        return v;
    }
    static Value fromMap(std::unordered_map<std::string, ValuePtr> m) {
        for (auto& kv : m) {
            if (!kv.second) kv.second = std::make_shared<Value>(Value::nil());
        }
        Value v;
        v.type = Type::MAP;
        v.data = std::move(m);
        return v;
    }
    static Value fromFunction(FunctionPtr f) { Value v; v.type = Type::FUNCTION; v.data = std::move(f); return v; }
    static Value fromClass(ClassPtr c) { Value v; v.type = Type::CLASS; v.data = std::move(c); return v; }
    static Value fromInstance(InstancePtr i) { Value v; v.type = Type::INSTANCE; v.data = std::move(i); return v; }
    static Value fromGenerator(GeneratorPtr g) { Value v; v.type = Type::GENERATOR; v.data = std::move(g); return v; }
    static Value fromPtr(void* p) { Value v; v.type = Type::PTR; v.data = p; return v; }
    static Value fromVec3(double x, double y, double z) { 
        Value v; 
        v.type = Type::VEC3; 
        v.data = std::make_shared<Vec3Object>(Vec3Object{x, y, z}); 
        return v; 
    }

    bool isTruthy() const;
    std::string toString() const;
    bool equals(const Value& other) const;
};

struct FunctionObject {
    std::string name;
    size_t arity = 0;
    size_t entryPoint = 0;  // bytecode index (into script->code when script is set)
    std::shared_ptr<ScriptCode> script;  // when set, function was defined in an imported script
    std::vector<ValuePtr> captures;  // lambda closure values (copied at creation); appended after args in CALL
    std::vector<std::string> paramNames;
    std::vector<ValuePtr> defaults;
    bool isBuiltin = false;
    size_t builtinIndex = 0;
    bool isGenerator = false;  // contains yield; call returns GeneratorObject instead of running
};

/* * resumable generator instance (call to generator function).*/
struct GeneratorObject {
    FunctionPtr fn;
    size_t ip = 0;
    std::vector<ValuePtr> locals;
    bool exhausted = false;
};

struct ClassObject {
    std::string name;
    std::shared_ptr<ClassObject> superClass;
    std::unordered_map<std::string, ValuePtr> methods;
    std::unordered_map<std::string, ValuePtr> staticFields;
};

struct InstanceObject {
    ClassPtr klass;
    std::unordered_map<std::string, ValuePtr> fields;
};

} // namespace kern

#endif // kERN_VALUE_HPP
