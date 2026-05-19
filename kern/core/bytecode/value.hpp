/* *
 * kern Value - Runtime value representation for the VM
 *
 * This file defines both the legacy Value struct (std::variant-based, ~80 bytes)
 * and the new TaggedValue (NaN-boxed, 8 bytes). During migration, the legacy
 * types remain for bytecode serialization and the ModuleContext API. The VM
 * internals switch to TaggedValue.
 */

#ifndef KERN_VALUE_HPP
#define KERN_VALUE_HPP

#include <cstdint>
#include <cstring>
#include <cmath>
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>
#include <variant>
#include <sstream>
#include <type_traits>

namespace kern {

// ─── Forward declarations ───────────────────────────────────────
struct Value;
struct ScriptCode;  // script bytecode + constants (see script_code.hpp)
struct FunctionObject;
struct ClassObject;
class VM;           // runtime/vm/vm.hpp — needed by TaggedValue::fromValue(Value, VM*)
struct InstanceObject;
struct GeneratorObject;
struct Vec3Object;
struct StructObject;
struct FfiClosure;
class TaggedValue;
struct ObjHeader;
struct ObjArray;
struct ObjMap;
struct ObjClosure;

// ─── ValuePtr alias — now NaN-boxed TaggedValue ────────────────
using ValuePtr = TaggedValue;
using StructPtr = std::shared_ptr<StructObject>;
using FunctionPtr = std::shared_ptr<FunctionObject>;
using ClassPtr = std::shared_ptr<ClassObject>;
using InstancePtr = std::shared_ptr<InstanceObject>;
using GeneratorPtr = std::shared_ptr<GeneratorObject>;
using Vec3Ptr = std::shared_ptr<Vec3Object>;
using FfiClosurePtr = std::shared_ptr<FfiClosure>;

// ====================================================================
//  NaN-BOXED TAGGED VALUE (8 bytes, trivially copyable)
// ====================================================================

/* *
 * IEEE-754 double NaN-boxing bit layout for boxed (non-float) values:
 *
 *  63  62      52  51  48  47                         0
 * ┌──┬──────────┬───┬──────┬──────────────────────────────┐
 * │ S│ Exponent │ N │ Tag  │           Payload            │
 * │ 0 │  0x7FF  │ 1 │ 3bit │           48 bits           │
 * └──┴──────────┴───┴──────┴──────────────────────────────┘
 *
 * Tagged (boxed) values: exponent == 0x7FF AND quiet-NaN bit SET.
 * Real doubles: exponent != 0x7FF, OR exponent == 0x7FF with QNaN bit CLEAR
 * (i.e., +Inf, -Inf, signalling NaN). This protects mathematical infinities
 * from being misclassified as boxed values.
 *
 * 3-bit Tag → 8 tag slots:
 *   0: NIL     1: BOOL     2: INT32     3: OBJ (all heap objects)
 *   4-7: reserved for future expansion
 *
 * Payload: all 48 lower bits are used (supports full x86-64 canonical
 * address range of 48 bits, avoiding ASLR high-bit truncation).
 */

enum class ValueTag : uint8_t {
    NIL       = 0x0,
    BOOL      = 0x1,
    INT32     = 0x2,
    OBJ       = 0x3,  // All heap-allocated objects; dispatch via ObjHeader::type
    // 0x4-0x7 reserved
};

// ─── ObjHeader: unified heap object base (GC-ready) ────────────

/* *
 * Every heap-allocated object in the Kern VM starts with this header.
 * The intrusive `next` pointer lets the VM track all allocations for
 * future mark-and-sweep GC without a side table.
 */

enum class ObjType : uint8_t {
    String,
    Array,
    Map,
    Closure,
    Class,
    Instance,
    Generator,
    Vec3,
    Struct,
    Ffi,
    RawPtr
};

struct ObjHeader {
    ObjType   type;
    bool      isMarked;    // Reserved for future GC mark phase
    ObjHeader* next;       // Intrusive linked list (VM-owned allocation chain)

    ObjHeader(ObjType t) : type(t), isMarked(false), next(nullptr) {}
};

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"

struct ObjString : ObjHeader {
    uint32_t length;
    uint32_t hash;
    char     chars[];      // Flexible array member (standard VM pattern)

