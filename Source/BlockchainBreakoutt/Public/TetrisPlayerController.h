// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TetrisPlayerController.generated.h"

/**
 * 
 */
UCLASS()
class BLOCKCHAINBREAKOUTT_API ATetrisPlayerController : public APlayerController
{
	GENERATED_BODY()

protected:
	virtual void SetupInputComponent() override;

public:
	UFUNCTION()
	void MoveTetrominoLeft();

	UFUNCTION()
	void MoveTetrominoRight();

	UFUNCTION()
	void RotateTetromino();

	UFUNCTION()
	void StartFastDrop();

	UFUNCTION()
	void StopFastDrop();
};
