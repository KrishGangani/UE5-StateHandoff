// P09 StateHandoff (demo) - baseline B1: the actor lifecycle hand-off, made pool-aware.

#pragma once

#include "Components/ActorComponent.h"
#include "Mass/EntityHandle.h"
#include "SHLifecycleComponent.generated.h"

/**
 * Baseline B1 (02 section 9): the way a game usually moves state between an entity and its actor today, following the
 * pattern of Epic's PersistenceLab sample (01b #10), made pool-aware so it is not a strawman:
 *  - hydrate (entity -> actor) when the actor starts playing (BeginPlay) or comes back from the pool (PrepareForGame);
 *  - dehydrate (actor -> entity) when the actor stops playing (EndPlay) or goes into the pool (PrepareForPooling).
 * The actor's copy counts as the agent's state only while it is hydrated (SHRouter, B1 rule).
 *
 * Mass sets the actor -> entity mapping only after the actor has spawned and started playing (01b #7), so hydration
 * retries on the next ticks until the mapping exists. The entity found at hydration is remembered for dehydration,
 * because the mapping is cleared before the actor is released (MassRepresentationActorManagement.cpp:135-141).
 *
 * On every demo actor in every configuration (so actors are the same size everywhere, SC7), but it acts only when the
 * workload runs B1.
 */
UCLASS(ClassGroup = "StateHandoff")
class USHLifecycleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	USHLifecycleComponent();

	/** True while the actor's copy is the agent's state: after a successful hydrate, until the next dehydrate. */
	bool IsHydrated() const { return bHydrated; }

	/** The actor goes into Mass's pool (called by ASHDemoAgent::PrepareForPooling). */
	void OnPrepareForPooling();

	/** The actor comes back from Mass's pool for some entity (called by ASHDemoAgent::PrepareForGame). */
	void OnPrepareForGame();

protected:
	//~ Begin UActorComponent
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	//~ End UActorComponent

private:
	/** True when this run is B1 (asks the workload subsystem). */
	bool IsActiveBaseline() const;

	/** Starts hydrating: now if the mapping exists, else on the next ticks. */
	void BeginHydrate();

	/** Copies the entity's four channels into the actor's state component. False if the actor isn't mapped yet. */
	bool TryHydrate();

	/** Copies the actor's state component back into the remembered entity, if it still exists. */
	void Dehydrate();

	FMassEntityHandle Entity;
	bool bHydrated = false;

	/** The router tests set the hydrated flag directly (Tests/SHRouterTests.cpp); no game code uses this. */
	friend struct FSHRouterTestAccess;
};
