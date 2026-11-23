#include "GrauTraffic.h"
#include "GrauAssets.h"
#include "GrauBike.h"
#include "GrauCityGenerator.h"
#include "GrauScoreComponent.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	TWeakObjectPtr<AGrauTrafficManager> GTrafficInstance;
	const FVector2D CarHalf(2.2f, 0.95f); // meio comprimento / meia largura (m)
	const FLinearColor CarColors[] = {
		FLinearColor(0.8f, 0.8f, 0.78f), FLinearColor(0.5f, 0.52f, 0.55f), FLinearColor(0.015f, 0.016f, 0.02f), FLinearColor(0.35f, 0.02f, 0.02f),
		FLinearColor(0.02f, 0.07f, 0.25f), FLinearColor(0.12f, 0.13f, 0.15f), FLinearColor(0.7f, 0.66f, 0.55f), FLinearColor(0.04f, 0.12f, 0.06f) };
}

// ------------------------------------------------------------------ carro

AGrauTrafficCar::AGrauTrafficCar()
{
	PrimaryActorTick.bCanEverTick = false;
	Box = CreateDefaultSubobject<UBoxComponent>(TEXT("Box"));
	Box->InitBoxExtent(FVector(220.f, 95.f, 70.f));
	Box->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	RootComponent = Box;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	UStaticMesh* Cube = CubeF.Object;
	// coordenadas relativas ao centro da caixa (chão em z = -70)
	AddPart(Cube, FVector(0.f, 0.f, -12.f), FVector(430.f, 184.f, 62.f), 0);        // carroceria
	AddPart(Cube, FVector(-20.f, 0.f, 40.f), FVector(220.f, 162.f, 52.f), 1);       // vidros
	AddPart(Cube, FVector(-20.f, 0.f, 68.f), FVector(185.f, 150.f, 6.f), 0);        // teto
	AddPart(Cube, FVector(215.f, 0.f, -20.f), FVector(4.f, 70.f, 16.f), 2);         // grade
	AddPart(Cube, FVector(0.f, 0.f, -8.f), FVector(330.f, 186.f, 10.f), 6);         // faixa (táxi)
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddPart(Cube, FVector(214.f, S * 62.f, 0.f), FVector(5.f, 42.f, 13.f), 3);  // farol
		AddPart(Cube, FVector(-214.f, S * 60.f, 6.f), FVector(5.f, 44.f, 12.f), 4); // lanterna
		for (int32 F = -1; F <= 1; F += 2)
		{
			UStaticMeshComponent* W = AddPart(CylF.Object, FVector(F * 135.f, S * 82.f, -37.f), FVector(66.f, 66.f, 24.f), 2);
			W->SetRelativeRotation(FRotator(0.f, 0.f, 90.f));
		}
	}

	OverrideMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("OverrideMesh"));
	OverrideMeshComp->SetupAttachment(Box);
	OverrideMeshComp->SetRelativeLocation(FVector(0.f, 0.f, -70.f));
	OverrideMeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Beam = CreateDefaultSubobject<USpotLightComponent>(TEXT("Beam"));
	Beam->SetupAttachment(Box);
	Beam->SetRelativeLocation(FVector(225.f, 0.f, 0.f));
	Beam->SetRelativeRotation(FRotator(-8.f, 0.f, 0.f));
	Beam->IntensityUnits = ELightUnits::Candelas;
	Beam->Intensity = 4000.f;
	Beam->AttenuationRadius = 3500.f;
	Beam->InnerConeAngle = 14.f;
	Beam->OuterConeAngle = 36.f;
	Beam->LightColor = FColor(255, 240, 215);
	Beam->CastShadows = false;
}

