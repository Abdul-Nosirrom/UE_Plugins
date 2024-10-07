// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CrossSectionMeshActor.generated.h"

UCLASS(ClassGroup=Geometry)
class GEOMETRYTOOLS_API ACrossSectionMeshActor : public AActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	ACrossSectionMeshActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Geometry, meta = (AllowPrivateAccess = "true"))
	class UCrossSectionMeshComponent* CrossSectionMesh;

	virtual void OnConstruction(const FTransform& Transform) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override
	{
		bIsMoving = !bFinished;
		Super::PostEditMove(bFinished);
	}
	bool bIsMoving;
#endif 
};
