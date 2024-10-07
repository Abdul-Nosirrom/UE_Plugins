// 


#include "ActionTemplates/Action_LevelPrimitive.h"
#include "RadicalMovementComponent.h"
#include "Components/LevelPrimitiveComponent.h"
#include "Actors/RadicalCharacter.h"
#include "Components/ActionSystemComponent.h"
#include "Components/SplineComponent.h"

void UAction_LevelPrimitive::OnActionActivated_Implementation()
{
	// NOTE: This is kind of a bad generalization. E.g VertState is a Primitive Action but we fall under STATE_Falling, we don't need manual intervention on that
	CurrentActorInfo->MovementComponent.Get()->SetMovementState(STATE_General);
	LevelPrimitive->OnPrimitiveActivated();
}

void UAction_LevelPrimitive::OnActionEnd_Implementation()
{
	CurrentActorInfo->MovementComponent.Get()->SetMovementState(STATE_Falling);
	LevelPrimitive->OnPrimitiveDeactivated();
}

bool UAction_LevelPrimitive::EnterCondition_Implementation()
{
	LevelPrimitive = CurrentActorInfo->ActionSystemComponent.Get()->GetActiveLevelPrimitive(PrimitiveTag);
	return LevelPrimitive != nullptr;
}

void UAction_LevelPrimitive::RetrieveSplineData(const USplineComponent* Spline, FVector& Position, FVector& Normal, FVector& Tangent) const
{
	const FVector ActorLocation = CurrentActorInfo->CharacterOwner->GetActorLocation();
	
	/* Retrieve Location */
	Position = Spline->FindLocationClosestToWorldLocation(ActorLocation, ESplineCoordinateSpace::World);

	/* Retrieve Normal (Up) */
	Normal = Spline->FindUpVectorClosestToWorldLocation(ActorLocation, ESplineCoordinateSpace::World);
	
	/* Retrieve Tangent (Forward)*/
	Tangent = Spline->FindTangentClosestToWorldLocation(ActorLocation, ESplineCoordinateSpace::World);
}
