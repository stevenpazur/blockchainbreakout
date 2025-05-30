// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Camera/CameraShakeBase.h"
#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "TetrominoShape.h"
#include "Blueprint/UserWidget.h"
#include "Math/Color.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundBase.h"
#include "TetrisBlock.h"
#include "TetrisBlockValue.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "DropState.h"
#include "GlowBlockAnimationData.h"
#include "LevelData.h"

#include "TetrisGrid.generated.h"

UENUM(BlueprintType)
enum class EMarketEvent : uint8
{
    None = 0,
    BullRun = 1,
    CryptoCrash = 2
};

UCLASS()
class BLOCKCHAINBREAKOUTT_API ATetrisGrid : public APawn
{
    GENERATED_BODY()

protected:
    virtual void BeginPlay() override;

public:
    // Sets default values for this actor's properties
    ATetrisGrid();

    // delegate binding

    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpdateUI);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpdateScore);
    DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnUpdateNotches);

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUpdateUI OnUpdateUI;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUpdateScore OnUpdateScore;

    UPROPERTY(BlueprintAssignable, Category = "Events")
    FOnUpdateNotches OnUpdateNotches;

    // end delegate binding

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Shapes")
    TArray<FTetrominoShape> TetrominoShapes;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Point Values")
    TArray<FTetrisBlockValue> PointValues;

    virtual void Tick(float DeltaTime) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetris")
    int32 GridWidth;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetris")
    int32 GridHeight;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetris")
    TSubclassOf<AActor> TetrisBlockBP;

    UFUNCTION(BlueprintCallable, Category = "Tetris")
    void SpawnTetromino();

    UPROPERTY(BlueprintReadWrite, Category = "Score")
    int32 Score;

    UPROPERTY(BlueprintReadWrite, Category = "Combos")
    int32 Combos;

    UPROPERTY(EditAnywhere, Category = "Tetromino Blueprints")
    TArray<TSubclassOf<AActor>> TetrominoBlueprints;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Blueprint Blocks")
    TSubclassOf<AActor> BitcoinBP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Blueprint Blocks")
    TSubclassOf<AActor> EthereumBP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Blueprint Blocks")
    TSubclassOf<AActor> PolkadotBP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Blueprint Blocks")
    TSubclassOf<AActor> SolanaBP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Blueprint Blocks")
    TSubclassOf<AActor> XrpBP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Blueprint Blocks")
    TSubclassOf<AActor> TetherBP;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tetromino Blueprint Blocks")
    TSubclassOf<AActor> UsdcBP;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> BullRunClass;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UUserWidget* BullRunWidget;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSubclassOf<UUserWidget> CryptoCrashClass;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UI")
    UUserWidget* CryptoCrashWidget;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Effects")
    TSoftClassPtr<UCameraShakeBase> CameraShakeClass;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* NudgeCue;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* RotateCue;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* LaserBurstCue;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* ExplosionCue;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* ButtonPushCue;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* ClickCue;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* ShuffleCue2;

    UPROPERTY(BlueprintReadOnly, Category = "Audio")
    USoundBase* StoneCue2;

    UPROPERTY(BlueprintReadOnly, Category = "Gameplay")
    FLevelData CurrentLevel;

    int32 CurrentLevelIndex;

    UPROPERTY(BlueprintReadOnly, Category = "Gameplay")
    TArray<FLevelData> LevelsDataTable;

    void GameOver();

    UUserWidget* ScoreWidget;

    void CheckAndClearFullRows();
    void ClearRow(int32 y);
    void MoveRowsDown(int32 ClearedRow);
    void MoveTetrominoLeft();
    void MoveTetrominoRight();
    std::tuple<bool, TArray<FVector2D>> CanRotateTetromino(AActor* PivotBlock);
    void RotateTetromino();
    void StartFastDrop();
    void StopFastDrop();

    void UpdateMarketValues();
    void UpdateMarketEvents();
    int MarketEventsInterval;

    EMarketEvent CurrentMarketEvent = EMarketEvent::None;

    // handle powerups and chain reactions
    void TriggerExplosion(AActor* HighValueToken1, AActor* HighValueToken2, FLinearColor ExplosionColor1, FLinearColor ExplosionColor2);
    void DestroyBlockAtLocation(FVector Location);
    bool CheckForHorizontalExplosions(AActor* Actor);
    bool CheckForVerticalExplosions(AActor* Actor);
    bool CheckForExplosions(AActor* Actor, FVector Direction);
    bool CheckForSuperBlockFormation(AActor* Actor);
    bool CheckForSuperDuperBlockFormation(AActor* Actor);
    void CheckForCombos();

    TArray<TSubclassOf<AActor>> SuperBlocks;

    UFUNCTION(BlueprintCallable, Category = "HackerMode")
    void FreezeTimeForHackerMode();
    UFUNCTION(BlueprintCallable, Category = "HackerMode")
    void ResumeTime();
    UClass* SecClass;
    FTetrominoShape NextTetrominoShape;
    TArray<AActor*> NextTetrominoBlocks;
    void SpawnDeadlySecRow();
    void SpawnOfficerTetromino();
    bool ShouldSpawnOfficerTetromino;
    bool InOfficerBlocksRound;
    int32 RoundsBeforeSecSpawn = 10;
    int32 RoundsLeftBeforeSecSpawn;

    void PrepareFirstTetromino();
    void PrepareNextTetromino();
    void PrepareOfficerTetromino();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combo Target")
    FString ComboTarget;
    void UpdateComboTarget();

    UFUNCTION(BlueprintCallable, Category = "SEC Raid")
    void ClearOfficerBlocks();

    // lerp niagara

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Niagara")
    UNiagaraComponent* RowNiagaraComponentLeft;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Niagara")
    UNiagaraComponent* RowNiagaraComponentRight;

    void SpawnRowClearEffect(FVector SpawnPoint, FLinearColor Color);

    void HandlePostPlacement(const TArray<FIntPoint>& PlacedBlocks);

