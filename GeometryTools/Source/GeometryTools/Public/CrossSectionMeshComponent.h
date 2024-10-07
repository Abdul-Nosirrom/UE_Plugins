// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "MeshExtrusionHelpers.h"
#include "Components/DynamicMeshComponent.h"
#include "CrossSectionMeshComponent.generated.h"

DECLARE_STATS_GROUP(TEXT("CrossSectionMesh_Game"), STATGROUP_GeometryTools, STATCAT_Advanced)

USTRUCT()
struct FCrossSectionMaterialAssignment
{
	GENERATED_BODY()

	FCrossSectionMaterialAssignment() : bUniqueTopFaceMat(false), bUniqueBackFaceMat(false), bUniqueSideFacesMat(false),
		TopFaceID(0), BackFaceID(0), SideFaceID(0) {}

	void RefreshMaterialIDs()
	{
		int8 CurrentID = 0;
		if (bUniqueTopFaceMat)
		{
			CurrentID++;
			TopFaceID = CurrentID;
		}
		if (bUniqueBackFaceMat)
		{
			CurrentID++;
			BackFaceID = CurrentID;
		}
		if (bUniqueSideFacesMat)
		{
			CurrentID++;
			SideFaceID = CurrentID;
		}
		if (bUniqueFrontFaceMat)
		{
			CurrentID++;
			FrontFaceID = CurrentID;
		}
		MaterialCount = CurrentID;
	}

	UPROPERTY(EditAnywhere, meta=(UIMin = 0, ClampMin = 0))
	float UVTilingLength = 100.f;
	/// @brief	Tiling for along the length and along the depth of cross section (X & Y Respectively) 
	UPROPERTY()
	FVector2f UVCrossSectionTiling = FVector2f::One();
	/// @brief	Tiling for the deck top & bottom depths respectively
	UPROPERTY()
	FVector2f UVTopBotDeckTiling = FVector2f::One();
	/// @brief	Bottom face depth tiling & back face height tiling respectively
	UPROPERTY()
	float UVHeightTiling = 1.f;
	
	UPROPERTY(EditAnywhere)
	uint8 bUniqueTopFaceMat : 1;
	UPROPERTY(EditAnywhere)
	uint8 bUniqueBackFaceMat : 1;
	UPROPERTY(EditAnywhere)
	uint8 bUniqueSideFacesMat : 1;
	UPROPERTY(EditAnywhere)
	uint8 bUniqueFrontFaceMat : 1;
	
	int8 TopFaceID, BackFaceID, SideFaceID, FrontFaceID, MaterialCount;
};

UCLASS(ClassGroup=(Geometry), meta=(BlueprintSpawnableComponent, DisplayName="Cross Section Mesh", PrioritizeCategories="Materials Settings Shape Dimensions"))
class GEOMETRYTOOLS_API UCrossSectionMeshComponent : public UDynamicMeshComponent
{
	GENERATED_BODY()

public:

