// Copyright 2023 CoC All rights reserved


#include "CrossSectionMeshComponent.h"

#include "MaterialDomain.h"
#include "MeshDescription.h"
#include "TriangleTypes.h"
#include "Components/SplineComponent.h"
#include "DynamicMesh/MeshAttributeUtil.h"
#include "DynamicMesh/MeshIndexUtil.h"
#include "DynamicMesh/MeshNormals.h"
#include "GeometryScript/MeshBasicEditFunctions.h"
#include "GeometryScript/MeshNormalsFunctions.h"
#include "GeometryScript/MeshUVFunctions.h"

DECLARE_CYCLE_STAT(TEXT("Construct Mesh Single"), STAT_ConstructMeshSingle, STATGROUP_GeometryTools)
DECLARE_CYCLE_STAT(TEXT("Construct Mesh Parallel"), STAT_ConstructMeshParallel, STATGROUP_GeometryTools)

void UCrossSectionMeshComponent::ConstructMesh()
{
	// Allow the mesh edits to be disabled
	if (bDisable) return;
	
	ValidateInputs();

	if (!bMeshDirty)// && !bDisableParallelUpdates)
	{
		SCOPE_CYCLE_COUNTER(STAT_ConstructMeshParallel)
		
		UpdateConstructedMesh();
		return;
	}

	SCOPE_CYCLE_COUNTER(STAT_ConstructMeshSingle)
	
	FDynamicMesh3 GeneratedMesh(true, true, true, true);
	GeneratedMesh.EnableAttributes();
	GeneratedMesh.EnableVertexColors(FVector3f::ZeroVector);
	GeneratedMesh.EnableVertexNormals(FVector3f::OneVector);
	GeneratedMesh.EnableTriangleGroups();
	GeneratedMesh.Attributes()->EnableMaterialID();
	GeneratedMesh.EnableVertexUVs(FVector2f::ZeroVector);
	
	// Clear index caches
	CrossSectionIndices.Empty();
	CrossSectionMirroredIndices.Empty();
	DeckCrossSectionIndices.Empty();
	DeckTopIndices.Empty();
	DeckFrontIndices.Empty();
	DeckBackIndices.Empty();
	DeckBottomIndices.Empty();
	UVMap.Empty();

	// Not used for mesh generation, just useful utility to have!
	TopEdgeVertices.Empty();
	TopEdgeIndices.Empty();
	
	// Check if cross section buffer was generated
	/*if (CrossSectionBuffer.IsEmpty() || CrossSectionBuffer.Num() != CrossSectionResolution)
	{
		CreateCrossSectionBuffer();
	}*/

	// Vertex Generation
	GenerateVerts(GeneratedMesh);
	//ParallelVertGeneration(GeneratedMesh);

	// Triangle Generation
	SetupConnectivity(GeneratedMesh);
	// Generate normal
	UE::Geometry::FMeshNormals::QuickComputeVertexNormalsForTriangles(GeneratedMesh, TriBuffer);
	
	// Confirm UVs after connectivity info
	for (const auto& UV : UVMap)
	{
		GeneratedMesh.SetVertexUV(UV.Key, UV.Value);
	}
	UE::Geometry::CopyVertexUVsToOverlay(GeneratedMesh, *GeneratedMesh.Attributes()->PrimaryUV());
	
	// Set the mesh and its collision
	SetMesh(MoveTemp(GeneratedMesh));

	// Default behavior is to flip normals if angle < 0 or spline extrusion
	if ((ExtrusionMethod == EExtrusionMethods::Rotation && RotateExtrudeAngle < 0.f) || ExtrusionMethod == EExtrusionMethods::Spline)
	{
		UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(GetDynamicMesh());
	}
	
	EnableComplexAsSimpleCollision();
	SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	UpdateCollision();

	// Flip normals if angle is negative
	//if (ExtrusionMethod == EExtrusionMethods::Spline || (RotateExtrudeAngle < 0.f && ExtrusionMethod == EExtrusionMethods::Rotation))
	//{
	//	UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(GetDynamicMesh());
	//}

	// Configure materials
	//SetNumMaterials()
	/*TArray<UMaterialInterface*> Materials;
	for (int i = 0; i <= MaterialAssignment.MaterialCount; i++)
	{
		UMaterialInterface* Mat = GetMaterial(i);
		if (!Mat)
		{
			Mat = UMaterial::GetDefaultMaterial(MD_Surface);
		}
		Materials.Add(Mat);
	}
	ConfigureMaterialSet(Materials);*/

	bMeshDirty = false;
}

void UCrossSectionMeshComponent::UpdateConstructedMesh()
{
	GetDynamicMesh()->EditMesh([&](FDynamicMesh3& EditMesh)
	{
		int NumVertices = EditMesh.MaxVertexID();
		VertexCount = NumVertices;
		ParallelFor(NumVertices, [&](int32 vID)
		{
			const FVector newPos = VertexPositionFromID(EditMesh, vID);
			EditMesh.SetVertex(vID, newPos);

			// Check if its meant to be a top-edge
			const int32 topIdx = TopEdgeIndices.Find(vID);
			if (topIdx != INDEX_NONE)
			{
				TopEdgeVertices[topIdx] = newPos;
			}
		});
		
		// Generate normal
		UE::Geometry::FMeshNormals::QuickComputeVertexNormalsForTriangles(EditMesh, TriBuffer);
		// Update UVs
		UE::Geometry::CopyVertexUVsToOverlay(EditMesh, *EditMesh.Attributes()->PrimaryUV());
	}, EDynamicMeshChangeType::GeneralEdit, EDynamicMeshAttributeChangeFlags::Unknown, false);
}

