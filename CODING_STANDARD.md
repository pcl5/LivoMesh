## LivoMesh 编码与协作规范

### 1. 目录结构与模块拆分
- 对外暴露的头文件放在 `include/` 下，遵循“一个组件一个头文件 + 同名实现文件”。
- 运行期配置集中在 `config/fast_livo2.yaml`，Base/Filter 分组已搭好，新增字段需落位到对应小节。
- 大型数据不入仓：仅在 `data/FAST_LIVO2/` 下按照 `plan.txt` 所示布局（images/depths/poses/…）准备本地运行数据。
- 如需 TSDF 之外的公共工具，放在 `3rdparty/` 或独立子目录，避免污染核心流水线。

### 2. 构建、运行与测试
- 统一采用 CMake out-of-tree 构建：
  ```bash
  cmake -S . -B build -DCMAKE_BUILD_TYPE=RelWithDebInfo
  cmake --build build -j$(nproc)
  ```
- 主程序以 YAML 配置驱动，常规验证命令：
  ```bash
  ./build/livomesh_app config/fast_livo2.yaml
  ```
- 单元/集成测试全部挂到 CTest，下游统一执行：
  ```bash
  ctest --test-dir build --output-on-failure
  ```
- 若添加新脚本或工具，确保其可在 README/该文档中找到调用方式，并在 CI 前自行跑通 `cmake --build` 与 `ctest`。

### 3. C++ 编码风格
- 语言标准：C++17+，遵循 clang-format 默认风格及 4 空格缩进。
- 命名空间统一置于 `tsdf::` 之下；类/结构使用 PascalCase，函数 camelCase，常量 SCREAMING_SNAKE_CASE。
- 头文件自包含，避免通过包含顺序隐式满足依赖；实现文件与头文件同名。
- 注释以中文简要说明复杂逻辑或约束，禁止 emoji；能自解释的代码不额外注释。
- 所有运行时参数须通过 `include/params.h` 内的结构体描述，提供稳健的解析与校验逻辑，并在 `interface.txt` 里同步接口说明。

### 4. 配置与数据约定
- `fast_livo2.yaml` 至少包含：
  ```yaml
  Base:
      cuda_en: False
      pcl_type: 0            # 0: pcd, 1: ply
      pcl_load: -1           # -1: 整图; 1: 多帧
      data_path: ./data/FAST_LIVO2/
      depth_path: ...
  Filter:
      Radius: 0.08
      Max error: 1.00
  ```
- 所有新增参数须：
  1. 在 YAML 中归入 Base/Filter 子节；
  2. 在 `tsdf::AppConfig` 中声明并实现解析；
  3. 更新 `interface.txt` 描述输入输出变化；
  4. 在 PR 说明中告知依赖的数据/路径变更。
- 数据目录需包含 `color_poses.txt`、`depth_poses.txt`、`depths/`、`images/`、`xiaojuchang.pcd` 等文件夹/文件；若数据有额外需求，记录到 `data/FAST_LIVO2/README.md`。
- 不将大型资产、敏感数据提交到 git；如需示例，提供脚本或 README 指引。

### 5. 测试策略
- 采用 GoogleTest，测试文件放在 `src/tests/`，命名遵循 `xxx_test.cc`，并镜像生产目录结构。
- 测试命名为 `ModuleName_Scenario_Expectation`，覆盖关键边界：稀疏帧、噪声、高斯滤波 CUDA 开关等。
- 集成测试运行 `livomesh_app` 对精简版 FAST_LIVO2 数据集，验证 IO、滤波和 TSDF 管线。
- 必要时通过 `ctest -T Coverage` 收集覆盖率，确保新增模块有可验证路径。

### 6. 提交流程
- Git 使用 Conventional Commits（例如 `feat: fuse multi-frame clouds`、`fix: guard empty depth tiles`）。
- 每个 PR 需：
  - 关联追踪 issue；
  - 总结配置或参数改动；
  - 若渲染效果有变化，附上前后对比图（mesh 截图等）；
  - 说明运行所需的新数据（如“requires xiaojuchang.pcd”）；
  - 明确 `cmake --build` 与 `ctest` 结果，若跳过测试需给出原因。

### 7. 文档与接口
- 新模块完成后，在 `interface.txt` 中添加接口签名与行为说明。
- 若模块具备独立参考实现/示例，可在 `reference/` 中补充，并在本规范或 README 中链接。
- 更新配置解析、数据接口等核心模块时，同时维护 `AGENTS.md` / 本规范，确保团队共享同一约束。

按照以上规范交付模块，可保证 LivoMesh 各组件结构清晰、配置可复现、测试可追踪，便于后续并行开发。