    ObjString() : ObjHeader(ObjType::String), length(0), hash(0) {}
};

#pragma GCC diagnostic pop

// ─── TaggedValue forward declares heap types it references ─────
// ObjArray, ObjMap, ObjClosure are defined AFTER TaggedValue since
// they depend on TaggedValue being complete. They are forward-declared above.

// ─── The 8-byte NaN-boxed Value ─────────────────────────────────

class alignas(8) TaggedValue {
    friend class VM;   // VM::markValue() and GC helpers need payload() access
    uint64_t bits_;

    // ── IEEE-754 NaN-boxing bit layout constants ────────────
    //  1 bit sign | 11 bits exponent | 1 bit QNaN | 3 bits tag | 48 bits payload
    static constexpr uint64_t EXPONENT_MASK    = 0x7FF0000000000000ULL;
    static constexpr uint64_t QNAN_BIT         = 0x0008000000000000ULL;
    static constexpr uint64_t TAGGED_VAL_MASK  = EXPONENT_MASK | QNAN_BIT;
    static constexpr uint64_t TAG_MASK         = 0x0007000000000000ULL;    // Bits 48-50 (3 bits)
    static constexpr int      TAG_SHIFT        = 48;
    static constexpr uint64_t PAYLOAD_MASK     = 0x0000FFFFFFFFFFFFULL;    // Bits 0-47 (48 bits)
    static constexpr uint64_t CANONICAL_NAN    = 0x7FF8000000000000ULL;
    static constexpr uint64_t SIGN_MASK        = 0x8000000000000000ULL;

    explicit constexpr TaggedValue(uint64_t bits) : bits_(bits) {}

    static constexpr uint64_t makeBoxed(ValueTag tag, uint64_t payload) {
        return EXPONENT_MASK | QNAN_BIT |
               (static_cast<uint64_t>(tag) << TAG_SHIFT) |
               (payload & PAYLOAD_MASK);
    }

    constexpr bool isBoxed() const {
        return (bits_ & TAGGED_VAL_MASK) == TAGGED_VAL_MASK;
    }

    constexpr ValueTag tag() const {
        if (!isBoxed()) return static_cast<ValueTag>(0xFF);
        return static_cast<ValueTag>((bits_ & TAG_MASK) >> TAG_SHIFT);
    }

    constexpr uint64_t payload() const {
        return bits_ & PAYLOAD_MASK;
    }

    ObjType objType() const {
        auto* hdr = reinterpret_cast<ObjHeader*>(payload());
        return hdr->type;
    }

public:
    // Default constructor — NIL
    TaggedValue() : bits_(makeBoxed(ValueTag::NIL, 0)) {}

    // ── Static factories ────────────────────────────────────
    static TaggedValue nil()           { return TaggedValue(makeBoxed(ValueTag::NIL, 0)); }

    static TaggedValue fromBool(bool b) {
        return TaggedValue(makeBoxed(ValueTag::BOOL, b ? 1ULL : 0ULL));
    }

    static TaggedValue fromInt32(int32_t i) {
        // Sign-extend int32 to 48-bit payload via int64_t, then mask
        uint64_t p = static_cast<uint64_t>(static_cast<int64_t>(i)) & PAYLOAD_MASK;
        return TaggedValue(makeBoxed(ValueTag::INT32, p));
    }

    static TaggedValue fromFloat(double d) {
        uint64_t raw;
        std::memcpy(&raw, &d, sizeof(raw));
        if ((raw & EXPONENT_MASK) == EXPONENT_MASK) {
            if (raw & QNAN_BIT) {
                return TaggedValue(CANONICAL_NAN);
            }
        }
        return TaggedValue(raw);
    }

    // ── Heap object factories ───────────────────────────────
    // All heap objects share ValueTag::OBJ; dispatch via ObjHeader::type.

