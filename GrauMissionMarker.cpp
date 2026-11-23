#include "GrauMissionMarker.h"
#include "GrauAssets.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

AGrauMissionMarker::AGrauMissionMarker()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));

	Beam = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Beam"));
	Beam->SetupAttachment(RootComponent);
	Beam->SetStaticMesh(CylF.Object);
	Beam->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Beam->CastShadow = false;

	Ring = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Ring"));
	Ring->SetupAttachment(RootComponent);
	Ring->SetStaticMesh(CylF.Object);
	Ring->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Ring->CastShadow = false;
}

void AGrauMissionMarker::SetColor(const FLinearColor& Color, float RadiusM)
{
	BaseRadius = RadiusM;
	const float Height = 45.f;
	Beam->SetRelativeLocation(FVector(0.f, 0.f, Height * 50.f));
	Beam->SetRelativeScale3D(FVector(RadiusM * 2.f, RadiusM * 2.f, Height));
	Beam->SetMaterial(0, FGrauAssets::Glow(this, Color, 6.f));
	Ring->SetRelativeLocation(FVector(0.f, 0.f, 22.f));
	Ring->SetRelativeScale3D(FVector(RadiusM * 2.6f, RadiusM * 2.6f, 0.04f));
	Ring->SetMaterial(0, FGrauAssets::Glow(this, Color, 14.f));
}

void AGrauMissionMarker::SetShown(bool bShow)
{
	SetActorHiddenInGame(!bShow);
}

void AGrauMissionMarker::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const float T = GetWorld()->GetTimeSeconds();
	const float S = BaseRadius * 2.6f * (1.f + FMath::Sin(T * 2.4f) * 0.06f);
	Ring->SetRelativeScale3D(FVector(S, S, 0.04f));
}
