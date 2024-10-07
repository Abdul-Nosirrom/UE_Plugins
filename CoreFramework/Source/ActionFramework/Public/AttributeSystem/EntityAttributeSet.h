// Copyright 2023 CoC All rights reserved
#pragma once
#include "CoreMinimal.h"
#include "EntityAttributeSet.generated.h"

/* Forward Declarations */
class UAttributeSystemComponent;
class UEntityAttributeSet;
struct FActionActorInfo;

///	@brief	What is placed in an EntityAttributeSet and accessed via reflection using FEntityAttributeReflector
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FEntityAttributeData
{
	GENERATED_BODY()
	
	FEntityAttributeData() : BaseValue(0.f), CurrentValue(0.f) {}

	FEntityAttributeData(float DefaultValue) :	BaseValue(DefaultValue),
												CurrentValue(DefaultValue) {}

	virtual ~FEntityAttributeData() {}

	float GetCurrentValue() const { return CurrentValue; }
	void SetCurrentValue(float NewValue) { CurrentValue = NewValue; }

	float GetBaseValue() const { return BaseValue; }
	void SetBaseValue(float NewValue) { BaseValue = NewValue; } 
	
protected:

	/// @brief  Permanent value of an attribute. Affected during AttributeEffects with 'Instant' Duration.
	UPROPERTY(Category="Attribute", BlueprintReadOnly)
	float BaseValue;
	/// @brief  Transient value of an attribute. Affected during AttributeEffects with non-'Instant' durations, resets
	///			to BaseValue when complete.
	UPROPERTY(Category="Attribute", BlueprintReadOnly)
	float CurrentValue;
};

///	@brief	Describes an attribute data inside an attribute set. Using this will allow for reflection benefits in editor UI
///			Therefore its best to use it in functions or anywhere a details panel will be exposed. Holds a reference to the
///			AttributeData its associated with via an FProperty
USTRUCT(BlueprintType)
struct ACTIONFRAMEWORK_API FEntityAttribute
{
	GENERATED_BODY()

	FEntityAttribute()
		: Attribute(nullptr)
		, AttributeOwner(nullptr)
	{}

	FEntityAttribute(FProperty* NewProperty)
	{
		if (IsEntityAttributeDataProperty(NewProperty))
		{
			Attribute = NewProperty;
		}

		if (Attribute.Get())
		{
			AttributeOwner = Attribute->GetOwnerStruct();
			Attribute->GetName(AttributeName);
		}
	}

	bool IsValid() const { return Attribute != nullptr; }

	void SetUProperty(FProperty* NewProperty)
	{
		Attribute = NewProperty;
		if (NewProperty)
		{
			AttributeOwner = Attribute->GetOwnerStruct();
			Attribute->GetName(AttributeName);
		}
		else
		{
			AttributeOwner = nullptr;
			AttributeName.Empty();
		}
	}

	FProperty* GetUProperty() const
	{
		return Attribute.Get();
	}

	UClass* GetAttributeSetClass() const
	{
		check(Attribute.Get());
		return CastChecked<UClass>(Attribute->GetOwner<UObject>());
	}
	
	/// @brief	Checks to see if the given FProperty is an EntityAttributeData (or a sub-struct)  
	static bool IsEntityAttributeDataProperty(const FProperty* Property);
	static void GetAllAttributeProperties(TArray<FProperty*>& OutProperties, FString FilterMetaStr=FString(), bool bUseEditorOnlyData=true);

	// Getters for AttributeData (const & non-const)
	FEntityAttributeData* GetEntityAttributeData(UEntityAttributeSet* Src) const;
	const FEntityAttributeData* GetEntityAttributeData(const UEntityAttributeSet* Src) const;

	// Setters and getters for CurrentValue of the attribute
	void SetAttributeValue(float& NewValue, UEntityAttributeSet* Dest) const;
	float GetAttributeValue(const UEntityAttributeSet* Src) const;

