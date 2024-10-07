// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "NativeGameplayTags.h"
#include "LevelPrimitiveComponent.generated.h"

/* Define Base Gameplay Tag Category */
UE_DECLARE_GAMEPLAY_TAG_EXTERN(TAG_LevelPrimitive_Base)

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPrimitiveActivatedSignature);

class UActionSystemComponent;

UENUM()
enum EPrimitiveBehavior 
{
	LP_None					UMETA(DisplayName="No Default Behavior"),
	LP_OnOverlapRoot		UMETA(DisplayName="Register On Overlap With Root"),
	LP_OnOverlapComponent	UMETA(DisplayName="Register On Overlap With Specified Component"),
	LP_OnHitRoot			UMETA(DisplayName="Register On Hit With Root"),
	LP_OnHitComponent		UMETA(DisplayName="Register On Hit With Specified Component")
};

UCLASS(ClassGroup=(LevelPrimitives), BlueprintType, Blueprintable, meta=(BlueprintSpawnableComponent))
class ACTIONFRAMEWORK_API ULevelPrimitiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	ULevelPrimitiveComponent();

	FORCEINLINE FGameplayTag GetTag() const { return TagToGrant; }

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	virtual void OnRegister() override;
	
	UFUNCTION(Category=Callbacks, BlueprintCallable)
	void OnPrimitiveActivated() { OnPrimitiveActivatedDelegate.Broadcast(); }
	UFUNCTION(Category=Callbacks, BlueprintCallable)
	void OnPrimitiveDeactivated() { OnPrimitiveDeactivatedDelegate.Broadcast(); }

	UFUNCTION(Category=Info, BlueprintGetter)
	FHitResult GetHitResultIfAny() const { return BehaviorHit; }
	
	UFUNCTION(BlueprintCallable)
	void RegisterLevelPrimitive(UActionSystemComponent* Target) { RegisterLevelPrimitive(Target, TagToGrant); }
	void RegisterLevelPrimitive(UActionSystemComponent* Target, FGameplayTag GrantingTag);

	UFUNCTION(BlueprintCallable)
	void UnRegisterLevelPrimitive(UActionSystemComponent* Target) { UnRegisterLevelPrimitive(Target, TagToGrant); }
	void UnRegisterLevelPrimitive(UActionSystemComponent* Target, FGameplayTag GrantingTag);

	FORCEINLINE void SetDefaultBehavior(EPrimitiveBehavior Behavior, UPrimitiveComponent* InComponent)
	{
		DefaultBehavior = Behavior;
		Component = InComponent;
	}

	UFUNCTION(Category="Editor", BlueprintCallable)
	FORCEINLINE void SetTagToGrant(FGameplayTag InTag) { TagToGrant = InTag; }
	
protected:
	UPROPERTY(Category="Behavior", EditAnywhere, BlueprintReadOnly)
	TEnumAsByte<EPrimitiveBehavior> DefaultBehavior;

	UPROPERTY(Category="Behavior", EditAnywhere, BlueprintReadOnly, meta=(EditCondition="DefaultBehavior==LP_OnOverlapComponent || DefaultBehavior==LP_OnHitComponent"))
	UPrimitiveComponent* Component;

	UPROPERTY(Category="Behavior", EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0", UIMin="0", ClampMax="1", UIMax="1", EditCondition="DefaultBehavior==LP_OnHitRoot || DefaultBehavior==LP_OnHitComponent"))
	float UnRegisterDelay = 0.2f;
	
	UPROPERTY( Category="GamplayTags", EditAnywhere, BlueprintReadOnly)
	FGameplayTag TagToGrant;

	UPROPERTY(Category="Behavior", BlueprintReadOnly, Getter="GetHitResultIfAny")
	FHitResult BehaviorHit;

	UPROPERTY(Category=Callbacks, BlueprintAssignable)
	FPrimitiveActivatedSignature OnPrimitiveActivatedDelegate;
	UPROPERTY(Category=Callbacks, BlueprintAssignable)
	FPrimitiveActivatedSignature OnPrimitiveDeactivatedDelegate;

	FTimerHandle DelayedExitTimerHandle;
	
	UFUNCTION(BlueprintImplementableEvent)
	void GenerateComponentsCallback();

	UFUNCTION(BlueprintCallable)
	USceneComponent* K2_GenerateRequiredComponent(TSubclassOf<USceneComponent> Class)
	{
		auto RequiredComponent = NewObject<USceneComponent>(GetOwner()->GetRootComponent(), Class, TEXT("Primitive Target Component"), RF_Public);
		RequiredComponent->Mobility = EComponentMobility::Movable; // NOTE: This is what gets it to show up in hierarchy
		RequiredComponent->SetupAttachment(GetOwner()->GetRootComponent());
		RequiredComponent->CreationMethod = CreationMethod;
		RequiredComponent->RegisterComponent();

		return RequiredComponent;
	} 
	
	template<typename T>
	T* GenerateRequiredComponent();

	/* Response Callback */
	virtual bool OnBehaviorInitiated(AActor* OtherActor);
	virtual bool OnBehaviorExited(AActor* OtherActor);

	void DelayedBehaviorExit(AActor* OtherActor);

	/* Default Response Behavior Callbacks */
	UFUNCTION()
	void OnActorBeginOverlap(AActor* OverlappedActor, AActor* OtherActor) { OnBehaviorInitiated(OtherActor); };
	UFUNCTION()
	void OnActorEndOverlap(AActor* OverlappedActor, AActor* OtherActor) { OnBehaviorExited(OtherActor);};
	UFUNCTION()
	void OnActorHit(AActor* SelfActor, AActor* OtherActor, FVector NormalImpulse, const FHitResult& Hit)
	{
		if (OnBehaviorInitiated(OtherActor))
		{
			BehaviorHit = Hit;
			DelayedBehaviorExit(OtherActor);
		}
	};

	UFUNCTION()
	void OnComponentBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult & SweepResult) {OnBehaviorInitiated(OtherActor);};
	UFUNCTION()
	void OnComponentEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, int32 OtherBodyIndex) {OnBehaviorExited(OtherActor);};
	UFUNCTION()
	void OnComponentHit(UPrimitiveComponent* HitComponent, AActor* OtherActor, UPrimitiveComponent* OtherComp, FVector NormalImpulse, const FHitResult& Hit)
	{
		if (OnBehaviorInitiated(OtherActor))
		{
			BehaviorHit = Hit;
			DelayedBehaviorExit(OtherActor);
		}
	};
	
#if WITH_EDITORONLY_DATA
	//UPROPERTY()
	class UBillboardComponent* SpriteComponent;
#endif

	//UPROPERTY(EditAnywhere)
	//class USplineComponent* Spline;

	//UPROPERTY(EditAnywhere)
	//TSubclassOf<AActor> ClassToGenerate;
	
	//UPROPERTY(EditAnywhere)
	//class UCapsuleComponent* SplineMesh;
	
};


