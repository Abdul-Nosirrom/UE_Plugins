// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"

/**
 * 
 */
class EDITORUTILITIES_API FAttackDataPropertyDetails : public IPropertyTypeCustomization
{
public:
	static TSharedRef<IPropertyTypeCustomization> MakeInstance();

	virtual void CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	virtual void CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils) override;
	void InitializeProperties(TSharedPtr<IPropertyHandle> PropertyHandle);

	TSharedPtr<IPropertyHandle> AttackDataPropertyHandle;
	
	TSharedPtr<IPropertyHandle> ImpactType;
	TSharedPtr<IPropertyHandle> AnimDirection;
	TSharedPtr<IPropertyHandle> HitStop;
	TSharedPtr<IPropertyHandle> HitStun;
	TSharedPtr<IPropertyHandle> KnockBack;
	TSharedPtr<IPropertyHandle> KnockBackReferenceFrame;
	TSharedPtr<IPropertyHandle> AirFriction;
	TSharedPtr<IPropertyHandle> GroundFriction;
	TSharedPtr<IPropertyHandle> HitRoll;
	TSharedPtr<IPropertyHandle> RollFriction;
	TSharedPtr<IPropertyHandle> LaunchType;
	TSharedPtr<IPropertyHandle> bFaceDownWithHit;
	TSharedPtr<IPropertyHandle> bVelocityRotation;
	TSharedPtr<IPropertyHandle> bReverseHit;
	TSharedPtr<IPropertyHandle> bSteadyFacing;
	TSharedPtr<IPropertyHandle> GroundReaction;
	TSharedPtr<IPropertyHandle> bFaceDownWhenGround;
	TSharedPtr<IPropertyHandle> BounceVelocity;
	TSharedPtr<IPropertyHandle> WallReaction;
	TSharedPtr<IPropertyHandle> EffectToApply;
	TSharedPtr<IPropertyHandle> HitSparkEffect;

};