UStaticMeshComponent* AGrauTrafficCar::AddPart(UStaticMesh* Mesh, const FVector& LocCm, const FVector& SizeCm, uint8 Kind)
{
	const FName Name(*FString::Printf(TEXT("Car_%02d"), Parts.Num()));
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Box);
	C->SetStaticMesh(Mesh);
	C->SetRelativeLocation(LocCm);
	C->SetRelativeScale3D(SizeCm / 100.f);
	C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Parts.Add(C);
	Kinds.Add(Kind);
	return C;
}

void AGrauTrafficCar::SetupLook(FRandomStream& Rng, UStaticMesh* OverrideMesh)
{
	const bool bTaxi = Rng.FRand() < 0.15f;
	const FLinearColor Paint = bTaxi ? FLinearColor(0.9f, 0.6f, 0.02f) : CarColors[Rng.RandRange(0, (int32)UE_ARRAY_COUNT(CarColors) - 1)];
	UMaterialInstanceDynamic* K[7];
	K[0] = FGrauAssets::Paint(this, Paint, 0.5f, 0.3f);
	K[1] = FGrauAssets::Surface(this, FLinearColor(0.01f, 0.012f, 0.015f), 0.06f, 0.9f);
	K[2] = FGrauAssets::Surface(this, FLinearColor(0.012f, 0.012f, 0.013f), 0.7f);
	K[3] = FGrauAssets::Emissive(this, FLinearColor(1.f, 0.95f, 0.85f), 60.f);
	K[4] = FGrauAssets::Emissive(this, FLinearColor(1.f, 0.04f, 0.02f), 25.f);
	K[5] = FGrauAssets::Surface(this, FLinearColor(0.6f, 0.6f, 0.62f), 0.3f, 1.f);
	K[6] = FGrauAssets::Paint(this, bTaxi ? FLinearColor(0.02f, 0.08f, 0.4f) : Paint, 0.5f, 0.3f);
	Mids.Reset();
	for (UMaterialInstanceDynamic* M : K)
	{
		Mids.Add(M);
	}
	for (int32 I = 0; I < Parts.Num(); ++I)
	{
		if (K[Kinds[I]]) Parts[I]->SetMaterial(0, K[Kinds[I]]);
		Parts[I]->SetVisibility(OverrideMesh == nullptr);
	}
	OverrideMeshComp->SetStaticMesh(OverrideMesh);
	Cruise = 11.f + Rng.FRand() * 5.f;
}

// ------------------------------------------------------------------ gerente do trânsito

AGrauTrafficManager::AGrauTrafficManager()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Rng.Initialize(555);
}

AGrauTrafficManager* AGrauTrafficManager::Get(const UWorld* World)
{
	AGrauTrafficManager* M = GTrafficInstance.Get();
	return (M && M->GetWorld() == World) ? M : nullptr;
}

void AGrauTrafficManager::BeginPlay()
{
	Super::BeginPlay();
	GTrafficInstance = this;
	for (TActorIterator<AGrauCityGenerator> It(GetWorld()); It; ++It)
	{
		City = *It;
		break;
	}
	SpawnCars();
}

void AGrauTrafficManager::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GTrafficInstance.Get() == this)
	{
		GTrafficInstance.Reset();
	}
	Super::EndPlay(Reason);
}

void AGrauTrafficManager::ComputeWaypoints(AGrauTrafficCar* Car) const
{
	const AGrauCityGenerator* C = City.Get();
	const FVector2D A = C->Nodes[Car->From].P;
	const FVector2D B = C->Nodes[Car->To].P;
	const FVector2D D = (B - A).GetSafeNormal();
	const FVector2D Right(-D.Y, D.X); // mão direita (Brasil)
	const float Off = Car->HW >= 9.f ? (Car->Lane ? 6.6f : 2.6f) : (Car->Lane ? 5.6f : 2.0f);
	const float Inset = Car->HW >= 9.f ? 13.f : 12.f;
	Car->Start = A + D * Inset + Right * Off;
	Car->End = B - D * Inset + Right * Off;
	Car->Dir = D;
}

