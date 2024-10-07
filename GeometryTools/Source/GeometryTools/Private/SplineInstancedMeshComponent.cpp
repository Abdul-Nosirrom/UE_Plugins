// Copyright 2023 CoC All rights reserved


#include "SplineInstancedMeshComponent.h"

#include "Components/SplineComponent.h"

void USplineInstancedMeshComponent::GenerateInstances()
{
	if (!Spline || Spline->GetOwner() != GetOwner())
	{
		Spline = GetOwner()->FindComponentByClass<USplineComponent>();
	}

	if (!Spline) return;
	
	const float TotalSplineLength = Spline->GetSplineLength();
	const uint32 NumInstances = FMath::Floor(TotalSplineLength / SegmentLength);
	const bool bNeedsRegen = NumInstances != GetInstanceCount();

	if (bNeedsRegen) ClearInstances();
	
	for (uint32 i = 0; i < NumInstances; i++)
	{
		const float Perc = i / (NumInstances - 1.f);
		const float CurrentDist = TotalSplineLength * Perc;
		const FTransform Transform = InstanceBaseTransform * Spline->GetTransformAtDistanceAlongSpline(CurrentDist, ESplineCoordinateSpace::Local);

		if (bNeedsRegen)
		{
			AddInstance(Transform);
		}
		else
		{
			UpdateInstanceTransform(i, Transform);
		}
	}
}

ASplineInstancedMeshActor::ASplineInstancedMeshActor()
{
	PrimaryActorTick.bCanEverTick = false;

	Spline = CreateDefaultSubobject<USplineComponent>(FName("Spline"));

	InstancedMeshComponent = CreateDefaultSubobject<USplineInstancedMeshComponent>(FName("InstancedMeshes"));
	InstancedMeshComponent->SetMobility(EComponentMobility::Movable);
	InstancedMeshComponent->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);

	SetRootComponent(InstancedMeshComponent);

	Spline->SetupAttachment(GetRootComponent());
}

void ASplineInstancedMeshActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR
	if (!bIsMoving)
#endif
	{
		InstancedMeshComponent->GenerateInstances();
	}
}
