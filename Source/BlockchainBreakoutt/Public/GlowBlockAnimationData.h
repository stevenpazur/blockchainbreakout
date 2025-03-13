#pragma once

#include "GlowBlockAnimationData.generated.h"

USTRUCT(BlueprintType)
struct FGlowBlockAnimationData
{
    GENERATED_BODY()

    UPROPERTY()
    TArray<AActor*> GlowBlocks;

    UPROPERTY()
    float ElapsedTime;

    UPROPERTY()
    FString BlockName;

    UPROPERTY()
    TArray<FVector> SuperBlockDropSpots;
};