void AGrauTrafficManager::PickEdge(AGrauTrafficCar* Car, int32 FromNode, int32 Avoid)
{
	const FGrauRoadNode& N = City->Nodes[FromNode];
	TArray<int32> Options;
	for (int32 I = 0; I < N.Links.Num(); ++I)
	{
		if (N.Links[I] != Avoid) Options.Add(I);
	}
	if (Options.Num() == 0)
	{
		for (int32 I = 0; I < N.Links.Num(); ++I) Options.Add(I);
	}
	const int32 Pick = Options[Rng.RandRange(0, Options.Num() - 1)];
	Car->From = FromNode;
	Car->To = N.Links[Pick];
	Car->HW = N.HalfWidth[Pick];
	Car->Lane = Rng.FRand() < 0.5f ? 1 : 0;
	ComputeWaypoints(Car);
}

void AGrauTrafficManager::SpawnCars()
{
	AGrauCityGenerator* C = City.Get();
	UWorld* World = GetWorld();
	if (!C || !World || C->Nodes.Num() == 0)
	{
		return;
	}
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (int32 I = 0; I < CarCount; ++I)
	{
		AGrauTrafficCar* Car = World->SpawnActor<AGrauTrafficCar>(AGrauTrafficCar::StaticClass(), FTransform::Identity, Params);
		if (!Car) continue;
		UStaticMesh* Mesh = CarMeshes.Num() ? CarMeshes[Rng.RandRange(0, CarMeshes.Num() - 1)] : nullptr;
		Car->SetupLook(Rng, Mesh);
		for (int32 Try = 0; Try < 30; ++Try)
		{
			PickEdge(Car, Rng.RandRange(0, C->Nodes.Num() - 1), -1);
			Car->Pos = FMath::Lerp(Car->Start, Car->End, 0.15f + Rng.FRand() * 0.7f);
			bool bFree = true;
			for (const AGrauTrafficCar* O : Cars)
			{
				if (FVector2D::Distance(O->Pos, Car->Pos) < 25.f) { bFree = false; break; }
			}
			if (bFree) break;
		}
		Car->Yaw = FMath::Atan2(Car->Dir.Y, Car->Dir.X);
		Car->Speed = Car->Cruise;
		Cars.Add(Car);
	}
}