	// Operator Overrides
	bool operator==(const FEntityAttribute& Other) const { return Attribute == Other.Attribute; }
	bool operator!=(const FEntityAttribute& Other) const { return Other.Attribute != Attribute; }
	
	/// @brief  Custom TypeHash to be a pointer hash of attribute data, and not attribute. This is important when using
	///			this struct as a key for a TMap
	friend uint32 GetTypeHash(const FEntityAttribute& InAttribute)
	{
		return PointerHash(InAttribute.Attribute.Get());
	}
	
#if WITH_EDITORONLY_DATA
	/** Custom serialization */
	void PostSerialize(const FArchive& Ar);
#endif 

	FString GetName() const
	{
		return AttributeName.IsEmpty() ? *GetNameSafe(Attribute.Get()) : AttributeName;
	}
	
	UPROPERTY(Category="EntityAttribute", VisibleAnywhere, BlueprintReadOnly)
	FString AttributeName;
	
private:

	friend class FEntityAttributePropertyDetails;
	
	UPROPERTY(Category="EntityAttribute", EditAnywhere)
	TFieldPath<FProperty> Attribute;

	UPROPERTY(Category="EntityAttribute", VisibleAnywhere)
	TObjectPtr<UStruct> AttributeOwner;
};

// TODO: Just make this a DataTable and auto-populate it
USTRUCT(BlueprintType)
struct FAttributeTableInit : public FTableRowBase
{
	GENERATED_BODY()

	FAttributeTableInit() {}
	FAttributeTableInit(FEntityAttribute InAttribute) : Attribute(InAttribute), InitialValue(0.f) {}

	UPROPERTY(Category="Attribute", EditDefaultsOnly)
	FEntityAttribute Attribute;

	UPROPERTY(Category="Attribute", EditDefaultsOnly, meta=(UIMin=0, ClampMin=0))
	float InitialValue;
};

UCLASS(ClassGroup=Actions, Blueprintable)
class ACTIONFRAMEWORK_API UAttributeInitDataTable : public UDataTable
{
	GENERATED_BODY()

public:
	UAttributeInitDataTable()
	{
		// This table only uses this row type
		RowStruct = FAttributeTableInit::StaticStruct();
	}

	const TArray<TSubclassOf<UEntityAttributeSet>>& GetRequiredAttributeSets() const { return AttributeSetsToInit; }
protected:
	
	UPROPERTY(Category="Attribute", EditDefaultsOnly)
	TArray<TSubclassOf<UEntityAttributeSet>> AttributeSetsToInit;

#if WITH_EDITOR 
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void AddRowInternal(FName RowName, uint8* RowDataPtr) override;
#endif
};

/* Forward Declarations */
struct FEntityEffectModCallbackData;

/// @brief	Defines the set of all EntityAttributes for your game. Subclass this and add attributes as desired.
///			For example, having a class structure like:
///			- UMyGameBaseAttributeSet: Defines all shared attributes [e.g Health, MaxHealth]
///			-- UCombatCharacterAttributeSet: Defines some more specific attributes [e.g DamageTaken, Defense]
///			--- UCharacterNameAttributeSet: Defines character specific attributes [e.g SandGauge]
UCLASS(DefaultToInstanced, Blueprintable)
class ACTIONFRAMEWORK_API UEntityAttributeSet : public UObject
{
	GENERATED_BODY()

public:
	/// @brief  Called just before modifying the value of an attribute. AttributeSet can make additional modifications here. Return true to continue, or false to throw out the modification.
	/// 
	///			NOTE: This is only called during 'Execute'. E.g, a modification to the 'Base Value' of an attribute. It's not called during an application of an AttributeEffect (E.g, +10 speed buff for 5 seconds)
	virtual bool PreEntityEffectExecute(const FEntityEffectModCallbackData& Data) { return true; }
	
