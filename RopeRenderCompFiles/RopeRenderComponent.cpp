// RopeRenderComponent.cpp

#include "RopeRenderComponent.h"

URopeRenderComponent::URopeRenderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	ISMC = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("ISMC"));
	ISMC->SetupAttachment(this);
	ISMC->SetCastShadow(false);
	ISMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void URopeRenderComponent::BeginPlay()
{
	Super::BeginPlay();

	if (ISMC && !ISMC->IsRegistered())
	{
		ISMC->RegisterComponent();
	}

	EnsureMesh();
}

void URopeRenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void URopeRenderComponent::EnsureMesh()
{
	if (ISMC && RopeMesh)
	{
		ISMC->SetStaticMesh(RopeMesh);
	}
}

float URopeRenderComponent::GetMeshLength() const
{
	if (!ISMC || !ISMC->GetStaticMesh())
		return 25.f; // Default fallback

	const FBoxSphereBounds MeshBounds = ISMC->GetStaticMesh()->GetBounds();
	const FVector Extent = MeshBounds.BoxExtent;

	// Mesh aligned on X axis, length = Extent.X * 2
	return Extent.X * 2.f;
}

void URopeRenderComponent::UpdateRopeRender(const TArray<FVector>& Points)
{
	if (!ISMC || Points.Num() < 2)
		return;

	EnsureMesh();

	const float MeshLength = bOverrideMeshLength ? MeshLengthOverride : GetMeshLength();
	if (MeshLength < KINDA_SMALL_NUMBER)
		return;

	ISMC->ClearInstances();

	const float SafeThickness = FMath::Max(Thickness, 0.0f);

	for (int32 i = 0; i < Points.Num() - 1; ++i)
	{
		const FVector SegmentStart = Points[i];
		const FVector SegmentEnd = Points[i + 1];
		const FVector Delta = SegmentEnd - SegmentStart;
		const float SegmentLength = Delta.Size();

		if (SegmentLength < KINDA_SMALL_NUMBER)
			continue;

		const FVector Direction = Delta / SegmentLength;

		const FRotator Rotation = FRotationMatrix::MakeFromX(Direction).Rotator();
		const FVector Scale(SegmentLength / MeshLength, SafeThickness, SafeThickness);

		const FTransform T(Rotation, SegmentStart, Scale);

		ISMC->AddInstance(T, true);
	}

	ISMC->MarkRenderStateDirty();
}
