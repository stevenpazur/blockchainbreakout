#include "TetrisGrid.h"

#include "Blueprint/UserWidget.h"
#include "Camera/CameraShakeBase.h"
#include "Components/AudioComponent.h"
#include "Components/InputComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Math/Color.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "TetrisBlock.h"
#include "TetrisBlockValue.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"

ATetrisGrid::ATetrisGrid()
{
    CurrentFallInterval = DefaultFallInterval;
    PrimaryActorTick.bCanEverTick = true;
    GridWidth = 15;
    GridHeight = 20;
    BlockFallSpeed = 100.0f;
    bIsBlockFalling = false;
    BlockFallDelay = 0.1f;

    Grid.SetNum(GridWidth);
    for (int32 i = 0; i < GridWidth; ++i)
    {
        Grid[i].SetNum(GridHeight);
        for (int32 j = 0; j < GridHeight; ++j)
        {
            Grid[i][j] = nullptr;
        }
    }

    SpawnLocation = FVector(-200.0f, 0.0f, GridHeight * 100.0f);
    NextTetrominoSpawnLocation = FVector(7890.0f, 3610.0f, 1240.0f);

    MarketEventsInterval = FMath::RandRange(30.0f, 45.0f);

    static ConstructorHelpers::FObjectFinder<USoundBase> NudgeBase(TEXT("/Game/Audio/zip_Cue"));
    if (NudgeBase.Succeeded())
    {
        NudgeCue = NudgeBase.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> RotateBase(TEXT("/Game/Audio/rotate_Cue"));
    if (RotateBase.Succeeded())
    {
        RotateCue = RotateBase.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> LaserBurstBase(TEXT("/Game/Audio/laser-burst-cue"));
    if (LaserBurstBase.Succeeded())
    {
        LaserBurstCue = LaserBurstBase.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> ExplosionBase(TEXT("/Game/Audio/splode_Cue"));
    if (ExplosionBase.Succeeded())
    {
        ExplosionCue = ExplosionBase.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> ButtonPushBase(TEXT("/Game/Audio/button_push_Cue"));
    if (ButtonPushBase.Succeeded())
    {
        ButtonPushCue = ButtonPushBase.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> ClickBase(TEXT("/Game/Audio/click_Cue"));
    if (ClickBase.Succeeded())
    {
        ClickCue = ClickBase.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> ShuffleBase2(TEXT("/Game/Audio/shuffle_Cue"));
    if (ShuffleBase2.Succeeded())
    {
        ShuffleCue2 = ShuffleBase2.Object;
    }

    static ConstructorHelpers::FObjectFinder<USoundBase> StoneBase2(TEXT("/Game/Audio/stone_Cue_2"));
    if (StoneBase2.Succeeded())
    {
        StoneCue2 = StoneBase2.Object;
    }

    RoundsLeftBeforeSecSpawn = RoundsBeforeSecSpawn;
}

void ATetrisGrid::BeginPlay()
{
    try {
        Super::BeginPlay();

        FString Path = TEXT("/Game/Blueprints/BP_TetrisBlock.BP_TetrisBlock_C");
        TetrisBlockBP = StaticLoadClass(UObject::StaticClass(), nullptr, *Path);
        if (!TetrisBlockBP)
        {
            UE_LOG(LogTemp, Warning, TEXT("Failed to set TetrisBlockBP class in BeginPlay"));
        }

        APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
        if (PlayerController)
        {
            PlayerController->bShowMouseCursor = true;
            PlayerController->SetInputMode(FInputModeGameOnly());
            PlayerController->Possess(this);
            EnableInput(PlayerController);
            UE_LOG(LogTemp, Warning, TEXT("Enabling player input"));
        }
        else{
            UE_LOG(LogTemp, Warning, TEXT("Failed to enable player input"));
        }

        TetrominoShapes.Add({ { FVector2D(0, -1), FVector2D(0, 0), FVector2D(0, 1), FVector2D(0, 2) } }); // I Shape
        TetrominoShapes.Add({ { FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(1, 1) } }); // O Shape
        TetrominoShapes.Add({ { FVector2D(0, 0), FVector2D(-1, 0), FVector2D(1, 0), FVector2D(0, 1) } }); // T Shape
        TetrominoShapes.Add({ { FVector2D(0, 0), FVector2D(1, 0), FVector2D(0, 1), FVector2D(-1, 1) } }); // S Shape
        TetrominoShapes.Add({ { FVector2D(0, 0), FVector2D(-1, 0), FVector2D(0, 1), FVector2D(1, 1) } }); // Z Shape
        TetrominoShapes.Add({ { FVector2D(0, 0), FVector2D(-1, 0), FVector2D(-1, 1), FVector2D(1, 0) } }); // J Shape
        TetrominoShapes.Add({ { FVector2D(0, 0), FVector2D(1, 0), FVector2D(1, 1), FVector2D(-1, 0) } }); // L Shape

        UTexture2D* BitcoinTickerTexture = GetTexture("/Game/Images/stock_market_textures/bitcoin_circ");
        UTexture2D* EthereumTickerTexture = GetTexture("/Game/Images/stock_market_textures/ethereum_circ");
        UTexture2D* XRPTickerTexture = GetTexture("/Game/Images/stock_market_textures/xrp_circ");
        UTexture2D* PolkadotTickerTexture = GetTexture("/Game/Images/stock_market_textures/polkadot_circ");
        UTexture2D* SolanaTickerTexture = GetTexture("/Game/Images/stock_market_textures/solana_circ");
        UTexture2D* TetherTickerTexture = GetTexture("/Game/Images/stock_market_textures/tether_circ");
        UTexture2D* USDCTickerTexture = GetTexture("/Game/Images/stock_market_textures/usdc_circ");

        PointValues.Add({ "bitcoin", "Bitcoin", "BTC", "$20,000", true, BitcoinTickerTexture, FLinearColor(20.0f, 11.0f, 3.0f) }); // bitcoin
        PointValues.Add({ "ethereum", "Ethereum", "ETH", "$12,000", false, EthereumTickerTexture, FLinearColor(15.0f, 15.0f, 15.0f) }); // ethereum
        PointValues.Add({ "xrp", "XRP", "XRP", "$15,000", true, XRPTickerTexture, FLinearColor::White }); // xrp
        PointValues.Add({ "polkadot", "Polkadot", "DOT", "$10", false, PolkadotTickerTexture, FLinearColor(10.0f, 5.0f, 5.0f) }); // polkadot
        PointValues.Add({ "solana", "Solana", "SOL", "$505", true, SolanaTickerTexture, FLinearColor(2.0f, 17.0f, 14.0f) }); // solana
        PointValues.Add({ "tether", "Tether", "USDT", "$100", true, TetherTickerTexture, FLinearColor(0.0f, 15.0f, 15.0f) }); // tether
        PointValues.Add({ "usdc", "USDC", "USDC", "$110", true, USDCTickerTexture, FLinearColor(10.0f, 5.0f, 15.0f) }); // usdc

        UpdateComboTarget();

        FString bitcoinPath = TEXT("/Game/Blueprints/BP_bitcoin.BP_bitcoin_C");
        UClass* BitcoinBPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *bitcoinPath));
        if (BitcoinBPClass)
        {
            TetrominoBlueprints.Add(BitcoinBPClass);
        }

        FString ethereumPath = TEXT("/Game/Blueprints/BP_ethereum.BP_ethereum_C");
        UClass* EthereumBPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *ethereumPath));
        if (EthereumBPClass)
        {
            TetrominoBlueprints.Add(EthereumBPClass);
        }

        FString xrpPath = TEXT("/Game/Blueprints/BP_xrp.BP_xrp_C");
        UClass* XrpBPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *xrpPath));
        if (XrpBPClass)
        {
            TetrominoBlueprints.Add(XrpBPClass);
        }

        FString polkadotPath = TEXT("/Game/Blueprints/BP_polkadot.BP_polkadot_C");
        UClass* PolkadotBPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *polkadotPath));
        if (PolkadotBPClass)
        {
            TetrominoBlueprints.Add(PolkadotBPClass);
        }

        FString solanaPath = TEXT("/Game/Blueprints/BP_solana.BP_solana_C");
        UClass* SolanaBPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *solanaPath));
        if (SolanaBPClass)
        {
            TetrominoBlueprints.Add(SolanaBPClass);
        }

        FString tetherPath = TEXT("/Game/Blueprints/BP_tether.BP_tether_C");
        UClass* TetherBPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *tetherPath));
        if (TetherBPClass)
        {
            TetrominoBlueprints.Add(TetherBPClass);
        }

        FString usdcPath = TEXT("/Game/Blueprints/BP_usdc.BP_usdc_C");
        UClass* UsdcBPClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *usdcPath));
        if (UsdcBPClass)
        {
            TetrominoBlueprints.Add(UsdcBPClass);
        }

        FString secPath = TEXT("/Game/Blueprints/BP_SEC.BP_SEC_C");
        SecClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *secPath));

        // add superblocks

        FString superBitcoinPath = TEXT("/Game/Blueprints/BP_superbitcoin.BP_superbitcoin_C");
        UClass* SuperBitcoinClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superBitcoinPath));
        if (SuperBitcoinClass)
        {
            SuperBlocks.Add(SuperBitcoinClass);
        }

        FString superEthereumPath = TEXT("/Game/Blueprints/BP_superethereum.BP_superethereum_C");
        UClass* SuperEthereumClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superEthereumPath));
        if (SuperEthereumClass)
        {
            SuperBlocks.Add(SuperEthereumClass);
        }

        FString superXrpPath = TEXT("/Game/Blueprints/BP_superxrp.BP_superxrp_C");
        UClass* SuperXrpClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superXrpPath));
        if (SuperXrpClass)
        {
            SuperBlocks.Add(SuperXrpClass);
        }

        FString superPolkadotPath = TEXT("/Game/Blueprints/BP_superpolkadot.BP_superpolkadot_C");
        UClass* SuperPolkadotClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superPolkadotPath));
        if (SuperPolkadotClass)
        {
            SuperBlocks.Add(SuperPolkadotClass);
        }

        FString superSolanaPath = TEXT("/Game/Blueprints/BP_supersolana.BP_supersolana_C");
        UClass* SuperSolanaClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superSolanaPath));
        if (SuperSolanaClass)
        {
            SuperBlocks.Add(SuperSolanaClass);
        }

        FString superTetherPath = TEXT("/Game/Blueprints/BP_supertether.BP_supertether_C");
        UClass* SuperTetherClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superTetherPath));
        if (SuperTetherClass)
        {
            SuperBlocks.Add(SuperTetherClass);
        }

        FString superUsdcPath = TEXT("/Game/Blueprints/BP_superusdc.BP_superusdc_C");
        UClass* SuperUsdcClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superUsdcPath));
        if (SuperUsdcClass)
        {
            SuperBlocks.Add(SuperUsdcClass);
        }

        FString bullRunPath = TEXT("/Game/Blueprints/W_bullrun.W_bullrun_C");
        BullRunClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *bullRunPath));

        FString cryptoCrashPath = TEXT("/Game/Blueprints/W_cryptocrash.W_cryptocrash_C");
        CryptoCrashClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *cryptoCrashPath));

        FString cameraShakePath = TEXT("/Game/Blueprints/BP_CameraShake.BP_CameraShake_C");
        CameraShakeClass = TSoftClassPtr<UCameraShakeBase>(FSoftObjectPath(cameraShakePath));

        GetWorldTimerManager().SetTimer(TetrominoFallTimerHandle, this, &ATetrisGrid::MoveTetrominoDown, CurrentFallInterval, true);

        GetWorldTimerManager().SetTimer(UpdateMarketValuesTimer, this, &ATetrisGrid::UpdateMarketValues, 1.0f, true, 0.0f);

        GetWorldTimerManager().SetTimer(UpdateMarketEventsTimer, this, &ATetrisGrid::UpdateMarketEvents, MarketEventsInterval, true, -1.0f);

        GetWorldTimerManager().SetTimer(UpdateComboTargetTimer, this, &ATetrisGrid::UpdateComboTarget, FMath::RandRange(30.0f, 45.0f), true, -1.0f);

        GetWorldTimerManager().SetTimer(MoveBlocksTimerHandle, this, &ATetrisGrid::MoveBlocksToDropDown, 0.5f, true, 0.0f);


        int32 NextTetrominoIndex = FMath::RandRange(0, TetrominoShapes.Num() - 1);
        NextTetrominoShape = TetrominoShapes[NextTetrominoIndex];

        PrepareNextTetromino();

        SpawnTetromino();
        RoundsLeftBeforeSecSpawn--;
    }
    catch (const std::exception& e) {
        UE_LOG(LogTemp, Error, TEXT("An exception was thrown in BeginPlay.  e: %s"), ANSI_TO_TCHAR(e.what()));
    }
}

