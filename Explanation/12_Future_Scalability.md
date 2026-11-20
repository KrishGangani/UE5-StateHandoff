# 12 · Future Scalability: StateHandoff

**Status:** filled in **Phase 10**. One entry is recorded early, below, because an owner decision in Phase 5 depends on it.

**Will contain:** How the design grows: adapters for GAS and blackboards, persistence, replication once Mass entities replicate through Iris, editor validation, batched captures. The first list is in 02_System_Architecture.md §11.

---

## A GAS adapter (recorded in Phase 5; not built in v1)

**Decision (owner, 2026-11-21):** v1 keeps the four demo channels and builds no Gameplay Ability System (GAS) adapter. "Not real GAS or blackboards" is a stated limit (`02` §12, limit 7). Revisit only if Phase 5 finishes early.

**Why it is not just another channel.** A channel today is a `UPROPERTY` struct on a component, copied by reflection. GAS keeps its state elsewhere and in richer forms (UE 5.8.4, `Engine/Plugins/Runtime/GameplayAbilities/Source/GameplayAbilities/Public/`, a plugin that is off by default, `GameplayAbilities.uplugin:13`):
- **Attributes** are `FGameplayAttributeData` members with reflected `BaseValue` and `CurrentValue` floats (`AttributeSet.h:21, 49–53`), inside `UAttributeSet` objects that the ability system component holds (`AbilitySystemComponent.h:1932`). They are reflected data, but one object further away than a component property: the channel spec would need a path "component → attribute set → property".
- **Active effects** live in a container on the component (`AbilitySystemComponent.h:1855`), with handles, durations and stacking, not as plain values. Copying memory would not be a correct hand-off; an adapter would have to remove and re-apply them through GAS's own API, keeping their remaining time. **[unverified]**: which GAS calls do this correctly, and their cost.
- **Granted abilities and cooldowns** are similar: state with lifetimes and handles, owned by the ability system.

**The growth path.**
1. A channel spec names an **adapter class** with virtual `Capture(Actor, Fragment)` / `Restore(Fragment, Actor)`, instead of a property. The contract (who owns the state when), the commit-time timing, the pre-spawn capture and the safety net are unchanged.
2. A first GAS adapter covers attributes only (reflected, so the existing checker compares them directly). Effects and abilities come after, each with its own fragment layout.
3. The same checker proves it: the oracle applies the same decided writes; fault injection must still be caught.
4. **Alternative:** keep a compact entity-owned copy as the authority and rebuild the ability system's state on bind (the `EntityOwned` mode, `02` §2), which avoids capture but pays a rebuild per bind **[inferred]**. Which is cheaper is a measurement, not a guess.
