// Copyright 2023 CoC All rights reserved


#include "Details/AttackDataDetails.h"

#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "Data/CombatData.h"
#include "Widgets/SBoolPropertyButton.h"

#define LOCTEXT_NAMESPACE "AttackDataDetailsCustomization"

TSharedRef<IPropertyTypeCustomization> FAttackDataPropertyDetails::MakeInstance()
{
	return MakeShareable(new FAttackDataPropertyDetails());

}

void FAttackDataPropertyDetails::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, FDetailWidgetRow& HeaderRow,
	IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	AttackDataPropertyHandle = PropertyHandle;
	InitializeProperties(AttackDataPropertyHandle);

	HeaderRow.NameContent()
	[
		SNew(STextBlock).Text(LOCTEXT("AtkTitle", "Attack Data"))
	];
}

void FAttackDataPropertyDetails::CustomizeChildren(TSharedRef<IPropertyHandle> PropertyHandle,
	IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	//bool bDummyBool = false;
	
	auto HeaderFonts = IDetailLayoutBuilder::GetDetailFont();
	HeaderFonts.TypefaceFontName = FName("Bold");
	HeaderFonts.Size = 1.5f * HeaderFonts.Size;
	
	// CORE
	ChildBuilder.AddCustomRow(LOCTEXT("Core", "Core Attack Info"))
	[
		SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("CoreText", "Core Attack Info")).Font(HeaderFonts)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot()
			[
				SNew(SProperty, ImpactType)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, HitStop)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, HitStun)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, AnimDirection)
			]
		]
	];

	// PHYSICS
	ChildBuilder.AddCustomRow(LOCTEXT("Physics", "Physics Info"))
	[
		SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("Phys", "Physics Info")).Font(HeaderFonts)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot()
			[
				SNew(SProperty, KnockBack)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, KnockBackReferenceFrame).ShouldDisplayName(false)
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot()
			[
				SNew(SProperty, AirFriction)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, GroundFriction)
			]
		]
	];

	// HIT INFO
	ChildBuilder.AddCustomRow(LOCTEXT("Hit", "Hit Behavior"))
	[
		SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("HitBehCat", "Hit Behavior")).Font(HeaderFonts)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot()
			[
				SNew(SProperty, LaunchType)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SBoolPropertyButton, bFaceDownWhenGround).Text(LOCTEXT("FaceDownButton", "Face Down?"))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SBoolPropertyButton, bVelocityRotation).Text(LOCTEXT("VelRotButton", "Velocity Rotation"))
			]
			+ SHorizontalBox::Slot()
			[	
				SNew(SBoolPropertyButton, bReverseHit).Text(LOCTEXT("RevHitButton", "Reverse Hit"))
			]
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot()
			[
				SNew(SProperty, GroundReaction)
			]
			+ SHorizontalBox::Slot()
			[	
				SNew(SBoolPropertyButton, bFaceDownWhenGround).Text(LOCTEXT("FaceDownGButton", "Face Down?"))
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, BounceVelocity).DisplayName(LOCTEXT("BoucneVelName", "Bounce"))
			]
		]
	];

	// Effect misc
	ChildBuilder.AddCustomRow(LOCTEXT("Misc", "Effects"))
	[
		SNew(SVerticalBox) + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Center)
		[
			SNew(STextBlock).Text(LOCTEXT("MiscText", "Effects")).Font(HeaderFonts)
		]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox) + SHorizontalBox::Slot()
			[
				SNew(SProperty, EffectToApply)
			]
			+ SHorizontalBox::Slot()
			[
				SNew(SProperty, HitSparkEffect)
			]
		]
	];
}

void FAttackDataPropertyDetails::InitializeProperties(TSharedPtr<IPropertyHandle> PropertyHandle)
{
	ImpactType = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,ImpactType));
	AnimDirection = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,AnimDirection));
	HitStop = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,HitStop));
	HitStun = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,HitStun));
	KnockBack = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,KnockBack));
	KnockBackReferenceFrame = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,KnockBackReferenceFrame));
	AirFriction = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,AirFriction));
	GroundFriction = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,GroundFriction));
	HitRoll = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,HitRoll));
	RollFriction = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,RollFriction));
	LaunchType = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,LaunchType));
	bFaceDownWithHit = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,bFaceDownWithHit));
	bVelocityRotation = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,bVelocityRotation));
	bReverseHit = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,bReverseHit));
	bSteadyFacing = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,bSteadyFacing));
	GroundReaction = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,GroundReaction));
	bFaceDownWhenGround = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,bFaceDownWhenGround));
	BounceVelocity = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,BounceVelocity));
	WallReaction = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,WallReaction));
	EffectToApply = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,EffectToApply));
	HitSparkEffect = PropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAttackData,HitSparkEffect));
}

#undef LOCTEXT_NAMESPACE