FVector UCrossSectionMeshComponent::VertexPositionFromID(FDynamicMesh3& Mesh, int32 vID) const
{
	const FVector3f Color = Mesh.GetVertexColor(vID);
	FVector2f UVs = FVector2f(1.f, 1.f);

	const float CrossSectionPercent = Color.X;
	const float LengthPercent = Color.Y;
	const float ZFlag = Color.Z;
	
	float Height = GetVertexBaseHeight(LengthPercent);
	
	// Derive XPos
	float XPos = 0.f;
	UVs.X = LengthPercent * MaterialAssignment.UVCrossSectionTiling.X;
	if (CrossSectionPercent < 0.f)
	{
		// Back wall or front vertex
		XPos = CrossSectionPercent == -1.f ? BackWallExtrusion.X : GetVertexDepth(LengthPercent, 0.f);
	}
	else
	{
		XPos = GetVertexDepth(LengthPercent, CrossSectionPercent);
	}

	// Derive YPos
	float YPos = LengthPercent * LengthWidget.Y;

	// Derive ZPos
	float ZPos = 0.f;
	if (ZFlag > 1.f)
	{
		if (ZFlag == 2.f)
		{
			ZPos = BackWallExtrusion.X == 0.f ? GetVertexCrossSectionHeight(1.f) * Height : Height;
			UVs.Y = MaterialAssignment.UVTopBotDeckTiling.X;
		}
		else if (ZFlag == 3.f)
		{
			ZPos = bMirrorShape ? -GetVertexCrossSectionHeight(CrossSectionPercent) * Height : GetBottomExtrudeHeight(LengthPercent);
			UVs.Y = 0.f;
		}
	}
	else
	{
		ZPos = ZFlag * GetVertexCrossSectionHeight(CrossSectionPercent) * Height;
		if (ZFlag == 0.f) ZPos = GetBottomExtrudeHeight(LengthPercent); // Cuz ZFlag = 0 so we fix it up here?

		if (ZFlag > 0.f)
		{
			// Y Represents the projected cross-section along the length
			UVs.Y = CrossSectionPercent * MaterialAssignment.UVCrossSectionTiling.Y;
		}
		else
		{
			// Y Represents going up along the cross-section at the two ends
			UVs.Y = MaterialAssignment.UVHeightTiling * ZPos / Height;
		}
	}

	FVector NewVertexPosition(XPos, YPos, ZPos);

	// Figure out UVs (only do this during editor because of a lot of lookups?)
#if WITH_EDITOR
	{
		auto DoesContain = [&](const TSet<int32>& buf, int32 idxToCheck)
		{
			return buf.Contains(idxToCheck);
		};
		// Along the cross section
		if (DoesContain(LU_CrossSectionMirroredIndices, vID) || DoesContain(LU_CrossSectionMirroredIndices, vID))
		{
			UVs.X = LengthPercent * MaterialAssignment.UVCrossSectionTiling.X;
			UVs.Y = CrossSectionPercent * MaterialAssignment.UVCrossSectionTiling.Y;
		}
		// Ends cross section (but not deck)
		else if (DoesContain(LU_DeckCrossSectionIndices, vID))
		{
			if (CrossSectionPercent >= 0.f)
			{
				const float CrossSectionLengthFactor = DepthWidget.X / (DepthWidget.X - BackWallExtrusion.X); 
				UVs = FVector2f(CrossSectionPercent * CrossSectionLengthFactor * MaterialAssignment.UVTopBotDeckTiling.Y, MaterialAssignment.UVHeightTiling * ZPos/Height);
				if (ZFlag == 3.f)
				{
					UVs.Y = 0;
				}
			}
			else
			{
				UVs = FVector2f(MaterialAssignment.UVTopBotDeckTiling.Y, 0.f);
				if (ZFlag == 2.f)
				{
					UVs.Y = MaterialAssignment.UVHeightTiling;
				}
			}
		}
		// Top part of deck
		else if (DoesContain(LU_DeckTopIndices, vID)) 
		{
			UVs = FVector2f(LengthPercent * MaterialAssignment.UVCrossSectionTiling.X, 0.f);
			if (ZFlag == 2.f)
			{
				UVs.Y = MaterialAssignment.UVTopBotDeckTiling.X;
			}
		}
		// Bottom face
		else if (DoesContain(LU_DeckBottomIndices, vID))
		{
			UVs.X = LengthPercent * MaterialAssignment.UVCrossSectionTiling.X;
			UVs.Y = (CrossSectionPercent == -2.f ? 0.f : 1.f) * MaterialAssignment.UVTopBotDeckTiling.Y; // 0 / 1 respectively
		}
		// Front Face
		else if (DoesContain(LU_DeckFrontIndices, vID))
		{
			UVs = FVector2f(LengthPercent * MaterialAssignment.UVCrossSectionTiling.X, 0.f);
			if (CrossSectionPercent != -2.f)
			{
				UVs.Y = MaterialAssignment.UVHeightTiling * ZPos/Height;
			}
		}
		// Back Face
		else if (DoesContain(LU_DeckBackIndices, vID))
		{
			UVs = FVector2f(LengthPercent * MaterialAssignment.UVCrossSectionTiling.X, 0.f);
			if (ZFlag == 2.f)
			{
				UVs.Y = MaterialAssignment.UVHeightTiling;
			}
		}
	}
	Mesh.SetVertexUV(vID, UVs); // Update UVs
#endif
	
	if (ExtrusionMethod == EExtrusionMethods::Straight)
	{
		FStraightExtrusion InstancedBasis = StraightBasis;
		InstancedBasis.UpdateBasis(LengthPercent);
		return InstancedBasis.Transform(NewVertexPosition);
	}
	if (ExtrusionMethod == EExtrusionMethods::Rotation)
	{
		FRotationExtrusion InstancedBasis = RotationBasis;
		InstancedBasis.UpdateBasis(LengthPercent);
		return InstancedBasis.Transform(NewVertexPosition);
	}
	if (ExtrusionMethod == EExtrusionMethods::Spline)
	{
		FSplineExtrusion InstancedBasis = SplineBasis;
		InstancedBasis.UpdateBasis(LengthPercent);
		return InstancedBasis.Transform(NewVertexPosition);
	}

	ensureMsgf(false, TEXT("How did we get here?"));
	return FVector::ZeroVector;
}

