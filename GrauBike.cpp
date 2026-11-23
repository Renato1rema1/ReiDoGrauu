#include "GrauBike.h"
#include "ReiDoGrau.h"
#include "GrauAssets.h"
#include "GrauEngineSynth.h"
#include "GrauScoreComponent.h"
#include "GrauTraffic.h"
#include "GrauCityGenerator.h"
#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/SpotLightComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "Camera/CameraComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Animation/AnimationAsset.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "UObject/ConstructorHelpers.h"

namespace
{
	constexpr float G = 9.81f;
	constexpr float GearMax[7] = { 0.f, 29.f, 40.f, 50.f, 60.f, 71.f, 86.f };   // m/s no corte de cada marcha
	constexpr float GearForce[7] = { 0.f, 2650.f, 2080.f, 1720.f, 1460.f, 1260.f, 1090.f };
	constexpr float Idle = 1400.f;
	constexpr float Redline = 14000.f;
	// centro de massa visto do contato traseiro (grau) e do dianteiro (stoppie)
	constexpr float LR = 1.0f, AR = 0.5f;
	constexpr float LF = 1.0f, AF = 0.56f;
	constexpr float WheelbaseM = 1.40f;
	constexpr float WheelRadiusM = 0.31f;
	const float TailAngle = FMath::Atan2(0.92f, 0.26f); // ~74°: rabeta encosta no chão
	constexpr float BoxForward = 70.f;   // centro da caixa de colisão à frente do contato traseiro (cm)
	constexpr float BoxUp = 85.f;        // ...e acima do chão (a base fica a 45 cm, passa por cima de meio-fio)
	constexpr float RakeDeg = 21.4f;
	const FVector BikeBodyOffset(70.f, 0.f, 55.f);
	const FVector RiderBodyOffset(58.f, 0.f, 112.f);

	float Approach(float V, float T, float Rate, float Dt)
	{
		return V < T ? FMath::Min(T, V + Rate * Dt) : FMath::Max(T, V - Rate * Dt);
	}
	float Damp(float A, float B, float Lambda, float Dt)
	{
		return FMath::Lerp(A, B, 1.f - FMath::Exp(-Lambda * Dt));
	}
	float DampAngleDeg(float A, float B, float Lambda, float Dt)
	{
		return A + FMath::FindDeltaAngleDegrees(A, B) * (1.f - FMath::Exp(-Lambda * Dt));
	}
	float Smooth(float E0, float E1, float V)
	{
		const float T = FMath::Clamp((V - E0) / (E1 - E0), 0.f, 1.f);
		return T * T * (3.f - 2.f * T);
	}
}

AGrauBike::AGrauBike()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;

	Collision = CreateDefaultSubobject<UBoxComponent>(TEXT("Collision"));
	Collision->InitBoxExtent(FVector(95.f, 30.f, 40.f));
	Collision->SetCollisionProfileName(UCollisionProfile::Pawn_ProfileName);
	Collision->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Collision->SetCollisionResponseToChannel(ECC_Visibility, ECR_Ignore);
	RootComponent = Collision;

	LeanPivot = CreateDefaultSubobject<USceneComponent>(TEXT("LeanPivot"));
	LeanPivot->SetupAttachment(Collision);
	LeanPivot->SetRelativeLocation(FVector(-BoxForward, 0.f, -BoxUp));

	WheeliePivot = CreateDefaultSubobject<USceneComponent>(TEXT("WheeliePivot"));
	WheeliePivot->SetupAttachment(LeanPivot);

	StoppiePivot = CreateDefaultSubobject<USceneComponent>(TEXT("StoppiePivot"));
	StoppiePivot->SetupAttachment(WheeliePivot);
	StoppiePivot->SetRelativeLocation(FVector(WheelbaseM * 100.f, 0.f, 0.f));

	ModelRoot = CreateDefaultSubobject<USceneComponent>(TEXT("ModelRoot"));
	ModelRoot->SetupAttachment(StoppiePivot);
	ModelRoot->SetRelativeLocation(FVector(-WheelbaseM * 100.f, 0.f, 0.f));

	BikeBody = CreateDefaultSubobject<UBoxComponent>(TEXT("BikeBody"));
	BikeBody->SetupAttachment(ModelRoot);
	BikeBody->SetRelativeLocation(BikeBodyOffset);
	BikeBody->InitBoxExtent(FVector(95.f, 20.f, 40.f));
	BikeBody->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	RiderBody = CreateDefaultSubobject<UBoxComponent>(TEXT("RiderBody"));
	RiderBody->SetupAttachment(ModelRoot);
	RiderBody->SetRelativeLocation(RiderBodyOffset);
	RiderBody->InitBoxExtent(FVector(35.f, 22.f, 40.f));
	RiderBody->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	RearWheel = CreateDefaultSubobject<USceneComponent>(TEXT("RearWheel"));
	RearWheel->SetupAttachment(BikeBody);
	RearWheel->SetRelativeLocation(FVector(0.f, 0.f, WheelRadiusM * 100.f) - BikeBodyOffset);

	FrontSteer = CreateDefaultSubobject<USceneComponent>(TEXT("FrontSteer"));
	FrontSteer->SetupAttachment(BikeBody);
	FrontSteer->SetRelativeLocation(FVector(113.f, 0.f, 100.f) - BikeBodyOffset);
	FrontSteer->SetRelativeRotation(FRotator(RakeDeg, 0.f, 0.f));

	SteerYaw = CreateDefaultSubobject<USceneComponent>(TEXT("SteerYaw"));
	SteerYaw->SetupAttachment(FrontSteer);

	FrontWheel = CreateDefaultSubobject<USceneComponent>(TEXT("FrontWheel"));
	FrontWheel->SetupAttachment(SteerYaw);
	FrontWheel->SetRelativeLocation(FVector(0.f, 0.f, -74.1f));
	FrontWheel->SetRelativeRotation(FRotator(-RakeDeg, 0.f, 0.f));

	BikeMeshComp = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("BikeMesh"));
	BikeMeshComp->SetupAttachment(BikeBody);
	BikeMeshComp->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	BikeMeshComp->SetRelativeLocation(-BikeBodyOffset);

	RiderSkeletal = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("RiderSkeletal"));
	RiderSkeletal->SetupAttachment(RiderBody);
	RiderSkeletal->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);

	Headlight = CreateDefaultSubobject<USpotLightComponent>(TEXT("Headlight"));
	Headlight->SetupAttachment(BikeBody);
	Headlight->SetRelativeLocation(FVector(152.f, 0.f, 86.f) - BikeBodyOffset);
	Headlight->SetRelativeRotation(FRotator(-7.f, 0.f, 0.f));
	Headlight->IntensityUnits = ELightUnits::Candelas;
	Headlight->Intensity = 9000.f;
	Headlight->AttenuationRadius = 7000.f;
	Headlight->InnerConeAngle = 10.f;
	Headlight->OuterConeAngle = 30.f;
	Headlight->LightColor = FColor(255, 240, 220);
	Headlight->CastShadows = true;

	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm"));
	SpringArm->SetupAttachment(Collision);
	SpringArm->SetUsingAbsoluteRotation(true);
	SpringArm->TargetArmLength = 460.f;
	SpringArm->bDoCollisionTest = true;
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 12.f;
	SpringArm->bUsePawnControlRotation = false;

	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(SpringArm, USpringArmComponent::SocketName);
	Camera->FieldOfView = 80.f;

	EngineSound = CreateDefaultSubobject<UGrauEngineSynth>(TEXT("EngineSound"));
	EngineSound->SetupAttachment(Collision);

	Score = CreateDefaultSubobject<UGrauScoreComponent>(TEXT("Score"));

	BuildPlaceholder();
}