void AGrauTrafficManager::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!City.IsValid())
	{
		return;
	}
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);
	const AGrauBike* Bike = Cast<AGrauBike>(UGameplayStatics::GetPlayerPawn(this, 0));
	const FVector2D BikePos = Bike ? FVector2D(Bike->RearContact().X, Bike->RearContact().Y) / 100.f : FVector2D(1e9f, 1e9f);

	for (AGrauTrafficCar* Car : Cars)
	{
		const float Along = FVector2D::DotProduct(Car->End - Car->Pos, Car->Dir);
		if (Along < 2.f)
		{
			PickEdge(Car, Car->To, Car->From);
		}
		const float AlongNow = FVector2D::DotProduct(Car->End - Car->Pos, Car->Dir);
		const FVector2D Target = AlongNow < 18.f ? Car->End + Car->Dir * 8.f : Car->End;
		float Aim = FMath::Atan2(Target.Y - Car->Pos.Y, Target.X - Car->Pos.X);
		if (FVector2D::DotProduct(Car->Pos - Car->Start, Car->Dir) < 0.f)
		{
			const FVector2D T = Car->Start + Car->Dir * 6.f;
			Aim = FMath::Atan2(T.Y - Car->Pos.Y, T.X - Car->Pos.X);
		}
		const float DY = FMath::FindDeltaAngleRadians(Car->Yaw, Aim);
		const float MaxTurn = 1.4f * Dt * FMath::Max(0.3f, Car->Speed / 10.f);
		Car->Yaw += FMath::Clamp(DY, -MaxTurn, MaxTurn);
		float Want = Car->Cruise * (FMath::Abs(DY) > 0.3f ? 0.55f : 1.f);
		const FVector2D F(FMath::Cos(Car->Yaw), FMath::Sin(Car->Yaw));
		auto Brake = [&](const FVector2D& Other, float Gap, float K)
		{
			const FVector2D D = Other - Car->Pos;
			const float Fw = FVector2D::DotProduct(D, F);
			const float Lat = FMath::Abs(D.X * F.Y - D.Y * F.X);
			if (Fw > 0.f && Fw < 16.f && Lat < 2.4f) Want = FMath::Min(Want, FMath::Max(0.f, (Fw - Gap) * K));
		};
		for (const AGrauTrafficCar* O : Cars)
		{
			if (O != Car) Brake(O->Pos, 6.f, 0.9f);
		}
		if (Bike && !Bike->bCrashed) Brake(BikePos, 5.f, 0.8f);
		Car->Speed += FMath::Clamp(Want - Car->Speed, -9.f * Dt, 3.5f * Dt);
		Car->Pos += F * Car->Speed * Dt;
		Car->SetActorLocationAndRotation(FVector(Car->Pos.X * 100.f, Car->Pos.Y * 100.f, 70.f), FRotator(0.f, FMath::RadiansToDegrees(Car->Yaw), 0.f));
		Car->NearT -= Dt;

		// tira fina: passou raspando acima de 55 km/h
		if (Bike && !Bike->bCrashed && Bike->KmH() > 55.f && Car->NearT <= 0.f && Bike->Score)
		{
			const float D = FVector2D::Distance(BikePos, Car->Pos);
			if (D < 3.1f && D > 1.2f)
			{
				Car->NearT = 4.f;
				Bike->Score->AddNearMiss(Bike->KmH());
			}
		}
	}
}

bool AGrauTrafficManager::HitBike(const FVector& RearCm, const FVector& Fwd, float SpeedMS, FGrauCarHit& Out) const
{
	const float R = 0.45f;
	const FVector2D Bc = FVector2D(RearCm.X, RearCm.Y) / 100.f + FVector2D(Fwd.X, Fwd.Y) * 0.75f;
	for (const AGrauTrafficCar* Car : Cars)
	{
		const FVector2D D = Bc - Car->Pos;
		if (D.SizeSquared() > 30.f) continue;
		const float C = FMath::Cos(Car->Yaw), S = FMath::Sin(Car->Yaw);
		const float Lx = D.X * C + D.Y * S;
		const float Ly = -D.X * S + D.Y * C;
		const float Px = FMath::Clamp(Lx, -CarHalf.X, CarHalf.X);
		const float Py = FMath::Clamp(Ly, -CarHalf.Y, CarHalf.Y);
		const float Ex = Lx - Px, Ey = Ly - Py;
		const float Dist = FMath::Sqrt(Ex * Ex + Ey * Ey);
		if (Dist >= R) continue;
		float Nlx = Ex, Nly = Ey;
		if (Dist < 1e-5f)
		{
			if (CarHalf.Y - FMath::Abs(Ly) < CarHalf.X - FMath::Abs(Lx)) { Nlx = 0.f; Nly = FMath::Sign(Ly); }
			else { Nlx = FMath::Sign(Lx); Nly = 0.f; }
		}
		const float Nl = FMath::Max(FMath::Sqrt(Nlx * Nlx + Nly * Nly), 1e-5f);
		Nlx /= Nl; Nly /= Nl;
		const FVector2D N(Nlx * C - Nly * S, Nlx * S + Nly * C);
		const FVector2D Rel = FVector2D(Fwd.X, Fwd.Y) * SpeedMS - FVector2D(C, S) * Car->Speed;
		Out.Impact = -FVector2D::DotProduct(Rel, N);
		Out.Normal = N;
		Out.PenCm = (R - Dist) * 100.f + 2.f;
		return true;
	}
	return false;
}