void UCrossSectionMeshComponent::GenerateVerts(FDynamicMesh3& GeneratedMesh)
{
	// Buffer Structure: 2 vector<ints> each of size CrossSectionResolution + 2
	// [X: Vertices of cross section including bottom deck, total # = 2 * CrossSectionResolution + 2]
	// [Y: Each cross section, 2]
	//DeckCrossSectionIndices.Reserve(2);
	int ExtraForBackDeck = bMirrorShape ? 0 : 2;
	TArray<int> InitTemplate;
	InitTemplate.Init(0, CrossSectionResolution*2 + ExtraForBackDeck);
	DeckCrossSectionIndices.Init(InitTemplate, 2);
	
	// Buffer Structure: LengthResolution number of Vector<ints>, each of size CrossSectionResolution
	// [X: Vertices of cross section along 1 length slice, total # = CrossSectionResolution]
	// [Y: Arrays of vertex indices along length slices, total # = LengthResolution]
	InitTemplate.Init(0, CrossSectionResolution);
	CrossSectionIndices.Init(InitTemplate, LengthResolution);
	CrossSectionMirroredIndices.Init(InitTemplate, LengthResolution);

	// Empty look up indices
	{
		LU_CrossSectionIndices.Empty();
		LU_CrossSectionMirroredIndices.Empty();
		LU_DeckCrossSectionIndices.Empty();
		LU_DeckTopIndices.Empty();
		LU_DeckFrontIndices.Empty();
		LU_DeckBackIndices.Empty();
		LU_DeckBottomIndices.Empty();
	}

	if (!bMirrorShape)
	{
		InitTemplate.Init(0, 2);
		// [X: Vertices of deck at the top, total # = 2]
		// [Y: Arrays of vertex indices along length slices, total # = LengthResolution]
		DeckTopIndices.Init(InitTemplate, LengthResolution);
		// [X: Vertices of deck at the front, total # = 2]
		// [Y: Arrays of vertex indices along length slices, total # = LengthResolution]
		DeckFrontIndices.Init(InitTemplate, LengthResolution);
		// [X: Vertices of deck at the back, total # = 2]
		// [Y: Arrays of vertex indices along length slices, total # = LengthResolution]
		DeckBackIndices.Init(InitTemplate, LengthResolution);
		// [X: Vertices of deck at the bottom, total # = 2]
		// [Y: Arrays of vertex indices along length slices, total # = LengthResolution]
		DeckBottomIndices.Init(InitTemplate, LengthResolution);
	}
	
	for (int yS = 0; yS < LengthResolution; yS++)
	{
		float YPercent = yS / (LengthResolution - 1.f);
		const float Y = LengthWidget.Y * YPercent;

		// Allow height curve to modulate global height along the curve
		float Height = GetVertexBaseHeight(YPercent);
		
		// Define basis vectors (used for normals) (normally, only right is the one thats gonna be changing, forward is too but we don't compute those normals directly but through tri-data) only for generateing vertex positions
		UpdateBasis(YPercent);
		
		// Create Verts
		for (int xS = 0; xS < CrossSectionResolution; xS++)
		{
			// Base Vertices
			const float Perc = xS * 1.f / (CrossSectionResolution-1.f);

			float Z = GetVertexCrossSectionHeight(Perc) * Height;
			float X = GetVertexDepth(YPercent, Perc);

			const FVector3d CrossSectionVert = Transform(X, Y, Z);
			const int idx = GeneratedMesh.AppendVertex(CrossSectionVert);
			GeneratedMesh.SetVertexColor(idx, FVector3f(Perc, YPercent, 1.f));
			
			CrossSectionIndices[yS][xS] = (idx);
			LU_CrossSectionIndices.Add(idx);

			// Set UVs (Normalize to 1 m along the length)
			UVMap.Add(idx, FVector2f(YPercent, Perc) * MaterialAssignment.UVCrossSectionTiling);

			// If mirrored, add in the mirrored version to a seperate buffer
			if (bMirrorShape)
			{
				if (Perc == 0.f || Perc == 1.f)
				{
					// Don't duplicate these vertices as they're shared, otherwise our normals are split
					CrossSectionMirroredIndices[yS][xS] = (idx);	
				}
				else
				{
					const FVector3d CrossSectionMirroredVert = Transform(X, Y, -Z);
					const int mirroredIdx = GeneratedMesh.AppendVertex(CrossSectionMirroredVert);
					GeneratedMesh.SetVertexColor(mirroredIdx, FVector3f(Perc, YPercent, -1.f));
					
					CrossSectionMirroredIndices[yS][xS] = (mirroredIdx);
					LU_CrossSectionIndices.Add(mirroredIdx);
					
					UVMap.Add(mirroredIdx, FVector2f(YPercent, Perc) * MaterialAssignment.UVCrossSectionTiling);
				}
			}
			
			// Cross Section Split Indices 
			if (yS == 0 || yS == LengthResolution-1)
			{
				int yIdx = yS == 0 ? 0 : 1;
				const float BottomHeight = bMirrorShape ? -Z : GetBottomExtrudeHeight(YPercent);
				int bottomSplitIdx = GeneratedMesh.AppendVertex(Transform(X, Y, BottomHeight));
				int topSplitIdx = GeneratedMesh.AppendVertex(CrossSectionVert);

				GeneratedMesh.SetVertexColor(bottomSplitIdx, FVector3f(Perc, YPercent, 3.f));
				GeneratedMesh.SetVertexColor(topSplitIdx, FVector3f(Perc, YPercent, 1.f));

				GeneratedMesh.SetVertexNormal(bottomSplitIdx, yIdx == 0 ? -YNormal() : YNormal());
				GeneratedMesh.SetVertexNormal(topSplitIdx, yIdx == 0 ? -YNormal() : YNormal());

				// Set UVs
				const float CrossSectionLengthFactor = DepthWidget.X / (DepthWidget.X - BackWallExtrusion.X); // We take the whole cross section as 1 UV element, scaled for back extrusion
				const float XUVAmount = Perc * CrossSectionLengthFactor * MaterialAssignment.UVTopBotDeckTiling.Y; // Along full Y Depth
				UVMap.Add(bottomSplitIdx, FVector2f(XUVAmount, 0.f));
				UVMap.Add(topSplitIdx, FVector2f(XUVAmount, MaterialAssignment.UVHeightTiling * Z/Height));
				
				DeckCrossSectionIndices[yIdx][2*xS] = (bottomSplitIdx);
				DeckCrossSectionIndices[yIdx][(2*xS)+1] = (topSplitIdx);
				LU_DeckCrossSectionIndices.Add(bottomSplitIdx);
				LU_DeckCrossSectionIndices.Add(topSplitIdx);
			}

			if (Perc == 1.f)
			{
				TopEdgeVertices.Add(CrossSectionVert); // Useful for vert generation
				TopEdgeIndices.Add(idx);
			}

			// Don't consider top / bottom deck elements if mirrored so it allows for cylindrical shapes
			if (!bMirrorShape)
			{
				// Top Part Of Deck Split
				if (Perc == 1.f)
				{
					const float HeightToUse = BackWallExtrusion.X == 0  ? GetVertexCrossSectionHeight(1.f) * Height : Height;
					FVector BackTop = Transform(BackWallExtrusion.X, Y, HeightToUse);

					int topSplitIdx = GeneratedMesh.AppendVertex(CrossSectionVert);
					int topDeckSplitIdx = GeneratedMesh.AppendVertex(BackTop);

					GeneratedMesh.SetVertexColor(topSplitIdx, FVector3f(Perc, YPercent, 1.f));
					GeneratedMesh.SetVertexColor(topDeckSplitIdx, FVector3f(-1.f, YPercent, 2.f));

					GeneratedMesh.SetVertexNormal(topSplitIdx, ZNormal());
					GeneratedMesh.SetVertexNormal(topDeckSplitIdx, ZNormal());
				
					DeckTopIndices[yS][0] = (topSplitIdx); // Front Top
					DeckTopIndices[yS][1] = (topDeckSplitIdx); // Back Top
					LU_DeckTopIndices.Add(topSplitIdx);
					LU_DeckTopIndices.Add(topDeckSplitIdx);
					
					// Set UVs
					UVMap.Add(topSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, 0.f));
					UVMap.Add(topDeckSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, 1.f * MaterialAssignment.UVTopBotDeckTiling.X));
				}
				// Bottom part of deck split
				else if (Perc == 0.f) 
				{
					// Create bottom face
					{
						// We create vertices that are at the bottom, so we get cube-y shapes!
						FVector FrontBottom = Transform(GetVertexDepth(YPercent, 0.f), Y, GetBottomExtrudeHeight(YPercent));
						FVector BackBottom = Transform(BackWallExtrusion.X, Y, GetBottomExtrudeHeight(YPercent));

						int bottomSplitIdx = GeneratedMesh.AppendVertex(FrontBottom);
						int bottomDeckSplitIdx = GeneratedMesh.AppendVertex(BackBottom);

						GeneratedMesh.SetVertexColor(bottomSplitIdx, FVector3f(-2.f, YPercent, 0.f));
						GeneratedMesh.SetVertexColor(bottomDeckSplitIdx, FVector3f(-1.f, YPercent, 0.f));

						GeneratedMesh.SetVertexNormal(bottomSplitIdx, -ZNormal());
						GeneratedMesh.SetVertexNormal(bottomDeckSplitIdx, -ZNormal());

						// Set UVs
						UVMap.Add(bottomSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, 0.f));
						UVMap.Add(bottomDeckSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, 1.f * MaterialAssignment.UVTopBotDeckTiling.Y));

						DeckBottomIndices[yS][0] = (bottomSplitIdx); // Front Top
						DeckBottomIndices[yS][1] = (bottomDeckSplitIdx); // Back Top
						LU_DeckBottomIndices.Add(bottomSplitIdx);
						LU_DeckBottomIndices.Add(bottomDeckSplitIdx);
					}
					// Create Front Face
					{
						// We create vertices that are at the bottom, so we get cube-y shapes!
						FVector FrontBottom = Transform(GetVertexDepth(YPercent, 0.f), Y, GetBottomExtrudeHeight(YPercent));

						int frontBottomSplitIdx = GeneratedMesh.AppendVertex(FrontBottom);
						int frontTopSplitIdx = GeneratedMesh.AppendVertex(CrossSectionVert);

						GeneratedMesh.SetVertexColor(frontBottomSplitIdx, FVector3f(-2.f, YPercent, 0.f));
						GeneratedMesh.SetVertexColor(frontTopSplitIdx, FVector3f(Perc, YPercent, 1.f));

						GeneratedMesh.SetVertexNormal(frontBottomSplitIdx, XNormal());
						GeneratedMesh.SetVertexNormal(frontTopSplitIdx, XNormal());

						// Set UVs
						UVMap.Add(frontBottomSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, 0.f));
						UVMap.Add(frontTopSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, MaterialAssignment.UVHeightTiling * Z/Height)); 

						DeckFrontIndices[yS][0] = (frontBottomSplitIdx); // Front Top
						DeckFrontIndices[yS][1] = (frontTopSplitIdx); // Back Top
						LU_DeckFrontIndices.Add(frontBottomSplitIdx);
						LU_DeckFrontIndices.Add(frontTopSplitIdx);
					}
				}
			}
		}
		
		// Append cross section & back faces of deck into DeckCrossSectionIndices
		if (!bMirrorShape)
		{
			const float HeightToUse = BackWallExtrusion.X == 0 ? GetVertexCrossSectionHeight(1.f) * Height : Height;
			
			FVector BackTop = Transform(BackWallExtrusion.X, Y, HeightToUse);
			FVector BackBottom = Transform(BackWallExtrusion.X, Y, GetBottomExtrudeHeight(YPercent));

			int backTopSplitIdx = GeneratedMesh.AppendVertex(BackTop);
			int backBottomSplitIdx = GeneratedMesh.AppendVertex(BackBottom);

			GeneratedMesh.SetVertexColor(backTopSplitIdx, FVector3f(-1.f, YPercent, 2.f));
			GeneratedMesh.SetVertexColor(backBottomSplitIdx, FVector3f(-1.f, YPercent, 0.f));

			GeneratedMesh.SetVertexNormal(backTopSplitIdx, -ZNormal());
			GeneratedMesh.SetVertexNormal(backBottomSplitIdx, -ZNormal());

			// Set UVs
			UVMap.Add(backBottomSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, 0.f));
			UVMap.Add(backTopSplitIdx, FVector2f(YPercent * MaterialAssignment.UVCrossSectionTiling.X, 1.f * MaterialAssignment.UVHeightTiling));
			
			DeckBackIndices[yS][0] = (backTopSplitIdx);
			DeckBackIndices[yS][1] = (backBottomSplitIdx);
			LU_DeckBackIndices.Add(backTopSplitIdx);
			LU_DeckBackIndices.Add(backBottomSplitIdx);

			if (yS == 0 || yS == LengthResolution-1) 
			{
				int yIdx = yS == 0 ? 0 : 1;

				int rightBottomSplitIdx = GeneratedMesh.AppendVertex(BackBottom);
				int rightTopSplitIdx = GeneratedMesh.AppendVertex(BackTop);

				GeneratedMesh.SetVertexColor(rightBottomSplitIdx, FVector3f(-1.f, YPercent, 0.f));
				GeneratedMesh.SetVertexColor(rightTopSplitIdx, FVector3f(-1.f, YPercent, 2.f));

				GeneratedMesh.SetVertexNormal(rightBottomSplitIdx, yIdx == 0 ? -YNormal() : YNormal());
				GeneratedMesh.SetVertexNormal(rightTopSplitIdx, yIdx == 0 ? -YNormal() : YNormal());

				// Set UVs
				UVMap.Add(rightBottomSplitIdx, FVector2f(1.f * MaterialAssignment.UVTopBotDeckTiling.Y, 0.f));
				UVMap.Add(rightTopSplitIdx, FVector2f(1.f * MaterialAssignment.UVTopBotDeckTiling.Y, 1.f * MaterialAssignment.UVHeightTiling));

				DeckCrossSectionIndices[yIdx][2*CrossSectionResolution] = (rightBottomSplitIdx);
				DeckCrossSectionIndices[yIdx][2*CrossSectionResolution+1] = (rightTopSplitIdx);
				LU_DeckCrossSectionIndices.Add(rightBottomSplitIdx);
				LU_DeckCrossSectionIndices.Add(rightTopSplitIdx);
			}
		}
	}
}