private:
    TArray<AActor*> CurrentTetrominoBlocks;
    FVector SpawnLocation;
    FVector NextTetrominoSpawnLocation;
    float BlockFallSpeed;
    bool bIsBlockFalling;
    bool bIsAnimating;
    bool bIsCheckingForCombos;
    void OnAnimationComplete();

    TArray<TArray<AActor*>> Grid; // 2D array to represent the grid
    FTimerHandle UpdateMarketValuesTimer;
    FTimerHandle UpdateMarketEventsTimer;
    FTimerHandle SpawnDeadlySecRowTimer;
    FTimerHandle UpdateComboTargetTimer;
    float BlockFallDelay;

    void MoveTetromino(const FVector2D& Direction);
    void MoveTetrominoDown();
    void SetGrid(int32 x, int32 y, AActor* actor);
    AActor* IsGridOccupied(int32 x, int32 y) const;
    FVector GridToWorld(int32 x, int32 y) const;

    virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

    FTimerHandle TetrominoFallTimerHandle;
    float CurrentFallInterval;
    float DefaultFallInterval = 0.5f;
    float BullRunFallInterval = 0.7f;
    float CryptoCrashFallInterval = 0.2f;
    float FastFallInterval = 0.05f;

    void InitializeTetrominoShapesAndBlueprints();

    FTetrisBlockValue* FindPointValueByName(const FString Input);
    int32 FindPointValueIndexByName(const FString Input);

    UTexture2D* GetTexture(FString source);
    void SpawnNiagaraSystem(FString Source, FVector SpawnLoc, FLinearColor ExplosionColor1, FLinearColor ExplosionColor2);
    void UpdateGridAtLocation(FVector Location);
    void MoveBlocksDown();
    void ClearThreeRows(int32 RowIndex);

    bool IsFastDropping;

    // Lerp Niagara

    FTimerHandle LerpTimerHandle;
    FVector StartLocationLeft;
    FVector EndLocationLeft;
    FVector StartLocationRight;
    FVector EndLocationRight;
    float Duration;
    float ElapsedTimeLeft;
    float ElapsedTimeRight;
    void UpdateNiagaraLocation();

    // void MoveBlockDown(AActor* Block, int32 DropDistance);
    void MoveBlocksToDropDown();
    TMap<AActor*, int32> BlocksToDrop;
    FTimerHandle MoveBlocksTimerHandle;
    float BlockMoveInterval = 0.2f;

    void MoveBlocksDownIncrementally();
    TArray<AActor*> BlocksToMove;
    TArray<int32> RowsToMove;
    bool CanMoveDown(int32 x, int32 y);

    int32 DropX;               // Stores the x-coordinate of the block
    int32 DropY1;              // Tracks the current y-coordinate of the block
    int32 DropLoopIndex;       // Tracks the loop iteration
    int32 DropDistance;        // Stores how far the block should drop
    FTimerHandle DropTimerHandle; // Timer handle for the drop operation
    void HandleDrop();         // Function to handle the drop logic
    void HandleMultipleDrops();
    TArray<FDropState> DropsArray; // Tracks all active drops
    bool bAnyDropInProgress;
    FTimerHandle CheckIfReadyToSpawnTetromino;
    void CheckIfReadyForNewTetromino();
    void CheckForBlocksToDrop();

    // glow super blocks
    void GlowBlocks();
    void GlowSuperDuperBlocks();
    void UpdateGlowMaterial();
    void UpdateSuperDuperGlowMaterial();
    void MakeSuperBlock();
    void MakeSuperDuperBlock();
    FTimerHandle GlowTimerHandle;
    TArray<UMaterialInstanceDynamic*> GlowMaterials;
    float GlowFactorStart = 0.0;
    float GlowFactorEnd = 1.0f;
    float GlowPowerStart = 1.0f;
    float GlowPowerEnd = 5.0f;
    float AnimationDuration = 2.0f; // Duration in seconds
    FGlowBlockAnimationData TargetActors;
    TArray<FVector> InitialScales;
    TArray<FVector> InitialLocations;
    FVector FinalScale;
    TArray<FVector> FinalLocations;

    // scale and move actors for super block formation
    void ScaleAndMoveActors(AActor* Actor1, AActor* Actor2, AActor* Actor3);

    // debugging
    void PrintScreen(FString message, float showDuration = 5.f);

    void IncrementScore(int32 TargetScore);

    UMaterialInstanceDynamic* GlowMaterialForBoard;
    UMaterialInstanceDynamic* BackgroundMaterialForBoard;
    UMaterialInstanceDynamic* WinMaterial;
    UClass* TetrisBoard;
    AActor* TetrisBoardInstance;
    void SetVictoryBoardMaterial(float ColorPickerValue, FVector BackgroundColor, float VictorySwitch);
    FTimerHandle VictoryTimerHandle;
    FTimerHandle BlinkBoardTimerHandle;
    void BlinkBoardColors();
    void Blink();
    float BlinkDuration = 3.0f;
    float ElapsedBlinking = 0.0f;
    float CurrentColorPickerValue = 0.0f;

    void ClearBoard();
    bool bIsClearing = false;
    void NextLevel();

    UClass* BombBlockClass;
    void SpawnBombExplosion(AActor* Actor);
};