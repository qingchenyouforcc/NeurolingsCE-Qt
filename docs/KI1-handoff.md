# KI-1 交接文档：Release 客户端堆损坏崩溃（Unresolved）

> 本文档写给后续接手的高级模型/工程师。目标只有一个：定位并修复
> **Release 客户端 `NeurolingsCE.exe` 的偶发堆损坏崩溃**（非 Debug 测试崩溃）。
> 不要重新审计 Mascot Registry，不要扩大为无目的的压力测试。

## 1. 状态

- **KI-1：Unresolved**，正式客户端发布阻断项。
- 已确认不是“测试自身错误”，也不是旧的 Debug/RelWithDebInfo 测试崩溃
  （旧测试崩溃见 `docs/known-issues.md`，是 `NeurolingsCETests` 的
  hotspot/window-push 问题，与本文客户端崩溃是两件事）。
- 已确认 Release 客户端在 2026-08-07 有 **3 次真实崩溃记录**（见下），
  不是单次偶发。
- 尚未拿到可用的崩溃栈（WER dump 被系统清理；带 PDB 的
  RelWithDebInfo 构建在单次观察中未复现）。

## 2. 崩溃证据（Windows Application 事件日志）

3 次崩溃全部为 `0xc0000374`（堆损坏被 ntdll 检测到），故障模块均为
`ntdll.dll`，错误偏移完全相同：`0x0000000000118ba5`，WER 摘要为
`PCH_C2_FROM_ntdll+0x00000000001617E4`。

| 时间 | 二进制 | 说明 |
| --- | --- | --- |
| 2026-08-07 15:48:06 | `build-staging-release/bin/NeurolingsCE.exe`（0.5.1.0，15:32 构建） | 早期 staging 观察期间 |
| 2026-08-07 16:12:44 | 同上 | 早期 staging 观察期间 |
| 2026-08-07 17:59:42 | `build-release/bin/NeurolingsCE.exe`（= `out/build/x64-Release`，15:43 构建） | 本轮观察期间；应用启动约 4 分钟后，已执行过若干 UI Automation 读取，无用户输入 |

查询命令（PowerShell）：

```powershell
Get-WinEvent -FilterHashtable @{LogName='Application'; Id=1000; StartTime=(Get-Date).AddHours(-12)} |
  Where-Object { $_.Message -match 'NeurolingsCE.exe' } |
  ForEach-Object { $_.Message }
```

注意：同一观察窗口内还有多起 `NeurolingsCETests.exe`（Debug）的
`0xc0000005`，属于旧的测试问题，不要混淆。

## 3. 已做过的验证（不要重复全量重做）

### 3.1 正常使用观察（2026-08-07 17:5x，build-release）

- 启动 Manager，主窗口正常出现。
- 通过 UI Automation（`SelectionItemPattern.Select`）切换
  Home/Store/Create/Combinations/Settings/About 页面。
- 点击“随机生成”启动 mascot，`NeurolingsCE-cli --json --list` 确认
  “Vedaling” 会话在运行。
- 有 mascot 时点关闭按钮会把窗口隐藏到托盘（设计行为，进程不退出）；
  `NeurolingsCE-cli --json --stop` 正常退出，无残留进程。
- 结果：**观察期间 build-release 崩溃一次**（17:59:42，见上表）。

### 3.2 cdb 观察（带 PDB 的 RelWithDebInfo，同一份源码）

- `build-relwithdebinfo`（2026-08-07 16:51 构建，含完整 PDB）在 cdb 下
  执行同样流程（页面切换 + 启动 mascot + `--stop`），**未复现崩溃**，
  cdb 干净退出。
- 因此本机当前没有“崩溃二进制 + 匹配 PDB”的可用组合；下一步需要先
  制造这个组合（见第 5 节）。

### 3.3 已有 cdb 命令模板

`-cf` 配合 `-g` 不可靠（`-g` 会跳过初始断点导致命令文件不执行）。
可用的模板（已实测能正确设置过滤并继续运行）：

```text
sxe -c ".echo ====KI1_CRASH====; !analyze -v; k; q" c0000374
sxe -c ".echo ====KI1_CRASH====; !analyze -v; k; q" c0000005
sxn e06d7363
sxn 80000003
sxn 4000001f
g
```

启动方式：

```powershell
$cdb='C:\Program Files (x86)\Windows Kits\10\Debuggers\x64\cdb.exe'
& $cdb -G -cf .tmp\ki1-cdb-cmds.txt -y <pdb目录> <exe>
```

