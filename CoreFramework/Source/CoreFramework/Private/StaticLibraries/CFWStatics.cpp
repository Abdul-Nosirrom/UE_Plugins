// 


#include "StaticLibraries/CFWStatics.h"
#include "Actors/RadicalCharacter.h"
#include "Kismet/GameplayStatics.h"

ARadicalCharacter* UCFWStatics::GetRadicalPlayer(const UObject* WorldContextObject, int32 PlayerIndex)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, PlayerIndex);
	return PC ? Cast<ARadicalCharacter>(PC->GetPawn()) : nullptr;
}
