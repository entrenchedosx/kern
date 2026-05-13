# OSKit for Kern

OSKit extends Kern for kernel/system-oriented development:

- Low-level memory wrappers (`alloc/free/realloc`, paging maps)
- Port I/O abstraction (`port.write`, `port.read`)
- Interrupt table registration (`interrupts/idt.kn`)
- Cooperative kernel threading scheduler (`scheduler/threading.kn`)
- Drivers (`screen`, `keyboard`, `disk`)
- In-memory VFS (`filesystem/vfs.kn`)
- Boot/build helpers (`boot/builder.kn`)

## Kernel target

Use `kern` kernel backend:

```powershell
kern --target kernel .\examples\oskit_minimal_kernel.kn -o .\dist\kernel.bin --freestanding
kern --target kernel .\examples\oskit_minimal_kernel.kn -o .\dist\kernel.bin --freestanding --iso
```

Optional:

- `--cross-prefix x86_64-elf-`
- `--run-qemu`
- `--linker-script <path>`