	/// @brief	Called just before an AttributeEffect is executed to modify the base value of an attribute. No more changes can be made.
	/// 
	///			NOTE: This is only called during 'Execute'. E.g, a modification to the 'Base Value' of an attribute. It's not called during an application of an AttributeEffect (E.g, +10 speed buff for 5 seconds)
	virtual void PostEntityEffectExecute(const FEntityEffectModCallbackData& Data) { }
	
	/// @brief	Called just before any modifications happen to an attribute. No additional context here since anything can trigger this.
	///			[Executed effects, duration based effects, effects being removed, immunity being applied, stacking rules changing, etc...]
	///			NewValue is a mutable reference, so you're able to clamp the newly applied value.
	///
	///			This function is meant to enforce things like "Health = Clamp(Health, 0, MaxHealth)"
	virtual void PreAttributeChange(const FEntityAttribute& Attribute, float& NewValue) {}
	
	/// @brief  Called just after any modification happens to an attribute.
	virtual void PostAttributeChange(const FEntityAttribute& Attribute, float OldValue, float NewValue) {}

	/// @brief	Called just before any modifications happen to an attribute. No additional context here since anything can trigger this.
	///			[Executed effects, duration based effects, effects being removed, immunity being applied, stacking rules changing, etc...]
	///			NewValue is a mutable reference, so you're able to clamp the newly applied value.
	///
	///			This function is meant to enforce things like "Health = Clamp(Health, 0, MaxHealth)"
	virtual void PreAttributeBaseChange(const FEntityAttribute& Attribute, float& NewValue) const {}

	/// @brief  Called just after any modification happens to an attribute.
	virtual void PostAttributeBaseChange(const FEntityAttribute& Attribute, float OldValue, float NewValue) const {}

	void InitAttributes(const UAttributeInitDataTable* InitialStats);
	
	// Owner Information
	AActor* GetOwningActor() const { return CastChecked<AActor>(GetOuter()); }
	UAttributeSystemComponent* GetOwningAttributeSystemComponent() const;
	FActionActorInfo* GetActorInfo() const;
	
	/// @brief  Print debug information to the log
	virtual void PrintDebug() {}
	
	/// @brief  Get debug info of each attributes name and current value. Used for debug displays.
	virtual TMap<FName, float> GetAttributeValuePair() const { return TMap<FName, float>(); };
};

#pragma region Helper Macros

#define ENTITYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	static FEntityAttribute Get##PropertyName##Attribute() \
	{ \
		static FProperty* Prop = FindFieldChecked<FProperty>(ClassName::StaticClass(), GET_MEMBER_NAME_CHECKED(ClassName, PropertyName)); \
		return Prop; \
	}

#define ENTITYATTRIBUTE_VALUE_GETTER(PropertyName) \
	FORCEINLINE float Get##PropertyName() const \
	{ \
		return PropertyName.GetCurrentValue(); \
	}

#define ENTITYATTRIBUTE_VALUE_SETTER(PropertyName) \
	FORCEINLINE void Set##PropertyName(float NewVal) \
	{ \
		UAttributeSystemComponent* AttributeComp = GetOwningAttributeSystemComponent(); \
		if (ensure(AttributeComp)) \
		{ \
			AttributeComp->SetAttributeBaseValue(Get##PropertyName##Attribute(), NewVal); \
		}; \
	}

#define ENTITYATTRIBUTE_VALUE_INITTER(PropertyName) \
	FORCEINLINE void Init##PropertyName(float NewVal) \
	{ \
		PropertyName.SetBaseValue(NewVal); \
		PropertyName.SetCurrentValue(NewVal); \
	}

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	ENTITYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	ENTITYATTRIBUTE_VALUE_GETTER(PropertyName) \
	ENTITYATTRIBUTE_VALUE_SETTER(PropertyName) \
	ENTITYATTRIBUTE_VALUE_INITTER(PropertyName)

#pragma endregion
