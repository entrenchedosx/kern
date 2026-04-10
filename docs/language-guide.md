# Language guide

Kern is a compiled, bytecode-interpreted language with Python-like surface syntax and explicit system-facing APIs.

## Syntax and semantics

- **Full reference:** [LANGUAGE_SYNTAX.md](LANGUAGE_SYNTAX.md)
- **Strict typing notes:** [STRICT_TYPES.md](STRICT_TYPES.md)
- **Standard library (`std.v1.*`):** [STDLIB_STD_V1.md](STDLIB_STD_V1.md)
- **Builtin surface (quick ref):** [BUILTIN_REFERENCE.md](BUILTIN_REFERENCE.md)

## Pipeline (learning / debugging)

```bash
kern --check script.kn
kern --ast script.kn
kern --bytecode script.kn
```

## Trust and capabilities

- [TRUST_MODEL.md](TRUST_MODEL.md)
- Runtime guard flags on `kern` (`--allow-unsafe`, permissions) — see `kern --help`.
