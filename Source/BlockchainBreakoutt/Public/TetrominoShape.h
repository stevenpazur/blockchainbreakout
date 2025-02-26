// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "TetrominoShape.generated.h"

USTRUCT(BlueprintType)
struct FTetrominoShape
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FVector2D> BlockOffsets; // Offset positions of the blocks relative to the pivot
};