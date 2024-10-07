// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "DynamicMeshActor.h"
#include "ShellMeshActor.generated.h"

UCLASS(ClassGroup=Geometry)
class GEOMETRYTOOLS_API AShellMeshActor : public ADynamicMeshActor
{
	GENERATED_BODY()

public:
	// Sets default values for this actor's properties
	AShellMeshActor();

protected:
	UPROPERTY(EditAnywhere)
	UStaticMesh* MeshAsset;
	UPROPERTY(EditAnywhere, meta=(UIMin=1, ClampMin=1, UIMax=256, ClampMax=256))
	int ShellCount;
	UPROPERTY(EditAnywhere, meta=(UIMin=1, ClampMin=1))
	float ShellOffsetAmount;
	
	virtual void OnConstruction(const FTransform& Transform) override;
};
