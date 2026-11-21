// P09 StateHandoff (demo) - the trait every demo entity has, in every configuration.

#pragma once

#include "MassEntityTraitBase.h"
#include "SHDemoStateTrait.generated.h"

/**
 * Adds what a demo agent is made of, identically in all five configurations (B0-B3, SH), so archetypes differ only by
 * what each configuration adds on top (02 section 4.2; memory fairness, SC7).
 *
 * The transform (the visualization trait requires it but doesn't add it), the actor fragment (the same), the agent
 * index, the demo tag, the entity's copy of the four state channels, and the checker's observed-representation record.
 */
UCLASS(meta = (DisplayName = "SH Demo State"))
class USHDemoStateTrait : public UMassEntityTraitBase
{
	GENERATED_BODY()

public:
	/** Baseline B2 only: also add FSHMirrorTag, so B2's mirror processor matches these entities. Set by the director. */
	UPROPERTY()
	bool bAddMirrorTag = false;

protected:
	virtual void BuildTemplate(FMassEntityTemplateBuildContext& BuildContext, const UWorld& World) const override;
};
