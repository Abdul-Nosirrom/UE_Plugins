// Copyright 2023 CoC All rights reserved


#include "CrossSectionMeshActor.h"

#include "CrossSectionMeshComponent.h"
#include "MaterialDomain.h"


// Sets default values
ACrossSectionMeshActor::ACrossSectionMeshActor()
{
	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = false;

	CrossSectionMesh = CreateDefaultSubobject<UCrossSectionMeshComponent>(FName("Cross Section Mesh"));
	CrossSectionMesh->SetMobility(EComponentMobility::Movable);
	CrossSectionMesh->SetGenerateOverlapEvents(false);
	CrossSectionMesh->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	CrossSectionMesh->CollisionType = ECollisionTraceFlag::CTF_UseDefault;
	
	CrossSectionMesh->SetMaterial(0, UMaterial::GetDefaultMaterial(MD_Surface));
	SetRootComponent(CrossSectionMesh);
}

void ACrossSectionMeshActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

#if WITH_EDITOR 
	if (!bIsMoving)
#endif
	{
		CrossSectionMesh->ConstructMesh();
	}
}