void UCrossSectionMeshComponent::SetupConnectivity(FDynamicMesh3& GeneratedMesh)
{
	TriBuffer.Empty();

	// Tris along the length of the mesh
	for (int y = 0; y < CrossSectionIndices.Num() - 1; y++)
	{
		for (int i = 0; i < CrossSectionIndices[y].Num() - 1; i++)
		{
			// Generate quads
			int i1 = CrossSectionIndices[y][i];
			int i2 = CrossSectionIndices[y][i+1];
			int i3 = CrossSectionIndices[y+1][i+1];
			int i4 = CrossSectionIndices[y+1][i];

			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i2, i3)); 
			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i3, i4));

			// Generate mirrored quads
			// If i = 0, we sample from CrossSectionIndices (not duplicating the seperation point so the normals there don't get split!)
			if (bMirrorShape)
			{
				i1 = CrossSectionMirroredIndices[y][i];
				i2 = CrossSectionMirroredIndices[y][i+1];
				i3 = CrossSectionMirroredIndices[y+1][i+1];
				i4 = CrossSectionMirroredIndices[y+1][i];
				// Flip winding number
				TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i3, i2)); 
				TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i4, i3));
			}
		}
	}

	// Proper left/right most cross sections (for split normals), we're doin quads so double up
	for (int y = 0; y < 2; y++)
	{
		for (int i = 0; i < DeckCrossSectionIndices[y].Num() - 3; i+=2) // Last point is back bottom
		{
			int i1 = DeckCrossSectionIndices[y][i];	 // Left Bottom
			int i2 = DeckCrossSectionIndices[y][i+1];// Left Top
			int i3 = DeckCrossSectionIndices[y][i+2];// Right Bottom
			int i4 = DeckCrossSectionIndices[y][i+3];// Right Top

			// Adjusted winding numbers depending on which side we're on
			if (y == 0 && !bRemoveStartEndCap)
			{
				TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i3, i2, MaterialAssignment.SideFaceID));
				TriBuffer.Add(GeneratedMesh.AppendTriangle(i2, i3, i4, MaterialAssignment.SideFaceID));
			}
			else if (y == 1 && !bRemoveFinalEndCap)
			{
				TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i2, i3, MaterialAssignment.SideFaceID));
				TriBuffer.Add(GeneratedMesh.AppendTriangle(i2, i4, i3, MaterialAssignment.SideFaceID));
			}
		}
	}

	// Everything below does not exist if mirrored
	if (bMirrorShape) return;

	// Top part of deck
	for (int y = 0; y < DeckTopIndices.Num() - 1; y++)
	{
		for (int i = 0; i < DeckTopIndices[y].Num() - 1; i++) // 2 vertices per top-fragment 
		{
			// Generate quads
			int i1 = DeckTopIndices[y][i];
			int i2 = DeckTopIndices[y][i+1];
			int i3 = DeckTopIndices[y+1][i+1];
			int i4 = DeckTopIndices[y+1][i];

			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i2, i3, MaterialAssignment.TopFaceID)); 
			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i3, i4, MaterialAssignment.TopFaceID)); 
		}
	}

	// Bottom part of deck (same as top)
	for (int y = 0; y < DeckBottomIndices.Num() - 1; y++)
	{
		for (int i = 0; i < DeckBottomIndices[y].Num() - 1; i++) // 2 vertices per bottom-fragment 
		{
			// Generate quads
			int i1 = DeckBottomIndices[y][i];
			int i2 = DeckBottomIndices[y][i+1];
			int i3 = DeckBottomIndices[y+1][i+1];
			int i4 = DeckBottomIndices[y+1][i];

			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i3, i2)); 
			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i4, i3)); 
		}
	}
	
	// Back part of deck
	for (int y = 0; y < DeckBackIndices.Num() - 1; y++)
	{
		for (int i = 0; i < DeckBackIndices[y].Num() - 1; i++) // 2 vertices per back-fragment 
		{
			// Generate quads
			int i1 = DeckBackIndices[y][i];
			int i2 = DeckBackIndices[y][i+1];
			int i3 = DeckBackIndices[y+1][i+1];
			int i4 = DeckBackIndices[y+1][i];

			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i2, i3, MaterialAssignment.BackFaceID)); 
			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i3, i4, MaterialAssignment.BackFaceID)); 
		}
	}
	
	// Front part of deck
	for (int y = 0; y < DeckFrontIndices.Num() - 1; y++)
	{
		for (int i = 0; i < DeckFrontIndices[y].Num() - 1; i++) // 2 vertices per back-fragment 
		{
			// Generate quads
			int i1 = DeckFrontIndices[y][i];
			int i2 = DeckFrontIndices[y][i+1];
			int i3 = DeckFrontIndices[y+1][i+1];
			int i4 = DeckFrontIndices[y+1][i];

			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i2, i3, MaterialAssignment.FrontFaceID)); 
			TriBuffer.Add(GeneratedMesh.AppendTriangle(i1, i3, i4, MaterialAssignment.FrontFaceID)); 
		}
	}

	// Remap material IDs from TriGroups
	
}

