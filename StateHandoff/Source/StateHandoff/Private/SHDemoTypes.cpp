// P09 StateHandoff (demo) - configuration names and transition classification.

#include "SHDemoTypes.h"

#include UE_INLINE_GENERATED_CPP_BY_NAME(SHDemoTypes)

namespace SHDemo
{
	namespace
	{
		enum class EHome : uint8 { Actor, Mesh, None };

		EHome HomeOf(const EMassRepresentationType Type)
		{
			if (UE::Mass::Representation::IsValidActorRepresentation(Type))
			{
				return EHome::Actor;
			}
			return UE::Mass::Representation::IsValidMeshRepresentation(Type) ? EHome::Mesh : EHome::None;
		}
	}

	bool ParseConfig(const FString& Text, ESHConfig& OutConfig)
	{
		for (int32 Index = 0; Index < static_cast<int32>(ESHConfig::Count); ++Index)
		{
			if (Text.Equals(GetConfigName(static_cast<ESHConfig>(Index)), ESearchCase::IgnoreCase))
			{
				OutConfig = static_cast<ESHConfig>(Index);
				return true;
			}
		}
		return false;
	}

	const TCHAR* GetConfigName(const ESHConfig Config)
	{
		switch (Config)
		{
		case ESHConfig::B0: return TEXT("B0");
		case ESHConfig::B1: return TEXT("B1");
		case ESHConfig::B2: return TEXT("B2");
		case ESHConfig::B3: return TEXT("B3");
		case ESHConfig::SH: return TEXT("SH");
		default:            return TEXT("?");
		}
	}

	ESHTransitionKind ClassifyTransition(const EMassRepresentationType From, const EMassRepresentationType To)
	{
		if (From == To)
		{
			return ESHTransitionKind::Count;
		}
		const EHome FromHome = HomeOf(From);
		const EHome ToHome = HomeOf(To);
		switch (FromHome)
		{
		case EHome::Actor:
			return ToHome == EHome::Actor ? ESHTransitionKind::ActorSwap
				: ToHome == EHome::Mesh ? ESHTransitionKind::ActorToMesh : ESHTransitionKind::ActorToNone;
		case EHome::Mesh:
			return ToHome == EHome::Actor ? ESHTransitionKind::MeshToActor
				: ToHome == EHome::None ? ESHTransitionKind::MeshToNone : ESHTransitionKind::Count; // static <-> skinned: not used here
		default:
			return ToHome == EHome::Actor ? ESHTransitionKind::NoneToActor : ESHTransitionKind::NoneToMesh;
		}
	}

	const TCHAR* GetTransitionName(const ESHTransitionKind Kind)
	{
		switch (Kind)
		{
		case ESHTransitionKind::ActorToMesh: return TEXT("ActorToMesh");
		case ESHTransitionKind::MeshToActor: return TEXT("MeshToActor");
		case ESHTransitionKind::MeshToNone:  return TEXT("MeshToNone");
		case ESHTransitionKind::NoneToMesh:  return TEXT("NoneToMesh");
		case ESHTransitionKind::ActorToNone: return TEXT("ActorToNone");
		case ESHTransitionKind::NoneToActor: return TEXT("NoneToActor");
		case ESHTransitionKind::ActorSwap:   return TEXT("ActorSwap");
		default:                             return TEXT("None");
		}
	}
}