UStaticMeshComponent* AGrauBike::AddPart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& LocCm, const FRotator& Rot, const FVector& SizeCm, uint8 Kind)
{
	const FName Name(*FString::Printf(TEXT("Part_%02d"), Parts.Num()));
	UStaticMeshComponent* C = CreateDefaultSubobject<UStaticMeshComponent>(Name);
	C->SetupAttachment(Parent);
	C->SetStaticMesh(Mesh);
	C->SetRelativeLocation(LocCm);
	C->SetRelativeRotation(Rot);
	C->SetRelativeScale3D(SizeCm / 100.f);
	C->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	C->SetGenerateOverlapEvents(false);
	C->bCastDynamicShadow = true;
	FGrauPart P;
	P.Comp = C;
	P.Kind = Kind;
	Parts.Add(P);
	return C;
}

void AGrauBike::BuildPlaceholder()
{
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	UStaticMesh* Cube = CubeF.Object;
	UStaticMesh* Cyl = CylF.Object;
	UStaticMesh* Sph = SphF.Object;
	const FVector B = BikeBodyOffset;
	const FRotator AxisY(0.f, 0.f, 90.f); // cilindro deitado com eixo em Y (rodas)

	// rodas: pneu preto, aro laranja visível entre pneu e cubo, raios que giram
	for (int32 W = 0; W < 2; ++W)
	{
		USceneComponent* Wheel = W == 0 ? RearWheel : FrontWheel;
		AddPart(Wheel, Cyl, FVector::ZeroVector, AxisY, FVector(63.f, 63.f, W == 0 ? 20.f : 15.f), 4);
		AddPart(Wheel, Cyl, FVector::ZeroVector, AxisY, FVector(45.f, 45.f, W == 0 ? 20.6f : 15.6f), 1);
		AddPart(Wheel, Cyl, FVector::ZeroVector, AxisY, FVector(37.f, 37.f, W == 0 ? 21.f : 16.f), 2);
		for (int32 K = 0; K < 5; ++K)
		{
			const float A = K * 72.f;
			const FVector R = FRotator(-A, 0.f, 0.f).RotateVector(FVector(0.f, 0.f, 9.f));
			AddPart(Wheel, Cube, R, FRotator(-A, 0.f, 0.f), FVector(3.f, W == 0 ? 21.5f : 16.5f, 17.f), 3);
		}
		// disco de freio
		AddPart(Wheel, Cyl, FVector(0.f, W == 0 ? 11.f : 9.f, 0.f), AxisY, FVector(30.f, 30.f, 0.8f), 3);
	}
	// garfo invertido dourado
	for (int32 S = -1; S <= 1; S += 2)
	{
		AddPart(SteerYaw, Cyl, FVector(0.f, S * 9.f, -22.f), FRotator::ZeroRotator, FVector(6.f, 6.f, 44.f), 9);
		AddPart(SteerYaw, Cyl, FVector(0.f, S * 9.f, -58.f), FRotator::ZeroRotator, FVector(5.f, 5.f, 30.f), 2);
	}
	AddPart(SteerYaw, Cube, FVector(0.f, 0.f, 0.f), FRotator::ZeroRotator, FVector(9.f, 26.f, 4.f), 3);

	// chassi e carenagem (coordenadas do modelo menos o centro do corpo)
	auto P = [&](UStaticMesh* M, FVector Loc, FRotator Rot, FVector Size, uint8 Kind) { return AddPart(BikeBody, M, Loc - B, Rot, Size, Kind); };
	for (int32 S = -1; S <= 1; S += 2)
	{
		P(Cube, FVector(30.f, S * 10.f, 39.f), FRotator(15.f, 0.f, 0.f), FVector(62.f, 5.f, 8.f), 3);                // balança
		P(Cube, FVector(100.f, S * 17.f, 62.f), FRotator(-35.f, 0.f, 0.f), FVector(72.f, 4.f, 50.f), 0);              // carenagem lateral
		P(Cube, FVector(104.f, S * 19.6f, 60.f), FRotator(-42.f, 0.f, 0.f), FVector(48.f, 1.f, 8.f), 1);              // faixa laranja
		P(Cube, FVector(96.f, S * 19.4f, 74.f), FRotator(-30.f, 0.f, 0.f), FVector(22.f, 1.f, 10.f), 2);              // entrada de ar
		P(Cube, FVector(152.f, S * 8.f, 86.f), FRotator(0.f, S * -20.f, S * 18.f), FVector(3.f, 9.f, 2.5f), 5);       // farol em "olho"
		P(Cube, FVector(128.f, S * 25.f, 104.f), FRotator::ZeroRotator, FVector(6.f, 11.f, 5.f), 0);                  // retrovisor
		P(Cyl, FVector(104.f, S * 21.f, 95.f), AxisY, FVector(2.8f, 2.8f, 24.f), 3);                                   // semi-guidão
		P(Cube, FVector(36.f, S * 17.f, 44.f), FRotator::ZeroRotator, FVector(4.f, 9.f, 2.f), 3);                     // pedaleira
	}
	P(Cube, FVector(84.f, 0.f, 50.f), FRotator::ZeroRotator, FVector(50.f, 30.f, 38.f), 3);                            // motor
	P(Cube, FVector(112.f, 0.f, 66.f), FRotator(-20.f, 0.f, 0.f), FVector(5.f, 40.f, 32.f), 2);                        // radiador
	P(Sph, FVector(80.f, 0.f, 93.f), FRotator::ZeroRotator, FVector(54.f, 38.f, 24.f), 0);                             // tanque
	P(Sph, FVector(82.f, 0.f, 104.f), FRotator::ZeroRotator, FVector(36.f, 14.f, 6.f), 1);                             // faixa do tanque
	P(Cube, FVector(47.f, 0.f, 87.f), FRotator(-6.f, 0.f, 0.f), FVector(34.f, 25.f, 6.f), 2);                          // banco
	P(Cube, FVector(10.f, 0.f, 95.f), FRotator(-12.f, 0.f, 0.f), FVector(62.f, 15.f, 12.f), 0);                        // rabeta
	P(Cube, FVector(4.f, 0.f, 101.5f), FRotator(-12.f, 0.f, 0.f), FVector(30.f, 15.4f, 1.5f), 1);                      // detalhe da rabeta
	P(Cube, FVector(-21.f, 0.f, 101.f), FRotator(-12.f, 0.f, 0.f), FVector(2.f, 12.f, 2.5f), 6);                       // lanterna
	P(Cube, FVector(95.f, 0.f, 34.f), FRotator::ZeroRotator, FVector(60.f, 28.f, 10.f), 0);                            // barriga
	P(Cube, FVector(138.f, 0.f, 89.f), FRotator(-20.f, 0.f, 0.f), FVector(36.f, 24.f, 22.f), 0);                       // bico
	P(Sph, FVector(149.f, 0.f, 84.f), FRotator::ZeroRotator, FVector(16.f, 22.f, 16.f), 0);                            // ponta do bico
	P(Cube, FVector(124.f, 0.f, 108.f), FRotator(35.f, 0.f, 0.f), FVector(26.f, 20.f, 1.5f), 8);                       // bolha
	P(Cyl, FVector(40.f, -18.f, 44.f), FRotator(-70.f, 0.f, 0.f), FVector(11.f, 11.f, 40.f), 3);                       // escape (lado direito)
	P(Cyl, FVector(52.f, 0.f, 70.f), FRotator(-30.f, 0.f, 0.f), FVector(6.f, 6.f, 30.f), 9);                           // amortecedor

	// piloto provisório (substituído pelo esqueleto se RiderMesh for definido)
	const FVector RB = RiderBodyOffset;
	auto R = [&](UStaticMesh* M, FVector Loc, FRotator Rot, FVector Size, uint8 Kind) { return AddPart(RiderBody, M, Loc - RB, Rot, Size, Kind); };
	auto Limb = [&](FVector A, FVector Bp, float Dia, uint8 Kind)
	{
		const FVector D = Bp - A;
		const FRotator Rot = FRotationMatrix::MakeFromZ(D.GetSafeNormal()).Rotator();
		return R(Cyl, (A + Bp) * 0.5f, Rot, FVector(Dia, Dia, D.Size()), Kind);
	};
	Limb(FVector(42.f, 0.f, 97.f), FVector(71.f, 0.f, 125.f), 32.f, 7);                   // tronco
	R(Sph, FVector(42.f, 0.f, 97.f), FRotator::ZeroRotator, FVector(30.f, 34.f, 24.f), 7); // quadril
	R(Sph, FVector(70.f, 0.f, 126.f), FRotator::ZeroRotator, FVector(28.f, 38.f, 22.f), 7); // ombros
	R(Sph, FVector(60.f, 0.f, 130.f), FRotator::ZeroRotator, FVector(18.f, 14.f, 10.f), 1); // corcunda aerodinâmica
	R(Sph, FVector(90.f, 0.f, 137.f), FRotator::ZeroRotator, FVector(33.f, 29.f, 30.f), 0); // capacete
	R(Sph, FVector(100.f, 0.f, 137.f), FRotator::ZeroRotator, FVector(16.f, 26.f, 11.f), 8); // viseira
	R(Cube, FVector(88.f, 0.f, 152.f), FRotator(-15.f, 0.f, 0.f), FVector(18.f, 3.f, 1.5f), 1); // faixa do capacete
	for (int32 S = -1; S <= 1; S += 2)
	{
		const FVector Shoulder(68.f, S * 19.f, 128.f), Elbow(88.f, S * 25.f, 108.f), Grip(104.f, S * 31.f, 95.f);
		const FVector Hip(42.f, S * 12.f, 94.f), Knee(74.f, S * 22.f, 78.f), Foot(36.f, S * 18.f, 47.f);
		Limb(Shoulder, Elbow, 11.f, 7);
		Limb(Elbow, Grip, 9.5f, 7);
		Limb(Hip, Knee, 16.f, 7);
		Limb(Knee, Foot, 12.f, 7);
		R(Cube, Grip, FRotator::ZeroRotator, FVector(9.f, 7.f, 6.f), 2);
		R(Cube, Foot + FVector(4.f, 0.f, -2.f), FRotator::ZeroRotator, FVector(25.f, 9.f, 12.f), 2);
		R(Cube, Knee + FVector(2.f, S * 7.f, -2.f), FRotator::ZeroRotator, FVector(9.f, 2.f, 6.f), 1); // slider do joelho
		R(Sph, Shoulder, FRotator::ZeroRotator, FVector(13.f, 13.f, 13.f), 1);
	}
}

