// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "SplineInstancedMeshComponent.generated.h"



UCLASS(ClassGroup=(Geometry), meta=(BlueprintSpawnableComponent, DisplayName="Spline Instancer Component"))
class GEOMETRYTOOLS_API USplineInstancedMeshComponent : public UInstancedStaticMeshComponent
{
	GENERATED_BODY()

public:

	UPROPERTY(Category=Mesh, EditAnywhere, BlueprintReadWrite)
	float SegmentLength = 100.f;
	UPROPERTY(Category=Mesh, EditAnywhere, BlueprintReadWrite)
	FTransform InstanceBaseTransform = FTransform::Identity;
	
	UPROPERTY(Transient)
	class USplineComponent* Spline;

	UFUNCTION(BlueprintCallable)
	void GenerateInstances();
};

UCLASS(ClassGroup=(Geometry))
class GEOMETRYTOOLS_API ASplineInstancedMeshActor : public AActor
{
	GENERATED_BODY()

public:
	ASplineInstancedMeshActor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Geometry, meta = (AllowPrivateAccess = "true"))
	USplineComponent* Spline;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Geometry, meta = (AllowPrivateAccess = "true"))
	USplineInstancedMeshComponent* InstancedMeshComponent;

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
