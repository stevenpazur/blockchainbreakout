// Fill out your copyright notice in the Description page of Project Settings.

#include "TetrisPlayerController.h"
#include "Engine/Engine.h"
#include "../TetrisGrid.h"

void ATetrisPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    InputComponent->BindAction("MoveLeft", IE_Pressed, this, &ATetrisPlayerController::MoveTetrominoLeft);
    InputComponent->BindAction("MoveRight", IE_Pressed, this, &ATetrisPlayerController::MoveTetrominoRight);
    InputComponent->BindAction("MoveUp", IE_Pressed, this, &ATetrisPlayerController::RotateTetromino);
    InputComponent->BindAction("MoveDown", IE_Pressed, this, &ATetrisPlayerController::StartFastDrop);
    InputComponent->BindAction("MoveDown", IE_Released, this, &ATetrisPlayerController::StopFastDrop);
}

void ATetrisPlayerController::MoveTetrominoLeft()
{
    ATetrisGrid* TetrisGrid = Cast<ATetrisGrid>(GetPawn());
    if (TetrisGrid)
    {
        TetrisGrid->MoveTetrominoLeft();
    }
}

void ATetrisPlayerController::MoveTetrominoRight()
{
    ATetrisGrid* TetrisGrid = Cast<ATetrisGrid>(GetPawn());
    if (TetrisGrid)
    {
        TetrisGrid->MoveTetrominoRight();
    }
}

void ATetrisPlayerController::RotateTetromino()
{
    ATetrisGrid* TetrisGrid = Cast<ATetrisGrid>(GetPawn());
    if (TetrisGrid)
    {
        TetrisGrid->RotateTetromino();
    }
}

void ATetrisPlayerController::StartFastDrop()
{
    ATetrisGrid* TetrisGrid = Cast<ATetrisGrid>(GetPawn());
    if (TetrisGrid)
    {
        TetrisGrid->StartFastDrop();
    }
}

void ATetrisPlayerController::StopFastDrop()
{
    ATetrisGrid* TetrisGrid = Cast<ATetrisGrid>(GetPawn());
    if (TetrisGrid)
    {
        TetrisGrid->StopFastDrop();
    }
}