void AGrauBike::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMaterials();
}

void AGrauBike::ApplyMaterials()
{
	Mids.Reset();
	UMaterialInstanceDynamic* K[10];
	K[0] = FGrauAssets::Paint(this, PaintColor, 0.6f, 0.28f);
	K[1] = FGrauAssets::Paint(this, AccentColor, 0.2f, 0.32f);
	K[2] = FGrauAssets::Surface(this, FLinearColor(0.012f, 0.012f, 0.013f), 0.7f, 0.f);
	K[3] = FGrauAssets::Surface(this, FLinearColor(0.45f, 0.46f, 0.48f), 0.3f, 1.f);
	K[4] = FGrauAssets::Surface(this, FLinearColor(0.01f, 0.01f, 0.01f), 0.85f, 0.f);
	K[5] = FGrauAssets::Emissive(this, FLinearColor(1.f, 0.95f, 0.88f), 80.f);
	K[6] = FGrauAssets::Emissive(this, FLinearColor(1.f, 0.05f, 0.02f), 40.f);
	K[7] = FGrauAssets::Surface(this, FLinearColor(0.015f, 0.015f, 0.017f), 0.42f, 0.1f);
	K[8] = FGrauAssets::Surface(this, FLinearColor(0.03f, 0.025f, 0.02f), 0.04f, 1.f);
	K[9] = FGrauAssets::Surface(this, FLinearColor(0.85f, 0.55f, 0.2f), 0.25f, 1.f);
	for (UMaterialInstanceDynamic* M : K)
	{
		Mids.Add(M);
	}
	for (const FGrauPart& P : Parts)
	{
		if (P.Comp && K[P.Kind])
		{
			P.Comp->SetMaterial(0, K[P.Kind]);
		}
	}

	// modelos reais, se configurados
	const bool bHasBike = BikeMesh != nullptr;
	BikeMeshComp->SetStaticMesh(BikeMesh);
	BikeMeshComp->SetRelativeTransform(FTransform(-BikeBodyOffset) * FTransform::Identity);
	if (bHasBike)
	{
		BikeMeshComp->SetRelativeTransform(BikeMeshOffset * FTransform(-BikeBodyOffset));
	}
	bSkeletalRider = RiderMesh != nullptr;
	RiderSkeletal->SetSkeletalMeshAsset(RiderMesh);
	RiderSkeletal->SetRelativeTransform(RiderMeshOffset);
	for (const FGrauPart& P : Parts)
	{
		if (!P.Comp)
		{
			continue;
		}
		const bool bRiderPart = P.Comp->GetAttachParent() == RiderBody;
		const bool bHide = bRiderPart ? bSkeletalRider : bHasBike;
		P.Comp->SetVisibility(!bHide);
	}
}

void AGrauBike::BeginPlay()
{
	Super::BeginPlay();
	ApplyMaterials();
	BikeBodyRel = BikeBody->GetRelativeTransform();
	RiderBodyRel = RiderBody->GetRelativeTransform();
	if (bSkeletalRider && RiderPose)
	{
		RiderSkeletal->PlayAnimation(RiderPose, true);
	}
	if (EngineSound)
	{
		EngineSound->Start();
	}

	// ponto de partida: PlayerStart do mapa ou a orla perto do posto
	FVector Start = GetActorLocation();
	float StartYaw = GetActorRotation().Yaw;
	bool bHasPlayerStart = false;
	for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
	{
		bHasPlayerStart = true;
		break;
	}
	if (!bHasPlayerStart)
	{
		for (TActorIterator<AGrauCityGenerator> It(GetWorld()); It; ++It)
		{
			Start = It->GetSpawnLocation();
			StartYaw = It->SpawnYaw;
			break;
		}
	}
	PlaceAt(Start, StartYaw);
	CamYaw = YawDeg;
}

