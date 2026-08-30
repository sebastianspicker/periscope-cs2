#!/usr/bin/env python3
"""Check the dependency boundaries shared by the Radar and Vision projects."""

from __future__ import annotations

import ast
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VISION_PACKAGE = ROOT / "vision" / "src" / "cs2_vision_access"
RADAR_ROOT = ROOT / "radar"


def python_imports(path: Path) -> set[str]:
    tree = ast.parse(path.read_text(encoding="utf-8"), filename=str(path))
    module_parts = list(path.relative_to(VISION_PACKAGE).with_suffix("").parts)
    package_parts = module_parts if module_parts[-1] == "__init__" else module_parts[:-1]
    if package_parts and package_parts[-1] == "__init__":
        package_parts.pop()
    imports: set[str] = set()
    for node in ast.walk(tree):
        if isinstance(node, ast.Import):
            imports.update(alias.name for alias in node.names)
        elif isinstance(node, ast.ImportFrom):
            if node.level:
                keep = max(0, len(package_parts) - node.level + 1)
                resolved = ["cs2_vision_access", *package_parts[:keep]]
                if node.module:
                    resolved.extend(node.module.split("."))
                imports.add(".".join(resolved))
            elif node.module:
                imports.add(node.module)
    return imports


def python_module_name(path: Path) -> str:
    parts = list(path.relative_to(VISION_PACKAGE).with_suffix("").parts)
    if parts[-1] == "__init__":
        parts.pop()
    return ".".join(("cs2_vision_access", *parts))


def vision_import_cycles() -> list[list[str]]:
    modules = {python_module_name(path): path for path in VISION_PACKAGE.rglob("*.py")}
    graph = {
        module: {
            imported
            for imported in python_imports(path)
            if imported in modules and imported != module
        }
        for module, path in modules.items()
    }
    index = 0
    stack: list[str] = []
    stacked: set[str] = set()
    indices: dict[str, int] = {}
    lowlinks: dict[str, int] = {}
    cycles: list[list[str]] = []

    def visit(module: str) -> None:
        nonlocal index
        indices[module] = index
        lowlinks[module] = index
        index += 1
        stack.append(module)
        stacked.add(module)
        for dependency in graph[module]:
            if dependency not in indices:
                visit(dependency)
                lowlinks[module] = min(lowlinks[module], lowlinks[dependency])
            elif dependency in stacked:
                lowlinks[module] = min(lowlinks[module], indices[dependency])
        if lowlinks[module] != indices[module]:
            return
        component: list[str] = []
        while stack:
            member = stack.pop()
            stacked.remove(member)
            component.append(member)
            if member == module:
                break
        if len(component) > 1:
            cycles.append(sorted(component))

    for module in graph:
        if module not in indices:
            visit(module)
    return sorted(cycles)


def check_vision(errors: list[str]) -> None:
    rules = {
        "domain": {"application", "workflows", "adapters", "interfaces"},
        "application": {"workflows", "adapters", "interfaces"},
        "workflows": {"adapters", "interfaces"},
        "adapters": {"workflows", "interfaces"},
    }
    prefix = "cs2_vision_access."
    concrete_runtime_modules = {
        "AppKit",
        "Quartz",
        "cupy",
        "cv2",
        "mss",
        "onnxruntime",
        "rfdetr",
        "supervision",
        "tkinter",
        "ultralytics",
    }
    for layer, forbidden in rules.items():
        layer_root = VISION_PACKAGE / layer
        if not layer_root.exists():
            errors.append(f"missing Vision architecture layer: {layer_root.relative_to(ROOT)}")
            continue
        for path in layer_root.rglob("*.py"):
            for imported in python_imports(path):
                imported_root = imported.split(".", 1)[0]
                if layer in {"domain", "application"} and imported_root in concrete_runtime_modules:
                    errors.append(
                        f"{path.relative_to(ROOT)} imports concrete runtime {imported_root}"
                    )
                if not imported.startswith(prefix):
                    continue
                imported_layer = imported[len(prefix) :].split(".", 1)[0]
                if imported_layer in forbidden:
                    errors.append(
                        f"{path.relative_to(ROOT)} imports forbidden layer {imported_layer}: {imported}"
                    )

    legacy_roots = {
        "bakeoff",
        "capture",
        "cli",
        "config",
        "cues",
        "dataset",
        "dataset_split",
        "evaluation",
        "frames",
        "gui",
        "inference",
        "labeling",
        "model_manifest",
        "predictions",
        "prefs",
        "renderer",
        "safety",
        "segmenters",
        "study",
        "training",
        "video",
    }
    for layer in ("domain", "application", "workflows", "adapters"):
        layer_root = VISION_PACKAGE / layer
        if not layer_root.exists():
            continue
        for path in layer_root.rglob("*.py"):
            for imported in python_imports(path):
                if not imported.startswith(prefix):
                    continue
                imported_root = imported[len(prefix) :].split(".", 1)[0]
                if imported_root in legacy_roots:
                    errors.append(
                        f"{path.relative_to(ROOT)} reaches a legacy compatibility facade: {imported}"
                    )

    for cycle in vision_import_cycles():
        errors.append(f"Vision import cycle: {' -> '.join(cycle)}")


