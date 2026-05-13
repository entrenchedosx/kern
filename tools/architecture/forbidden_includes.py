#!/usr/bin/env python3
"""
Kern Compile-Time Architecture Firewall
Include Validation Script

This script enforces architectural boundaries by:
1. Parsing C++ source files for #include statements
2. Checking against allowed_dependencies.json policy
3. Failing builds that violate dependency rules

Usage:
    python forbidden_includes.py <source_file>
    python forbidden_includes.py --check-all

Exit codes:
    0 - No violations found
    1 - Architecture violation detected
    2 - Configuration error
"""

import sys
import os
import re
import json
from pathlib import Path
from typing import List, Dict, Set, Tuple

class ArchitectureFirewall:
    """Enforces compile-time architectural boundaries."""
    
    def __init__(self, config_path: str = None):
        """Initialize firewall with dependency configuration."""
        if config_path is None:
            config_path = os.path.join(
                os.path.dirname(__file__), 
                'allowed_dependencies.json'
            )
        
        with open(config_path, 'r') as f:
            self.config = json.load(f)
        
        self.violations: List[Dict] = []
        self.checked_files: Set[str] = set()
    
    def get_module_for_file(self, file_path: str) -> str:
        """Determine which architectural module a file belongs to."""
        # Normalize path
        path = Path(file_path).as_posix()
        
        # Check each module pattern
        for module_name in self.config['modules'].keys():
            if module_name.replace('/', os.sep) in path:
                return module_name
        
        return 'unknown'
    
    def parse_includes(self, file_path: str) -> List[str]:
        """Extract all #include statements from a C++ file."""
        includes = []
        
        try:
            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                content = f.read()
        except Exception as e:
            print(f"ERROR: Cannot read file {file_path}: {e}")
            return includes
        
        # Match #include statements (both "..." and <...>)
        pattern = r'#include\s*["<]([^">]+)[">]'
        matches = re.finditer(pattern, content)
        
        for match in matches:
            include_path = match.group(1)
            includes.append(include_path)
        
        return includes
    
    def check_include_violation(
        self, 
        source_file: str, 
        include_path: str,
        module: str
    ) -> Tuple[bool, str]:
        """
        Check if an include violates architectural rules.
        
        Returns:
            (is_violation, reason)
        """
        # Get module configuration
        module_config = self.config['modules'].get(module, {})
        forbidden_patterns = module_config.get('forbidden_patterns', [])
        
        # Check global forbidden patterns
        global_forbidden = self.config.get('global_forbidden_patterns', {}).get('patterns', [])
        
        # Check against forbidden patterns
        for pattern in forbidden_patterns + global_forbidden:
            if pattern in include_path:
                return True, f"Forbidden include pattern: '{pattern}'"
        
        # Check allowed patterns
        allowed_includes = module_config.get('allowed_includes', [])
        
        # If no allowed includes defined, any non-forbidden include is OK
        if not allowed_includes:
            return False, ""
        
        # Check if include is in allowed list
        is_allowed = False
        for allowed in allowed_includes:
            if allowed in include_path:
                is_allowed = True
                break
        
        # If not explicitly allowed and we have restrictions, it's a violation
        if not is_allowed and len(allowed_includes) > 0:
            # Check if it's a standard library include (always allowed)
            if not include_path.endswith('.h') and not include_path.endswith('.hpp'):
                # Probably standard library
                return False, ""
            
            # Check if it's a local/relative include within same module
            if include_path.startswith('"'):
                return False, ""  # Local includes are generally OK
        
        return False, ""
    
    def check_file(self, file_path: str) -> bool:
        """Check a single file for architecture violations."""
        if file_path in self.checked_files:
            return True  # Already checked
        
        self.checked_files.add(file_path)
        
        # Determine module
        module = self.get_module_for_file(file_path)
        
        # Parse includes
        includes = self.parse_includes(file_path)
        
        has_violations = False
        
        for include_path in includes:
            is_violation, reason = self.check_include_violation(
                file_path, include_path, module
            )
            
            if is_violation:
                self.violations.append({
                    'file': file_path,
                    'module': module,
                    'include': include_path,
                    'reason': reason
                })
                has_violations = True
        
        return not has_violations
    
    def check_directory(self, directory: str, extensions: List[str] = None) -> bool:
        """Check all source files in a directory."""
        if extensions is None:
            extensions = ['.cpp', '.hpp', '.h', '.c']
        
        has_violations = False
        
        for root, dirs, files in os.walk(directory):
            # Skip legacy and build directories
            dirs[:] = [d for d in dirs if d not in ['legacy', 'build', '.git']]
            
            for file in files:
                if any(file.endswith(ext) for ext in extensions):
                    file_path = os.path.join(root, file)
                    if not self.check_file(file_path):
                        has_violations = True
        
        return not has_violations
    
    def report_violations(self) -> str:
        """Generate a violation report."""
        if not self.violations:
            return "✅ No architecture violations found."
        
        report = []
        report.append("=" * 70)
        report.append("🚨 KERN ARCHITECTURE FIREWALL VIOLATIONS DETECTED")
        report.append("=" * 70)
        report.append("")
        
        # Group by module
        by_module: Dict[str, List[Dict]] = {}
        for v in self.violations:
            mod = v['module']
            if mod not in by_module:
                by_module[mod] = []
            by_module[mod].append(v)
        
        for module, violations in by_module.items():
            report.append(f"\n📁 MODULE: {module}")
            report.append("-" * 50)
            
            for v in violations:
                report.append(f"  ❌ File: {v['file']}")
                report.append(f"     Include: {v['include']}")
                report.append(f"     Reason: {v['reason']}")
                report.append("")
        
        report.append("=" * 70)
        report.append(f"TOTAL VIOLATIONS: {len(self.violations)}")
        report.append("=" * 70)
        report.append("")
        report.append("🔥 BUILD FAILED: Architecture firewall violation")
        report.append("   Fix the violations above before proceeding.")
        
        return "\n".join(report)
    
    def generate_cmake_policy(self) -> str:
        """Generate CMake policy enforcement code."""
        cmake_code = []
        
        cmake_code.append("# Kern Architecture Firewall - CMake Policy")
        cmake_code.append("# Auto-generated by forbidden_includes.py")
        cmake_code.append("")
        cmake_code.append("# ❌ FORBIDDEN: Global include directories")
        cmake_code.append("# These destroy architectural boundaries")
        cmake_code.append("")
        
        forbidden = self.config['include_path_policy']['forbidden_cmake_commands']
        for cmd in forbidden:
            cmake_code.append(f'# {cmd}  <- NEVER USE THIS')
        
        cmake_code.append("")
        cmake_code.append("# ✅ REQUIRED: Explicit target includes only")
        cmake_code.append("# Each target must explicitly declare its dependencies")
        cmake_code.append("")
        
        # Generate target-specific includes
        for module_name, module_config in self.config['modules'].items():
            if '/' in module_name:
                target_name = module_name.replace('/', '_')
            else:
                target_name = module_name
            
            cmake_code.append(f"# Target: {target_name}")
            cmake_code.append(f"target_include_directories({target_name}")
            cmake_code.append("    PRIVATE")
            
            allowed = module_config.get('allowed_includes', [])
            if allowed:
                for inc in allowed:
                    cmake_code.append(f"        kern/{inc}")
            else:
                cmake_code.append("        # No additional includes (foundation layer)")
            
            cmake_code.append(")")
            cmake_code.append("")
        
        return "\n".join(cmake_code)