void UCrossSectionMeshComponent::ValidateInputs()
{
	if (bMirrorShape) BackWallExtrusion.X = 0.f;

	// Check if we have a mesh, otherwise set it to dirty for initial init
	if (GetMesh()->VertexCount() == 0.f) bMeshDirty = true;
	// Make sure we have our look up indices since we need those to update UVs, if we dont have them, regenerate the mesh so we get them
	if (LU_CrossSectionIndices.IsEmpty()) bMeshDirty = true; // just checking 1 single it probably reflects the state of all index arrays
	
	// Ensure widgets are properly sized
	HeightWidget = FVector(0,0, FMath::Abs(HeightWidget.Z));
	LengthWidget = FVector(0, LengthWidget.Y, 0);
	DepthWidget = FVector(FMath::Abs(DepthWidget.X), 0, 0);
	BackWallExtrusion = FVector(-FMath::Abs(BackWallExtrusion.X), 0, 0);
	
	// Verify we have a spline component to do spline extrusion, otherwise set it to striaght
	if (ExtrusionMethod == EExtrusionMethods::Spline)
	{
		if (const auto SplineComponent = GetOwner()->FindComponentByClass<USplineComponent>())
		{
			LengthWidget.Y = SplineComponent->GetSplineLength();
			SplineBasis.Initialize(SplineComponent, SplineOffset);
			if (bUseMeshSegmentLength)
			{
				LengthResolution = FMath::Max(SplineComponent->GetSplineLength() / MeshSegmentLength, 2);
				bMeshDirty = true;
			}
			else
				MeshSegmentLength = FMath::Max(SplineComponent->GetSplineLength() / (1.f * LengthResolution), 2);
		}
		else
		{
			ExtrusionMethod = EExtrusionMethods::Straight;
		}
	}
	else if (ExtrusionMethod == EExtrusionMethods::Rotation)
	{
		if (bSyncRadius) Radius.Y = Radius.X;
		
		Radius = Radius.GetAbs();
		RotationBasis.Initialize(Radius, FMath::DegreesToRadians(RotateExtrudeAngle), FMath::DegreesToRadians(RotationOffset), SpiralHeight, bUseHeightSpiralCurve, HeightSpiralCurve, bCenterPivot);
		LengthWidget.Y = FMath::Abs(Radius.X * FMath::DegreesToRadians(RotateExtrudeAngle));

		// Depending on orientation, these can't become greater than our radius as determined by the arc length
		RotationDepth = FMath::Clamp(FMath::Abs(RotationDepth), 0, RotateExtrudeAngle < 0.f ? Radius.GetMin() : FMath::Abs(RotationDepth));
		RotationBackExtrusion = FMath::Clamp(FMath::Abs(RotationBackExtrusion), 0, RotateExtrudeAngle > 0.f ? Radius.GetMin() : FMath::Abs(RotationBackExtrusion));
		DepthWidget = FVector(RotationDepth, 0, 0);
		BackWallExtrusion = FVector(-RotationBackExtrusion, 0, 0);
	}
	else
	{
		// Straight Extrusion
		StraightBasis.Initialize(LengthWidget.Y, HeightSpiral, bUseHeightSpiralCurve, HeightSpiralCurve);
	}

	// Compute Tiling for each segment
	{
		MaterialAssignment.UVCrossSectionTiling.X = LengthWidget.Y / MaterialAssignment.UVTilingLength;
		MaterialAssignment.UVCrossSectionTiling.Y = DepthWidget.X / MaterialAssignment.UVTilingLength;

		MaterialAssignment.UVTopBotDeckTiling.X = FMath::Abs(BackWallExtrusion.X) / MaterialAssignment.UVTilingLength;
		MaterialAssignment.UVTopBotDeckTiling.Y = (DepthWidget.X - BackWallExtrusion.X) / MaterialAssignment.UVTilingLength;

		MaterialAssignment.UVHeightTiling = HeightWidget.Z / MaterialAssignment.UVTilingLength;
	}

	// Refresh material IDs
	MaterialAssignment.RefreshMaterialIDs();

	// Ensure cache data to be used for updates isn't lost, refresh if so
	if (TopEdgeIndices.IsEmpty() || TopEdgeVertices.IsEmpty())
	{
		bMeshDirty = true;
	}
}

