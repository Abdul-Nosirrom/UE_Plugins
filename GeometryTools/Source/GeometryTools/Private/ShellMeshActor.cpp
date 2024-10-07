// Copyright 2023 CoC All rights reserved


#include "ShellMeshActor.h"

#include "DynamicMeshEditor.h"
#include "GeometryScript/MeshAssetFunctions.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshBooleanFunctions.h"
#include "GeometryScript/MeshModelingFunctions.h"
#include "GeometryScript/MeshVertexColorFunctions.h"


// Sets default values
AShellMeshActor::AShellMeshActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;
}

void AShellMeshActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// No Mesh? No Shells!
	if (!MeshAsset) return;

	UDynamicMeshPool* MeshPool = GetComputeMeshPool();

	// Clear our current mesh
	GetDynamicMeshComponent()->GetDynamicMesh()->Reset();

	// Generate shells
	for (int s = 0; s < ShellCount; s++)
	{
		// Generate the shell
		UDynamicMesh* PooledMesh = MeshPool->RequestMesh();
		FGeometryScriptCopyMeshFromAssetOptions AssetOptions;
		FGeometryScriptMeshReadLOD RequestedLOD; 
		EGeometryScriptOutcomePins Outcome;
		UGeometryScriptLibrary_StaticMeshFunctions::CopyMeshFromStaticMesh(MeshAsset, PooledMesh, AssetOptions, RequestedLOD, Outcome);
		if (Outcome == EGeometryScriptOutcomePins::Failure)
		{
			UE_LOG(LogGeometry, Error, TEXT("Failed copying mesh from static mesh for ShellMesh generation!"));
			MeshPool->FreeAllMeshes();
			return;
		}

		// Set its vertex colors
		const float Percent = s / (ShellCount - 1.f);
		FGeometryScriptColorFlags Flags;
		UGeometryScriptLibrary_MeshVertexColorFunctions::SetMeshConstantVertexColor(PooledMesh, FLinearColor::White * Percent, Flags);

		// Offset the created mesh by its normals before appending
		//FGeometryScriptMeshOffsetOptions OffsetOptions;
		FGeometryScriptMeshOffsetFacesOptions OffsetOptions;
		OffsetOptions.Distance = Percent * ShellOffsetAmount;
		UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffsetFaces(PooledMesh, OffsetOptions, FGeometryScriptMeshSelection());
		//UGeometryScriptLibrary_MeshModelingFunctions::ApplyMeshOffset(PooledMesh, OffsetOptions);

		// Append it to our existing mesh
		UGeometryScriptLibrary_MeshBasicEditFunctions::AppendMesh(GetDynamicMeshComponent()->GetDynamicMesh(), PooledMesh, FTransform::Identity);

		// Return the pooled mesh since its no longer needed
		MeshPool->ReturnMesh(PooledMesh);
	}

	MeshPool->FreeAllMeshes();
}