FVector AGrauBike::Forward() const
{
	const float R = FMath::DegreesToRadians(YawDeg);
	return FVector(FMath::Cos(R), FMath::Sin(R), 0.f);
}

FVector AGrauBike::FocusPoint() const
{
	if (bCrashed)
	{
		if (bSkeletalRider && RiderSkeletal)
		{
			return RiderSkeletal->GetComponentLocation();
		}
		return RiderBody->GetComponentLocation();
	}
	return Pos + Forward() * 70.f + FVector(0.f, 0.f, 95.f);
}

void AGrauBike::PlaceAt(const FVector& Location, float InYawDeg)
{
	YawDeg = InYawDeg;
	Pos = Location;
	float Gz;
	if (TraceGround(Pos.X, Pos.Y, Pos.Z + 300.f, Gz))
	{
		Pos.Z = Gz;
	}
	SpeedMS = 0.f; Lean = 0.f; Pitch = 0.f; Omega = 0.f; Vy = 0.f;
	bAir = false; AirTime = 0.f; Gear = 1; Rpm = Idle; ShiftTimer = 0.f;
	Throttle = 0.f; Brake = 0.f; SteerS = 0.f; Wheelspin = 0.f; bBurnout = false; bScraping = false;
	GroundVy = 0.f; GroundPitch = 0.f; LastGroundZ = Pos.Z; Susp = 0.f; SuspVel = 0.f; FootDown = 1.f;
	SafePos = Pos; SafeYaw = YawDeg;
	const FVector Fwd = Forward();
	SetActorLocationAndRotation(Pos + Fwd * BoxForward + FVector(0.f, 0.f, BoxUp), FRotator(0.f, YawDeg, 0.f), false, nullptr, ETeleportType::TeleportPhysics);
	SyncVisual(0.f);
}

FGrauInput AGrauBike::ReadInput() const
{
	FGrauInput In;
	const APlayerController* PC = Cast<APlayerController>(GetController());
	if (!PC)
	{
		return In;
	}
	auto Down = [PC](const FKey& K) { return PC->IsInputKeyDown(K); };
	In.Throttle = (Down(EKeys::W) || Down(EKeys::Up)) ? 1.f : 0.f;
	In.Brake = (Down(EKeys::S) || Down(EKeys::Down)) ? 1.f : 0.f;
	In.Steer = ((Down(EKeys::D) || Down(EKeys::Right)) ? 1.f : 0.f) - ((Down(EKeys::A) || Down(EKeys::Left)) ? 1.f : 0.f);
	In.bPull = Down(EKeys::LeftShift) || Down(EKeys::RightShift) || Down(EKeys::Gamepad_FaceButton_Bottom);
	In.bPush = Down(EKeys::LeftControl) || Down(EKeys::X) || Down(EKeys::Gamepad_FaceButton_Right);
	const float Rt = PC->GetInputAnalogKeyState(EKeys::Gamepad_RightTriggerAxis);
	const float Lt = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftTriggerAxis);
	const float Lx = PC->GetInputAnalogKeyState(EKeys::Gamepad_LeftX);
	if (Rt > 0.05f || Lt > 0.05f || FMath::Abs(Lx) > 0.15f)
	{
		In.bAnalog = true;
		In.Throttle = FMath::Max(In.Throttle, Rt);
		In.Brake = FMath::Max(In.Brake, Lt);
		if (FMath::Abs(Lx) > 0.15f)
		{
			In.Steer = Lx;
		}
	}
	return In;
}

void AGrauBike::AddEvent(EGrauEvent Type, float Value, float Value2, bool bFlag, const FString& Reason)
{
	FGrauBikeEvent E;
	E.Type = Type;
	E.Value = Value;
	E.Value2 = Value2;
	E.bFlag = bFlag;
	E.Reason = Reason;
	FrameEvents.Add(E);
}

void AGrauBike::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	FrameEvents.Reset();
	const float Dt = FMath::Min(DeltaSeconds, 0.1f);

	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (PC->WasInputKeyJustPressed(EKeys::C) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Top))
		{
			CycleCamera();
		}
		if (PC->WasInputKeyJustPressed(EKeys::R) && !bCrashed)
		{
			Respawn();
		}
	}

	const FGrauInput In = ReadInput();
	const float Step = 1.f / 120.f;
	Accum += Dt;
	int32 Steps = 0;
	while (Accum >= Step && Steps < 12)
	{
		StepPhysics(Step, In);
		Accum -= Step;
		++Steps;
	}
	if (Steps >= 12)
	{
		Accum = 0.f;
	}

	// câmera lenta no começo do tombo
	if (SlowMoTimer > 0.f)
	{
		SlowMoTimer -= DeltaSeconds / FMath::Max(UGameplayStatics::GetGlobalTimeDilation(this), 0.05f);
		if (SlowMoTimer <= 0.f)
		{
			UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
		}
	}
	if (bCrashed && CrashTime > 3.2f)
	{
		Respawn();
	}

	if (Score)
	{
		Score->UpdateScore(Dt, this, FrameEvents);
	}
	SyncVisual(Dt);
	UpdateCamera(Dt);

	if (EngineSound)
	{
		const float Skid = (bBurnout ? 1.f : 0.f) + FMath::Clamp(Wheelspin, 0.f, 1.f) * 0.6f + ((Brake > 0.8f && KmH() > 30.f) ? 0.3f : 0.f);
		EngineSound->SetEngineState(Rpm, Throttle, FMath::Abs(SpeedMS), !bCrashed && !bMuted, bScraping, Skid);
	}
}

