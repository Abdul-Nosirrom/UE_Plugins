// Copyright 2023 CoC All rights reserved


#include "AttributeSystem/EntityEffectBlueprint.h"

#if WITH_EDITOR
UEntityEffectBlueprint* UEntityEffectBlueprint::FindRootEntityEffectBlueprint(UEntityEffectBlueprint* DerivedBlueprint)
{
	UEntityEffectBlueprint* ParentBP = NULL;

	// Determine if there is a entity effect blueprint in the ancestry of this class
	for (UClass* ParentClass = DerivedBlueprint->ParentClass; ParentClass != UObject::StaticClass(); ParentClass = ParentClass->GetSuperClass())
	{
		if (UEntityEffectBlueprint* TestBP = Cast<UEntityEffectBlueprint>(ParentClass->ClassGeneratedBy))
		{
			ParentBP = TestBP;
		}
	}

	return ParentBP;
}
#endif