注意：cdb 被调试进程挂起时，`NeurolingsCE-cli` 会超时并尝试拉起第二个
实例，产生干扰；CLI 检查必须在被调试进程可响应时进行。

## 4. 已知线索与假设（按置信度排序，均未证实）

1. **Release 特有 / 优化相关**：3 次崩溃都在优化构建（Release /
   staging-Release）；带 PDB 的 RelWithDebInfo（同样 /O2）单次未复现，
   不排除布局/时序差异。崩溃点全部在 ntdll 堆校验，属于“事后发现”，
   真正的破坏点可能在崩溃前任意时刻。
2. **UI Automation / Qt 辅助功能路径**：观察期间 UIA 读取会触发一次
   OLEAUT32 的可恢复 C++ 异常（`8002801D Library not registered`，
   `Oleacc.dll` 加载后出现），cdb 日志可见。不确定它是否是破坏者，
   但 3 次崩溃中有 1 次（17:59:42）确实发生在 UIA 访问之后。
3. **时序相关**：崩溃并非启动即发生（17:59:42 那次约 4 分钟后）；
   可能涉及定时器/窗口观察器/更新检查/后台网络。
4. 与旧的 `NeurolingsCETests` hotspot 崩溃**无关**（不同可执行文件、
   不同代码路径）。

## 5. 下一步建议（按优先级）

1. **先制造“崩溃二进制 + 匹配 PDB”组合**：用同一份当前源码新开一个
   Release 构建目录，配置
   `-DCMAKE_MSVC_DEBUG_INFORMATION_FORMAT=ProgramDatabase`
   （或直接使用 `RelWithDebInfo`），确认规则：后续分析全部基于该组合。
2. **自动抓 dump**：为本机启用 WER LocalDumps（DumpType=2，
   DumpFolder 指向仓库 `.tmp\dumps`），或直接在被调试器下复现；
   拿到 dump 后优先执行 `!analyze -v`、`k`、`.ecxr`、
   `!heap -p -a <bad address>`。
3. **PageHeap / Application Verifier**：对上述组合启用
   `gflags /p /enable NeurolingsCE.exe /full` 或 AppVerifier Heap 校验，
   让破坏点直接崩溃到精确指令。注意会改变堆布局与时序，可能降低
   复现率，但一旦复现定位价值最高。
4. **对照实验**：区分 UIA 是否必要——
   a) 纯启动 + 空闲 5 分钟（无任何 UIA）；b) 启动 + 同样 UIA 序列；
   各跑 1-3 次即可，不要刷几十次。
5. **怀疑面**（拿到栈之前不要大改代码）：
   - Qt 6.8.3 在 Windows 上的 UIA/辅助功能桥（可尝试在无
     accessibility 查询下运行对比）；
   - ElaWidgetTools 自定义控件与 Qt 辅助功能的交互；
   - 全局/静态对象生命周期、`std::function` 回调、定时器回调、
     窗口观察器（`windowObserver`）与平台线程；
   - 堆破坏优先怀疑点：任何持有裸指针/`reinterpret_cast`、
     `QListWidgetItem` 生命周期、线程间共享 `QObject` 的地方。
6. **修复原则**：根因明确后才做最小修复 + 回归测试；不得用
   sleep/重试/吞异常绕过；不得因“Release 通过”或“本轮未复现”而关闭
   KI-1；修复后至少在 Release（含 PDB 组合）下复跑一次真实路径。

## 6. 相关文件/产物

- 崩溃二进制：`build-release/bin/NeurolingsCE.exe`（无 PDB）、
  `build-staging-release/bin/NeurolingsCE.exe`（无 PDB）。
- 带 PDB 的同源码构建：`build-relwithdebinfo/bin/NeurolingsCE.exe` +
  `NeurolingsCE.pdb`（16:51 构建，本轮 cdb 未复现）。
- cdb 日志：`.tmp/ki1-cdb-run.log`、`.tmp/ki1-cdb-run2.log`（gitignore）。
- 截图：`.tmp/ki1-manager.png`。
- 旧测试崩溃文档：`docs/known-issues.md`。
- 构建入口：`src/tools/build-windows-ninja.cmd`（固定代码页，与崩溃无关，
  但新构建目录请用它，避免 msvc_deps_prefix 编码问题）。