void AGrauBike::StepPhysics(float Dt, const FGrauInput& In)
{
	if (bCrashed)
	{
		UpdateCrash(Dt);
		return;
	}
	const float VAbs = FMath::Abs(SpeedMS);

	// ---- entradas suavizadas (teclado digital vira analógico) ----
	Throttle = In.bAnalog ? Damp(Throttle, In.Throttle, 14.f, Dt) : Approach(Throttle, In.Throttle, In.Throttle > Throttle ? 2.4f : 3.6f, Dt);
	Brake = Approach(Brake, In.Brake, 7.f, Dt);
	SteerS = In.bAnalog ? Damp(SteerS, In.Steer, 10.f, Dt) : Approach(SteerS, In.Steer, 3.2f, Dt);
	const bool bPull = In.bPull;
	const bool bPush = In.bPush;
	const bool bPullEdge = bPull && !bPrevPull;
	bPrevPull = bPull;
	bPullHeld = bPull;
	bPushHeld = bPush;

	// ---- câmbio automático e giro ----
	bBurnout = Brake > 0.5f && Throttle > 0.5f && VAbs < 3.f && !bAir;
	ShiftTimer -= Dt;
	float RpmT = Idle + (VAbs / GearMax[Gear]) * (Redline - Idle);
	if (Gear == 1)
	{
		RpmT = FMath::Max(RpmT, Idle + Throttle * 5200.f * (1.f - Smooth(0.f, 13.f, VAbs)) + Throttle * 1500.f);
	}
	if (bBurnout)
	{
		RpmT = 8500.f + Throttle * 4000.f + FMath::Sin(GetWorld()->GetTimeSeconds() * 20.f) * 300.f;
	}
	if (bAir)
	{
		RpmT = FMath::Max(RpmT, Idle + Throttle * (Redline - Idle));
	}
	if (ShiftTimer <= 0.f && !bAir)
	{
		if (Rpm > 13200.f && Gear < 6 && Throttle > 0.2f)
		{
			++Gear; ShiftTimer = 0.14f; AddEvent(EGrauEvent::Shift, 1.f);
		}
		else if (Gear > 1 && VAbs < GearMax[Gear - 1] * 0.62f)
		{
			--Gear; ShiftTimer = 0.1f; AddEvent(EGrauEvent::Shift, 0.f);
		}
	}
	Rpm = Damp(Rpm, FMath::Clamp(RpmT, Idle, Redline + 200.f), ShiftTimer > 0.f ? 25.f : 10.f, Dt);
	const float N = (Rpm - Idle) / (Redline - Idle);
	const float Torque = 0.62f + 0.38f * Smooth(0.05f, 0.6f, N) - 0.35f * Smooth(0.95f, 1.02f, N);

	// ---- forças longitudinais ----
	float FDrive = Throttle * GearForce[Gear] * Torque;
	FDrive = FMath::Min(FDrive, PowerWatts / FMath::Max(VAbs, 4.f));
	if (ShiftTimer > 0.f) FDrive *= 0.15f;
	if (Gear == 6 && Rpm > Redline - 80.f) FDrive = 0.f;
	const float NRear = MassKg * G * (0.52f + 0.48f * FMath::Clamp(Pitch / 0.3f, 0.f, 1.f));
	const float MaxF = Grip * NRear * 1.08f;
	Wheelspin = FMath::Max(0.f, (FDrive - MaxF) / MaxF);
	FDrive = FMath::Min(FDrive, MaxF);
	if (bBurnout || Pitch < -0.03f || SpeedMS < -0.1f) FDrive = 0.f;
	const bool bFrontUp = Pitch > 0.035f || bAir;
	const float FbFront = bFrontUp ? 0.f : Brake * 3050.f * Grip * (bPush ? 1.12f : 1.f);
	const float FbRear = Pitch < -0.03f ? 0.f : Brake * 1150.f * Grip;
	const float Sign = VAbs > 0.05f ? FMath::Sign(SpeedMS) : 0.f;
	const float Drag = 0.5f * 1.2f * 0.45f * SpeedMS * VAbs + 18.f * Sign
		+ (Grip < 0.7f ? (260.f + 35.f * VAbs) * Sign : 0.f)
		+ (bScraping ? 900.f * Sign : 0.f);
	float Acc;
	if (bAir)
	{
		Acc = -Drag / MassKg;
	}
	else
	{
		Acc = (FDrive - (FbFront + FbRear) * Sign - Drag) / MassKg;
	}
	const float Before = SpeedMS;
	SpeedMS += Acc * Dt;
	if (FDrive == 0.f && Before != 0.f && FMath::Sign(SpeedMS) != FMath::Sign(Before))
	{
		SpeedMS = 0.f;
	}
	// ré "no pezinho"
	if (Brake > 0.6f && Throttle < 0.1f && VAbs < 0.3f && !bFrontUp)
	{
		ReverseTimer += Dt;
		if (ReverseTimer > 0.5f) SpeedMS = -1.2f;
	}
	else
	{
		ReverseTimer = 0.f;
	}

	// ---- arfagem: grau (pivô na roda traseira) / stoppie (pivô na dianteira) ----
	const float InertiaR = MassKg * LR * LR * 1.05f;
	const float InertiaF = MassKg * LF * LF * 1.05f;
	if (!bAir)
	{
		const float Back = bPull ? 1.f : 0.f;
		const float Fwd = bPush ? 1.f : 0.f;
		const float PhiR = Pitch + AR + 0.1f * Back - 0.08f * Fwd;
		const float TauR = MassKg * LR * ((FDrive - FbRear - FMath::Max(Drag, 0.f)) / MassKg * FMath::Sin(PhiR) - G * FMath::Cos(PhiR)) + 300.f * Back - 220.f * Fwd;
		const float ADec = FbFront / MassKg;
		const float PhiF = -Pitch + AF + 0.24f * Fwd - 0.06f * Back;
		const float TauF = MassKg * LF * (ADec * FMath::Sin(PhiF) - G * FMath::Cos(PhiF));
		if (bPullEdge && Throttle > 0.35f && Rpm > 4200.f && VAbs < 48.f && Pitch >= -0.01f)
		{
			// solta a embreagem: impulso que levanta a frente
			const float GearK = Gear <= 2 ? 1.f : (Gear == 3 ? 0.75f : 0.5f);
			Omega += (0.95f + 1.7f * Throttle * FMath::Clamp(N + 0.2f, 0.f, 1.f)) * GearK;
			Rpm = FMath::Max(Idle, Rpm - 1800.f);
			AddEvent(EGrauEvent::Clutch);
		}
		if (Pitch > 0.f || (Pitch == 0.f && (TauR > 0.f || Omega > 0.f)))
		{
			float Alpha = TauR / InertiaR;
			if (bAssist && Pitch > 0.03f) Alpha *= 0.55f;
			Omega += Alpha * Dt;
			Omega *= FMath::Exp(-(bAssist ? 2.6f : 1.2f) * Dt);
			Pitch += Omega * Dt;
			if (Pitch <= 0.f)
			{
				if (Omega < -2.6f) AddEvent(EGrauEvent::Land, -Omega);
				SuspVel -= FMath::Min(2.5f, -Omega * 0.35f);
				Pitch = 0.f;
				Omega = 0.f;
			}
			bScraping = false;
			if (Pitch >= TailAngle)
			{
				if (Omega > 3.4f) { Crash(TEXT("passou"), Omega); return; }
				Pitch = TailAngle;
				if (Omega > 0.f) Omega = 0.f;
				bScraping = VAbs > 1.f;
				if (VAbs < 1.5f) { Crash(TEXT("caiu-de-costas"), 1.f); return; }
			}
		}
		else if (Pitch < 0.f || (Pitch == 0.f && TauF > 0.f && VAbs > 2.f))
		{
			Omega -= (TauF / InertiaF) * Dt;
			Omega *= FMath::Exp(-(bAssist ? 2.2f : 1.0f) * Dt);
			Pitch += Omega * Dt;
			if (Pitch >= 0.f)
			{
				Pitch = 0.f;
				if (Omega > 2.f) SuspVel -= 1.2f;
				Omega = 0.f;
			}
			if (VAbs < 0.6f && Pitch < 0.f) Omega += 4.f * Dt;
			if (Pitch < -0.82f) { Crash(TEXT("capotou"), -Omega); return; }
			bScraping = false;
		}
		else
		{
			Omega = 0.f;
			bScraping = false;
		}
	}
	else
	{
		// no ar o piloto gira a moto com o corpo
		Omega += ((bPull ? 2.4f : 0.f) - (bPush ? 2.4f : 0.f) + Throttle * 0.5f) * Dt;
		Omega *= FMath::Exp(-0.4f * Dt);
		Pitch += Omega * Dt;
		bScraping = false;
	}

	// ---- inclinação e direção ----
	const float LeanMax = (Pitch > 0.05f ? 0.55f : 0.95f) * Smooth(2.f, 14.f, VAbs);
	const float LeanT = bAir ? Lean : SteerS * LeanMax;
	Lean = Damp(Lean, LeanT, Pitch > 0.05f ? 3.5f : 5.5f, Dt);
	const float YawHigh = VAbs > 0.5f ? (G * FMath::Tan(Lean)) / FMath::Max(VAbs, 2.f) : 0.f;
	const float SteerAng = SteerS * 0.55f * (1.f - Smooth(4.f, 16.f, VAbs));
	const float YawLow = (SpeedMS * FMath::Tan(SteerAng)) / WheelbaseM;
	float YawRate = FMath::Lerp(YawLow, YawHigh * (SpeedMS < 0.f ? -1.f : 1.f), Smooth(3.f, 10.f, VAbs));
	if (Pitch > 0.05f) YawRate *= 0.75f;
	if (bAir) YawRate = 0.f;
	bool bDonut = false;
	if (bBurnout && FMath::Abs(SteerS) > 0.2f)
	{
		// zerinho: gira em volta da roda da frente
		const FVector FrontPos = Pos + Forward() * (WheelbaseM * 100.f);
		YawDeg += FMath::RadiansToDegrees(SteerS * 2.1f * Dt);
		Pos = FrontPos - Forward() * (WheelbaseM * 100.f);
		AddEvent(EGrauEvent::Donut, Dt);
		bDonut = true;
	}
	else
	{
		YawDeg += FMath::RadiansToDegrees(YawRate * Dt);
	}
	SteerYaw->SetRelativeRotation(FRotator(0.f, FMath::RadiansToDegrees(SteerAng) * 0.9f, 0.f));

	// ---- movimento com colisão ----
	if (MoveAndCollide(Dt))
	{
		return;
	}

	// ---- chão, rampas e voo ----
	float GroundZ = Pos.Z - 100000.f;
	float NewGrip = Grip;
	const bool bGround = TraceGround(Pos.X, Pos.Y, Pos.Z, GroundZ, &NewGrip);
	Grip = NewGrip;
	if (!bAir)
	{
		const float VyG = (GroundZ - LastGroundZ) / 100.f / FMath::Max(Dt, 1e-4f);
		if (!bGround || GroundZ < Pos.Z - 60.f || (GroundZ < Pos.Z - 30.f && GroundVy > 1.2f))
		{
			bAir = true;
			Vy = GroundVy > 1.2f ? GroundVy : 0.f;
			AirTime = 0.f;
			AddEvent(EGrauEvent::Takeoff);
		}
		else if (GroundZ < Pos.Z - 2.f)
		{
			Pos.Z = FMath::Max(GroundZ, Pos.Z - 350.f * Dt);
		}
		else
		{
			if (GroundZ > Pos.Z + 5.f && GroundZ - Pos.Z < 30.f)
			{
				SuspVel -= (GroundZ - Pos.Z) / 100.f * 6.f;
			}
			Pos.Z = GroundZ;
		}
		GroundVy = FMath::Clamp(VyG, -20.f, 20.f);
	}
	else
	{
		AirTime += Dt;
		Vy -= G * Dt;
		Pos.Z += Vy * 100.f * Dt;
		if (bGround && Pos.Z <= GroundZ)
		{
			const float Impact = -Vy;
			Pos.Z = GroundZ;
			bAir = false;
			GroundVy = 0.f;
			if (Pitch < -0.35f || Pitch > 1.2f || Impact > 11.f)
			{
				Crash(TEXT("pouso"), Impact);
				return;
			}
			if (Pitch < 0.05f) { Pitch = FMath::Max(Pitch, 0.f); Omega = 0.f; }
			else Omega = FMath::Min(Omega, 0.f) * 0.3f;
			SuspVel -= FMath::Min(4.f, Impact * 0.5f);
			AddEvent(EGrauEvent::Landing, AirTime, Impact, Pitch < 0.6f);
		}
	}
	LastGroundZ = GroundZ;

	// inclinação do terreno (rampas, ladeiras) só para o visual
	if (!bAir)
	{
		const FVector F = Pos + Forward() * (WheelbaseM * 100.f);
		float FrontZ;
		if (TraceGround(F.X, F.Y, Pos.Z + 40.f, FrontZ))
		{
			const float Target = FMath::Clamp(FMath::Atan2(FrontZ - Pos.Z, WheelbaseM * 100.f), -0.5f, 0.5f);
			GroundPitch = Damp(GroundPitch, Target, 18.f, Dt);
		}
	}

	SetActorLocation(FVector(GetActorLocation().X, GetActorLocation().Y, Pos.Z + BoxUp), false, nullptr, ETeleportType::None);
	if (bDonut)
	{
		SetActorLocation(Pos + Forward() * BoxForward + FVector(0.f, 0.f, BoxUp), false);
	}

	// trânsito: carro que bate na moto (inclusive por trás)
	if (AGrauTrafficManager* Traffic = AGrauTrafficManager::Get(GetWorld()))
	{
		FGrauCarHit Hit;
		if (Traffic->HitBike(Pos, Forward(), SpeedMS, Hit))
		{
			if (Hit.Impact > 6.f)
			{
				Crash(TEXT("carro"), Hit.Impact);
				return;
			}
			Pos += FVector(Hit.Normal.X, Hit.Normal.Y, 0.f) * Hit.PenCm;
			SpeedMS *= 0.6f;
			SetActorLocation(Pos + Forward() * BoxForward + FVector(0.f, 0.f, BoxUp), true);
		}
	}

	// caiu no mar / fora do mundo
	if (Pos.Z < SeaLevelCm - 40.f)
	{
		Crash(TEXT("mar"), 1.f);
		return;
	}

	// ---- rodas e suspensão (visual) ----
	const float RearV = bBurnout ? 28.f : SpeedMS * (1.f + Wheelspin * 0.6f);
	RearAngle += RearV / WheelRadiusM * Dt;
	FrontAngle += SpeedMS / WheelRadiusM * Dt;
	SuspVel += (-Susp * 160.f - SuspVel * 14.f) * Dt;
	Susp = FMath::Clamp(Susp + SuspVel * Dt, -0.08f, 0.05f);
	FootDown = Damp(FootDown, (VAbs < 0.8f && !bBurnout) ? 1.f : 0.f, 6.f, Dt);

	// ponto seguro para reaparecer
	SafeTimer += Dt;
	if (SafeTimer > 1.f && !bAir && Pitch == 0.f && VAbs > 2.f && Grip > 0.7f)
	{
		SafeTimer = 0.f;
		SafePos = Pos;
		SafeYaw = YawDeg;
	}
}