    static TaggedValue fromObj(ObjHeader* h) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(h) & PAYLOAD_MASK));
    }

    // Convenience wrappers for ObjHeader-derived types.
    // fromString is defined inline (ObjString is complete above).
    // fromArray/fromMap/fromClosure are declared here but defined after
    // their struct definitions below.
    static TaggedValue fromString(ObjString* s)  { return fromObj(s); }
    static TaggedValue fromArray(ObjArray* a);
    static TaggedValue fromMap(ObjMap* m);
    static TaggedValue fromClosure(ObjClosure* c);

    // Legacy pointer-based factories
    static TaggedValue fromFunction(FunctionObject* f) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(f) & PAYLOAD_MASK));
    }
    static TaggedValue fromClass(ClassObject* c) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(c) & PAYLOAD_MASK));
    }
    static TaggedValue fromInstance(InstanceObject* i) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(i) & PAYLOAD_MASK));
    }
    static TaggedValue fromGenerator(GeneratorObject* g) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(g) & PAYLOAD_MASK));
    }
    static TaggedValue fromVec3(Vec3Object* v) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(v) & PAYLOAD_MASK));
    }
    static TaggedValue fromStruct(StructObject* s) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(s) & PAYLOAD_MASK));
    }
    static TaggedValue fromFfi(FfiClosure* f) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(f) & PAYLOAD_MASK));
    }
    static TaggedValue fromPtr(void* p) {
        return TaggedValue(makeBoxed(ValueTag::OBJ,
            reinterpret_cast<uint64_t>(p) & PAYLOAD_MASK));
    }

    // ── Legacy conversion factory (defined in vm.cpp) ──────
    // Converts a legacy Value (std::variant-based, 80 bytes) to a
    // TaggedValue (NaN-boxed, 8 bytes).  Needs access to the heap
    // allocation helpers (allocObjString etc.) defined in vm.cpp.
    // If vm is non-null, the resulting heap objects are linked into
    // the VM's GC tracking list (firstObject_ chain).
    static TaggedValue fromValue(const Value& v, VM* vm = nullptr);

    // ── Raw access (for serialization) ──────────────────────
    constexpr uint64_t rawBits() const { return bits_; }
    static TaggedValue fromRawBits(uint64_t raw) { return TaggedValue(raw); }

    // ── Type checking ───────────────────────────────────────
    constexpr bool isNil()    const { return tag() == ValueTag::NIL; }
    constexpr bool isBool()   const { return tag() == ValueTag::BOOL; }
    constexpr bool isInt32()  const { return tag() == ValueTag::INT32; }

    // isFloat: true when bit pattern does NOT match our exact boxed signature.
    // Correctly excludes +Inf, -Inf, and signalling NaN from being tagged.
    constexpr bool isFloat()  const { return (bits_ & TAGGED_VAL_MASK) != TAGGED_VAL_MASK; }

    constexpr bool isObj()    const { return tag() == ValueTag::OBJ; }

    // Heap-type checks: fast tag check first, then ObjHeader dispatch
    bool isString()    const { return isObj() && objType() == ObjType::String; }
    bool isArray()     const { return isObj() && objType() == ObjType::Array; }
    bool isMap()       const { return isObj() && objType() == ObjType::Map; }
    bool isClosure()   const { return isObj() && objType() == ObjType::Closure; }
    bool isFunction()  const { return isObj() && objType() == ObjType::Closure; }
    bool isClass()     const { return isObj() && objType() == ObjType::Class; }
    bool isInstance()  const { return isObj() && objType() == ObjType::Instance; }
    bool isGenerator() const { return isObj() && objType() == ObjType::Generator; }
    bool isVec3()      const { return isObj() && objType() == ObjType::Vec3; }
    bool isStruct()    const { return isObj() && objType() == ObjType::Struct; }
    bool isFfi()       const { return isObj() && objType() == ObjType::Ffi; }
    bool isPtr()       const { return isObj() && objType() == ObjType::RawPtr; }

    constexpr bool isNumeric() const {
        return isFloat() || isInt32();
    }

    bool isTruthy() const;

    // ── Accessors (type-unsafe: caller must check isXxx first) ─
    bool        asBool()   const;
    int32_t     asInt32()  const;
    double      asFloat()  const;
    int64_t     asInt64()  const;

    ObjString*      asStringPtr()  const;
    ObjArray*       asArrayPtr()   const;
    ObjMap*         asMapPtr()     const;
    ObjClosure*     asClosurePtr() const;

    // Legacy accessors
    FunctionObject* asFunctionPtr() const;
    ClassObject*    asClassPtr()   const;
    InstanceObject* asInstancePtr() const;
    GeneratorObject* asGeneratorPtr() const;
    Vec3Object*     asVec3Ptr()    const;
    StructObject*   asStructPtr()  const;
    FfiClosure*     asFfiPtr()     const;
    void*           asRawPtr()     const;

    // ── Utilities ───────────────────────────────────────────
    std::string toString() const;
    bool equals(const TaggedValue& other) const;
    std::string typeName() const;
};