	/// FOr move edit checks [https://forums.unrealengine.com/t/how-to-detect-transform-change-in-editor/361134/2]
	UPROPERTY(Category=Materials, EditAnywhere)
	FCrossSectionMaterialAssignment MaterialAssignment;

	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite)
	bool bForceRebuild = false;
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite)
	bool bFlipNormals = false;
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite)
	bool bDisable = false;
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite)
	bool bRemoveStartEndCap = false;
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite)
	bool bRemoveFinalEndCap = false;
	UPROPERTY(Category=Settings, VisibleAnywhere, BlueprintReadWrite)
	float VertexCount = 0;
	UPROPERTY(Category=Settings, EditAnywhere)
	EExtrusionMethods ExtrusionMethod;
	/// @brief	Defines number of verts to allocate for the cross section
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite, meta=(UIMin=2, ClampMin=2, UIMax=100, ClampMax=100))
	int CrossSectionResolution = 2;
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite, meta=(UIMin=2, ClampMin=2, UIMax=5120, ClampMax=5120, EditCondition="ExtrusionMethod!=EExtrusionMethods::Spline || !bUseMeshSegmentLength"))
	int LengthResolution = 2;

	// Spline Extrusion Settings
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Spline"))
	bool bUseMeshSegmentLength = false;
	/// @brief	How we subdivide the mesh along the spline, a division per segment length
	UPROPERTY(Category=Settings, EditAnywhere, meta=(UIMin=10, ClampMin=10, EditConditionHides, EditCondition="bUseMeshSegmentLength && ExtrusionMethod==EExtrusionMethods::Spline"))
	float MeshSegmentLength = 100.f;
	UPROPERTY(Category=Settings, EditAnywhere, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Spline"))
	FTransform SplineOffset;
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Spline && bUseHeightCurve"))
	float HeightCurveSplineTiling = 1.f;
	UPROPERTY(Category=Settings, EditAnywhere, BlueprintReadWrite, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Spline && bUseDepthCurve"))
	float DepthCurveSplineTiling = 1.f;
	// ~
	
	// Rotation Extrusion Settings
	UPROPERTY(Category=Settings, EditAnywhere, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	float RotateExtrudeAngle = 0.f;
	UPROPERTY(Category=Settings, EditAnywhere, meta=(UIMin=-360, ClampMin=-360, UIMax=360, ClampMax=360, EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	float RotationOffset = 0.f;
	UPROPERTY(Category=Settings, EditAnywhere, meta=(UIMin=0, ClampMin=0, EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	FVector2D Radius = FVector2D(100, 100);
	UPROPERTY(Category=Settings, EditAnywhere, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	float SpiralHeight = 0.f;
	UPROPERTY(Category=Settings, EditAnywhere, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	bool bSyncRadius = true;
	UPROPERTY(Category=Settings, EditAnywhere, meta=(EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	bool bCenterPivot = false;
	UPROPERTY(Category=Settings, EditAnywhere, meta=(UIMin=0, ClampMin=0, EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	float RotationDepth = 10.f;
	UPROPERTY(Category=Settings, EditAnywhere, meta=(UIMin=0, ClampMin=0, EditConditionHides, EditCondition="ExtrusionMethod==EExtrusionMethods::Rotation"))
	float RotationBackExtrusion = 0.f;
	// ~
	
	// Cross-Section Shape Info
	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite)
	bool bMirrorShape = false;
	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite)
	FRuntimeFloatCurve CrossSectionShape;

	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bUseHeightCurve = false;
	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(EditCondition=bUseHeightCurve))
	FRuntimeFloatCurve HeightCurve;

	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bUseDepthCurve = false;
	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(EditCondition=bUseDepthCurve))
	FRuntimeFloatCurve DepthCurve;

	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bUseBottomWallCurve = false;
	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(EditCondition=bUseBottomWallCurve))
	FRuntimeFloatCurve BottomCurve;

	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite)
	float HeightSpiral = 0.f;
	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(InlineEditConditionToggle))
	bool bUseHeightSpiralCurve = false;
	UPROPERTY(Category=Shape, EditAnywhere, BlueprintReadWrite, meta=(EditCondition=bUseHeightSpiralCurve))
	FRuntimeFloatCurve HeightSpiralCurve;

	
	// Dimensions specifications
	// X [0, -Value]	Only if non-mirrored! We throw away the deck if we mirror
	UPROPERTY(Category=Dimensions, EditAnywhere, meta=(MakeEditWidget, EditCondition="!bMirrorShape", EditConditionHides))
	FVector BackWallExtrusion;
	// Z [0, +Value]
	UPROPERTY(Category=Dimensions, EditAnywhere, meta=(MakeEditWidget))
	FVector HeightWidget;
	// Y [0, +Value] or [0, -Value] Extruding the cross section
	UPROPERTY(Category=Dimensions, EditAnywhere, meta=(MakeEditWidget, EditCondition="ExtrusionMethod==EExtrusionMethods::Straight"))
	FVector LengthWidget;
	// X [0, +Value] For the cross-section
	UPROPERTY(Category=Dimensions, EditAnywhere, meta=(MakeEditWidget, EditCondition="ExtrusionMethod!=EExtrusionMethods::Rotation"))
	FVector DepthWidget;
	// Z [0, -Value]
	UPROPERTY(Category=Dimensions, EditAnywhere, meta=(MakeEditWidget))
	FVector BottomExtrudeWidget = FVector::ZeroVector;


public:
	UFUNCTION(Category=Geometry, BlueprintCallable)
	void ConstructMesh();
	UFUNCTION(Category=Geometry, BlueprintPure)
	const TArray<FVector>& GetTopEdgeVertices() const { return TopEdgeVertices; }
	UFUNCTION(Category=Geometry, BlueprintCallable)
	void SetExplicitCurve(UCurveFloat* InCurve) { CrossSectionShape.ExternalCurve = InCurve; }

	virtual void PostLoad() override {Super::PostLoad(); if (ExtrusionMethod == EExtrusionMethods::Rotation) HeightSpiral = SpiralHeight;}

protected:

	void UpdateConstructedMesh();
	FVector VertexPositionFromID(FDynamicMesh3& Mesh, int32 vID) const;
	void GenerateVerts(FDynamicMesh3& Mesh);
	void SetupConnectivity(FDynamicMesh3& Mesh);

private:

	void ValidateInputs();

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	// Z Base
	float GetVertexBaseHeight(float LengthPercent) const;
	// Z Cross Section Amount
	float GetVertexCrossSectionHeight(float CrossSectionPercent) const;
	// X 
	float GetVertexDepth(float LengthPercent, float CrossSectionPercent) const;

	float GetBottomExtrudeHeight(float LengthPercent) const;

	
	// Util
	void UpdateBasis(float Percent);
	FVector Transform(float X, float Y, float Z) const;

	FVector3f XNormal() const;
	FVector3f YNormal() const;
	FVector3f ZNormal() const;
	// ~

	UPROPERTY()
	bool bMeshDirty = true;
	FStraightExtrusion StraightBasis;
	FRotationExtrusion RotationBasis;
	FSplineExtrusion SplineBasis;

	TMap<int, FVector2f> UVMap; 
	TMap<int, int> TriIDMap;
	TArray<int> TriBuffer;
	
	TArray<int32> GenericTriIDs, TopTriIDs, BackTriIDs, SideTriIDs, FrontTriIDs;

	//TArray<float> CrossSectionBuffer;
	// Need to mark these as UPROPERTY because we do end up "regening" UVs on reload and this needs to 'save'
	TArray<TArray<int32>> CrossSectionIndices;
	TArray<TArray<int32>> CrossSectionMirroredIndices;
	TArray<TArray<int32>> DeckCrossSectionIndices;
	TArray<TArray<int32>> DeckTopIndices;
	TArray<TArray<int32>> DeckFrontIndices;
	TArray<TArray<int32>> DeckBackIndices;
	TArray<TArray<int32>> DeckBottomIndices;

	// These are used specifically for queries when we reload and the above dont exit yet
	UPROPERTY()
	TSet<int32> LU_CrossSectionIndices;
	UPROPERTY()
	TSet<int32> LU_CrossSectionMirroredIndices;
	UPROPERTY()
	TSet<int32> LU_DeckCrossSectionIndices;
	UPROPERTY()
	TSet<int32> LU_DeckTopIndices;
	UPROPERTY()
	TSet<int32> LU_DeckFrontIndices;
	UPROPERTY()
	TSet<int32> LU_DeckBackIndices;
	UPROPERTY()
	TSet<int32> LU_DeckBottomIndices;

	UPROPERTY()
	TArray<FVector> TopEdgeVertices;
	UPROPERTY()
	TArray<int32> TopEdgeIndices;
};