bool AGrauBike::MoveAndCollide(float Dt)
{
	const FVector Fwd = Forward();
	SetActorRotation(FRotator(0.f, YawDeg, 0.f), ETeleportType::None);
	const FVector Desired = Pos + Fwd * (SpeedMS * 100.f * Dt) + Fwd * BoxForward + FVector(0.f, 0.f, BoxUp);
	FVector Delta = Desired - GetActorLocation();
	Delta.Z = 0.f;
	FHitResult Hit;
	AddActorWorldOffset(Delta, true, &Hit, ETeleportType::None);
	if (Hit.bBlockingHit)
	{
		FVector Nrm = Hit.ImpactNormal;
		Nrm.Z = 0.f;
		if (!Nrm.Normalize())
		{
			Nrm = -Fwd;
		}
		const float Dot = FVector::DotProduct(Fwd, Nrm);
		float Vn = Dot * SpeedMS;
		const AGrauTrafficCar* Car = Cast<AGrauTrafficCar>(Hit.GetActor());
		if (Car)
		{
			const FVector Rel = Fwd * SpeedMS - Car->GetCarVelocityMS();
			Vn = FVector::DotProduct(Rel, Nrm);
		}
		if (Vn < -CrashImpactMS)
		{
			Crash(Car ? TEXT("carro") : TEXT("bateu"), -Vn);
			return true;
		}
		if (Vn < -1.f)
		{
			AddEvent(EGrauEvent::Bump, -Vn);
			SpeedMS *= 1.f - 0.85f * FMath::Abs(Dot);
		}
		FVector Remaining = Delta * (1.f - Hit.Time);
		Remaining -= Nrm * FVector::DotProduct(Remaining, Nrm);
		Remaining.Z = 0.f;
		if (!Remaining.IsNearlyZero())
		{
			AddActorWorldOffset(Remaining, true, nullptr, ETeleportType::None);
		}
	}
	const FVector Loc = GetActorLocation();
	Pos.X = Loc.X - Fwd.X * BoxForward;
	Pos.Y = Loc.Y - Fwd.Y * BoxForward;
	return false;
}