def check_radar(errors: list[str]) -> None:
    old_root = RADAR_ROOT / "code"
    if old_root.exists():
        remaining = [path for path in old_root.rglob("*") if path.is_file()]
        if remaining:
            errors.append("radar/code still contains files after the architecture migration")

    for relative in ("src", "scenarios"):
        source_root = RADAR_ROOT / relative
        if not source_root.exists():
            errors.append(f"missing Radar architecture layer: radar/{relative}")
            continue
        for path in source_root.rglob("*"):
            if path.suffix not in {".c", ".cc", ".cpp", ".h", ".hpp"}:
                continue
            content = path.read_text(encoding="utf-8", errors="replace")
            if re.search(r'#\s*include\s*[<"](?:adapters/)?real/', content):
                errors.append(f"{path.relative_to(ROOT)} imports a real adapter")
            if re.search(r"^\s*#\s*if\s+0(?:\s|$)", content, re.MULTILINE):
                errors.append(f"{path.relative_to(ROOT)} contains a disabled implementation block")

    domain_root = RADAR_ROOT / "src" / "domain"
    for path in domain_root.rglob("*"):
        if path.suffix not in {".c", ".cc", ".cpp", ".h", ".hpp"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        if re.search(
            r'#\s*include\s*[<"](?:sim|ac_sim|cs2|fps|lab|teams|application|scenarios|adapters)/',
            content,
        ):
            errors.append(f"{path.relative_to(ROOT)} depends outward from the domain")

    for name in ("DomainTargets.cmake", "SimulationTargets.cmake", "ScenarioTargets.cmake"):
        path = RADAR_ROOT / "cmake" / name
        if path.exists() and '"${RADAR_SOURCE_DIR}/adapters"' in path.read_text(encoding="utf-8"):
            errors.append(f"{path.relative_to(ROOT)} exposes real-adapter headers to an inner target")

    cmake_files = [RADAR_ROOT / "CMakeLists.txt", *sorted((RADAR_ROOT / "cmake").glob("*.cmake"))]
    for path in cmake_files:
        if not path.exists():
            continue
        content = path.read_text(encoding="utf-8")
        if re.search(r"\bfile\s*\(\s*GLOB(?:_RECURSE)?\b", content, re.IGNORECASE):
            errors.append(f"{path.relative_to(ROOT)} uses implicit source globbing")

    root_cmake = RADAR_ROOT / "CMakeLists.txt"
    if not root_cmake.exists():
        errors.append("missing radar/CMakeLists.txt")
        return
    combined = "\n".join(path.read_text(encoding="utf-8") for path in cmake_files if path.exists())
    for option in (
        "LR_ENABLE_REAL_RPM",
        "LR_ENABLE_REAL_SYSCALL",
        "LR_ENABLE_REAL_KERNEL",
        "LR_ENABLE_REAL_VMX",
        "LR_ENABLE_REAL_DMA",
        "LR_ENABLE_REAL_SMM",
        "LR_ENABLE_REAL_GPU",
        "LR_ENABLE_REAL_NET",
        "LR_ENABLE_REAL_UEFI",
        "LR_ENABLE_REAL_LINUX",
        "LR_ENABLE_REAL_ALL",
    ):
        pattern = rf'^\s*option\s*\(\s*{option}\s+"[^"]*"\s+OFF\s*\)'
        if not re.search(pattern, combined, re.IGNORECASE | re.MULTILINE):
            errors.append(f"{option} is not explicitly declared OFF by default")

    simulation_targets = RADAR_ROOT / "cmake" / "SimulationTargets.cmake"
    if simulation_targets.exists():
        content = simulation_targets.read_text(encoding="utf-8")
        if "shellcode.cpp" in content or "adapters/real" in content:
            errors.append("default simulation target includes an opt-in real source")

    for obsolete_option in (
        "LR_ENABLE_XORSTR_OBFUSCATION",
        "LR_ENABLE_PEB_EAT_RESOLUTION",
        "LR_ENABLE_HIJACK_READER",
        "LR_ENABLE_PERISCOPE_ENTITY",
        "LR_ENABLE_PERISCOPE_HUD",
        "LR_ENABLE_PERISCOPE_RADAR",
        "LR_ENABLE_PERISCOPE_OVERLAY",
        "LR_ENABLE_BATCH_READ_JITTER",
        "LR_ENABLE_MEMORY_HIDE",
        "LR_ENABLE_DECOY_RENDER",
        "LR_ENABLE_TEMPORAL_PHASE",
    ):
        if re.search(rf"\b{obsolete_option}\b", combined):
            errors.append(f"obsolete decorative option remains: {obsolete_option}")

    real_targets = RADAR_ROOT / "cmake" / "RealTargets.cmake"
    app_targets = RADAR_ROOT / "cmake" / "AppTargets.cmake"
    if real_targets.exists():
        content = real_targets.read_text(encoding="utf-8")
        if "LR_REAL_PLATFORM_SOURCES" in content:
            errors.append("real capabilities are aggregated into a platform source bag")
        for option, target in (
            ("LR_ENABLE_REAL_RPM", "ac_real_rpm"),
            ("LR_ENABLE_REAL_SYSCALL", "ac_real_syscall"),
            ("LR_ENABLE_REAL_KERNEL", "ac_real_kernel"),
            ("LR_ENABLE_REAL_VMX", "ac_real_vmx"),
            ("LR_ENABLE_REAL_DMA", "ac_real_dma"),
            ("LR_ENABLE_REAL_GPU", "ac_real_gpu"),
            ("LR_ENABLE_REAL_NET", "ac_real_net"),
            ("LR_ENABLE_REAL_SMM", "ac_real_smm"),
            ("LR_ENABLE_REAL_UEFI", "ac_real_uefi"),
        ):
            if option not in content or target not in content:
                errors.append(f"real capability isolation missing: {option} -> {target}")
        if "LR_ENABLE_SHELLCODE_INJECTION" not in combined or not re.search(
            r"LR_ENABLE_SHELLCODE_INJECTION requires LR_ENABLE_REAL_SYSCALL=ON", combined
        ) or not re.search(
            r"LR_ENABLE_SHELLCODE_INJECTION requires a 64-bit Windows lab host", combined
        ):
            errors.append("shellcode capability matrix lacks syscall and Windows x64 gates")
        if not re.search(
            r"if\s*\(LR_ENABLE_SHELLCODE_INJECTION\).*?"
            r"lr_real\s*\(ac_real_shellcode\s+\$\{LR_REAL_SHELLCODE_SOURCES\}\).*?"
            r"target_link_libraries\s*\(ac_real_shellcode\s+PUBLIC\s+ac_real_syscall\)",
            content,
            re.DOTALL,
        ):
            errors.append("shellcode target is not isolated behind its capability gate")
    if app_targets.exists():
        content = app_targets.read_text(encoding="utf-8")
        if not re.search(
            r"if\s*\(TARGET\s+ac_real_shellcode\).*?"
            r"target_link_libraries\s*\(ac_real_pipeline\s+PUBLIC\s+ac_real_shellcode\)",
            content,
            re.DOTALL,
        ):
            errors.append("real pipeline does not link the enabled shellcode capability")

    radar_ci = ROOT / ".github" / "workflows" / "radar-ci.yml"
    if not radar_ci.exists():
        errors.append("missing Radar Windows real-capability CI lane")
    else:
        content = radar_ci.read_text(encoding="utf-8")
        required_ci_tokens = (
            "windows-latest",
            "LR_ENABLE_REAL_RPM=ON",
            "LR_ENABLE_REAL_SYSCALL=ON",
            "LR_ENABLE_REAL_GPU=ON",
            "LR_ENABLE_SHELLCODE_INJECTION=ON",
            "--target ac_real_pipeline",
        )
        if any(token not in content for token in required_ci_tokens):
            errors.append("Radar CI does not build the supported shellcode capability combination")

    forbidden_simulation_markers = (
        r"LR_SIMULATION_REAL_PORT",
        r"\breal::",
        r"windows\.h",
        r"\b(?:QueryPerformance|VirtualQuery|GetCurrentProcess|GetModuleFile|"
        r"CreateFile|DeleteFile|FindFirst|FindNext|RegOpen|RegDelete|RegEnum|"
        r"GetEnvironment|PowerCreateRequest|steady_clock|random_device)\b",
        r"\b__rdtscp\b",
    )
    for path in (RADAR_ROOT / "src" / "simulation").rglob("*"):
        if path.suffix not in {".c", ".cc", ".cpp", ".h", ".hpp"}:
            continue
        content = path.read_text(encoding="utf-8", errors="replace")
        if any(re.search(marker, content) for marker in forbidden_simulation_markers):
            errors.append(f"simulation source retains a host-operation marker: {path.relative_to(ROOT)}")

    if (RADAR_ROOT / "src" / "simulation" / "ac_sim" / "shellcode.cpp").exists():
        errors.append("operational shellcode remains in the simulation layer")
    real_pipeline = RADAR_ROOT / "adapters" / "real" / "application" / "radar_pipeline.hpp"
    sim_pipeline = RADAR_ROOT / "src" / "application" / "radar_pipeline.hpp"
    if real_pipeline.exists() and sim_pipeline.exists():
        content = real_pipeline.read_text(encoding="utf-8")
        if "namespace radar::real_adapter" not in content or "class RealFramePipeline" not in content:
            errors.append("real pipeline does not have an unambiguous adapter type name")


def main() -> int:
    errors: list[str] = []
    check_radar(errors)
    check_vision(errors)
    if errors:
        print("Architecture check failed:", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1
    print("Architecture checks passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
