// RopeActor.cpp

#include "RopeActor.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"

DEFINE_LOG_CATEGORY(LogRope);

// ====================================
// CONSTRUCTOR
// ====================================

ARopeActor::ARopeActor()
{
	PrimaryActorTick.bCanEverTick = true;

	USceneComponent* Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	StartPointComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StartPoint"));
	StartPointComponent->SetupAttachment(RootComponent);
	StartPointComponent->SetRelativeLocation(FVector(0.f, 0.f, 0.f));

	EndPointComponent = CreateDefaultSubobject<USceneComponent>(TEXT("EndPoint"));
	EndPointComponent->SetupAttachment(RootComponent);
	EndPointComponent->SetRelativeLocation(FVector(0.f, 0.f, -200.f));

	RenderComponent = CreateDefaultSubobject<URopeRenderComponent>(TEXT("RenderComponent"));
	RenderComponent->SetupAttachment(RootComponent);

	SimulationComponent = CreateDefaultSubobject<URopeSimulationComponent>(TEXT("SimulationComponent"));
}

// ====================================
// BEGIN PLAY
// ====================================

void ARopeActor::BeginPlay()
{
	Super::BeginPlay();

	SyncEndpointsFromComponents();

	if (SimulationComponent)
	{
		SimulationComponent->InitializeSimulation(this);
	}

	if (!bInitialized)
	{
		Initialize(Start, End, Params);
	}
	else
	{
		LastFramePositions = Positions;
	}

	if (RenderComponent)
	{
		RenderComponent->Thickness = RopeThickness;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

// ====================================
// INITIALIZATION
// ====================================

void ARopeActor::Initialize(const FRopeEndPoint& InStart, const FRopeEndPoint& InEnd, const FRopeParams& InParams)
{
	Start = InStart;
	End = InEnd;
	Params = InParams;

	Params.SegmentCount = FMath::Max(1, Params.SegmentCount);

	RestLength = (InEnd.Location - InStart.Location).Size();
	if (RestLength < 1.f) RestLength = 1.f;

	const int32 NumPoints = Params.SegmentCount + 1;
	Positions.SetNum(NumPoints);
	LastFramePositions.SetNum(NumPoints);

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float Alpha = (NumPoints > 1) ? (float)i / (NumPoints - 1) : 0.f;
		const FVector Pos = FMath::Lerp(Start.Location, End.Location, Alpha);

		Positions[i] = Pos;
		LastFramePositions[i] = Pos;
	}

	bInitialized = true;

	UE_LOG(LogRope, Log, TEXT("ARopeActor::Initialize - RestLength=%.2f, Segments=%d"), RestLength, Params.SegmentCount);

	if (RenderComponent)
	{
		RenderComponent->Thickness = RopeThickness;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

// ====================================
// TICK
// ====================================

void ARopeActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	UWorld* World = GetWorld();
	if (!World) return;

#if WITH_EDITOR
	if (World->WorldType == EWorldType::Editor)
	{
		SyncEndpointsFromComponents();
		RebuildRopeGeometry();
		return;
	}
#endif

	if (!World->IsGameWorld() || World->IsPaused())
	{
		return;
	}

	if (!bInitialized || Positions.Num() < 2)
	{
		return;
	}

	SyncEndpointsFromComponents();
	UpdateAttachedEndpoints();

	if (SimulationComponent)
	{
		SimulationComponent->Simulate(DeltaTime);
	}

	if (RenderComponent)
	{
		RenderComponent->Thickness = RopeThickness;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

// ====================================
// ON CONSTRUCTION
// ====================================

void ARopeActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (bInitialized)
	{
		SyncComponentsFromEndpoints();
		if (RenderComponent)
		{
			RenderComponent->Thickness = RopeThickness;
			RenderComponent->UpdateRopeRender(Positions);
		}
		return;
	}

	SyncEndpointsFromComponents();
	RebuildRopeGeometry();
}

// ====================================
// SPAWN ROPE
// ====================================

ARopeActor* ARopeActor::SpawnRope(UObject* WorldContext, const FRopeEndPoint& StartPoint, const FRopeEndPoint& EndPoint, const FRopeParams& SpawnParams)
{
	if (!WorldContext)
	{
		UE_LOG(LogRope, Warning, TEXT("SpawnRope: WorldContext is null."));
		return nullptr;
	}

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContext, EGetWorldErrorMode::LogAndReturnNull);
	if (!World)
	{
		UE_LOG(LogRope, Error, TEXT("SpawnRope: Failed to get UWorld."));
		return nullptr;
	}

	FActorSpawnParameters SpawnInfo;
	SpawnInfo.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	ARopeActor* Rope = World->SpawnActor<ARopeActor>(ARopeActor::StaticClass(), FTransform::Identity, SpawnInfo);
	if (!Rope)
	{
		UE_LOG(LogRope, Error, TEXT("SpawnRope: Failed to spawn ARopeActor."));
		return nullptr;
	}

	Rope->Initialize(StartPoint, EndPoint, SpawnParams);
	return Rope;
}

// ====================================
// UPDATE ENDPOINTS
// ====================================

void ARopeActor::UpdateEndPoints(const FRopeEndPoint& NewStartPoint, const FRopeEndPoint& NewEndPoint)
{
	Start = NewStartPoint;
	End = NewEndPoint;

	SyncComponentsFromEndpoints();

	if (!bInitialized)
	{
		RebuildRopeGeometry();
		return;
	}

	Positions[0] = Start.Location;
	Positions.Last() = End.Location;

	LastFramePositions[0] = Start.Location;
	LastFramePositions.Last() = End.Location;

	if (RenderComponent)
	{
		RenderComponent->Thickness = RopeThickness;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

// ====================================
// BLUEPRINT HELPERS
// ====================================

void ARopeActor::AttachStartToActor(AActor* Actor, FName SocketName)
{
	if (!Actor)
	{
		UE_LOG(LogRope, Warning, TEXT("AttachStartToActor: Actor is null."));
		return;
	}

	Start.AttachedActor = Actor;
	Start.Socket = SocketName;

	if (SocketName != NAME_None)
	{
		if (const USceneComponent* RootComp = Actor->GetRootComponent())
		{
			Start.Location = RootComp->GetSocketLocation(SocketName);
		}
	}
	else
	{
		Start.Location = Actor->GetActorLocation();
	}

	SyncComponentsFromEndpoints();

	UE_LOG(LogRope, Log, TEXT("Start attached to %s (Socket: %s)"), *Actor->GetName(), *SocketName.ToString());
}

void ARopeActor::AttachEndToActor(AActor* Actor, FName SocketName)
{
	if (!Actor)
	{
		UE_LOG(LogRope, Warning, TEXT("AttachEndToActor: Actor is null."));
		return;
	}

	End.AttachedActor = Actor;
	End.Socket = SocketName;

	if (SocketName != NAME_None)
	{
		if (const USceneComponent* RootComp = Actor->GetRootComponent())
		{
			End.Location = RootComp->GetSocketLocation(SocketName);
		}
	}
	else
	{
		End.Location = Actor->GetActorLocation();
	}

	SyncComponentsFromEndpoints();

	UE_LOG(LogRope, Log, TEXT("End attached to %s (Socket: %s)"), *Actor->GetName(), *SocketName.ToString());
}

void ARopeActor::DetachStart()
{
	Start.AttachedActor = nullptr;
	Start.Socket = NAME_None;
	UE_LOG(LogRope, Log, TEXT("Start detached."));
}

void ARopeActor::DetachEnd()
{
	End.AttachedActor = nullptr;
	End.Socket = NAME_None;
	UE_LOG(LogRope, Log, TEXT("End detached."));
}

void ARopeActor::SetStartLocation(FVector NewLocation)
{
	Start.Location = NewLocation;
	SyncComponentsFromEndpoints();

	if (bInitialized)
	{
		Positions[0] = Start.Location;
		LastFramePositions[0] = Start.Location;
	}
}

void ARopeActor::SetEndLocation(FVector NewLocation)
{
	End.Location = NewLocation;
	SyncComponentsFromEndpoints();

	if (bInitialized)
	{
		Positions.Last() = End.Location;
		LastFramePositions.Last() = End.Location;
	}
}

// ====================================
// UPDATE ATTACHED ENDPOINTS
// ====================================

void ARopeActor::UpdateAttachedEndpoints()
{

	
	if (Start.AttachedActor.IsValid())
	{
		if (Start.Socket != NAME_None)
		{
			if (const USceneComponent* RootComp = Start.AttachedActor->GetRootComponent())
			{
				Start.Location = RootComp->GetSocketLocation(Start.Socket);
			}
		}
		else
		{
			Start.Location = Start.AttachedActor->GetActorLocation();
		}
	}

	if (!End.bLooseEnd && End.AttachedActor.IsValid())
	{
		if (End.Socket != NAME_None)
		{
			if (const USceneComponent* RootComp = End.AttachedActor->GetRootComponent())
				End.Location = RootComp->GetSocketLocation(End.Socket);
		}
		else
		{
			End.Location = End.AttachedActor->GetActorLocation();
		}

		Positions.Last() = End.Location;
	}
}


// ====================================
// SYNC FUNCTIONS
// ====================================

void ARopeActor::SyncEndpointsFromComponents()
{
	// STARTPOINT : toujours sync
	if (StartPointComponent)
	{
		Start.Location = StartPointComponent->GetComponentLocation();
	}

	// ENDPOINT : sync SEULEMENT si NON loose
	if (EndPointComponent && !End.bLooseEnd)
	{
		End.Location = EndPointComponent->GetComponentLocation();
	}

	// ANCHORS SI INITIALISÉ
	if (bInitialized)
	{
		// Start toujours anchor
		Positions[0] = Start.Location;

		// End anchor seulement si pas LooseEnd
		if (!End.bLooseEnd)
		{
			Positions.Last() = End.Location;
		}
	}
}


void ARopeActor::SyncComponentsFromEndpoints()
{
	if (StartPointComponent)
	{
		StartPointComponent->SetWorldLocation(Start.Location);
	}
	if (EndPointComponent)
	{
		EndPointComponent->SetWorldLocation(End.Location);
	}
}

void ARopeActor::RebuildRopeGeometry()
{
	const int32 SegmentCount = FMath::Max(1, Params.SegmentCount);
	const int32 NumPoints = SegmentCount + 1;

	Positions.SetNum(NumPoints);
	LastFramePositions.SetNum(NumPoints);

	const FVector A = Start.Location;
	const FVector B = End.Location;

	for (int32 i = 0; i < NumPoints; ++i)
	{
		const float Alpha = (NumPoints > 1) ? (float)i / (NumPoints - 1) : 0.f;
		const FVector P = FMath::Lerp(A, B, Alpha);

		Positions[i] = P;
		LastFramePositions[i] = P;
	}

	if (RenderComponent)
	{
		RenderComponent->Thickness = RopeThickness;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

// ====================================
// WORLD OFFSET
// ====================================

void ARopeActor::ApplyWorldOffset(const FVector& InOffset, bool bWorldShift)
{
	Super::ApplyWorldOffset(InOffset, bWorldShift);

	for (int32 i = 0; i < Positions.Num(); ++i)
	{
		Positions[i] += InOffset;
		LastFramePositions[i] += InOffset;
	}

	Start.Location += InOffset;
	End.Location += InOffset;

	if (RenderComponent)
	{
		RenderComponent->Thickness = RopeThickness;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

// ====================================
// GETTERS
// ====================================

float ARopeActor::GetCurrentRopeLength() const
{
	if (Positions.Num() < 2) return 0.f;

	float TotalLength = 0.f;
	for (int32 i = 0; i < Positions.Num() - 1; ++i)
	{
		TotalLength += (Positions[i + 1] - Positions[i]).Size();
	}
	return TotalLength;
}

float ARopeActor::GetStretchRatio() const
{
	if (RestLength < 1.f) return 1.f;
	return GetCurrentRopeLength() / RestLength;
}

// ====================================
// SIMULATION RUNTIME API
// ====================================

void ARopeActor::SetGravityScale(float NewGravityScale)
{
	Params.GravityScale = NewGravityScale;
}

void ARopeActor::SetDamping(float NewDamping)
{
	Params.Damping = FMath::Clamp(NewDamping, 0.f, 1.f);
}

void ARopeActor::SetElasticity(float NewElasticity)
{
	Params.Elasticity = FMath::Clamp(NewElasticity, 0.f, 1.f);
}

void ARopeActor::SetSegmentCount(int32 NewSegmentCount)
{
	Params.SegmentCount = FMath::Max(1, NewSegmentCount);
	if (!bInitialized)
	{
		RebuildRopeGeometry();
	}
}

void ARopeActor::SetConstraintIterations(int32 NewIterations)
{
	Params.ConstraintIterations = FMath::Clamp(NewIterations, 1, 20);
}

void ARopeActor::SetMaxStretchRatio(float NewRatio)
{
	Params.MaxStretchRatio = FMath::Clamp(NewRatio, 0.f, 3.f);
}

void ARopeActor::SetCollisionEnabled(bool bEnabled)
{
	Params.bEnableCollision = bEnabled;
}

void ARopeActor::SetCollisionRadius(float NewRadius)
{
	Params.CollisionRadius = FMath::Max(0.f, NewRadius);
}

void ARopeActor::SetCollisionChannel(ECollisionChannel NewChannel)
{
	Params.CollisionChannel = NewChannel;
}

void ARopeActor::SetCollisionIterations(int32 NewIterations)
{
	Params.CollisionIterations = FMath::Clamp(NewIterations, 1, 5);
}

void ARopeActor::SetRopeMesh(UStaticMesh* NewMesh)
{
	if (RenderComponent)
	{
		RenderComponent->RopeMesh = NewMesh;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

void ARopeActor::SetRopeThickness(float NewThickness)
{
	RopeThickness = FMath::Max(0.f, NewThickness);
	if (RenderComponent)
	{
		RenderComponent->Thickness = RopeThickness;
		RenderComponent->UpdateRopeRender(Positions);
	}
}

void ARopeActor::RebuildRope()
{
	RebuildRopeGeometry();
}

// ====================================
// EDITOR CALLBACKS
// ====================================

#if WITH_EDITOR
void ARopeActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	const FName PropertyName = PropertyChangedEvent.GetPropertyName();

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARopeActor, Start) ||
		PropertyName == GET_MEMBER_NAME_CHECKED(ARopeActor, End))
	{
		SyncComponentsFromEndpoints();
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARopeActor, Params))
	{
		if (!bInitialized)
		{
			RebuildRopeGeometry();
		}
	}

	if (PropertyName == GET_MEMBER_NAME_CHECKED(ARopeActor, RopeThickness))
	{
		if (RenderComponent)
		{
			RenderComponent->Thickness = RopeThickness;
			RenderComponent->UpdateRopeRender(Positions);
		}
	}
}

void ARopeActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	SyncEndpointsFromComponents();

	if (!bInitialized)
	{
		RebuildRopeGeometry();
	}
}
#endif