bool AGrauBike::TraceGround(float X, float Y, float FromZ, float& OutZ, float* OutGrip) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(GrauGround), false, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	const FVector Start(X, Y, FromZ + 150.f);
	const FVector End(X, Y, FromZ - 3000.f);
	if (World->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
	{
		OutZ = Hit.ImpactPoint.Z;
		if (OutGrip)
		{
			const UPrimitiveComponent* C = Hit.GetComponent();
			*OutGrip = (C && C->ComponentHasTag(TEXT("Sand"))) ? 0.55f : 1.05f;
		}
		return true;
	}
	return false;
}

void AGrauBike::Crash(const FString& Reason, float Impact)
{
	if (bCrashed)
	{
		return;
	}
	bCrashed = true;
	CrashTime = 0.f;
	CrashReason = Reason;
	bCrashCamSet = false;
	const FVector Fwd = Forward();
	const FVector Vel = (Fwd * SpeedMS + FVector(0.f, 0.f, Vy)) * 100.f;
	const float Sp = FMath::Min(FMath::Abs(SpeedMS), 30.f);

	auto Launch = [&](UPrimitiveComponent* Body, float VelScale, float Up, float Spin, float Mass)
	{
		Body->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
		Body->SetSimulatePhysics(true);
		Body->SetMassOverrideInKg(NAME_None, Mass, true);
		const FVector V = Vel * VelScale + FVector(FMath::FRandRange(-200.f, 200.f), FMath::FRandRange(-200.f, 200.f), Up);
		Body->SetPhysicsLinearVelocity(V);
		Body->SetPhysicsAngularVelocityInDegrees(FVector(FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f), FMath::FRandRange(-1.f, 1.f)) * Spin);
	};
	Launch(BikeBody, 0.85f, 100.f + Sp * 6.f, 120.f + Sp * 15.f, 200.f);
	if (bSkeletalRider)
	{
		RiderSkeletal->DetachFromComponent(FDetachmentTransformRules::KeepWorldTransform);
		RiderSkeletal->SetCollisionProfileName(TEXT("Ragdoll"));
		RiderSkeletal->SetAllBodiesSimulatePhysics(true);
		RiderSkeletal->SetSimulatePhysics(true);
		RiderSkeletal->WakeAllRigidBodies();
		RiderSkeletal->SetAllPhysicsLinearVelocity(Vel + FVector(0.f, 0.f, 250.f + Sp * 10.f));
		bRagdoll = true;
	}
	else
	{
		Launch(RiderBody, 1.f, 250.f + Sp * 10.f, 160.f + Sp * 20.f, 80.f);
	}
	Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Headlight->SetVisibility(true);
	SpeedMS = 0.f;
	bScraping = false;
	UGameplayStatics::SetGlobalTimeDilation(this, 0.35f);
	SlowMoTimer = 1.4f;
	AddEvent(EGrauEvent::Crash, Impact, 0.f, false, Reason);
	UE_LOG(LogGrau, Log, TEXT("Tombo: %s (impacto %.1f m/s)"), *Reason, Impact);
}

void AGrauBike::UpdateCrash(float Dt)
{
	CrashTime += Dt;
}

void AGrauBike::Respawn()
{
	UGameplayStatics::SetGlobalTimeDilation(this, 1.f);
	SlowMoTimer = 0.f;
	auto Restore = [&](UPrimitiveComponent* Body, const FTransform& Rel)
	{
		Body->SetSimulatePhysics(false);
		Body->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		Body->AttachToComponent(ModelRoot, FAttachmentTransformRules::KeepRelativeTransform);
		Body->SetRelativeTransform(Rel);
	};
	Restore(BikeBody, BikeBodyRel);
	Restore(RiderBody, RiderBodyRel);
	if (bRagdoll)
	{
		RiderSkeletal->SetSimulatePhysics(false);
		RiderSkeletal->SetAllBodiesSimulatePhysics(false);
		RiderSkeletal->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
		RiderSkeletal->AttachToComponent(RiderBody, FAttachmentTransformRules::KeepRelativeTransform);
		RiderSkeletal->SetRelativeTransform(RiderMeshOffset);
		if (RiderPose)
		{
			RiderSkeletal->PlayAnimation(RiderPose, true);
		}
		bRagdoll = false;
	}
	Collision->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	bCrashed = false;
	CrashTime = 0.f;
	PlaceAt(SafePos, SafeYaw);
	CamYaw = YawDeg;
}

void AGrauBike::SyncVisual(float Dt)
{
	if (bCrashed)
	{
		return;
	}
	LeanPivot->SetRelativeRotation(FRotator(0.f, 0.f, FMath::RadiansToDegrees(Lean)));
	WheeliePivot->SetRelativeRotation(FRotator(FMath::RadiansToDegrees(FMath::Max(Pitch, 0.f) + GroundPitch), 0.f, 0.f));
	StoppiePivot->SetRelativeRotation(FRotator(-FMath::RadiansToDegrees(FMath::Max(-Pitch, 0.f)), 0.f, 0.f));
	RearWheel->SetRelativeRotation(FRotator(-FMath::RadiansToDegrees(RearAngle), 0.f, 0.f));
	FrontWheel->SetRelativeRotation(FRotator(-RakeDeg - FMath::RadiansToDegrees(FrontAngle), 0.f, 0.f));
	BikeBody->SetRelativeLocation(BikeBodyRel.GetLocation() + FVector(0.f, 0.f, Susp * 100.f));
	// piloto fica mais em pé no grau e joga o corpo pra dentro da curva
	const float RiderPitch = -FMath::RadiansToDegrees(FMath::Max(Pitch, 0.f)) * 0.35f + (bPullHeld ? -6.f : 0.f) + (bPushHeld ? 5.f : 0.f);
	RiderBody->SetRelativeLocation(RiderBodyRel.GetLocation() + FVector(0.f, 0.f, Susp * 100.f));
	RiderBody->SetRelativeRotation(FRotator(RiderPitch, 0.f, FMath::RadiansToDegrees(Lean) * 0.18f));
}