void ATetrisGrid::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

UTexture2D* ATetrisGrid::GetTexture(FString source)
{
    UTexture2D* texture = nullptr;
    
    FStringAssetReference TextureAssetRef(source);
    texture = Cast<UTexture2D>(TextureAssetRef.TryLoad());

    if (texture == nullptr)
    {
        UE_LOG(LogTemp, Log, TEXT("Failed to load image texture %s"), *source);
    }

    return texture;
}

void ATetrisGrid::PrepareNextTetromino()
{
    if (UWorld* World = GetWorld())
    {
        for (AActor* NextBlockActor : NextTetrominoBlocks)
        {
            NextBlockActor->Destroy();
        }

        NextTetrominoBlocks.Empty();

        const FTetrominoShape& SelectedShape = NextTetrominoShape;
        TSubclassOf<AActor> TetrominoBlueprint;
        
        int32 ShapeIndex = FMath::RandRange(0, TetrominoShapes.Num() - 1);
        NextTetrominoShape = TetrominoShapes[ShapeIndex];

        int32 CryptoBlockIndex;

        for (const FVector2D& Offset : NextTetrominoShape.BlockOffsets)
        {
            CryptoBlockIndex = FMath::RandRange(0, TetrominoBlueprints.Num() - 1);
            TetrominoBlueprint = TetrominoBlueprints[CryptoBlockIndex];

            FVector BlockLocation = NextTetrominoSpawnLocation + FVector(Offset.X * 100.0f, 0.0f, Offset.Y * 100.0f);
            AActor* Block = World->SpawnActor<AActor>(TetrominoBlueprint, BlockLocation, FRotator::ZeroRotator);

            if (Block)
            {
                NextTetrominoBlocks.Add(Block);
            }
        }
        UE_LOG(LogTemp, Warning, TEXT("Preparing regular tetromino"));
    }
}

void ATetrisGrid::PrepareOfficerTetromino()
{
    if (UWorld* World = GetWorld())
    {
        if (SecClass)
        {
            for (AActor* NextBlockActor : NextTetrominoBlocks)
            {
                NextBlockActor->Destroy();
            }

            NextTetrominoBlocks.Empty();

            TArray<FVector2D> BlockOffsets;

            for (int32 x = -8; x < GridWidth - 8; ++x)
            {
                BlockOffsets.Add(FVector2D(x, 0));
            }

            NextTetrominoShape = { BlockOffsets };

            TSubclassOf<AActor> TetrominoBlueprint = SecClass;
            
            for (const FVector2D& Offset : BlockOffsets)
            {
                FVector BlockLocation = NextTetrominoSpawnLocation + FVector(Offset.X * 100.0f, 0.0f, Offset.Y * 100.0f);
                AActor* Block = World->SpawnActor<AActor>(TetrominoBlueprint, BlockLocation, FRotator::ZeroRotator);

                if (Block)
                {
                    NextTetrominoBlocks.Add(Block);
                }
            }

            UE_LOG(LogTemp, Warning, TEXT("Preparing officer tetromino"));
        }
    }
}

