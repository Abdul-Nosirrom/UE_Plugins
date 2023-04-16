// 

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "CFWStatics.generated.h"

/**
 * 
 */
UCLASS()
class COREFRAMEWORK_API UCFWStatics : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

	// --- Player functions ------------------------------

	/**
	* Returns the pawn for the player controller at the specified player index, will return null if the pawn is not a character.
	* This will not include characters of remote clients with no available player controller, you can iterate the PlayerStates list for that.
	*
	* @param PlayerIndex	Index in the player controller list, starting first with local players and then available remote ones
	*/
	UFUNCTION(BlueprintPure, Category="Game", meta=(WorldContext="WorldContextObject", UnsafeDuringActorConstruction="true"))
	static class ARadicalCharacter* GetRadicalPlayer(const UObject* WorldContextObject, int32 PlayerIndex);
};
