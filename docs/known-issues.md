# 已知问题

## KI-1（客户端）：Release 客户端堆损坏崩溃

状态：**未修复（Unresolved，正式客户端发布阻断项）**

2026-08-07 三次真实复现（`build-staging-release` ×2、`build-release` ×1）：
`NeurolingsCE.exe` 以 `0xc0000374`（堆损坏）崩溃在 ntdll，错误偏移均为
`0x118ba5`（WER：`PCH_C2_FROM_ntdll+0x1617E4`）。带 PDB 的
RelWithDebInfo 单次观察未复现，暂无可用崩溃栈。

完整证据、复现步骤、cdb 模板与下一步调查计划见
[`docs/KI1-handoff.md`](KI1-handoff.md)。

## KI-1：Debug/RelWithDebInfo 下 `NeurolingsCETests` 引擎测试崩溃

状态：**未修复（独立问题，与 Mascot Registry 安全修正无关）**

### 现象

- Debug（MSVC 2022 BuildTools）与 RelWithDebInfo（MSVC 2022/2026）的
  `NeurolingsCETests` 无法通过：表现为 0xC0000005 退出或在同一位置挂起
  （2026-08-07 复测：两者均在 hotspot/window-push 区域停止，Debug 挂起
  超过 60 秒、RelWithDebInfo 挂起超过 45 秒）；
- Release 配置同一测试通过（2/2，含全部 GitHub/投稿回归测试）；
- 崩溃前会打印 3 条 hotspot 断言失败：
  `hotspot fixture should resolve Pat at the pressed coordinate`、
  `first completed pat action should restart Pat`、
  `second completed pat action should restart Pat while held`。

### 最小复现命令

```powershell
cmake --build build-debug --config Debug --parallel 8
ctest --test-dir build-debug -C Debug --output-on-failure
# 或直接运行：
.\build-debug\bin\NeurolingsCETests.exe
```

### 崩溃栈（cdb，Debug 构建）

```text
NeurolingsCETests!std::_Func_class<bool,double,double>::operator()+0x44
NeurolingsCETests!shijima::mascot::environment::request_window_push+0x86
NeurolingsCETests!`anonymous namespace'::testWindowPushBehaviorGate+0x57a
NeurolingsCETests!main+0xa3
```

崩溃点在 `request_window_push` 调用 `window_push_callback` 时，`std::function`
内部指针已被破坏（读地址 0x4c19a0118 等无效地址）。

### 待查方向

- `testWindowPushBehaviorGate` 中 `env->window_push_callback` 的生命周期
  （lambda 捕获 `pushes` 引用，`env` 为 shared_ptr）；
- 前置 hotspot 测试失败是否表明引擎行为/未初始化数据在 Debug 下不同；
- 是否有早期测试（如包验证 ZIP 解析）写入越界导致堆损坏；
- 不同构建配置行为不同可能来自未定义行为、生命周期错误或测试顺序依赖。

### 处理原则

- 不得把“Release 通过”当作健康证明；
- 在修复前，客户端/服务的状态按本仓库 `IMPLEMENTATION_PLAN.md` 与
  交付报告分组件标记；
- 修复时补充最小复现测试并验证 Debug/RelWithDebInfo/Release 三配置。

### 本轮状态（2026-08-07）

- 仍为正式客户端发布阻断项；
- 本轮未执行 ASan/Application Verifier/PageHeap，也未完成单测试隔离
  （测试可执行文件不支持过滤）；仅保留 cdb 崩溃栈与复现命令；
- 复测确认同一位置存在“崩溃”与“挂起”两种表现，符合未定义行为/
  时序相关特征，而非单纯的断言失败；
- 后续优先验证：单跑 `testWindowPushBehaviorGate` 是否独立崩溃、前置
  包验证测试是否造成堆损坏、`env->window_push_callback` 是否在 tick 期间
  被覆盖/释放。

## KI-2：Release 退出崩溃（已复核，不再复现）

上一轮在陈旧 `build-release/bin/NeurolingsCE-cli.exe` 上复现的
`QString::~QString` 退出崩溃，在当前 MSVC 2026 Insiders 工具链全量重建后
不再复现（`--version`/`--help`/`--mascot validate` 退出码均正常，
CTest Release 2/2）。结论为旧工具链/旧产物问题；正式发布仍建议使用
MSVC 2022 稳定工具链。
