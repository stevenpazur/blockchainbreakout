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
#include "DropState.h"
#include "UObject/ConstructorHelpers.h"
#include "Kismet/KismetMathLibrary.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "limits"
#include "EngineUtils.h"

ATetrisGrid::ATetrisGrid()
{
    LevelsDataTable = {
        { "1", true, 2, 100000, 0.5f, FVector(0.0f, 0.39f, 1.0f) },
        { "2", true, 2, 500000, 0.4f, FVector(1.0f, 0.0f, 0.0f)},
        { "3", true, 2, 1000000, 0.4f, FVector(1.0f, 1.0f, 0.0f) },
        { "4", true, 3, 2000000, 0.4f, FVector(0.0f, 1.0f, 0.0f) },
        { "5", true, 3, 1000000, 0.3f, FVector(1.0f, 0.8f, 0.0f) },
        { "6", true, 4, 4000000, 0.3f, FVector(0.5f, 0.0f, 0.5f) },
        { "7", true, 4, 7000000, 0.3f, FVector(1.0f, 0.0f, 0.76f) },
        { "8", true, 4, 9000000, 0.3f, FVector(0.0f, 1.0f, 0.75f) }
    };

    CurrentLevel = LevelsDataTable[0];
    CurrentLevelIndex = 0;

    DefaultFallInterval = CurrentLevel.FallingSpeed;
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

        OnUpdateScore.Broadcast();

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
        else
        {
			PrintScreen("Failed to load super bitcoin class");
        }

        FString superEthereumPath = TEXT("/Game/Blueprints/BP_superethereum.BP_superethereum_C");
        UClass* SuperEthereumClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superEthereumPath));
        if (SuperEthereumClass)
        {
            SuperBlocks.Add(SuperEthereumClass);
        }
        else
        {
			PrintScreen("Failed to load super ethereum class");
        }

        FString superXrpPath = TEXT("/Game/Blueprints/BP_superxrp.BP_superxrp_C");
        UClass* SuperXrpClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superXrpPath));
        if (SuperXrpClass)
        {
            SuperBlocks.Add(SuperXrpClass);
        }
        else
        {
			PrintScreen("Failed to load super xrp class");
        }

        FString superPolkadotPath = TEXT("/Game/Blueprints/BP_superpolkadot.BP_superpolkadot_C");
        UClass* SuperPolkadotClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superPolkadotPath));
        if (SuperPolkadotClass)
        {
            SuperBlocks.Add(SuperPolkadotClass);
        }
        else
        {
			PrintScreen("Failed to load super polkadot class");
        }

        FString superSolanaPath = TEXT("/Game/Blueprints/BP_supersolana.BP_supersolana_C");
        UClass* SuperSolanaClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superSolanaPath));
        if (SuperSolanaClass)
        {
            SuperBlocks.Add(SuperSolanaClass);
        }
        else
        {
			PrintScreen("Failed to load super solana class");
        }

        FString superTetherPath = TEXT("/Game/Blueprints/BP_supertether.BP_supertether_C");
        UClass* SuperTetherClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superTetherPath));
        if (SuperTetherClass)
        {
            SuperBlocks.Add(SuperTetherClass);
        }
        else
        {
			PrintScreen("Failed to load super tether class");
        }

        FString superUsdcPath = TEXT("/Game/Blueprints/BP_superusdc.BP_superusdc_C");
        UClass* SuperUsdcClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *superUsdcPath));
        if (SuperUsdcClass)
        {
            SuperBlocks.Add(SuperUsdcClass);
        }
        else
        {
			PrintScreen("Failed to load super usdc class");
        }

        FString bombBlockPath = TEXT("/Game/Blueprints/BP_bomb.BP_bomb_C");
        BombBlockClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *bombBlockPath));

        FString bullRunPath = TEXT("/Game/Blueprints/W_bullrun.W_bullrun_C");
        BullRunClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *bullRunPath));

        FString cryptoCrashPath = TEXT("/Game/Blueprints/W_cryptocrash.W_cryptocrash_C");
        CryptoCrashClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, *cryptoCrashPath));

        FString cameraShakePath = TEXT("/Game/Blueprints/BP_CameraShake.BP_CameraShake_C");
        CameraShakeClass = TSoftClassPtr<UCameraShakeBase>(FSoftObjectPath(cameraShakePath));

        TetrisBoard = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), nullptr, TEXT("/Game/Blueprints/BP_TetrisGrid.BP_TetrisGrid_C")));

        UMaterialInterface* GlowBoardInst = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Materials/M_glow_inst.M_glow_inst")));
		GlowMaterialForBoard = UMaterialInstanceDynamic::Create(GlowBoardInst, this);

		UMaterialInterface* BackgroundBoardInst = Cast<UMaterialInterface>(StaticLoadObject(UMaterialInterface::StaticClass(), nullptr, TEXT("/Game/Materials/M_hologram_board.M_hologram_board")));
		BackgroundMaterialForBoard = UMaterialInstanceDynamic::Create(BackgroundBoardInst, this);

        FActorSpawnParameters SpawnParams;
        TetrisBoardInstance = GetWorld()->SpawnActor<AActor>(TetrisBoard, FVector(-500.0f, 0.0f, 0.0f), FRotator::ZeroRotator, SpawnParams);

        if (TetrisBoardInstance)
        {
            const TSet<UActorComponent*>& Components = TetrisBoardInstance->GetComponents();
            const TArray<FString> TargetComponents = { "LeftBound", "RightBound", "Floor", "Ceiling" };

            for (UActorComponent* Component : Components)
            {
                if (Component && TargetComponents.Contains(Component->GetName()))
                {
                    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
                    if (MeshComponent)
                    {
                        MeshComponent->SetMaterial(0, GlowMaterialForBoard);
                    }
                }
                else if (Component && Component->GetName() == "Background")
                {
                    UStaticMeshComponent* MeshComponent = Cast<UStaticMeshComponent>(Component);
                    if (MeshComponent)
                    {
                        if (BackgroundMaterialForBoard)
                        {
                            BackgroundMaterialForBoard->SetVectorParameterValue(TEXT("Color"), CurrentLevel.BackgroundColor);
                            MeshComponent->SetMaterial(0, BackgroundMaterialForBoard);
                        }
                    }
                }
            }
        }

        GetWorldTimerManager().SetTimer(TetrominoFallTimerHandle, this, &ATetrisGrid::MoveTetrominoDown, CurrentFallInterval, true);

        UpdateMarketValues();
        GetWorldTimerManager().SetTimer(UpdateMarketValuesTimer, this, &ATetrisGrid::UpdateMarketValues, 1.0f, true, 0.0f);

        GetWorldTimerManager().SetTimer(UpdateMarketEventsTimer, this, &ATetrisGrid::UpdateMarketEvents, MarketEventsInterval, true, -1.0f);

        GetWorldTimerManager().SetTimer(UpdateComboTargetTimer, this, &ATetrisGrid::UpdateComboTarget, FMath::RandRange(30.0f, 45.0f), true, -1.0f);

        GetWorldTimerManager().SetTimer(MoveBlocksTimerHandle, this, &ATetrisGrid::MoveBlocksToDropDown, 0.1f, true, 0.0f);


        int32 NextTetrominoIndex = FMath::RandRange(0, TetrominoShapes.Num() - 1);
        NextTetrominoShape = TetrominoShapes[NextTetrominoIndex];

        // PrepareFirstTetromino();
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

