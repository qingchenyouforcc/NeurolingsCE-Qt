# scripting/duktape 嵌入式 Duktape

本目录包含 Duktape JavaScript 引擎的 amalgamated 源码和平台配置。NeurolingsCE 通过上级 scripting/context.cc 使用它；这里不定义 mascot 业务 API。

## 文件说明

| 文件 | 作用 |
|---|---|
| duktape.h | Duktape 公共 C API。 |
| duktape.cc | Duktape amalgamated 实现，提供脚本虚拟机。 |
| duk_config.h | 当前平台/编译器的 Duktape 配置。 |
| _duk_config.h | Duktape 配置的内部/兼容头。 |

这是 vendored 代码。修改前先确认是否可以在 context.cc、变量桥接或编译选项中解决；若确需升级/补丁，保留来源和许可证信息，并运行 scripting 超时与异常测试。
