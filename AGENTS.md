# Repository Guidelines

## Project Structure & Module Organization
`src/` holds the TSDF reconstruction pipeline (ingestion, filtering, TSDF fusion, mesh export) and should be split into small modules so each interface can be documented in `interface.txt`. Public headers belong in `include/` with one header per component. Runtime parameters live in `config/fast_livo2.yaml`; keep CUDA toggles, point-cloud paths, and filter knobs grouped under Base/Filter as already scaffolded. Large assets are not committed—stage LIVO frames under `data/FAST_LIVO2/` using the plan.txt layout (`images/`, `depths/`, `poses/`, etc.) so developers can reproduce runs locally without bloating the repo.

## Build, Test, and Development Commands
Use a standard CMake out-of-tree build:
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build -j$(nproc)
```
After building, run the main executable with the YAML config to validate IO wiring:
```bash
./build/livomesh_app config/fast_livo2.yaml
```
Tests should be wired into CTest so `ctest --test-dir build --output-on-failure` becomes the single validation step in CI or before commits.

## Coding Style & Naming Conventions
Stick to modern C++ (C++17 or later), 4-space indentation, and clang-format defaults to keep headers and sources aligned. Namespaces should stay under `tsdf::`, classes/structs use PascalCase (`TsdfReconstructor`), functions use camelCase, and constants use SCREAMING_SNAKE_CASE. Keep headers self-contained, pair them with identically named `.cc` files, and write concise Chinese comments only where the logic is non-obvious.

## Testing Guidelines
Adopt GoogleTest for unit coverage; co-locate test files under `src/tests/` mirroring the production folder names (e.g., `tsdf_reconstructor_test.cc`). Name test cases `ModuleName_Scenario_Expectation` so failures are searchable. Aim for edge-condition coverage (sparse frames, noisy lidar, CUDA disabled). Integration tests should exercise the dataset ingestion path by running the app on a trimmed `FAST_LIVO2` subset. Collect coverage via `ctest -T Coverage` when possible.

## Commit & Pull Request Guidelines
Initialize git (if not already) and follow Conventional Commits (`feat: fuse multi-frame clouds`, `fix: guard empty depth tiles`). Each PR should link to a tracking issue, summarize configuration changes, include before/after mesh screenshots when rendering shifts, and note any data prerequisites (e.g., “requires xiaojuchang.pcd”). Reviewers expect `cmake --build` and `ctest` to pass; describe any skipped tests explicitly and explain why.

## Data & Configuration Tips
Keep sensitive or heavy datasets out of git—document required files inside `data/FAST_LIVO2/README.md` instead. When tweaking `fast_livo2.yaml`, validate new paths with `loadAppConfig` before merging and update `interface.txt` whenever you touch module inputs/outputs so downstream agents have accurate contracts.
