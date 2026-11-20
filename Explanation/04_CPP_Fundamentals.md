# 04 · C++ Fundamentals: StateHandoff

**Rule:** only the C++ ideas this project's code actually relies on, each with a tiny example first and then where it appears in our code. This file grows with every step. Phase 4 contains almost no C++ (two one-line module files), so this first version covers only what those lines use; Phase 5 adds the rest (structs and inheritance for fragments, `virtual`/`override` for traits and processors, weak pointers, lambdas for queries, `const` correctness).

---

## 1. `#include` and the preprocessor

```cpp
#include "Modules/ModuleManager.h"   // pastes the header's text here before compiling
```
- The preprocessor runs before the compiler and replaces `#include` with the contents of the file. Quotes search the module's include paths first, which come from `Build.cs` (`06` §2).
- **Include what you use:** include the header that declares what you use, not a large header that happens to include it. It keeps builds fast and makes dependencies visible.
- **In our code:** both module files include only `Modules/ModuleManager.h`, the header that declares the module macros.

## 2. Macros that generate code

```cpp
#define SQUARE(x) ((x) * (x))
int Nine = SQUARE(3);                 // the preprocessor writes ((3) * (3))
```
- A function-like macro is text substitution, done before compilation. Unreal uses macros to generate boilerplate that every module, class or property needs.
- Macros don't respect namespaces or types and are hard to debug, which is why Unreal limits them to generated plumbing.
- **In our code:** `IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, StateHandoff, "StateHandoff")` expands to the module's entry point (`06` §1). Its arguments are a class name, a module name and a (now ignored) string, and the macro turns them into functions and globals.

## 3. Comments as documentation

```cpp
// The game module needs no startup logic, so the engine's default implementation is enough.
```
- A comment explains **why**, not what: the code already says what it does.
- **In our code:** every file starts with a one-line comment saying which part of the system it is, and each non-obvious line says why it exists.

## 4. C# in an Unreal project (`Build.cs`, `Target.cs`)

```csharp
public class StateHandoff : ModuleRules { public StateHandoff(ReadOnlyTargetRules Target) : base(Target) { } }
```
- Build rules are **C#**, compiled by UnrealBuildTool, not by the C++ compiler. A class deriving from `ModuleRules` or `TargetRules`, with a constructor that takes the target and calls `base(Target)`, is all UBT needs.
- **In our code:** two target files and two module rule files (`05` A2, A3, A6).