void ATetrisGrid::PrepareFirstTetromino()
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
            CryptoBlockIndex = 0;
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
    if (!bIsClearing)
    {
        if (UWorld* World = GetWorld())
        {
            for (int32 i = 0; i < NextTetrominoShape.BlockOffsets.Num(); ++i)
            {
                FVector2D Offset = NextTetrominoShape.BlockOffsets[i];
                TSubclassOf<AActor> TetrominoBlueprint = NextTetrominoBlocks[i]->GetClass();
                if (TetrominoBlueprint == SecClass)
                {
                    UE_LOG(LogTemp, Error, TEXT("Whyyy?"));
                }
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

        for (int32 x = 0; x < GridWidth; ++x)
        {
            for (int32 y = 0; y < GridHeight; ++y)
            {
                if (Grid[x][y] && IsValid(Grid[x][y]))
                {
                    if (Grid[x][y]->Tags.Contains("CannotBlowUpYet"))
                    {
                        Grid[x][y]->Tags.Remove("CannotBlowUpYet");
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
            InOfficerBlocksRound = false;
        }

        GetWorldTimerManager().SetTimer(CheckIfReadyToSpawnTetromino, this, &ATetrisGrid::CheckIfReadyForNewTetromino, .2f, true);
    }
}

void ATetrisGrid::CheckIfReadyForNewTetromino()
{
    if (!bAnyDropInProgress)
    {
        GetWorldTimerManager().ClearTimer(CheckIfReadyToSpawnTetromino);
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
            int maxInt32 = std::numeric_limits<int>::max();

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
                                IntValue = FMath::Clamp(IntValue * 2, 0, maxInt32 - 1);
                            }
                            else if (CurrentMarketEvent == EMarketEvent::CryptoCrash)
                            {
                                IntValue /= 2;
                            }

                            RowScore = FMath::Clamp(RowScore + IntValue, 0, maxInt32 - 1);
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
            Combos = FMath::Clamp(Combos + 1, 0, 5);
            OnUpdateNotches.Broadcast();

            y--;

            IncrementScore(Score + RowScore);
        }
    }
}

void ATetrisGrid::MoveBlocksDownIncrementally()
{
    bool bAnyBlockMoved = false;

    for (AActor* Block : BlocksToMove)
    {
        FVector CurrentLocation = Block->GetActorLocation();
        FVector NewLocation = CurrentLocation - FVector(0.0f, 0.0f, 100.0f); // Move down by 10 units

        FVector2D GridCoords = FVector2D((CurrentLocation.X + 1000.0f) / 100.0f, CurrentLocation.Z / 100.0f);
        bool CanDo = CanMoveDown(GridCoords.X, GridCoords.Y);

        if (CanDo)
        {
            Block->SetActorLocation(NewLocation);
            bAnyBlockMoved = true;
        }
    }

    if (bAnyBlockMoved)
    {
        // Continue moving blocks down
        GetWorldTimerManager().SetTimer(MoveBlocksTimerHandle, this, &ATetrisGrid::MoveBlocksDownIncrementally, 0.5f, false);
    }
    else
    {
        // Stop the timer and clear the list of blocks to move
        GetWorldTimerManager().ClearTimer(MoveBlocksTimerHandle);
        BlocksToMove.Empty();
        RowsToMove.Empty();
    }
}

bool ATetrisGrid::CanMoveDown(int32 x, int32 y)
{
    if (x > 0 && y > 0)
    {
        if (Grid[x][y] != nullptr && Grid[x][y - 1] == nullptr)
        {
            return true;
        }
    }

    return false;
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
        int maxInt32 = std::numeric_limits<int>::max();

        // have at least $2 as a value so the value can go back up if possible
        int32 NewScore = FMath::Clamp(IntValue * pointMultiplier, 2, maxInt32);
        FString DollarAmount = FString::Printf(TEXT("$%d"), NewScore);
        
        int maxFloat = std::numeric_limits<float>::max();
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

        SpawnNiagaraSystem(TEXT("/Game/VFX/NS_Explosion"), (Token1Location + Token2Location) / 2.0f, ExplosionColor1, ExplosionColor2);
    }
}

void ATetrisGrid::CheckForBlocksToDrop()
{
    DropsArray.Empty();

    for (int32 x = 0; x < GridWidth; ++x)
    {
        int32 EmptySpacesBelow = 0; // Tracks empty spaces below the current block

        for (int32 y = 0; y < GridHeight; ++y) // Process the column from bottom to top
        {
            if (Grid[x][y] == nullptr)
            {
                EmptySpacesBelow++; // Count empty spaces
            }
            else if (Grid[x][y]->Tags.Contains(FName("TetrisBlock")))
            {
                // A block is found; calculate its drop distance
                if (EmptySpacesBelow > 0)
                {
                    DropsArray.Add(FDropState(x, y, 0, EmptySpacesBelow)); // Store the block's drop state
                }
            }
            else
            {
                // An obstacle is encountered; reset empty spaces below
                EmptySpacesBelow = 0;
            }
        }
    }


    // Start the timer for the drops
    if (DropsArray.Num() > 0)
    {
        GetWorldTimerManager().SetTimer(
            DropTimerHandle, this,
            &ATetrisGrid::HandleMultipleDrops,
            0.2f, true
        );
    }
}

void ATetrisGrid::HandleMultipleDrops()
{
    bAnyDropInProgress = false;
    
    for (FDropState& Drop : DropsArray)
    {
        if (Drop.LoopIndex < Drop.DropDistance)
        {
            bAnyDropInProgress = true;

            // Execute drop logic for the current block
            FVector NewLocation = Grid[Drop.X][Drop.Y1]->GetActorLocation() - FVector(0.0f, 0.0f, 100.0f);
            Grid[Drop.X][Drop.Y1]->SetActorLocation(NewLocation);

            Grid[Drop.X][Drop.Y1 - 1] = Grid[Drop.X][Drop.Y1];
            Grid[Drop.X][Drop.Y1] = nullptr;

            Drop.Y1--;
            Drop.LoopIndex++;
        }
    }

    // Stop the timer if all drops are complete
    if (!bAnyDropInProgress)
    {
        GetWorldTimerManager().ClearTimer(DropTimerHandle);
        CheckForCombos();
    }
}

void ATetrisGrid::MoveBlocksToDropDown()
{
    if (!bIsCheckingForCombos)
    {
        UE_LOG(LogTemp, Warning, TEXT("BlocksToDrop length: %d"), BlocksToDrop.Num());

        bIsAnimating = true;

        bool bBlockMoved = false;

        for (auto& Elem : BlocksToDrop)
        {
            AActor* Actor = Elem.Key;
            int32& Value = Elem.Value;

            if (Value > 0)
            {
                FVector CurrentLocation = Actor->GetActorLocation();
                FVector NewLocation = CurrentLocation - FVector(0.0f, 0.0f, 100.0f);

                // Check if the new location is above the bottom of the board
                if (NewLocation.Z >= 0.0f)
                {
                    Actor->SetActorLocation(NewLocation);
                    Value = Value - 1;
                    bBlockMoved = true;

                    UE_LOG(LogTemp, Warning, TEXT("Dropping block %s, Value: %d"), *Actor->GetName(), Value);
                }
                else
                {
                    Value = 0; // Set Value to 0 if it has reached the bottom
                    UE_LOG(LogTemp, Warning, TEXT("Block %s has reached the bottom."), *Actor->GetName());
                }
            }
        }

        // If no blocks moved, end the animation
        if (!bBlockMoved)
        {
            bIsAnimating = false;
        }
    }
}

void ATetrisGrid::CheckForCombos()
{
    bIsCheckingForCombos = true;
    bool bHasFoundCombo = false;

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

                    if (GridBlock->Tags.Contains(FName("CanClearThreeRows")) && !GridBlock->Tags.Contains(FName("CannotBlowUpYet")))
                    {
                        if (FoundValue->BlockName + "_circ" == ComboTarget)
                        {
                            // Combos = FMath::Clamp(Combos + 1, 0, 5);
                        }
                        ClearThreeRows(y);
                        SpawnNiagaraSystem(TEXT("/Game/VFX/NS_Explosion"), GridBlock->GetActorLocation(), FoundValue->Color, FoundValue->Color);
                        SpawnRowClearEffect(GridBlock->GetActorLocation(), FoundValue->Color);
                        bHasFoundCombo = true;
                    }
                }
                else if (IsValid(GridBlock) && GridBlock->Tags.Contains("BombBlock"))
                {
                    SpawnBombExplosion(GridBlock);
                }
            }
        }
    }

    TArray<AActor*> BlocksToCheckForExplosions;

    for (int32 x = 0; x < GridWidth; ++x)
    {
        for (int32 y = 0; y < GridHeight; ++y)
        {
            AActor* Block = Grid[x][y];

            if (Block)
            {
                if (CurrentLevel.BlockPairingSet <= 4)
                {
                    // Check for 4-of-a-kind
                    if (CheckForSuperDuperBlockFormation(Block))
                    {
                        continue;
                    }
                }

                if (CurrentLevel.BlockPairingSet <= 3)
                {
                    // Check for super blocks
                    if (CheckForSuperBlockFormation(Block))
                    {
                        // If a super block is formed, skip checking for explosions for this block
                        continue;
                    }
                }

                // Add the block to the list for explosion checks
                BlocksToCheckForExplosions.Add(Block);
            }
        }
    }

    if (CurrentLevel.BlockPairingSet <= 2)
    {
        for (AActor* Block : BlocksToCheckForExplosions)
        {
            // Check for horizontal and vertical high-value token combinations
            bool bHorizontal = CheckForHorizontalExplosions(Block);
            bool bVertical = CheckForVerticalExplosions(Block);

            if (bHorizontal || bVertical)
            {
                bHasFoundCombo = true;
            }
        }
    }

    if (bHasFoundCombo)
    {
        CheckForBlocksToDrop();
    }

    CheckAndClearFullRows();

    bIsCheckingForCombos = false;
}