void AGrauBike::CycleCamera()
{
	CameraMode = static_cast<EGrauCamera>((static_cast<uint8>(CameraMode) + 1) % 4);
}

void AGrauBike::SetMuted(bool bMute)
{
	bMuted = bMute;
	if (EngineSound)
	{
		EngineSound->SetVolumeMultiplier(bMute ? 0.f : 1.f);
	}
}

void AGrauBike::UpdateCamera(float Dt)
{
	const float T = GetWorld()->GetTimeSeconds();
	const float Kmh = KmH();
	const FVector Focus = FocusPoint();
	const FVector Fwd = Forward();
	const FVector Right(-Fwd.Y, Fwd.X, 0.f);
	const float LeanDeg = FMath::RadiansToDegrees(Lean);
	const float P = FMath::Max(Pitch, 0.f);

	// mouse / analógico direito para olhar em volta
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		float Mx = 0.f, My = 0.f;
		PC->GetInputMouseDelta(Mx, My);
		Mx += PC->GetInputAnalogKeyState(EKeys::Gamepad_RightX) * 3.f;
		My += PC->GetInputAnalogKeyState(EKeys::Gamepad_RightY) * 2.f;
		if (FMath::Abs(Mx) + FMath::Abs(My) > 0.01f)
		{
			CamYawOff += Mx * 2.5f;
			CamPitchOff = FMath::Clamp(CamPitchOff - My * 2.f, -12.f, 50.f);
			LastMouseTime = T;
		}
	}
	if (T - LastMouseTime > 1.6f)
	{
		CamYawOff = Damp(CamYawOff, 0.f, 1.8f, Dt);
		CamPitchOff = Damp(CamPitchOff, 0.f, 1.8f, Dt);
	}

	CamShake = Damp(CamShake, 0.f, 4.f, Dt);
	CamKick = Damp(CamKick, 0.f, 3.f, Dt);
	for (const FGrauBikeEvent& E : FrameEvents)
	{
		if (E.Type == EGrauEvent::Clutch) CamKick = 6.f;
		if (E.Type == EGrauEvent::Land || E.Type == EGrauEvent::Landing) CamShake = FMath::Max(CamShake, FMath::Min(8.f, E.Value * 1.5f));
		if (E.Type == EGrauEvent::Bump) CamShake = FMath::Max(CamShake, 4.f);
		if (E.Type == EGrauEvent::Crash) CamShake = 12.f;
	}
	const float Shake = CamShake + FMath::Square(FMath::Min(Kmh, 300.f) / 300.f) * 2.5f;
	Camera->SetRelativeLocation(FVector(0.f, FMath::Sin(T * 37.1f) * Shake * 0.5f, FMath::Sin(T * 41.3f) * Shake * 0.5f));

	FPostProcessSettings& PP = Camera->PostProcessSettings;
	PP.bOverride_DepthOfFieldFocalDistance = false;
	PP.bOverride_DepthOfFieldFstop = false;

	if (bCrashed)
	{
		// câmera de tombo: fica onde estava e acompanha o piloto rolando
		if (!bCrashCamSet)
		{
			CrashCamPos = Camera->GetComponentLocation() + FVector(0.f, 0.f, 120.f);
			bCrashCamSet = true;
		}
		FVector Want = CrashCamPos;
		const float D = FVector::Dist2D(Want, Focus);
		if (D < 500.f)
		{
			Want += (Want - Focus).GetSafeNormal2D() * (500.f - D);
		}
		SpringArm->bEnableCameraLag = false;
		SpringArm->bDoCollisionTest = false;
		SpringArm->TargetArmLength = 0.f;
		SpringArm->SetWorldLocation(FMath::VInterpTo(SpringArm->GetComponentLocation(), Want, Dt, 3.f));
		SpringArm->SetWorldRotation((Focus - SpringArm->GetComponentLocation()).Rotation());
		CamFov = Damp(CamFov, 65.f, 2.f, Dt);
		Camera->SetFieldOfView(CamFov);
		return;
	}

	switch (CameraMode)
	{
	case EGrauCamera::Helmet:
	{
		SpringArm->bEnableCameraLag = false;
		SpringArm->bDoCollisionTest = false;
		SpringArm->TargetArmLength = 0.f;
		const FVector Eye = ModelRoot->GetComponentTransform().TransformPosition(FVector(98.f, 0.f, 150.f));
		SpringArm->SetWorldLocation(Eye);
		SpringArm->SetWorldRotation(FRotator(-9.f + FMath::RadiansToDegrees(P) * 0.6f, YawDeg, LeanDeg * 0.9f));
		CamFov = Damp(CamFov, 95.f + Kmh * 0.04f, 4.f, Dt);
		break;
	}
	case EGrauCamera::Cinema:
	{
		SpringArm->bEnableCameraLag = false;
		SpringArm->bDoCollisionTest = false;
		SpringArm->TargetArmLength = 0.f;
		const float Sway = FMath::Sin(T * 0.15f);
		const FVector CamPos = Focus + Right * (260.f + Sway * 40.f) + Fwd * (240.f + Sway * 120.f) + FVector(0.f, 0.f, -45.f);
		SpringArm->SetWorldLocation(CamPos);
		SpringArm->SetWorldRotation((Focus - CamPos).Rotation());
		PP.bOverride_DepthOfFieldFocalDistance = true;
		PP.DepthOfFieldFocalDistance = FVector::Dist(CamPos, Focus);
		PP.bOverride_DepthOfFieldFstop = true;
		PP.DepthOfFieldFstop = 2.0f;
		CamFov = Damp(CamFov, 42.f, 3.f, Dt);
		break;
	}
	default:
	{
		const bool bNear = CameraMode == EGrauCamera::Near;
		CamYaw = DampAngleDeg(CamYaw, YawDeg + (SpeedMS < -0.5f ? 180.f : 0.f), 4.2f, Dt);
		SpringArm->bEnableCameraLag = true;
		SpringArm->bDoCollisionTest = true;
		SpringArm->TargetArmLength = (bNear ? 310.f : 460.f) + Kmh * (bNear ? 0.4f : 0.8f) + P * 60.f;
		SpringArm->SetWorldLocation(Focus + FVector(0.f, 0.f, bNear ? 20.f : 45.f));
		const float ArmPitch = -(bNear ? 7.f : 10.f) - CamPitchOff + FMath::RadiansToDegrees(P) * 0.15f;
		const float Swing = FMath::RadiansToDegrees(P * 0.6f);
		SpringArm->SetWorldRotation(FRotator(ArmPitch, CamYaw + CamYawOff + Swing, 0.f));
		Camera->SetRelativeRotation(FRotator(0.f, 0.f, -LeanDeg * 0.12f));
		CamFov = Damp(CamFov, (bNear ? 84.f : 78.f) + FMath::Min(Kmh, 280.f) * 0.07f + CamKick, 3.f, Dt);
		break;
	}
	}
	if (CameraMode != EGrauCamera::Chase && CameraMode != EGrauCamera::Near)
	{
		Camera->SetRelativeRotation(FRotator::ZeroRotator);
	}
	Camera->SetFieldOfView(CamFov);
}