void ATetrisGrid::SpawnTetromino()
{
    if (UWorld* World = GetWorld())
    {
        for (int32 i = 0; i < NextTetrominoShape.BlockOffsets.Num(); ++i)
        {
            FVector2D Offset = NextTetrominoShape.BlockOffsets[i];
            TSubclassOf<AActor> TetrominoBlueprint = NextTetrominoBlocks[i]->GetClass();
            FVector BlockLocation = SpawnLocation + FVector(Offset.X * 100.0f, 0.0f, Offset.Y * 100.0f);
            AActor* NextBlock = World->SpawnActor<AActor>(TetrominoBlueprint, BlockLocation, FRotator::ZeroRotator);

            if (NextBlock)
            {
                NextBlock->Tags.Add(FName("TetrisBlock"));
                CurrentTetrominoBlocks.Add(NextBlock);
            }
        }

        if (RoundsLeftBeforeSecSpawn != 1)
        {
            for (AActor* NextBlock : NextTetrominoBlocks)
            {
                NextBlock->Destroy();
            }
            NextTetrominoBlocks.Empty();
            PrepareNextTetromino();
        }
        else
        {
            for (AActor* NextBlock : NextTetrominoBlocks)
            {
                NextBlock->Destroy();
            }
            NextTetrominoBlocks.Empty();
            PrepareOfficerTetromino();
        }
    }
}

void ATetrisGrid::MoveTetromino(const FVector2D& Direction)
{
    try {
        bool bCanMove = true;

        for (AActor* Block : CurrentTetrominoBlocks)
        {
            FVector CurrentLocation = Block->GetActorLocation();
            FVector2D GridPosition = FVector2D((CurrentLocation.X + 1000.0f) / 100.0f, CurrentLocation.Z / 100.0f);
            FVector2D NewGridPosition = GridPosition + Direction;

            if (NewGridPosition.X < 0 || NewGridPosition.X >= GridWidth || NewGridPosition.Y < 0 || IsGridOccupied(NewGridPosition.X, NewGridPosition.Y) != nullptr)
            {
                bCanMove = false;
                break;
            }
        }

        if (bCanMove)
        {
            if (ShuffleCue2)
            {
                UGameplayStatics::PlaySoundAtLocation(this, ShuffleCue2, GetActorLocation());
            }

            for (AActor* Block : CurrentTetrominoBlocks)
            {
                FVector CurrentLocation = Block->GetActorLocation();
                FVector NewLocation = CurrentLocation + FVector(Direction.X * 100.0f, 0.0f, Direction.Y * 100.0f);
                Block->SetActorLocation(NewLocation);
            }
        }
    }
    catch (const std::exception& e) {
        UE_LOG(LogTemp, Error, TEXT("An exception was thrown.  e: %s"), ANSI_TO_TCHAR(e.what()));
    }
}

void ATetrisGrid::MoveTetrominoLeft()
{
    MoveTetromino(FVector2D(-1, 0));
}

void ATetrisGrid::MoveTetrominoRight()
{
    MoveTetromino(FVector2D(1, 0));
}

void ATetrisGrid::MoveTetrominoDown()
{
    bool bCanMove = true;

    // Check if movement is possible
    for (AActor* Block : CurrentTetrominoBlocks)
    {
        FVector CurrentLocation = Block->GetActorLocation();
        FVector2D GridPosition = FVector2D((CurrentLocation.X + 1000.0f) / 100.0f, CurrentLocation.Z / 100.0f);
        FVector2D NewGridPosition = GridPosition + FVector2D(0, -1);

        if (NewGridPosition.Y < 0 || IsGridOccupied(NewGridPosition.X, NewGridPosition.Y) != nullptr)
        {
            bCanMove = false;
            break;
        }
    }

    // Move Tetromino if possible
    if (bCanMove)
    {
        for (AActor* Block : CurrentTetrominoBlocks)
        {
            FVector CurrentLocation = Block->GetActorLocation();
            FVector NewLocation = CurrentLocation + FVector(0.0f, 0.0f, -100.0f);
            Block->SetActorLocation(NewLocation);
        }
    }
    else
    {
        // Set the Tetromino blocks as occupied in the grid
        for (AActor* Block : CurrentTetrominoBlocks)
        {
            FVector CurrentLocation = Block->GetActorLocation();
            int32 GridX = FMath::RoundToInt((CurrentLocation.X + 1000.0f) / 100.0f);
            int32 GridY = FMath::RoundToInt(CurrentLocation.Z / 100.0f);

            // Trigger game over if a block is placed at the top of the grid
            if (GridY >= GridHeight - 1)
            {
                GameOver();
                return;
            }

            SetGrid(GridX, GridY, Block);
        }

        CurrentTetrominoBlocks.Empty();

        for (int x = 0; x < GridWidth - 1; x++)
        {
            for (int y = 0; y < GridHeight - 1; y++)
            {
                AActor* GridBlock = Grid[x][y];
                if (GridBlock != nullptr)
                {
                    if (IsValid(GridBlock) && GridBlock->Tags.Contains("TetrisBlock"))
                    {
                        FTetrisBlockValue* FoundValue = FindPointValueByName(*GridBlock->GetName());

                        if (GridBlock->Tags.Contains(FName("CanClearThreeRows")))
                        {
                            if (FoundValue->BlockName + "_circ" == ComboTarget)
                            {
                                Combos = FMath::Clamp(Combos + 1, 0, 5);
                            }
                            ClearThreeRows(y);
                            SpawnNiagaraSystem(TEXT("/Game/VFX/NS_Explosion"), GridBlock->GetActorLocation(), FoundValue->Color, FoundValue->Color);
                            SpawnRowClearEffect(GridBlock->GetActorLocation(), FoundValue->Color);
                        }
                    }
                }
            }
        }

        CheckAndClearFullRows();

        CheckForCombos();

        RoundsLeftBeforeSecSpawn--;

        if (RoundsLeftBeforeSecSpawn <= 0)
        {
            ShouldSpawnOfficerTetromino = true;
            RoundsLeftBeforeSecSpawn = RoundsBeforeSecSpawn;
        }

        if (InOfficerBlocksRound)
        {
            ShouldSpawnOfficerTetromino = false;
            GetWorldTimerManager().ClearTimer(SpawnDeadlySecRowTimer);
            GetWorldTimerManager().SetTimer(SpawnDeadlySecRowTimer, this, &ATetrisGrid::SpawnDeadlySecRow, FMath::RandRange(5.0f, 10.0f), true, -1.0f);
            InOfficerBlocksRound = false;
        }

        if (ShouldSpawnOfficerTetromino)
        {
            SpawnOfficerTetromino();
            InOfficerBlocksRound = true;
        }
        else
        {
            SpawnTetromino();
        }
    }
}

void ATetrisGrid::SetGrid(int32 x, int32 y, AActor* actor = nullptr)
{
    if (x >= 0 && x < GridWidth && y >= 0 && y < GridHeight)
    {
        Grid[x][y] = actor;
    }
}

AActor* ATetrisGrid::IsGridOccupied(int32 x, int32 y) const
{
    if (x >= 0 && x < GridWidth && y >= 0 && y < GridHeight)
    {
        return Grid[x][y];
    }
    return nullptr;
}

FVector ATetrisGrid::GridToWorld(int32 x, int32 y) const
{
    return FVector(x * 100.0f, 0.0f, y * 100.0f);
}