void ATetrisGrid::DestroyBlockAtLocation(FVector Location)
{
    TArray<AActor*> FoundActors;
    UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);
    int maxInt32 = std::numeric_limits<int>::max();

    for (AActor* Actor : FoundActors)
    {
        FVector ActorLocation = Actor->GetActorLocation();
        if ((Actor->Tags.Contains(FName("TetrisBlock")) || Actor->Tags.Contains("BombBlock")) && ActorLocation == Location)
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
                    int32 IntValue = FMath::Clamp(FCString::Atoi(*FoundValue->ScoreValue.Replace(TEXT("$"), TEXT("")).Replace(TEXT(","), TEXT("")).Replace(TEXT("."), TEXT(""))), 0, maxInt32 - 1);

                    if (CurrentMarketEvent == EMarketEvent::BullRun)
                    {
                        IntValue = FMath::Clamp(IntValue * 2, 0, maxInt32 - 1);
                    }
                    else if (CurrentMarketEvent == EMarketEvent::CryptoCrash)
                    {
                        IntValue /= 2;
                    }

                    IncrementScore(Score + IntValue);
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
            if ((Grid[GridX][GridY]->Tags.Contains(FName("TetrisBlock")) || Grid[GridX][GridY]->Tags.Contains(FName("BombBlock"))) && 
            !Grid[GridX][GridY]->Tags.Contains(FName("SuperBlock")))
            {
                Grid[GridX][GridY]->Destroy();
                Grid[GridX][GridY] = nullptr;
            }
        }
    }
}

