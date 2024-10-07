// Copyright 2023 CoC All rights reserved

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Engine/DataAsset.h"
#include "TargetingPreset.generated.h"


UENUM(BlueprintType)
enum class ETargetingReferenceFrame : uint8
{
	/// @brief	Use the actor who requested the targeting's rotation as the reference frame for angles and distance offsets
	Actor,
	/// @brief	Use the default world basis as the reference frame for angles and distance offsets
	World,
	/// @brief	Use the camera rotation as the reference frame for angles and distance offsets. Falls back to Actor if targeting
	///			is performed on a non-player actor
	Camera,
	/// @brief	Use the input direction as the "Forward" vector, using this however would then ask for a backup in case input is zero
	Input
};

USTRUCT(BlueprintType)
struct COMBATFRAMEWORK_API FTargetingResult
{
	GENERATED_BODY()

	FTargetingResult() : TargetingComponent(nullptr) {}
	FTargetingResult(class UTargetingQueryComponent* InTargetingComp) : TargetingComponent(InTargetingComp) {}
	FTargetingResult(UTargetingQueryComponent* InTargetingComp, float InDist, float InAngle) : TargetingComponent(InTargetingComp), DistanceTo(InDist), Angle(InAngle) {}

	UPROPERTY(BlueprintReadOnly)
	UTargetingQueryComponent* TargetingComponent;
	UPROPERTY(BlueprintReadOnly)
	float DistanceTo;
	UPROPERTY(BlueprintReadOnly)
	float Angle;
};

USTRUCT(BlueprintType)
struct COMBATFRAMEWORK_API FTargetingAngleFilter
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	ETargetingReferenceFrame ReferenceFrame;
	/// @brief  Heading vector in the given reference frame to get angles from
	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	FVector HeadingVector = FVector::ForwardVector;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, UIMin=0, ClampMax=360, UIMax=360))
	FVector2D AngleThresholds;
};

USTRUCT(BlueprintType)
struct COMBATFRAMEWORK_API FTargetingSettings
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite)
	TWeakObjectPtr<AActor> Instigator;
	
	/// @brief  Reference frame for angle calculations (e.g angle from "what" forward vector?)
	UPROPERTY(Category="Settings", EditAnywhere, BlueprintReadWrite)
	ETargetingReferenceFrame AngleReferenceFrame;
	/// @brief  Backup reference frame in case input is zero
	UPROPERTY(Category="Settings", EditAnywhere, BlueprintReadWrite, meta=(InvalidEnumValues="ETargetingReferenceFrame::Input", EditCondition="AngleReferenceFrame==ETargetingReferenceFrame::Input", EditConditionHides))
	ETargetingReferenceFrame BackupAngleReferenceFrame;

	/// @brief  Reference frame for applying distance offsets. Becomes Center = ActorLocation + RefFrame.Rotate(DistanceCenterOffset);
	UPROPERTY(Category="Settings", EditAnywhere, BlueprintReadWrite, meta=(InvalidEnumValues="ETargetingReferenceFrame::Input"))
	ETargetingReferenceFrame DistanceOffsetReferenceFrame;
	/// @brief	If true, we do a shape sweep to the possible target and discard it if we encounter any hits along the way that isnt the target
	UPROPERTY(Category="Settings", EditAnywhere, BlueprintReadWrite)
	bool bRequireLineOfSight = true;
	/// @brief	If true, we discard any potential targets that are not in camera view
	UPROPERTY(Category="Settings", EditAnywhere, BlueprintReadWrite)
	bool bRequireOnScreen = true;

	/// @brief	Target must have all these tags in their targeting query component identity tags
	UPROPERTY(Category="Filters", EditAnywhere, BlueprintReadWrite, meta=(GameplayTagFilter="Targeting"))
	FGameplayTagContainer MustHaveAllTags;
	/// @brief	Target must have at least one of these tags in their targeting query component identity tags
	UPROPERTY(Category="Filters", EditAnywhere, BlueprintReadWrite, meta=(GameplayTagFilter="Targeting"))
	FGameplayTagContainer MustHaveAnyTags;
	/// @brief	Target must not have any of these tags in their tareting query component identity tags
	UPROPERTY(Category="Filters", EditAnywhere, BlueprintReadWrite, meta=(GameplayTagFilter="Targeting"))
	FGameplayTagContainer MustNotHaveTags;
	/// @brief  Additional angular filter options, wont be taken into account during scoring, just used to invalidate results
	UPROPERTY(Category="Filters", EditAnywhere, BlueprintReadWrite, meta=(NoElementDuplicate))
	TArray<FTargetingAngleFilter> AdditionalAngleFilters;
	
	/// @brief  Weight of normalized distance. Comparison result [DistWeight * NormDist + AngleWeight * NormAngle], highest wins.
	UPROPERTY(Category="Weights", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	float DistanceWeight;
	/// @brief  Weight of normalized angles. Comparison result [DistWeight * NormDist + AngleWeight * NormAngle], highest wins.
	UPROPERTY(Category="Weights", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, UIMin=0, ClampMax=1, UIMax=1))
	float AngleWeight;

	/// @brief  Discards any target found to be outside of this angle range
	UPROPERTY(Category="Thresholds", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, UIMin=0, ClampMax=360, UIMax=360))
	FVector2D AngleThresholds;
	/// @brief  Discards any target found to be outside of this distance range. We use Y (max) as the overlap radius, then X (min)
	///			to "make a hole in the sphere"
	UPROPERTY(Category="Thresholds", EditAnywhere, BlueprintReadWrite)
	FVector2D DistanceThresholds;
	
	/// @brief	Offset where the "Sphere Center" is for the initial overlap check, converted to the appropriate reference frame based on DistanceCheckReferenceFrame. 
	UPROPERTY(Category="Offsets", EditAnywhere, BlueprintReadWrite)
	FVector DistanceCenterOffset;
	/// @brief  Offset of where the "Best Distance" is. For example, if its 5m, then 5m away from the center is considered the best distance score, but the actual overlap isn't offset.
	///			Useful in cases of "ranged attack" targeting, where you still wanna target close by targets, but prefer ones who are some distance away.
	UPROPERTY(Category="Offsets", EditAnywhere, BlueprintReadWrite)
	float BestDistanceOffset;
	//UPROPERTY(Category="Offsets", EditAnywhere, BlueprintReadWrite, meta=(ClampMin=0, UIMin=0, ClampMax=360, UIMax=360))
	//float AngleCenterOffset;
	
	bool ProcessTargets(const TArray<FOverlapResult>& PotentialTargets, FTargetingResult& OutTarget) const;

	bool IsTargetValid(const UPrimitiveComponent* CheckTarget, float& Dist, float& Angle, float& NormalizedDist, float& NormalizedAngle) const;
	
	FVector GetCenterPosition() const;
	float GetSphereRadius() const;
	FRotator GetBasis(ETargetingReferenceFrame RefFrame) const;
	
	void DoVisualLog() const;
};

UCLASS(BlueprintType, CollapseCategories)
class COMBATFRAMEWORK_API UTargetingPreset : public UDataAsset
{
	GENERATED_BODY()

	friend class FTargetingSettingsDetails;
	
public:

	FORCEINLINE FTargetingSettings GetSettings() const { return TargetingSettings; }
	
protected:
	
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, meta=(ShowOnlyInnerProperties))
	FTargetingSettings TargetingSettings;
};