void ATetrisGrid::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    PlayerInputComponent->BindAction("MoveLeft", IE_Pressed, this, &ATetrisGrid::MoveTetrominoLeft);
    PlayerInputComponent->BindAction("MoveRight", IE_Pressed, this, &ATetrisGrid::MoveTetrominoRight);
    PlayerInputComponent->BindAction("MoveUp", IE_Pressed, this, &ATetrisGrid::RotateTetromino);
    PlayerInputComponent->BindAction("MoveDown", IE_Pressed, this, &ATetrisGrid::StartFastDrop);
    PlayerInputComponent->BindAction("MoveDown", IE_Released, this, &ATetrisGrid::StopFastDrop);
}

void ATetrisGrid::CheckAndClearFullRows()
{
    for (int32 y = 0; y < GridHeight; ++y)
    {
        bool bIsRowFull = true;
        for (int32 x = 0; x < GridWidth; ++x)
        {
            AActor* GridActor = IsGridOccupied(x, y);
            if (!GridActor)
            {
                bIsRowFull = false;
                break;
            }

            if (!GridActor->Tags.Contains(FName("TetrisBlock")))
            {
                bIsRowFull = false;
            }
        }

        if (bIsRowFull)
        {
            int32 RowScore = 0;
            for (int32 x = 0; x < GridWidth; x++)
            {
                FVector BlockLocation = FVector((x * 100.0f) - 1000.0f, 0.0f, y * 100.0f);
                TArray<AActor*> FoundActors;
                UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);

                for (AActor* Actor : FoundActors)
                {
                    if (Actor->GetActorLocation() == BlockLocation)
                    {
                        FTetrisBlockValue* FoundValue = FindPointValueByName(*Actor->GetName());

                        if (FoundValue)
                        {
                            int32 IntValue = FCString::Atoi(*FoundValue->ScoreValue.Replace(TEXT("$"), TEXT("")).Replace(TEXT(","), TEXT("")).Replace(TEXT("."), TEXT("")));

                            if (CurrentMarketEvent == EMarketEvent::BullRun)
                            {
                                IntValue *= 2;
                            }
                            else if (CurrentMarketEvent == EMarketEvent::CryptoCrash)
                            {
                                IntValue /= 2;
                            }

                            RowScore += IntValue;
                        }
                        else
                        {
                            UE_LOG(LogTemp, Error, TEXT("Could not find point value for actor %s"), *Actor->GetName());
                        }
                    }
                }
            }

            ClearRow(y);
            MoveRowsDown(y);

            Score += RowScore;
            y--;
        }
    }
}

void ATetrisGrid::ClearRow(int32 y)
{
    for (int32 x = 0; x < GridWidth; ++x)
    {
        AActor* GridActor = Grid[x][y];
        if (GridActor)
        {
            GridActor->Destroy();
        }

        SetGrid(x, y, nullptr);
    }
}

void ATetrisGrid::MoveRowsDown(int32 ClearedRow)
{
    for (int32 y = ClearedRow; y < GridHeight - 1; ++y)
    {
        for (int32 x = 0; x < GridWidth; ++x)
        {
            AActor* bIsOccupied = IsGridOccupied(x, y + 1);

            SetGrid(x, y, bIsOccupied);

            if (Grid[x][y])
            {
                FVector NewLocation = GridToWorld(x - 10, y);
                FVector OldLocation = GridToWorld(x - 10, y + 1);
                FHitResult HitResult;
                if (GetWorld()->LineTraceSingleByChannel(HitResult, OldLocation, OldLocation + (FVector::UpVector * 5), ECC_Visibility))
                {
                    if (AActor* BlockActor = HitResult.GetActor())
                    {
                        BlockActor->SetActorLocation(NewLocation);
                    }
                }
            }
            else
            {
                SetGrid(x, y, nullptr);
            }
        }
    }

    // Clear the top row
    for (int32 x = 0; x < GridWidth; ++x)
    {
        SetGrid(x, GridHeight - 1, nullptr);
    }
}

std::tuple<bool, TArray<FVector2D>> ATetrisGrid::CanRotateTetromino(AActor* PivotBlock)
{
    if (InOfficerBlocksRound)
    {
        return std::make_tuple(false, TArray<FVector2D>());
    }

    FVector PivotWorldLocation = PivotBlock->GetActorLocation();

    // Convert pivot world coordinates to grid coordinates
    int32 PivotGridX = FMath::RoundToInt((PivotWorldLocation.X + 1000.0f) / 100.0f);
    int32 PivotGridY = FMath::RoundToInt(PivotWorldLocation.Z / 100.0f);

    bool bCanRotate = true;
    TArray<FVector2D> NewGridPositions;

    for (AActor* Block : CurrentTetrominoBlocks)
    {
        FVector WorldLocation = Block->GetActorLocation();

        // Convert world coordinates to grid coordinates
        int32 GridX = FMath::RoundToInt((WorldLocation.X + 1000.0f) / 100.0f);
        int32 GridY = FMath::RoundToInt(WorldLocation.Z / 100.0f);

        // Calculate relative position to the pivot in grid space
        FVector2D RelativePosition = FVector2D(GridX - PivotGridX, GridY - PivotGridY);

        // Rotate 90 degrees clockwise in grid space
        FVector2D RotatedPosition(-RelativePosition.Y, RelativePosition.X);

        FVector2D NewGridPosition = FVector2D(PivotGridX, PivotGridY) + RotatedPosition;

        // Check if the new grid position is valid
        int32 NewGridX = FMath::RoundToInt(NewGridPosition.X);
        int32 NewGridY = FMath::RoundToInt(NewGridPosition.Y);

        if (NewGridX < 0 || NewGridX >= GridWidth || NewGridY < 0 || NewGridY >= GridHeight || IsGridOccupied(NewGridX, NewGridY) != nullptr)
        {
            bCanRotate = false;
            break;
        }

        NewGridPositions.Add(NewGridPosition);
    }

    return std::make_tuple(bCanRotate, NewGridPositions);
}

void ATetrisGrid::RotateTetromino()
{
    if (CurrentTetrominoBlocks.Num() > 0)
    {
        bool bCanRotate = true;
        TArray<FVector2D> NewGridPositions;

        for (int32 blockIndex = 0; blockIndex < CurrentTetrominoBlocks.Num(); blockIndex++) {
            AActor* PivotBlock = CurrentTetrominoBlocks[0];
            std::tuple<bool, TArray<FVector2D>> canRotateTetrominoResult = CanRotateTetromino(PivotBlock);
            bCanRotate = std::get<0>(canRotateTetrominoResult);
            NewGridPositions = std::get<1>(canRotateTetrominoResult);

            if (bCanRotate) {
                break;
            }
            else {
                NewGridPositions.Empty();
            }
        }

        // Apply the new positions if the rotation is valid
        if (bCanRotate)
        {
            if (ShuffleCue2)
            {
                UGameplayStatics::PlaySoundAtLocation(this, ShuffleCue2, GetActorLocation());
            }

            for (int32 i = 0; i < CurrentTetrominoBlocks.Num(); ++i)
            {
                FVector2D GridPos = NewGridPositions[i];

                // Convert grid coordinates back to world coordinates
                FVector NewWorldLocation((GridPos.X * 100.0f) - 1000.0f, 0.0f, GridPos.Y * 100.0f);
                CurrentTetrominoBlocks[i]->SetActorLocation(NewWorldLocation);
            }
        }
    }
}

void ATetrisGrid::StartFastDrop()
{
    GetWorldTimerManager().ClearTimer(TetrominoFallTimerHandle);
    IsFastDropping = true;
    GetWorld()->GetTimerManager().SetTimer(TetrominoFallTimerHandle, this, &ATetrisGrid::MoveTetrominoDown, FastFallInterval, true);
}