#if WITH_EDITOR

void UCrossSectionMeshComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	static TArray<FName> RegenMeshProperties = TArray<FName>(
{
		GET_MEMBER_NAME_CHECKED(UCrossSectionMeshComponent, bUseMeshSegmentLength),
		GET_MEMBER_NAME_CHECKED(UCrossSectionMeshComponent, MeshSegmentLength),
		GET_MEMBER_NAME_CHECKED(UCrossSectionMeshComponent, bMirrorShape),
		GET_MEMBER_NAME_CHECKED(UCrossSectionMeshComponent, CrossSectionResolution),
		GET_MEMBER_NAME_CHECKED(UCrossSectionMeshComponent, LengthResolution)
		});
	
	if (RegenMeshProperties.Contains(PropertyName))
	{
		bMeshDirty = true;
	}

	if (bFlipNormals)
	{
		UGeometryScriptLibrary_MeshNormalsFunctions::FlipNormals(GetDynamicMesh());
		bFlipNormals = false;
	}

	if (bForceRebuild)
	{
		bMeshDirty = true;
		ConstructMesh();
		bForceRebuild = false;
	}
}
#endif


float UCrossSectionMeshComponent::GetVertexBaseHeight(float LengthPercent) const
{
	if (bUseHeightCurve && ExtrusionMethod == EExtrusionMethods::Spline)
	{
		LengthPercent = LengthPercent * HeightCurveSplineTiling;
		LengthPercent -= FMath::Floor(LengthPercent);
	}
	const float HeightConstant = HeightWidget.Z;
	if (bUseHeightCurve && HeightCurve.GetRichCurveConst()->HasAnyData())
	{
		return FMath::Max(0, HeightCurve.GetRichCurveConst()->Eval(LengthPercent * HeightCurve.GetRichCurveConst()->GetLastKey().Time) * HeightConstant);
	}
	return HeightConstant;
}

