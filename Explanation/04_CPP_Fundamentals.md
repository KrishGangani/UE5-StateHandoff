# 04 · C++ Fundamentals: StateHandoff

**Rule:** only the C++ ideas this project's code actually relies on, each with a tiny example first and then where it appears in our code. This file grows with every step. Sections 1–4 cover Phase 4; sections 5–11 were added in S1.

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

## 5. Structs, inheritance and the base-class contract

```cpp
struct FBase { };                                          // an empty base: no data, just a "kind"
struct FHealth : public FBase { float Value = 100.f; };   // default member initialiser
```
- A `struct` is a class whose members are public by default. Deriving from a base makes the type "a kind of" that base, so code (here, Mass) can check `IsChildOf` and accept it.
- A **default member initialiser** (`= 100.f`, `= INDEX_NONE`) gives every new instance a known value without writing a constructor, which keeps the struct trivially copyable (a Mass requirement for fragments, `06` §6).
- **In our code:** `FSHAgentIdFragment : FMassFragment`, `FSHDemoAgentTag : FMassTag`.

## 6. `virtual`, `override` and `const` member functions

```cpp
struct FShape { virtual float Area() const = 0; virtual ~FShape() = default; };
struct FSquare : FShape { float Side = 1.f; float Area() const override { return Side * Side; } };
```
- `virtual` = the call is chosen at run time from the object's real type (through a table of function pointers). `= 0` = pure virtual: subclasses must implement it.
- `override` makes the compiler check that you really replace a base function; a typo becomes an error instead of a silent new function.
- `const` after the parameter list promises the function doesn't change the object.
- **In our code:** `USHDemoStateTrait::BuildTemplate(…) const override` (the base declares it pure virtual); the processors' `ConfigureQueries(…) override`, which first call `Super::ConfigureQueries` so the engine's queries exist before the tag is added. `Super` is Unreal's alias for the direct base class.

## 7. `constexpr`, `namespace` and `enum class`

```cpp
namespace Tuning { constexpr int32 MaxActors = 200; }      // a compile-time constant with a scoped name
enum class EPhase : uint8 { WarmUp, Capture, Done };        // scoped, 1 byte, no silent conversion to int
```
- `constexpr` values are computed by the compiler: no storage, no initialisation order, usable as array sizes.
- A `namespace` groups names so they can't clash (`SHBench::GridSpacing` vs any other `GridSpacing`).
- `enum class` names must be written `EPhase::Capture`, and the enum doesn't turn into an `int` by accident. `: uint8` fixes its size.
- **In our code:** `namespace SHBench` (all tuning constants with their reasons), `namespace SHMassProcessors`, `enum class ERunPhase : uint8`.

## 8. `static_cast` and signed vs unsigned

```cpp
int32 Count = 4000;
Spawn(static_cast<uint32>(Count));   // explicit: "I know this is non-negative"
```
- `static_cast` is the explicit, compile-time-checked conversion between related types. Writing it shows the reader that the conversion is deliberate.
- Passing a signed `int32` to an unsigned `uint32` parameter converts silently; a negative value would become a huge number.
- **In our code:** `SpawnEntities(Template, static_cast<uint32>(Settings.NumAgents), Agents)`, after the director has already returned for `NumAgents <= 0`; `static_cast<int32>(EMassRepresentationType::…)` to index the counts array.

## 9. References, pointers and "checked" access

```cpp
FTransformFragment& T = EntityManager.GetFragmentDataChecked<FTransformFragment>(Entity);  // must exist
if (const FMassActorFragment* A = EntityManager.GetFragmentDataPtr<FMassActorFragment>(Entity)) { /* may be null */ }
```
- A reference can't be null; the "Checked" function asserts instead of returning null, for data that must exist.
- A pointer can be null, so the code tests it before use (the `if (… = …)` form declares and tests in one line).
- **In our code:** the director writes transforms through `GetFragmentDataChecked` (a missing fragment would be a template bug) and reads representations through `GetFragmentDataPtr` (a destroyed entity returns null).

## 10. `TSharedPtr` and deliberate lifetime

```cpp
TSharedPtr<FContext> Context = MakeContext();   // reference-counted; the object lives while a pointer holds it
Fill(); Context.Reset();                        // release now: the object's destructor runs here
```
- A shared pointer deletes its object when the last pointer to it goes away. Holding one on purpose controls *when* that happens.
- **In our code:** the director holds the creation context returned by `SpawnEntities` until the transforms and indices are written, then calls `Reset()`, so the entities' creation notifications see real values (`05` B5).

## 11. Function-local `static`

```cpp
UMesh* Get() { static UMesh* Cached = Find(); return Cached; }   // Find() runs once, on the first call
```
- A `static` local is initialised once, the first time the line runs, and kept afterwards.
- **In our code:** `static ConstructorHelpers::FObjectFinder<UStaticMesh> Cylinder(…)` in the agent's constructor: the asset lookup runs once per class, not once per spawned actor.