bool ATetrisGrid::CheckForExplosions(AActor* Actor, FVector Direction)
{
    if (Actor->Tags.Contains(FName("SuperBlock")) || Actor->Tags.Contains(FName("GlowBlock")))
    {
        return false;
    }

    bool bToReturn = false;
    
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
            if (ActorValue->BlockName == AdjacentValue->BlockName)
            {
                if (Actor != AdjacentActor)
                {
                    bToReturn = true;
                    TriggerExplosion(Actor, AdjacentActor, ActorValue->Color, AdjacentValue->Color);

                    if (ActorValue->BlockName + "_circ" == ComboTarget || AdjacentValue->BlockName + "_circ" == ComboTarget)
                    {
                        // Combos = FMath::Clamp(Combos + 1, 0, 5);
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

    return bToReturn;
}

bool ATetrisGrid::CheckForHorizontalExplosions(AActor* Actor)
{
    return CheckForExplosions(Actor, FVector(100.0f, 0.0f, 0.0f));
}

bool ATetrisGrid::CheckForVerticalExplosions(AActor* Actor)
{
    return CheckForExplosions(Actor, FVector(0.0f, 0.0f, 100.0f));
}

// bool ATetrisGrid::CheckForSuperBlockFormation(AActor* Actor)
// {
//     if (IsValid(Actor))
//     {
//         // Define relative positions for possible super block formations
//         static const TArray<TArray<FVector>> Directions = {
//             // Horizontal
//             { FVector(100.0f, 0.0f, 0.0f), FVector(200.0f, 0.0f, 0.0f) },
//             // Vertical
//             { FVector(0.0f, 0.0f, 100.0f), FVector(0.0f, 0.0f, 200.0f) },
//             // L-shape configurations
//             { FVector(100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 100.0f) },
//             { FVector(-100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, 100.0f) },
//             { FVector(100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -100.0f) },
//             { FVector(-100.0f, 0.0f, 0.0f), FVector(0.0f, 0.0f, -100.0f) },
//             { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, 100.0f) },
//             { FVector(100.0f, 0.0f, 0.0f), FVector(100.0f, 0.0f, -100.0f) },
//             { FVector(0.0f, 0.0f, 100.0f), FVector(100.0f, 0.0f, 100.0f) },
//             { FVector(0.0f, 0.0f, -100.0f), FVector(100.0f, 0.0f, -100.0f) }
//         };

//         FVector BlockLocation = Actor->GetActorLocation();
//         FTetrisBlockValue* ActorValue = FindPointValueByName(*Actor->GetName());
//         if (!ActorValue) return false;

//         TArray<AActor*> FoundActors;
//         UGameplayStatics::GetAllActorsOfClass(GetWorld(), AActor::StaticClass(), FoundActors);

//         for (const auto& DirectionSet : Directions)
//         {
//             FVector Loc1 = BlockLocation + DirectionSet[0];
//             FVector Loc2 = BlockLocation + DirectionSet[1];

//             AActor* Actor1 = nullptr;
//             AActor* Actor2 = nullptr;

//             for (AActor* FoundActor : FoundActors)
//             {
//                 if (FoundActor->Tags.Contains(FName("GlowBlock"))) continue;

//                 FVector FoundLocation = FoundActor->GetActorLocation();
//                 if (FoundLocation == Loc1)
//                 {
//                     Actor1 = FoundActor;
//                 }
//                 else if (FoundLocation == Loc2)
//                 {
//                     Actor2 = FoundActor;
//                 }

//                 if (Actor1 && Actor2)
//                 {
//                     break;
//                 }
//             }

//             if (Actor1 && Actor2)
//             {
//                 FTetrisBlockValue* Value1 = FindPointValueByName(*Actor1->GetName());
//                 FTetrisBlockValue* Value2 = FindPointValueByName(*Actor2->GetName());

//                 if (Value1 && Value2 && ActorValue->BlockName == Value1->BlockName && ActorValue->BlockName == Value2->BlockName)
//                 {
//                     TargetActors = {
//                         { Actor, Actor1, Actor2 },
//                         0.0f,
//                         Actor->GetName(),
//                         { Actor->GetActorLocation(), Actor1->GetActorLocation(), Actor2->GetActorLocation() }
//                     };
//                     GlowBlocks();

//                     return true;
//                 }
//             }
//         }

//         return false;
//     }
//     else
//     {
//         return false;
//     }
// }

bool ATetrisGrid::CheckForSuperBlockFormation(AActor* Actor)
{
    if (!IsValid(Actor)) return false;
    if (Actor->GetName().Contains("super")) return false;

    FVector BlockLocation = Actor->GetActorLocation();
    FTetrisBlockValue* ActorValue = FindPointValueByName(*Actor->GetName());
    if (!ActorValue) return false;

    // Stores blocks already visited to prevent infinite loops
    TSet<AActor*> VisitedBlocks;
    TArray<AActor*> MatchingBlocks;

    // Lambda function for recursive DFS
    auto DFS = [&](AActor* CurrentBlock, auto&& DFSRef) -> void
    {
        if (!IsValid(CurrentBlock) || VisitedBlocks.Contains(CurrentBlock)) return;

        // Mark the current block as visited
        VisitedBlocks.Add(CurrentBlock);
        MatchingBlocks.Add(CurrentBlock);

        FVector CurrentLocation = CurrentBlock->GetActorLocation();

        // Check neighbors in four directions: left, right, up, down
        static const TArray<FVector> Directions = {
            FVector(100.0f, 0.0f, 0.0f),  // Right
            FVector(-100.0f, 0.0f, 0.0f), // Left
            FVector(0.0f, 0.0f, 100.0f),  // Up
            FVector(0.0f, 0.0f, -100.0f)  // Down
        };

        for (const FVector& Direction : Directions)
        {
            FVector NeighborLocation = CurrentLocation + Direction;

            // Find the neighbor block
            AActor* NeighborBlock = nullptr;
            for (AActor* FoundActor : TActorRange<AActor>(GetWorld()))
            {
                if (FoundActor->GetActorLocation() == NeighborLocation &&
                    !VisitedBlocks.Contains(FoundActor))
                {
                    FTetrisBlockValue* NeighborValue = FindPointValueByName(*FoundActor->GetName());
                    if (NeighborValue && NeighborValue->BlockName == ActorValue->BlockName && 
                        !Actor->Tags.Contains("GlowBlock") && !FoundActor->Tags.Contains("GlowBlock") &&
                        !Actor->GetName().Contains("super") && !FoundActor->GetName().Contains("super"))
                    {
                        NeighborBlock = FoundActor;
                        break;
                    }
                }
            }

            // Recur to the neighbor block if it's valid
            if (NeighborBlock)
            {
                DFSRef(NeighborBlock, DFSRef);
            }
        }
    };

    // Start the DFS from the initial block
    DFS(Actor, DFS);

    // Check if enough blocks were matched (e.g., 4 blocks for a "super duper block")
    if (MatchingBlocks.Num() >= 3)
    {
        // Collect block locations
        TArray<FVector> BlockLocations;
        TArray<AActor*> FirstMatchingBlocks;
        // for (AActor* Block : MatchingBlocks)
        for (int32 i = 0; i < 3; ++i)
        {
            AActor* Block = MatchingBlocks[i];
            if (IsValid(Block))
            {
                FirstMatchingBlocks.Add(Block);
                BlockLocations.Add(Block->GetActorLocation());
                Block->Tags.Add(FName("ToGlow"));
            }
        }

        // Store matching blocks and trigger effects
        TargetActors = {
            FirstMatchingBlocks,
            0.0f,
            Actor->GetName(),
            BlockLocations
        };
        // GlowSuperDuperBlocks();
        GlowBlocks();

        return true;
    }

    return false;
}

bool ATetrisGrid::CheckForSuperDuperBlockFormation(AActor* Actor)
{
    if (!IsValid(Actor)) return false;
    if (Actor->GetName().Contains("super")) return false;

    FVector BlockLocation = Actor->GetActorLocation();
    FTetrisBlockValue* ActorValue = FindPointValueByName(*Actor->GetName());
    if (!ActorValue) return false;

    // Stores blocks already visited to prevent infinite loops
    TSet<AActor*> VisitedBlocks;
    TArray<AActor*> MatchingBlocks;

    // Lambda function for recursive DFS
    auto DFS = [&](AActor* CurrentBlock, auto&& DFSRef) -> void
    {
        if (!IsValid(CurrentBlock) || VisitedBlocks.Contains(CurrentBlock)) return;

        // Mark the current block as visited
        VisitedBlocks.Add(CurrentBlock);
        MatchingBlocks.Add(CurrentBlock);

        FVector CurrentLocation = CurrentBlock->GetActorLocation();

        // Check neighbors in four directions: left, right, up, down
        static const TArray<FVector> Directions = {
            FVector(100.0f, 0.0f, 0.0f),  // Right
            FVector(-100.0f, 0.0f, 0.0f), // Left
            FVector(0.0f, 0.0f, 100.0f),  // Up
            FVector(0.0f, 0.0f, -100.0f)  // Down
        };

        for (const FVector& Direction : Directions)
        {
            FVector NeighborLocation = CurrentLocation + Direction;

            // Find the neighbor block
            AActor* NeighborBlock = nullptr;
            for (AActor* FoundActor : TActorRange<AActor>(GetWorld()))
            {
                if (FoundActor->GetActorLocation() == NeighborLocation &&
                    !VisitedBlocks.Contains(FoundActor))
                {
                    FTetrisBlockValue* NeighborValue = FindPointValueByName(*FoundActor->GetName());
                    if (NeighborValue && NeighborValue->BlockName == ActorValue->BlockName && 
                        !Actor->Tags.Contains("GlowBlock") && !FoundActor->Tags.Contains("GlowBlock") &&
                        !Actor->GetName().Contains("super") && !FoundActor->GetName().Contains("super"))
                    {
                        NeighborBlock = FoundActor;
                        break;
                    }
                }
            }

            // Recur to the neighbor block if it's valid
            if (NeighborBlock)
            {
                DFSRef(NeighborBlock, DFSRef);
            }
        }
    };

    // Start the DFS from the initial block
    DFS(Actor, DFS);

    // Check if enough blocks were matched (e.g., 4 blocks for a "super duper block")
    if (MatchingBlocks.Num() >= 4)
    {
        // Collect block locations
        TArray<FVector> BlockLocations;
        TArray<AActor*> FirstMatchingBlocks;
        // for (AActor* Block : MatchingBlocks)
        for (int32 i = 0; i < 4; ++i)
        {
            AActor* Block = MatchingBlocks[i];
            if (IsValid(Block))
            {
                FirstMatchingBlocks.Add(Block);
                BlockLocations.Add(Block->GetActorLocation());
                Block->Tags.Add(FName("ToGlow"));
            }
        }

        // Store matching blocks and trigger effects
        TargetActors = {
            FirstMatchingBlocks,
            0.0f,
            Actor->GetName(),
            BlockLocations
        };
        // GlowSuperDuperBlocks();
        GlowBlocks();

        return true;
    }

    return false;
}

void ATetrisGrid::MakeSuperBlock()
{
    if (!bIsClearing)
    {
        int32 SuperBlockIndex = FindPointValueIndexByName(*TargetActors.BlockName);
        if (SuperBlockIndex != INDEX_NONE && SuperBlockIndex > -1 && SuperBlockIndex < SuperBlocks.Num())
        {
            TSubclassOf<AActor> BlockClass = SuperBlocks[SuperBlockIndex];

            if (BlockClass)
            {
                FActorSpawnParameters SpawnParams;
                SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

                FVector WorldLocation = TargetActors.SuperBlockDropSpots[0];
                AActor* SuperBlock = GetWorld()->SpawnActor<AActor>(BlockClass, WorldLocation, FRotator::ZeroRotator, SpawnParams);
                if (SuperBlock)
                {
                    SuperBlock->Tags.Add(FName("TetrisBlock"));
                    SuperBlock->Tags.Add(FName("SuperBlock"));
                    SuperBlock->Tags.Add(FName("CannotBlowUpYet"));

                    int32 GridX = FMath::RoundToInt((WorldLocation.X + 1000.0f) / 100.0f);
                    int32 GridY = FMath::RoundToInt(WorldLocation.Z / 100.0f);

                    SetGrid(GridX, GridY, SuperBlock);
                    SuperBlock->Tags.Add(FName("CanClearThreeRows"));
                }
                else
                {
                    PrintScreen(FString::Printf(TEXT("Failed to spawn Super Block!  Block class: %s"), *BlockClass->GetName()));
                }
            }
            else
            {
				PrintScreen("Failed to get Super Block class!");
            }
        }
        else
        {
			PrintScreen(FString::Printf(TEXT("Failed to find Super Block index!  Block name: %s"), *TargetActors.BlockName));
        }

        CheckForBlocksToDrop();
    }
}

void ATetrisGrid::MakeSuperDuperBlock()
{
    if (!bIsClearing)
    {
        if (BombBlockClass)
        {
            FActorSpawnParameters SpawnParams;
            SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

            FVector WorldLocation = TargetActors.SuperBlockDropSpots[0];
            AActor* BombBlock = GetWorld()->SpawnActor<AActor>(BombBlockClass, WorldLocation, FRotator::ZeroRotator, SpawnParams);
            if (BombBlock)
            {
                BombBlock->Tags.Add(FName("BombBlock"));
                BombBlock->Tags.Add(FName("SuperDuperBlock"));
                BombBlock->Tags.Add(FName("CannotBlowUpYet"));

                int32 GridX = FMath::RoundToInt((WorldLocation.X + 1000.0f) / 100.0f);
                int32 GridY = FMath::RoundToInt(WorldLocation.Z / 100.0f);

                SetGrid(GridX, GridY, BombBlock);
                if (GridX + 1 < GridWidth)
                {
                    if (Grid[GridX + 1][GridY] != nullptr)
                    {
                        FVector Loc = FVector(((GridX + 1) * 100.0f) - 1000.0f, 0.0f, GridY * 100.0f);
                        DestroyBlockAtLocation(Loc);
                        UpdateGridAtLocation(Loc);
                    }
                    SetGrid(GridX + 1, GridY, BombBlock);
                }
                if (GridY + 1 < GridHeight)
                {
                    if (Grid[GridX][GridY + 1] != nullptr)
                    {
                        FVector Loc = FVector((GridX * 100.0f) - 1000.0f, 0.0f, (GridY + 1) * 100.0f);
                        DestroyBlockAtLocation(Loc);
                        UpdateGridAtLocation(Loc);
                    }
                    SetGrid(GridX, GridY + 1, BombBlock);
                }
                if (GridX + 1 < GridWidth && GridY + 1 < GridHeight)
                {
                    if (Grid[GridX + 1][GridY + 1] != nullptr)
                    {
                        FVector Loc = FVector(((GridX + 1) * 100.0f) - 1000.0f, 0.0f, (GridY + 1) * 100.0f);
                        DestroyBlockAtLocation(Loc);
                        UpdateGridAtLocation(Loc);
                    }
                    SetGrid(GridX + 1, GridY + 1, BombBlock);
                }
                BombBlock->Tags.Add(FName("CanSuperDuper"));
            }
        }

        CheckForBlocksToDrop();
    }
}

void ATetrisGrid::GlowBlocks()
{
    if (TargetActors.GlowBlocks.Num() < 3) return; // Ensure we have at least 3 actors

    for (AActor* Actor : TargetActors.GlowBlocks)
    {
        if (Actor && IsValid(Actor) && !Actor->Tags.Contains(FName("GlowBlock")) && Actor->Tags.Contains(FName("ToGlow")))
        {
            Actor->Tags.Add(FName("GlowBlock"));
            UStaticMeshComponent* ActorMesh = Actor->FindComponentByClass<UStaticMeshComponent>();
            if (ActorMesh)
            {
                // Get the Material from the Static Mesh Component
                UMaterialInterface* ActorMaterial = ActorMesh->GetMaterial(0); // Index 0 for the first material
                if (ActorMaterial)
                {
                    if (!ActorMaterial->GetName().Contains("MaterialInstanceDynamic"))
                    {
                        UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(ActorMaterial, this);
                        if (DynamicMaterial)
                        {
                            ActorMesh->SetMaterial(0, DynamicMaterial);
                            GlowMaterials.Add(DynamicMaterial);
                        }
                    }
                }
            }
        }
    }

    // Initialize scaling and movement animation parameters
    if (TargetActors.GlowBlocks.Num() > 2)
    {
        InitialScales.Empty();
        InitialLocations.Empty();
        FinalLocations.Empty();

        for (AActor* TargetActor : TargetActors.GlowBlocks)
        {
            if (TargetActor)
            {
                InitialScales.Add(TargetActor->GetActorScale3D());
                InitialLocations.Add(TargetActor->GetActorLocation());
                if (TargetActor == TargetActors.GlowBlocks[0])
                {
                    FinalLocations.Add(TargetActor->GetActorLocation());
                }
                else
                {
                    FinalLocations.Add(FMath::VInterpTo(TargetActor->GetActorLocation(), InitialLocations[0], 1.0f, 100.0f));
                }
            }
        }
    }

    // Start glow and animation timer
    if (GlowMaterials.Num() > 0)
    {
        GlowFactorStart = 0.0f;
        GlowFactorEnd = 1.0f;
        GlowPowerStart = 1.0f;
        GlowPowerEnd = 5.0f;
        AnimationDuration = 0.25f; // Duration in seconds

        GetWorldTimerManager().SetTimer(GlowTimerHandle, this, &ATetrisGrid::UpdateGlowMaterial, 0.01f, true);
    }
}

void ATetrisGrid::GlowSuperDuperBlocks()
{
    if (TargetActors.GlowBlocks.Num() < 4) return; // Ensure we have at least 3 actors

    for (AActor* Actor : TargetActors.GlowBlocks)
    {
        if (Actor && IsValid(Actor) && !Actor->Tags.Contains(FName("GlowBlock")) && Actor->Tags.Contains(FName("ToGlow")))
        {
            Actor->Tags.Add(FName("GlowBlock"));
            UStaticMeshComponent* ActorMesh = Actor->FindComponentByClass<UStaticMeshComponent>();
            if (ActorMesh)
            {
                // Get the Material from the Static Mesh Component
                UMaterialInterface* ActorMaterial = ActorMesh->GetMaterial(0); // Index 0 for the first material
                if (ActorMaterial)
                {
                    if (!ActorMaterial->GetName().Contains("MaterialInstanceDynamic"))
                    {
                        UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(ActorMaterial, this);
                        if (DynamicMaterial)
                        {
                            ActorMesh->SetMaterial(0, DynamicMaterial);
                            GlowMaterials.Add(DynamicMaterial);
                        }
                    }
                }
            }
        }
    }

    // Initialize scaling and movement animation parameters
    if (TargetActors.GlowBlocks.Num() > 2)
    {
        InitialScales.Empty();
        InitialLocations.Empty();
        FinalLocations.Empty();

        for (AActor* TargetActor : TargetActors.GlowBlocks)
        {
            if (TargetActor)
            {
                InitialScales.Add(TargetActor->GetActorScale3D());
                InitialLocations.Add(TargetActor->GetActorLocation());
                if (TargetActor == TargetActors.GlowBlocks[0])
                {
                    FinalLocations.Add(TargetActor->GetActorLocation());
                }
                else
                {
                    FinalLocations.Add(FMath::VInterpTo(TargetActor->GetActorLocation(), InitialLocations[0], 1.0f, 100.0f));
                }
            }
        }
    }

    // Start glow and animation timer
    if (GlowMaterials.Num() > 0)
    {
        GlowFactorStart = 0.0f;
        GlowFactorEnd = 1.0f;
        GlowPowerStart = 1.0f;
        GlowPowerEnd = 5.0f;
        AnimationDuration = 0.25f; // Duration in seconds

        GetWorldTimerManager().SetTimer(GlowTimerHandle, this, &ATetrisGrid::UpdateSuperDuperGlowMaterial, 0.01f, true);
    }
}

void ATetrisGrid::UpdateGlowMaterial()
{
    if (GlowMaterials.Num() > 0 && TargetActors.GlowBlocks.Num() > 2)
    {
        // Increment elapsed time
        TargetActors.ElapsedTime += 0.01f; // Increment matches the timer interval

        // Calculate alpha for Lerp (clamped between 0.0 and 1.0)
        float Alpha = FMath::Clamp(TargetActors.ElapsedTime / AnimationDuration, 0.0f, 1.0f);

        // Update glow parameters
        for (UMaterialInstanceDynamic* GlowMaterial : GlowMaterials)
        {
            float GlowFactor = FMath::Lerp(GlowFactorStart, GlowFactorEnd, Alpha);
            float GlowPower = FMath::Lerp(GlowPowerStart, GlowPowerEnd, Alpha);
            GlowMaterial->SetScalarParameterValue(TEXT("GlowFactor"), GlowFactor);
            GlowMaterial->SetScalarParameterValue(TEXT("GlowPower"), GlowPower);
        }

        // Update scales and locations
        for (int32 i = 0; i < TargetActors.GlowBlocks.Num(); ++i)
        {
            if (TargetActors.GlowBlocks[i])
            {
                // Scale
                FVector CurrentScale = FMath::Lerp(InitialScales[i], FinalScale, Alpha);
                TargetActors.GlowBlocks[i]->SetActorScale3D(CurrentScale);

                // Location (only move Actor2 and Actor3)
                if (i > 0) // Skip Actor1
                {
                    FVector CurrentLocation = FMath::Lerp(InitialLocations[i], FinalLocations[i], Alpha);
                    TargetActors.GlowBlocks[i]->SetActorLocation(CurrentLocation);
                }
            }
        }

        // Stop the timer when animation completes
        if (Alpha >= 1.0f)
        {
            GlowMaterials.Empty();

            // for (AActor* GlowBlockActor : TargetActors.GlowBlocks)
            for (int32 g = 0; g < TargetActors.GlowBlocks.Num(); ++g)
            {
                DestroyBlockAtLocation(TargetActors.SuperBlockDropSpots[g]);
                UpdateGridAtLocation(TargetActors.SuperBlockDropSpots[g]);
            }

            TargetActors.GlowBlocks.Empty();
            MakeSuperBlock();
            GetWorldTimerManager().ClearTimer(GlowTimerHandle);
        }
    }
}

void ATetrisGrid::UpdateSuperDuperGlowMaterial()
{
    if (GlowMaterials.Num() > 0 && TargetActors.GlowBlocks.Num() > 2)
    {
        // Increment elapsed time
        TargetActors.ElapsedTime += 0.01f; // Increment matches the timer interval

        // Calculate alpha for Lerp (clamped between 0.0 and 1.0)
        float Alpha = FMath::Clamp(TargetActors.ElapsedTime / AnimationDuration, 0.0f, 1.0f);

        // Update glow parameters
        for (UMaterialInstanceDynamic* GlowMaterial : GlowMaterials)
        {
            float GlowFactor = FMath::Lerp(GlowFactorStart, GlowFactorEnd, Alpha);
            float GlowPower = FMath::Lerp(GlowPowerStart, GlowPowerEnd, Alpha);
            GlowMaterial->SetScalarParameterValue(TEXT("GlowFactor"), GlowFactor);
            GlowMaterial->SetScalarParameterValue(TEXT("GlowPower"), GlowPower);
        }

        // Update scales and locations
        for (int32 i = 0; i < TargetActors.GlowBlocks.Num(); ++i)
        {
            if (TargetActors.GlowBlocks[i])
            {
                // Scale
                FVector CurrentScale = FMath::Lerp(InitialScales[i], FinalScale, Alpha);
                TargetActors.GlowBlocks[i]->SetActorScale3D(CurrentScale);

                // Location (only move Actor2 and Actor3)
                if (i > 0) // Skip Actor1
                {
                    FVector CurrentLocation = FMath::Lerp(InitialLocations[i], FinalLocations[i], Alpha);
                    TargetActors.GlowBlocks[i]->SetActorLocation(CurrentLocation);
                }
            }
        }

        // Stop the timer when animation completes
        if (Alpha >= 1.0f)
        {
            GlowMaterials.Empty();

            // for (AActor* GlowBlockActor : TargetActors.GlowBlocks)
            for (int32 g = 0; g < TargetActors.GlowBlocks.Num(); ++g)
            {
                DestroyBlockAtLocation(TargetActors.SuperBlockDropSpots[g]);
                UpdateGridAtLocation(TargetActors.SuperBlockDropSpots[g]);
            }

            TargetActors.GlowBlocks.Empty();
            MakeSuperDuperBlock();
            GetWorldTimerManager().ClearTimer(GlowTimerHandle);
        }
    }
}

void ATetrisGrid::ScaleAndMoveActors(AActor* Actor1, AActor* Actor2, AActor* Actor3)
{
    if (Actor1 && Actor2 && Actor3)
    {
        // Scale all three actors down by 50%
        const FVector ScaleFactor = FVector(0.5f, 0.5f, 0.5f); // 50% reduction in scale
        Actor1->SetActorScale3D(Actor1->GetActorScale3D() * ScaleFactor);
        Actor2->SetActorScale3D(Actor2->GetActorScale3D() * ScaleFactor);
        Actor3->SetActorScale3D(Actor3->GetActorScale3D() * ScaleFactor);

        // Get the position of the first actor
        FVector Actor1Location = Actor1->GetActorLocation();

        // Move Actor2 100 units closer to Actor1
        FVector Actor2Location = Actor2->GetActorLocation();
        FVector Direction2 = (Actor1Location - Actor2Location).GetSafeNormal(); // Direction vector
        Actor2Location += Direction2 * 100.0f; // Move 100 units closer
        Actor2->SetActorLocation(Actor2Location);

        // Move Actor3 100 units closer to Actor1
        FVector Actor3Location = Actor3->GetActorLocation();
        FVector Direction3 = (Actor1Location - Actor3Location).GetSafeNormal(); // Direction vector
        Actor3Location += Direction3 * 100.0f; // Move 100 units closer
        Actor3->SetActorLocation(Actor3Location);
    }
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
    PrintScreen("running clear three rows");
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
                    if (Grid[x][y] != nullptr && IsValid(Grid[x][y]))
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
    if (!bIsClearing)
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
    CheckForBlocksToDrop();
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

void ATetrisGrid::PrintScreen(FString message, float showDuration)
{
    if (GEngine)
    {
        GEngine->AddOnScreenDebugMessage(-1, showDuration, FColor::Green, message);
    }
}

void ATetrisGrid::IncrementScore(int32 NewScore)
{
    int maxInt32 = std::numeric_limits<int>::max();

    Score = FMath::Clamp(NewScore, 0, maxInt32 - 1);

    if (Score >= CurrentLevel.TargetScore)
    {
		SetVictoryBoardMaterial(0.0f, CurrentLevel.BackgroundColor, 1.0f);

        GetWorld()->GetTimerManager().SetTimer(VictoryTimerHandle, this, &ATetrisGrid::BlinkBoardColors, 0.2f, true, 2.0f);

        ClearBoard();
    }

    OnUpdateScore.Broadcast();
}

void ATetrisGrid::BlinkBoardColors()
{
    GetWorld()->GetTimerManager().ClearTimer(VictoryTimerHandle);
    GetWorld()->GetTimerManager().SetTimer(BlinkBoardTimerHandle, this, &ATetrisGrid::Blink, 0.2f, true, 0.0f);
}

void ATetrisGrid::Blink()
{
    ElapsedBlinking += 0.2f;

    // Get all components (TSet<UActorComponent*>)
    const TSet<UActorComponent*>& Components = TetrisBoardInstance->GetComponents();
    const TArray<FString> TargetComponents = { "LeftBound", "RightBound", "Floor", "Ceiling" };

    if (CurrentColorPickerValue == 1.0f)
    {
        CurrentColorPickerValue = 0.0f;
    }
    else
    {
        CurrentColorPickerValue = 1.0f;
    }

    SetVictoryBoardMaterial(CurrentColorPickerValue, CurrentLevel.BackgroundColor, 1.0f);

    if (ElapsedBlinking >= BlinkDuration)
    {
        GetWorld()->GetTimerManager().ClearTimer(BlinkBoardTimerHandle);
        ElapsedBlinking = 0.0f;

        NextLevel();
    }
}

void ATetrisGrid::SetVictoryBoardMaterial(float ColorPickerValue, FVector BackgroundColor, float VictorySwitch)
{
    try {
        if (GlowMaterialForBoard != nullptr && IsValid(GlowMaterialForBoard))
        {
            GlowMaterialForBoard->SetScalarParameterValue(TEXT("ColorPicker"), ColorPickerValue);
            GlowMaterialForBoard->SetVectorParameterValue(TEXT("Color"), BackgroundColor);
            GlowMaterialForBoard->SetScalarParameterValue(TEXT("VictorySwitch"), VictorySwitch);
        }
        else
        {
            PrintScreen("Victory material is null!");
        }

        UMaterialInstanceDynamic* BackgroundMaterial = BackgroundMaterialForBoard;
        if (BackgroundMaterial != nullptr && IsValid(BackgroundMaterial))
        {
            BackgroundMaterial->SetVectorParameterValue(TEXT("Color"), BackgroundColor);
        } // crashes here
        else
        {
            PrintScreen("Background material is null!");
        }
    }
	catch (const std::exception& e) {
		PrintScreen(FString::Printf(TEXT("Exception: %s"), *FString(e.what())));
	}
	catch (...)
	{
		PrintScreen("Unknown exception occurred!");
	}
}

void ATetrisGrid::ClearBoard()
{
    for (int32 x = 0; x < GridWidth; ++x)
    {
        for (int32 y = 0; y < GridHeight; ++y)
        {
            if (Grid[x][y] != nullptr)
            {
                Grid[x][y]->Destroy();
                Grid[x][y] = nullptr;
            }
        }
    }

    for (AActor* Block : CurrentTetrominoBlocks)
    {
        Block->Destroy();
    }
    CurrentTetrominoBlocks.Empty();

    for (AActor* NextBlock : NextTetrominoBlocks)
    {
        NextBlock->Destroy();
    }
    NextTetrominoBlocks.Empty();

    bIsClearing = true;
}

void ATetrisGrid::NextLevel()
{
    CurrentLevelIndex = FMath::Clamp(CurrentLevelIndex + 1, 0, LevelsDataTable.Num() - 1);
    CurrentLevel = LevelsDataTable[CurrentLevelIndex];
    DefaultFallInterval = CurrentLevel.FallingSpeed;
    bIsClearing = false;
    Score = 0;
    OnUpdateScore.Broadcast();

    RoundsLeftBeforeSecSpawn = RoundsBeforeSecSpawn;
	SetVictoryBoardMaterial(0.0f, CurrentLevel.BackgroundColor, 0.0f);
    CurrentColorPickerValue = 0.0f;
    PrepareNextTetromino();
    SpawnTetromino();
}

void ATetrisGrid::SpawnBombExplosion(AActor* Actor)
{
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

    FVector Token1Location = Actor->GetActorLocation();

    // Destroy high-value tokens and update grid
    DestroyBlockAtLocation(Token1Location);

    // Trigger explosion effects at adjacent blocks
    for (const FVector& Offset : ExplosionOffsets)
    {
        FVector ExplosionLocation1 = Token1Location + Offset;

        FVector2D GridLocation = FVector2D((ExplosionLocation1.X + 1000.0f) / 100.0f, ExplosionLocation1.Z / 100.0f);
        if (GridLocation.X >= 0 && GridLocation.X < GridWidth && GridLocation.Y >= 0 && GridLocation.Y < GridHeight)
        {
            if (Grid[GridLocation.X][GridLocation.Y] != nullptr)
            {
                if (!Grid[GridLocation.X][GridLocation.Y]->Tags.Contains(FName("BombBlock")))
                {
                    // Directly update the grid at the expected explosion locations
                    UpdateGridAtLocation(ExplosionLocation1);
                }
            }
        }

        APlayerController* PlayerController = GetWorld()->GetFirstPlayerController();
        if (PlayerController && CameraShakeClass.IsValid())
        {
            UClass* LoadedCameraShakeClass = CameraShakeClass.Get();
            PlayerController->ClientStartCameraShake(LoadedCameraShakeClass);
        }
        PrintScreen("SPLODE!");

        // SpawnNiagaraSystem(TEXT("/Game/VFX/NS_Explosion"), (Token1Location + Token2Location) / 2.0f, ExplosionColor1, ExplosionColor2);
    }
}
