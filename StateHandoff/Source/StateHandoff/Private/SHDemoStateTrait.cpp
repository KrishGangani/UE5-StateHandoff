// P09 StateHandoff (demo) - USHDemoStateTrait.

#include "SHDemoStateTrait.h"

#include "Mass/EntityFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "SHDemoState.h"
#include "SHDemoTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHDemoStateTrait)

void USHDemoStateTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// The visualization trait only *requires* these two (MassVisualizationTrait.cpp:67-69); some trait must add them.
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassActorFragment>();

	BuildContext.AddFragment<FSHAgentIdFragment>();
	BuildContext.AddTag<FSHDemoAgentTag>();

	// The entity's copy of the four state channels (01 section 6.1): in every configuration, so archetypes differ only by
	// what a configuration adds (SC7). Defaults are the structs' own member initialisers, the same as the oracle's.
	BuildContext.AddFragment<FSHHealthState>();
	BuildContext.AddFragment<FSHCooldownState>();
	BuildContext.AddFragment<FSHMemoryState>();
	BuildContext.AddFragment<FSHReservationState>();

	// The checker's own record of the representation, so transitions are counted identically in every configuration.
	BuildContext.AddFragment<FSHObservedFragment>();
}