static_assert(sizeof(TaggedValue) == 8, "TaggedValue must be exactly 8 bytes");
static_assert(std::is_trivially_copyable_v<TaggedValue>, "TaggedValue must be trivially copyable");

// ── Comparison operators for TaggedValue ──────────────────────────
inline bool operator==(const TaggedValue& a, const TaggedValue& b) { return a.equals(b); }
inline bool operator!=(const TaggedValue& a, const TaggedValue& b) { return !a.equals(b); }

// ─── Heap object structs (depend on TaggedValue being complete) ─

struct ObjArray : ObjHeader {
    uint32_t    capacity;
    uint32_t    count;
    TaggedValue* elements;

    ObjArray() : ObjHeader(ObjType::Array), capacity(0), count(0), elements(nullptr) {}

    // Range-for support
    TaggedValue* begin() { return elements; }
    const TaggedValue* begin() const { return elements; }
    TaggedValue* end() { return elements + count; }
    const TaggedValue* end() const { return elements + count; }
};

struct ObjMap : ObjHeader {
    std::unordered_map<std::string, TaggedValue> entries;

    ObjMap() : ObjHeader(ObjType::Map) {}
};

struct ObjClosure : ObjHeader {
    FunctionObject*       fn;
    std::vector<TaggedValue> captures;

    ObjClosure(FunctionObject* f) : ObjHeader(ObjType::Closure), fn(f) {}
};

// ── Inline definitions for TaggedValue factories that depend on complete ObjXxx types ─
inline TaggedValue TaggedValue::fromArray(ObjArray* a)    { return fromObj(a); }
inline TaggedValue TaggedValue::fromMap(ObjMap* m)        { return fromObj(m); }
inline TaggedValue TaggedValue::fromClosure(ObjClosure* c){ return fromObj(c); }

// ====================================================================
//  LEGACY VALUE (std::variant-based, ~80 bytes, heap-allocated)
// ====================================================================

struct Vec3Object {
    double x = 0.0;
    double y = 0.0;
    double z = 0.0;
};

struct StructObject {
    std::string name;
    std::unordered_map<std::string, ValuePtr> fields;
};

struct FfiClosure {
    void* fnPtr;
    std::string returnType;
    std::vector<std::string> paramTypes;
};

struct Value {
    enum class Type { NIL, BOOL, INT, FLOAT, STRING, ARRAY, MAP, FUNCTION, CLASS, INSTANCE, GENERATOR, PTR, VEC3, STRUCT, FFI_FN };
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
        Vec3Ptr,
        StructPtr,
        FfiClosurePtr
    > data;