float UCrossSectionMeshComponent::GetVertexCrossSectionHeight(float CrossSectionPercent) const
{
	if (CrossSectionShape.GetRichCurveConst()->HasAnyData())
	{
		return CrossSectionShape.GetRichCurveConst()->Eval(CrossSectionPercent * CrossSectionShape.GetRichCurveConst()->GetLastKey().Time);
	}
	return 1.f;
}

float UCrossSectionMeshComponent::GetVertexDepth(float LengthPercent, float CrossSectionPercent) const
{
	if (bUseDepthCurve && ExtrusionMethod == EExtrusionMethods::Spline)
	{
		LengthPercent = LengthPercent * DepthCurveSplineTiling;
		LengthPercent -= FMath::Floor(LengthPercent);
	}
	const float DepthConstant = (1.f - CrossSectionPercent) * DepthWidget.X;
	if (bUseDepthCurve && DepthCurve.GetRichCurveConst()->HasAnyData())
	{
		return DepthCurve.GetRichCurveConst()->Eval(LengthPercent * DepthCurve.GetRichCurveConst()->GetLastKey().Time) * DepthConstant;
	}
	return DepthConstant;
}

float UCrossSectionMeshComponent::GetBottomExtrudeHeight(float LengthPercent) const
{
	float BottomConst = BottomExtrudeWidget.Z;
	if (bUseBottomWallCurve && BottomCurve.GetRichCurveConst()->HasAnyData())
	{
		BottomConst *= BottomCurve.GetRichCurveConst()->Eval(LengthPercent * BottomCurve.GetRichCurveConst()->GetLastKey().Time);
	}

	return BottomConst;
}