void ATetrisGrid::StopFastDrop()
{
    GetWorldTimerManager().ClearTimer(TetrominoFallTimerHandle);
    IsFastDropping = false;
    GetWorld()->GetTimerManager().SetTimer(TetrominoFallTimerHandle, this, &ATetrisGrid::MoveTetrominoDown, CurrentFallInterval, true);
}

void ATetrisGrid::GameOver()
{
    // Clear the Tetromino fall timer
    GetWorld()->GetTimerManager().ClearTimer(TetrominoFallTimerHandle);

    // Ensure the player controller is valid before attempting to load the level
    APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
    if (PlayerController)
    {
        // Set the input mode to UI only and show the mouse cursor
        PlayerController->bShowMouseCursor = true;
        FInputModeUIOnly InputMode;
        PlayerController->SetInputMode(InputMode);

        // Load the main menu level
        FString LevelName = TEXT("/Game/Maps/MainMenu");
        UGameplayStatics::OpenLevel(GetWorld(), FName(*LevelName));
    }
}

FTetrisBlockValue* ATetrisGrid::FindPointValueByName(const FString Input)
{
    for (FTetrisBlockValue& PointValue : PointValues)
    {
        if (Input.Contains(PointValue.BlockName))
        {
            return &PointValue;
        }
    }
    return nullptr; // Return nullptr if not found
}

int32 ATetrisGrid::FindPointValueIndexByName(const FString Input)
{
    for (int32 Index = 0; Index < PointValues.Num(); Index++)
    {
        if (Input.Contains(PointValues[Index].BlockName))
        {
            return Index;
        }
    }
    return INDEX_NONE; // Return -1 if not found
}

void ATetrisGrid::UpdateMarketValues()
{
    // Map to keep track of block counts
    TMap<FString, int32> BlockCounts;

    // Initialize counts to 0
    for (const FTetrisBlockValue& PointValue : PointValues)
    {
        BlockCounts.Add(PointValue.BlockName, 0);
    }

    // Count the blocks on the board
    for (int32 x = 0; x < GridWidth; x++)
    {
        for (int32 y = 0; y < GridHeight; y++)
        {
            FVector BlockLocation = FVector((x * 100.0f) - 1000.0f, 0.0f, y * 100.0f);
            TArray<AActor*> FoundActors;
            UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);

            for (AActor* Actor : FoundActors)
            {
                if (Actor->GetActorLocation() == BlockLocation)
                {
                    FString ActorName = Actor->GetName();
                    for (FTetrisBlockValue& PointValue : PointValues)
                    {
                        if (ActorName.Contains(PointValue.BlockName))
                        {
                            BlockCounts[PointValue.BlockName]++;
                            break;
                        }
                    }
                }
            }
        }
    }

    // Update scores based on counts
    for (FTetrisBlockValue& PointValue : PointValues)
    {
        float pointMultiplier = FMath::RandRange(0.8f, 1.2f);

        if (PointValue.ForceVolatilityToGoDown)
        {
            pointMultiplier = 0.99f;
            PointValue.ForceVolatilityToGoDown = false;
        }
        else if (PointValue.ForceVolatilityToGoUp)
        {
            pointMultiplier = 1.2f;
            PointValue.ForceVolatilityToGoUp = false;
        }

        int32 IntValue = FCString::Atoi(*PointValue.ScoreValue.Replace(TEXT("$"), TEXT("")).Replace(TEXT(","), TEXT("")).Replace(TEXT("."), TEXT("")));
        int32 NewScore = IntValue * pointMultiplier;
        FString DollarAmount = FString::Printf(TEXT("$%d"), NewScore);

        PointValue.ScoreValue = DollarAmount;

        if (pointMultiplier <= 1.0f)
        {
            PointValue.VolatilityGoingUp = false;
        }
        else
        {
            PointValue.VolatilityGoingUp = true;
        }
    }

    OnUpdateUI.Broadcast();
}

void ATetrisGrid::UpdateMarketEvents() {
    int RandomMarketEvent = FMath::RandRange(0, 1);

    switch (RandomMarketEvent)
    {
    case 0:
        if (BullRunClass)
        {
            BullRunWidget = CreateWidget<UUserWidget>(GetWorld(), BullRunClass);
            if (BullRunWidget)
            {
                BullRunWidget->AddToViewport();
            }
        }

        CurrentMarketEvent = EMarketEvent::BullRun;
        // clear and reset the fall interval
        CurrentFallInterval = BullRunFallInterval;
        GetWorldTimerManager().ClearTimer(TetrominoFallTimerHandle);
        GetWorldTimerManager().SetTimer(TetrominoFallTimerHandle, this, &ATetrisGrid::MoveTetrominoDown, IsFastDropping ? FastFallInterval : CurrentFallInterval, true);
        break;
    case 1:
        if (CryptoCrashClass)
        {
            CryptoCrashWidget = CreateWidget<UUserWidget>(GetWorld(), CryptoCrashClass);
            if (CryptoCrashWidget)
            {
                CryptoCrashWidget->AddToViewport();
            }
        }

        CurrentMarketEvent = EMarketEvent::CryptoCrash;
        // clear and reset the fall interval
        CurrentFallInterval = CryptoCrashFallInterval;
        GetWorldTimerManager().ClearTimer(TetrominoFallTimerHandle);
        GetWorldTimerManager().SetTimer(TetrominoFallTimerHandle, this, &ATetrisGrid::MoveTetrominoDown, IsFastDropping ? FastFallInterval : CurrentFallInterval, true);
        break;
    }
    
    MarketEventsInterval = FMath::RandRange(30.0f, 45.0f);
}

// unique interactions and power ups

void ATetrisGrid::TriggerExplosion(AActor* HighValueToken1, AActor* HighValueToken2, FLinearColor ExplosionColor1, FLinearColor ExplosionColor2)
{
    if (ExplosionCue)
    {
        UGameplayStatics::PlaySoundAtLocation(this, ExplosionCue, GetActorLocation());
    }

    TArray<FVector> ExplosionOffsets = {
        FVector(100.0f, 0.0f, 0.0f),   // Right
        FVector(-100.0f, 0.0f, 0.0f),  // Left
        FVector(0.0f, 0.0f, 100.0f),   // Up
        FVector(0.0f, 0.0f, -100.0f),  // Down
        FVector(100.0f, 0.0f, 100.0f),   // Top Right
        FVector(-100.0f, 0.0f, 100.0f),  // Top Left
        FVector(100.0f, 0.0f, -100.0f),  // Bottom Right
        FVector(-100.0f, 0.0f, -100.0f)  // Bottom Left
    };

    FVector Token1Location = HighValueToken1->GetActorLocation();
    FVector Token2Location = HighValueToken2->GetActorLocation();

    // Destroy high-value tokens and update grid
    DestroyBlockAtLocation(Token1Location);
    DestroyBlockAtLocation(Token2Location);

    // Trigger explosion effects at adjacent blocks
    for (const FVector& Offset : ExplosionOffsets)
    {
        FVector ExplosionLocation1 = Token1Location + Offset;
        FVector ExplosionLocation2 = Token2Location + Offset;

        // Directly update the grid at the expected explosion locations
        UpdateGridAtLocation(ExplosionLocation1);
        UpdateGridAtLocation(ExplosionLocation2);

        APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
        if (PlayerController && CameraShakeClass.IsValid())
        {
            UClass* LoadedCameraShakeClass = CameraShakeClass.Get();
            PlayerController->ClientStartCameraShake(LoadedCameraShakeClass);
        }

        SpawnNiagaraSystem(TEXT("/Game/VFX/NS_Explosion"), (ExplosionLocation1 + ExplosionLocation2) / 2.0f, ExplosionColor1, ExplosionColor2);
    }

    MoveBlocksDown();
}

