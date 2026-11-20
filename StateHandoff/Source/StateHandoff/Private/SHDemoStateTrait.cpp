// P09 StateHandoff (demo) - USHDemoStateTrait.

#include "SHDemoStateTrait.h"

#include "Mass/EntityFragments.h"
#include "MassActorSubsystem.h"
#include "MassEntityTemplateRegistry.h"
#include "SHDemoTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHDemoStateTrait)

void USHDemoStateTrait::BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const
{
	// The visualization trait only *requires* these two (MassVisualizationTrait.cpp:67-69); some trait must add them.
	BuildContext.AddFragment<FTransformFragment>();
	BuildContext.AddFragment<FMassActorFragment>();

	BuildContext.AddFragment<FSHAgentIdFragment>();
	BuildContext.AddTag<FSHDemoAgentTag>();
}
