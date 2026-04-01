import sys

from kern_to_exe.cli import try_headless

if __name__ == "__main__":
    code = try_headless()
    if code is not None:
        sys.exit(code)
    from kern_to_exe.app import main

    main()