void ATetrisGrid::MoveBlocksDown()
{
    for (int32 x = 0; x < GridWidth; ++x)
    {
        for (int32 y = 1; y < GridHeight; ++y)  // Start from row 1 as row 0 is the bottom
        {
            if (Grid[x][y] != nullptr && Grid[x][y - 1] == nullptr)
            {
                if (Grid[x][y]->Tags.Contains(FName("TetrisBlock")))
                {
                    int32 DropDistance = 0;

                    // Calculate how far the block can drop
                    for (int32 dropY = y - 1; dropY >= 0 && Grid[x][dropY] == nullptr; --dropY)
                    {
                        DropDistance++;
                    }

                    if (DropDistance > 0)
                    {
                        // Move the block down
                        // FVector NewLocation = Grid[x][y]->GetActorLocation() - FVector(0.0f, 0.0f, 100.0f * DropDistance);
                        // Grid[x][y]->SetActorLocation(NewLocation);

                        BlocksToDrop.Add(Grid[x][y], DropDistance);

                        // Update the grid
                        Grid[x][y - DropDistance] = Grid[x][y];
                        Grid[x][y] = nullptr;

                        // UE_LOG(LogTemp, Log, TEXT("Moved block from (%d, %d) to (%d, %d)"), x, y, x, y - DropDistance);
                    }
                }
            }
        }
    }

    CheckForCombos();
}

void ATetrisGrid::MoveBlocksToDropDown()
{
    if (BlocksToDrop.Num() == 0)
    {
        return;
    }

    for (auto& Elem : BlocksToDrop)
    {
        AActor* Actor = Elem.Key;
        int32 Value = Elem.Value;

        if (Value > 0)
        {
            UE_LOG(LogTemp, Warning, TEXT("Dropping block %s"), *Actor->GetName());

            FVector NewLocation = Actor->GetActorLocation() - FVector(0.0f, 0.0f, 100.0f);
            Actor->SetActorLocation(NewLocation);

            Value = Value - 1;
        }
        else
        {
            BlocksToDrop.Remove(Actor);
        }
    }
}

// void ATetrisGrid::MoveBlockDown(AActor* Block, int32 DropDistance)
// {
//     FVector NewLocation = Block->GetActorLocation() - FVector(0.0f, 0.0f, 100.0f);
//     Block->SetActorLocation(NewLocation);

//     int32 CurrentIndex = BlocksToDrop[Block];
//     int32 NextIndex = CurrentIndex + 1;

//     if (NextIndex < DropDistance)
//     {
//         BlocksToDrop[Block] = NextIndex;
//         GetWorldTimerManager().SetTimer(MoveBlocksTimerHandle, [this, Block, DropDistance]()
//         {
//             MoveBlockDown(Block, DropDistance);
//         }, BlockMoveInterval, false);
//     }
//     else
//     {
//         BlocksToDrop.Remove(Block);

//         // Convert world coordinates to grid coordinates
//         FVector ActorLocation = Block->GetActorLocation();
//         int32 GridX = FMath::RoundToInt((ActorLocation.X + 500.0f) / 100.0f);
//         int32 GridY = FMath::RoundToInt(ActorLocation.Z / 100.0f);

//         // Ensure the actor is not already present at the new location
//         if (GridX >= 0 && GridX < GridWidth && GridY >= 0 && GridY < GridHeight)
//         {
//             Grid[GridX][GridY] = Block;
//         }

//         UE_LOG(LogTemp, Log, TEXT("Moved block to new position at Grid[%d][%d]"), GridX, GridY);
//     }
// }

// void ATetrisGrid::MoveBlocksDown()
// {
//     BlocksToDrop.Empty(); // Clear previous blocks to drop

//     for (int32 x = 0; x < GridWidth; ++x)
//     {
//         for (int32 y = 1; y < GridHeight; ++y) // Start from row 1 as row 0 is the bottom
//         {
//             if (Grid[x][y] != nullptr && Grid[x][y - 1] == nullptr)
//             {
//                 if (Grid[x][y]->Tags.Contains(FName("TetrisBlock")))
//                 {
//                     int32 DropDistance = 0;

//                     // Calculate how far the block can drop
//                     for (int32 dropY = y - 1; dropY >= 0 && Grid[x][dropY] == nullptr; --dropY)
//                     {
//                         DropDistance++;
//                     }

//                     if (DropDistance > 0)
//                     {
//                         AActor* Block = Grid[x][y];
//                         BlocksToDrop.Add(Block, 0); // Initialize drop index

//                         // Clear the grid spot
//                         Grid[x][y] = nullptr;

//                         // Start moving the block down incrementally
//                         MoveBlockDown(Block, DropDistance);

//                         UE_LOG(LogTemp, Log, TEXT("Scheduled move from (%d, %d) to (%d, %d)"), x, y, x, y - DropDistance);
//                     }
//                 }
//             }
//         }
//     }

//     // Delay combo check to after all blocks have moved
//     GetWorldTimerManager().SetTimer(MoveBlocksTimerHandle, [this]()
//     {
//         CheckForCombos();
//     }, (BlockMoveInterval * GridHeight), false);
// }

void ATetrisGrid::CheckForCombos()
{
    TArray<AActor*> BlocksToCheckForExplosions;

    for (int32 x = 0; x < GridWidth; ++x)
    {
        for (int32 y = 0; y < GridHeight; ++y)
        {
            AActor* Block = Grid[x][y];

            if (Block)
            {
                // Check for super blocks
                if (CheckForSuperBlockFormation(Block))
                {
                    // If a super block is formed, skip checking for explosions for this block
                    continue;
                }

                // Add the block to the list for explosion checks
                BlocksToCheckForExplosions.Add(Block);
            }
        }
    }

    for (AActor* Block : BlocksToCheckForExplosions)
    {
        // Check for horizontal and vertical high-value token combinations
        CheckForHorizontalExplosions(Block);
        CheckForVerticalExplosions(Block);
    }
}

void ATetrisGrid::DestroyBlockAtLocation(FVector Location)
{
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);

    for (AActor* Actor : FoundActors)
    {
        FVector ActorLocation = Actor->GetActorLocation();
        if (Actor->Tags.Contains(FName("TetrisBlock")) && ActorLocation == Location)
        {
            // Convert world coordinates to grid coordinates
            int32 GridX = FMath::RoundToInt((ActorLocation.X + 1000.0f) / 100.0f);
            int32 GridY = FMath::RoundToInt(ActorLocation.Z / 100.0f);

            // Check if the coordinates are within grid bounds
            if (GridX >= 0 && GridX < GridWidth && GridY >= 0 && GridY < GridHeight)
            {
                FTetrisBlockValue* FoundValue = FindPointValueByName(*Actor->GetName());

                if (FoundValue)
                {
                    int32 IntValue = FCString::Atoi(*FoundValue->ScoreValue.Replace(TEXT("$"), TEXT("")).Replace(TEXT(","), TEXT("")).Replace(TEXT("."), TEXT("")));

                    if (CurrentMarketEvent == EMarketEvent::BullRun)
                    {
                        IntValue *= 2;
                    }
                    else if (CurrentMarketEvent == EMarketEvent::CryptoCrash)
                    {
                        IntValue /= 2;
                    }

                    Score += IntValue;
                }

                SetGrid(GridX, GridY, nullptr);
                Actor->Destroy();
            }
        }
    }
}

void ATetrisGrid::UpdateGridAtLocation(FVector Location)
{
    int32 GridX = FMath::RoundToInt((Location.X + 1000.0f) / 100.0f);
    int32 GridY = FMath::RoundToInt(Location.Z / 100.0f);

    if (GridX >= 0 && GridX < GridWidth && GridY >= 0 && GridY < GridHeight)
    {
        if (Grid[GridX][GridY] != nullptr)
        {
            if (Grid[GridX][GridY]->Tags.Contains(FName("TetrisBlock")))
            {
                Grid[GridX][GridY]->Destroy();
                Grid[GridX][GridY] = nullptr;
            }
        }
    }
}