    Value() : type(Type::NIL), data(std::monostate{}) {}
    static Value nil() { return Value(); }
    static Value fromBool(bool b) { Value v; v.type = Type::BOOL; v.data = b; return v; }
    static Value fromInt(int64_t i) { Value v; v.type = Type::INT; v.data = i; return v; }
    static Value fromFloat(double d) { Value v; v.type = Type::FLOAT; v.data = d; return v; }
    static Value fromString(std::string s) { Value v; v.type = Type::STRING; v.data = std::move(s); return v; }
    static Value fromArray(std::vector<ValuePtr> a) {
        // TaggedValue is never null; default-constructs to NIL
        Value v;
        v.type = Type::ARRAY;
        v.data = std::move(a);
        return v;
    }
    static Value fromMap(std::unordered_map<std::string, ValuePtr> m) {
        // TaggedValue is never null; default-constructs to NIL
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
    static Value fromFfi(FfiClosurePtr f) { Value v; v.type = Type::FFI_FN; v.data = std::move(f); return v; }
    static Value fromStruct(StructPtr s) { Value v; v.type = Type::STRUCT; v.data = std::move(s); return v; }

    bool isTruthy() const;
    std::string toString() const;
    bool equals(const Value& other) const;
    std::string typeName() const;

    // Getter methods for native bindings
    int32_t asInt() const;
    double asFloat() const;
    bool asBool() const;
    std::string asString() const;
    Vec3Ptr asVec3() const;
};

struct FunctionObject {
    std::string name;
    size_t arity = 0;
    size_t entryPoint = 0;
    std::shared_ptr<ScriptCode> script;
    std::vector<ValuePtr> captures;
    std::vector<std::string> paramNames;
    std::vector<ValuePtr> defaults;
    bool isBuiltin = false;
    size_t builtinIndex = 0;
    bool isGenerator = false;
    bool isStructConstructor = false;
};

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

// ====================================================================
//  TaggedValue inline definitions
// ====================================================================

inline bool TaggedValue::isTruthy() const {
    if (isBoxed()) {
        switch (tag()) {
            case ValueTag::NIL:   return false;
            case ValueTag::BOOL:  return payload() != 0;
            case ValueTag::INT32: return static_cast<int32_t>(bits_ & PAYLOAD_MASK) != 0;
            case ValueTag::OBJ: {
                auto* hdr = reinterpret_cast<ObjHeader*>(payload());
                if (!hdr) return false;
                switch (hdr->type) {
                    case ObjType::String:  return static_cast<ObjString*>(hdr)->length > 0;
                    case ObjType::Array:   return static_cast<ObjArray*>(hdr)->count > 0;
                    case ObjType::Map:     return !static_cast<ObjMap*>(hdr)->entries.empty();
                    case ObjType::Closure:
                    case ObjType::Class:
                    case ObjType::Instance:
                    case ObjType::Generator:
                    case ObjType::Vec3:
                    case ObjType::Struct:
                    case ObjType::Ffi:
                    case ObjType::RawPtr:
                        return true;
                }
            }
            default:
                return true;
        }
    }
    // Unboxed float: compare bits (positive zero = 0, negative zero = 0x8000000000000000)
    return bits_ != 0 && bits_ != 0x8000000000000000ULL;
}

// ── Conversion helper: TaggedValue → legacy Value ──────────────────
// Builtins must return legacy Value (the std::variant-based type).
// This helper converts a NaN-boxed TaggedValue into the corresponding
// Value variant for return from VM::BuiltinFn callbacks.
inline Value taggedValueToValue(const TaggedValue& tv) {
    if (tv.isNil())    return Value::nil();
    if (tv.isBool())   return Value::fromBool(tv.asBool());
    if (tv.isInt32())  return Value::fromInt(static_cast<int64_t>(tv.asInt32()));
    if (tv.isFloat())  return Value::fromFloat(tv.asFloat());
    if (tv.isString()) return Value::fromString(tv.asStringPtr()->chars);
    if (tv.isVec3()) {
        auto* v = tv.asVec3Ptr();
        return Value::fromVec3(v->x, v->y, v->z);
    }
    if (tv.isPtr())    return Value::fromPtr(tv.asRawPtr());
    if (tv.isFunction()) return Value::fromFunction(
        std::shared_ptr<FunctionObject>(tv.asFunctionPtr(), [](FunctionObject*){}));
    if (tv.isClass())  return Value::fromClass(
        std::shared_ptr<ClassObject>(tv.asClassPtr(), [](ClassObject*){}));
    if (tv.isInstance()) return Value::fromInstance(
        std::shared_ptr<InstanceObject>(tv.asInstancePtr(), [](InstanceObject*){}));
    if (tv.isGenerator()) return Value::fromGenerator(
        std::shared_ptr<GeneratorObject>(tv.asGeneratorPtr(), [](GeneratorObject*){}));
    if (tv.isFfi())    return Value::fromFfi(
        std::shared_ptr<FfiClosure>(tv.asFfiPtr(), [](FfiClosure*){}));
    if (tv.isStruct()) return Value::fromStruct(
        std::shared_ptr<StructObject>(tv.asStructPtr(), [](StructObject*){}));
    if (tv.isArray()) {
        auto* arr = tv.asArrayPtr();
        std::vector<ValuePtr> elems;
        elems.reserve(arr->count);
        for (uint32_t i = 0; i < arr->count; ++i)
            elems.push_back(arr->elements[i]);
        return Value::fromArray(std::move(elems));
    }
    if (tv.isMap()) {
        return Value::fromMap(tv.asMapPtr()->entries);
    }
    return Value::nil();
}

} // namespace kern

#endif // KERN_VALUE_HPP
