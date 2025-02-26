// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/Blueprint.h"
#include "TetrisBlockValue.generated.h"

USTRUCT(BlueprintType)
struct FTetrisBlockValue
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlockName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlockNameDisplay;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString BlockSymbol;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString ScoreValue;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool VolatilityGoingUp;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTexture2D* StockTickerTexture;

    FLinearColor Color;

    bool ForceVolatilityToGoUp;

    bool ForceVolatilityToGoDown;
};