void ATetrisGrid::CheckForExplosions(AActor* Actor, FVector Direction)
{
    if (Actor->Tags.Contains(FName("SuperBlock")))
    {
        return;
    }

    FVector BlockLocation = Actor->GetActorLocation();
    FVector TargetLocation = BlockLocation + Direction;

    AActor* AdjacentActor = IsGridOccupied(FMath::RoundToInt((TargetLocation.X + 1000.0f) / 100.0f), FMath::RoundToInt(TargetLocation.Z / 100.0f));
    if (AdjacentActor && AdjacentActor->Tags.Contains(FName("TetrisBlock")) && !AdjacentActor->Tags.Contains(FName("SuperBlock")))
    {
        FTetrisBlockValue* ActorValue = FindPointValueByName(*Actor->GetName());
        FTetrisBlockValue* AdjacentValue = FindPointValueByName(*AdjacentActor->GetName());

        if (ActorValue && AdjacentValue)
        {
            static const TArray<FString> HighRiskBlocks = { "bitcoin", "ethereum", "solana" };
            static const TArray<FString> StablecoinBlocks = { "usdc", "tether" };
            if ((HighRiskBlocks.Contains(ActorValue->BlockName) && HighRiskBlocks.Contains(AdjacentValue->BlockName)) || 
                ActorValue->BlockName == AdjacentValue->BlockName)
            {
                TriggerExplosion(Actor, AdjacentActor, ActorValue->Color, AdjacentValue->Color);

                if (ActorValue->BlockName + "_circ" == ComboTarget || AdjacentValue->BlockName + "_circ" == ComboTarget)
                {
                    Combos = FMath::Clamp(Combos + 1, 0, 5);
                }

                // trigger market update
                if (HighRiskBlocks.Contains(ActorValue->BlockName) && HighRiskBlocks.Contains(AdjacentValue->BlockName))
                {
                    // trigger market shutdown
                    for (FTetrisBlockValue& PointValue : PointValues)
                    {
                        PointValue.ForceVolatilityToGoDown = true;
                    }
                }
                else if (StablecoinBlocks.Contains(ActorValue->BlockName) && StablecoinBlocks.Contains(AdjacentValue->BlockName))
                {
                    // trigger a market uptick
                    for (FTetrisBlockValue& PointValue : PointValues)
                    {
                        PointValue.ForceVolatilityToGoUp = true;
                    }
                }
            }
        }
    }
}

void ATetrisGrid::CheckForHorizontalExplosions(AActor* Actor)
{
    CheckForExplosions(Actor, FVector(100.0f, 0.0f, 0.0f));
}

void ATetrisGrid::CheckForVerticalExplosions(AActor* Actor)
{
    CheckForExplosions(Actor, FVector(0.0f, 0.0f, 100.0f));
}

bool ATetrisGrid::CheckForSuperBlockFormation(AActor* Actor)
{
    // Define relative positions for possible super block formations
    static const TArray<TArray<FVector>> Directions = {
        // Horizontal
        { FVector(100.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f) },
        // Vertical
        { FVector(0.0f, 0.0f, 100.0f), FVector(0.0f, 0.0f, 200.0f) },
        // L-shape configurations
        { FVector(100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 100.0f) },
        { FVector(-100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 100.0f) },
        { FVector(100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -100.0f) },
        { FVector(-100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -100.0f) },
        { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, 100.0f) },
        { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, -100.0f) },
        { FVector(0.0f, 0.0f, 100.0f), FVector(100.0f, 0.0f, 100.0f) },
        { FVector(0.0f, 0.0f, -100.0f), FVector(100.0f, 0.0f, -100.0f) }
    };

    FVector BlockLocation = Actor->GetActorLocation();
    FTetrisBlockValue* ActorValue = FindPointValueByName(*Actor->GetName());
    if (!ActorValue) return false;

    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);

    for (const auto& DirectionSet : Directions)
    {
        FVector Loc1 = BlockLocation + DirectionSet[0];
        FVector Loc2 = BlockLocation + DirectionSet[1];

        AActor* Actor1 = nullptr;
        AActor* Actor2 = nullptr;

        for (AActor* FoundActor : FoundActors)
        {
            FVector FoundLocation = FoundActor->GetActorLocation();
            if (FoundLocation == Loc1)
            {
                Actor1 = FoundActor;
            }
            else if (FoundLocation == Loc2)
            {
                Actor2 = FoundActor;
            }

            if (Actor1 && Actor2)
            {
                break;
            }
        }

        if (Actor1 && Actor2)
        {
            FTetrisBlockValue* Value1 = FindPointValueByName(*Actor1->GetName());
            FTetrisBlockValue* Value2 = FindPointValueByName(*Actor2->GetName());

            if (Value1 && Value2 && ActorValue->BlockName == Value1->BlockName && ActorValue->BlockName == Value2->BlockName)
            {
                DestroyBlockAtLocation(Actor->GetActorLocation());
                DestroyBlockAtLocation(Actor1->GetActorLocation());
                DestroyBlockAtLocation(Actor2->GetActorLocation());

                UpdateGridAtLocation(Actor->GetActorLocation());
                UpdateGridAtLocation(Actor1->GetActorLocation());
                UpdateGridAtLocation(Actor2->GetActorLocation());

                int32 SuperBlockIndex = FindPointValueIndexByName(*Actor->GetName());
                if (SuperBlockIndex != INDEX_NONE)
                {
                    TSubclassOf<AActor> BlockClass = SuperBlocks[SuperBlockIndex];

                    FActorSpawnParameters SpawnParams;
                    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

                    FVector WorldLocation = Actor->GetActorLocation();
                    AActor* SuperBlock = GetWorld()->SpawnActor<AActor>(BlockClass, WorldLocation, FRotator::ZeroRotator, SpawnParams);
                    if (SuperBlock)
                    {
                        SuperBlock->Tags.Add(FName("TetrisBlock"));
                        SuperBlock->Tags.Add(FName("SuperBlock"));

                        int32 GridX = FMath::RoundToInt((WorldLocation.X + 1000.0f) / 100.0f);
                        int32 GridY = FMath::RoundToInt(WorldLocation.Z / 100.0f);

                        SetGrid(GridX, GridY, SuperBlock);
                        SuperBlock->Tags.Add(FName("CanClearThreeRows"));
                    }
                }

                MoveBlocksDown();
                return true;
            }
        }
    }

    return false;
}

void ATetrisGrid::SpawnNiagaraSystem(FString Source, FVector SpawnLoc, FLinearColor ExplosionColor1, FLinearColor ExplosionColor2)
{
    UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *Source);

    if (NiagaraSystem)
    {
        FRotator SpawnRotation = FRotator(0.0f, 0.0f, 0.0f);

        UNiagaraComponent* NiagaraComponent = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraSystem, SpawnLoc, SpawnRotation);
        
        if (NiagaraComponent)
        {
            // Set the user parameter on the Niagara component
            NiagaraComponent->SetVariableLinearColor(TEXT("User.ExplosionColor1"), ExplosionColor1);
            NiagaraComponent->SetVariableLinearColor(TEXT("User.ExplosionColor2"), ExplosionColor2);
        }
    }
}