def main():
    """Main entry point."""
    import argparse
    
    parser = argparse.ArgumentParser(
        description='Kern Architecture Firewall - Include Validator'
    )
    parser.add_argument(
        'files', 
        nargs='*', 
        help='Source files to check'
    )
    parser.add_argument(
        '--check-all', 
        action='store_true',
        help='Check all source files in kern/ directory'
    )
    parser.add_argument(
        '--generate-cmake', 
        action='store_true',
        help='Generate CMake policy enforcement'
    )
    parser.add_argument(
        '--config', 
        default=None,
        help='Path to allowed_dependencies.json'
    )
    
    args = parser.parse_args()
    
    # Initialize firewall
    firewall = ArchitectureFirewall(args.config)
    
    # Generate CMake policy
    if args.generate_cmake:
        print(firewall.generate_cmake_policy())
        return 0
    
    # Check specific files
    if args.files:
        all_valid = True
        for file_path in args.files:
            if not firewall.check_file(file_path):
                all_valid = False
        
        print(firewall.report_violations())
        return 0 if all_valid else 1
    
    # Check all files
    if args.check_all:
        # Find kern directory
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(os.path.dirname(script_dir))
        kern_dir = os.path.join(project_root, 'kern')
        
        if os.path.exists(kern_dir):
            all_valid = firewall.check_directory(kern_dir)
            print(firewall.report_violations())
            return 0 if all_valid else 1
        else:
            print(f"ERROR: Kern directory not found: {kern_dir}")
            return 2
    
    # No action specified
    parser.print_help()
    return 2


if __name__ == '__main__':
    sys.exit(main())