void UCrossSectionMeshComponent::UpdateBasis(float Percent)
{
	switch (ExtrusionMethod)
	{
		case EExtrusionMethods::Straight:
			return StraightBasis.UpdateBasis(Percent);
		case EExtrusionMethods::Rotation:
			return RotationBasis.UpdateBasis(Percent);
		case EExtrusionMethods::Spline:
			return SplineBasis.UpdateBasis(Percent);
	}
}

FVector UCrossSectionMeshComponent::Transform(float X, float Y, float Z) const
{
	switch (ExtrusionMethod)
	{
		case EExtrusionMethods::Straight:
			return StraightBasis.Transform(X, Y, Z);
		case EExtrusionMethods::Rotation:
			return RotationBasis.Transform(X, Y, Z);
		case EExtrusionMethods::Spline:
			return SplineBasis.Transform(X, Y, Z);
	}
	return FVector::ZeroVector;
}

FVector3f UCrossSectionMeshComponent::XNormal() const
{
	switch (ExtrusionMethod)
	{
		case EExtrusionMethods::Straight:
			return StraightBasis.Xn;
		case EExtrusionMethods::Rotation:
			return RotationBasis.Xn;
		case EExtrusionMethods::Spline:
			return SplineBasis.Xn;
	}
	return FVector3f::ZeroVector;
}

FVector3f UCrossSectionMeshComponent::YNormal() const
{
	switch (ExtrusionMethod)
	{
		case EExtrusionMethods::Straight:
			return StraightBasis.Yn;
		case EExtrusionMethods::Rotation:
			return RotationBasis.Yn;
		case EExtrusionMethods::Spline:
			return SplineBasis.Yn;
	}
	return FVector3f::ZeroVector;
}

FVector3f UCrossSectionMeshComponent::ZNormal() const
{
	switch (ExtrusionMethod)
	{
		case EExtrusionMethods::Straight:
			return StraightBasis.Zn;
		case EExtrusionMethods::Rotation:
			return RotationBasis.Zn;
		case EExtrusionMethods::Spline:
			return SplineBasis.Zn;
	}
	return FVector3f::ZeroVector;
}