void ATetrisGrid::ClearThreeRows(int32 RowIndex)
{
    if (LaserBurstCue)
    {
        UGameplayStatics::PlaySoundAtLocation(this, LaserBurstCue, GetActorLocation());
    }

    // Ensure the row index is within bounds
    if (RowIndex >= 0 && RowIndex < GridHeight - 1)
    {
        // Clear the specified rows
        for (int32 y = RowIndex - 1; y <= RowIndex + 1; ++y)
        {
            UE_LOG(LogTemp, Warning, TEXT("Doing for row %d"), y);
            for (int32 x = 0; x < GridWidth; ++x)
            {
                if (y >= 0 && y < GridHeight)
                {
                    if (Grid[x][y] != nullptr)
                    {
                        AActor* GridBlock = Grid[x][y];
                        GridBlock->Tags.Add(FName("Destroy"));
                    }
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Warning, TEXT("Invalid row index: %d"), RowIndex);
    }

    for (int32 y = 0; y < GridHeight; ++y)
    {
        for (int32 x = 0; x < GridWidth; ++x)
        {
            if (Grid[x][y] != nullptr)
            {
                AActor* GridBlock = Grid[x][y];
                if (GridBlock->Tags.Contains(FName("Destroy")))
                {
                    DestroyBlockAtLocation(FVector((x * 100.0f) - 1000.0f, 0.0f, y * 100.0f));
                    UpdateGridAtLocation(FVector((x * 100.0f) - 1000.0f, 0.0f, y * 100.0f));
                }
            }
        }
    }

    MoveBlocksDown();
}

void ATetrisGrid::FreezeTimeForHackerMode()
{
    // Freeze time by setting the global time dilation to zero
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 0.0f);

    // Log the action for debugging purposes
    UE_LOG(LogTemp, Log, TEXT("Time has been frozen for Hacker Mode."));
}

void ATetrisGrid::ResumeTime()
{
    // Resume time by setting the global time dilation back to one
    UGameplayStatics::SetGlobalTimeDilation(GetWorld(), 1.0f);

    // Log the action for debugging purposes
    UE_LOG(LogTemp, Log, TEXT("Time has been resumed."));
}

void ATetrisGrid::SpawnDeadlySecRow()
{
    // spawn sec raid widget to the player

    ShouldSpawnOfficerTetromino = true;
}

void ATetrisGrid::SpawnOfficerTetromino()
{
    if (UWorld* World = GetWorld())
    {
        if (SecClass)
        {
            for (int32 i = 0; i < NextTetrominoShape.BlockOffsets.Num(); ++i)
            {
                FVector2D Offset = NextTetrominoShape.BlockOffsets[i];
                TSubclassOf<AActor> TetrominoBlueprint = SecClass;

                FVector BlockLocation = SpawnLocation + FVector(Offset.X * 100.0f, 0.0f, Offset.Y * 100.0f);
                AActor* Block = World->SpawnActor<AActor>(TetrominoBlueprint, BlockLocation, FRotator::ZeroRotator);

                if (Block)
                {
                    Block->Tags.Add(FName("OfficerBlock"));
                    CurrentTetrominoBlocks.Add(Block);
                }
            }

            if (RoundsLeftBeforeSecSpawn != 1)
            {
                for (AActor* NextBlock : NextTetrominoBlocks)
                {
                    NextBlock->Destroy();
                }
                NextTetrominoBlocks.Empty();
                PrepareNextTetromino();
            }
            else
            {
                for (AActor* NextBlock : NextTetrominoBlocks)
                {
                    NextBlock->Destroy();
                }
                NextTetrominoBlocks.Empty();
                PrepareOfficerTetromino();
            }
        }
    }
}

void ATetrisGrid::ClearOfficerBlocks()
{
    int32 OfficerBlockCount = 0;
    for (int32 x = 0; x < GridWidth; ++x)
    {
        for (int32 y = 0; y < GridHeight; ++y)
        {
            AActor* GridActor = Grid[x][y];
            if (GridActor)
            {
                if (GridActor->Tags.Contains(FName("OfficerBlock")))
                {
                    GridActor->Destroy();
                    SetGrid(x, y, nullptr);
                    OfficerBlockCount++;
                }
            }
        }
    }

    UE_LOG(LogTemp, Warning, TEXT("Cleared out %d officer blocks"), OfficerBlockCount);
    MoveBlocksDown();
    Combos = 0;
}

void ATetrisGrid::UpdateComboTarget()
{
    int32 targetIndex = FMath::RandRange(0, PointValues.Num() - 1);
    ComboTarget = PointValues[targetIndex].BlockName + "_circ";
}

void ATetrisGrid::SpawnRowClearEffect(FVector SpawnPoint, FLinearColor Color)
{
    StartLocationRight = SpawnPoint;
    EndLocationRight = FVector(450.0f, 0.0f, SpawnPoint.Z);
    StartLocationLeft = SpawnPoint;
    EndLocationLeft = FVector(-1050.0f, 0.0f, SpawnPoint.Z);
    Duration = 0.5f;
    ElapsedTimeLeft = 0.0f;
    ElapsedTimeRight = 0.0f;

    UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, TEXT("/Game/VFX/NS_Row_Clear_Effect"));

    if (NiagaraSystem)
    {
        FRotator SpawnRotationLeft = FRotator(0.0f, -90.0f, 0.0f);
        FRotator SpawnRotationRight = FRotator(0.0f, 90.0f, 0.0f);

        UNiagaraComponent* NiagaraComponentLeft = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraSystem, StartLocationLeft, SpawnRotationLeft);
        UNiagaraComponent* NiagaraComponentRight = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), NiagaraSystem, StartLocationRight, SpawnRotationRight);

        if (NiagaraComponentLeft)
        {
            NiagaraComponentLeft->SetVariableLinearColor(TEXT("User.RibbonColor"), Color);
        }

        if (NiagaraComponentRight)
        {
            NiagaraComponentRight->SetVariableLinearColor(TEXT("User.RibbonColor"), Color);
        }

        RowNiagaraComponentLeft = NiagaraComponentLeft;
        RowNiagaraComponentRight = NiagaraComponentRight;
    }

    GetWorldTimerManager().SetTimer(LerpTimerHandle, this, &ATetrisGrid::UpdateNiagaraLocation, 0.01f, true);
}

void ATetrisGrid::UpdateNiagaraLocation()
{
    float DistanceLeft = FVector::Distance(StartLocationLeft, EndLocationLeft);
    float DistanceRight = FVector::Distance(StartLocationRight, EndLocationRight);
    float SpeedLeft = 1000.0f;
    float SpeedRight = 1000.0f;

    // Update left row-clear effect
    FVector CurrentLocationLeft = RowNiagaraComponentLeft->GetComponentLocation();
    FVector DirectionLeft = (EndLocationLeft - StartLocationLeft).GetSafeNormal();
    FVector NewLocationLeft = CurrentLocationLeft + DirectionLeft * SpeedLeft * 0.01f;

    if (FVector::Distance(StartLocationLeft, NewLocationLeft) >= DistanceLeft)
    {
        NewLocationLeft = EndLocationLeft;
        RowNiagaraComponentLeft->SetFloatParameter(FName("OpacityParameter"), 0.0f);
    }

    RowNiagaraComponentLeft->SetWorldLocation(NewLocationLeft);

    // Update right row-clear effect
    FVector CurrentLocationRight = RowNiagaraComponentRight->GetComponentLocation();
    FVector DirectionRight = (EndLocationRight - StartLocationRight).GetSafeNormal();
    FVector NewLocationRight = CurrentLocationRight + DirectionRight * SpeedRight * 0.01f;

    if (FVector::Distance(StartLocationRight, NewLocationRight) >= DistanceRight)
    {
        NewLocationRight = EndLocationRight;
        RowNiagaraComponentRight->SetFloatParameter(FName("OpacityParameter"), 0.0f);
    }

    RowNiagaraComponentRight->SetWorldLocation(NewLocationRight);

    // Stop the timer when both components reach the end location
    if (NewLocationLeft == EndLocationLeft && NewLocationRight == EndLocationRight)
    {
        GetWorldTimerManager().ClearTimer(LerpTimerHandle);
    }
}
