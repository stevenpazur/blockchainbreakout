#pragma once

#include "DropState.generated.h"

USTRUCT(BlueprintType)
struct FDropState
{
    GENERATED_BODY()

    UPROPERTY()
    int32 X;          // The x-coordinate of the block

    UPROPERTY()
    int32 Y1;         // Current y-coordinate of the block

    UPROPERTY()
    int32 LoopIndex;  // Tracks the loop iteration

    UPROPERTY()
    int32 DropDistance; // How far the block should drop

    static FDropState Create(int32 InX, int32 InY1, int32 InLoopIndex, int32 InDropDistance)
    {
        FDropState Struct;
        Struct.X = InX;
        Struct.Y1 = InY1;
        Struct.LoopIndex = InLoopIndex;
        Struct.DropDistance = InDropDistance;
        return Struct;
    }
};