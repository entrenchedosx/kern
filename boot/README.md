# Kern Kernel Build Outputs

`kern --target kernel` emits:

- `kernel.bin` (freestanding kernel image)
- optional `bootable.iso` when `--iso` is used

Default generated linker script and GRUB config are created in temporary build output under `.kern-kernel-build`.