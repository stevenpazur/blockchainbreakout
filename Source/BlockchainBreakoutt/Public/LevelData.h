#pragma once

#include "LevelData.generated.h"

USTRUCT(BlueprintType)
struct FLevelData
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString LevelName = "1";

    UPROPERTY()
    bool UseRowOfOrcBlocks;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BlockPairingSet = 2;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TargetScore;

    UPROPERTY()
    float FallingSpeed;

    UPROPERTY()
    FVector BackgroundColor;
};
