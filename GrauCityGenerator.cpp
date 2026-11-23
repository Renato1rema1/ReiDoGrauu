#include "GrauCityGenerator.h"
#include "GrauAssets.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"
#include "Algo/Sort.h"

using namespace GrauLayout;

namespace
{
	enum class ESpecial : uint8 { None, Posto, Pista, Praca };
	ESpecial SpecialAt(int32 I, int32 J)
	{
		if (I == 5 && J == 0) return ESpecial::Posto;
		if (I == 1 && J == 2) return ESpecial::Pista;
		if (I == 4 && J == 3) return ESpecial::Praca;
		return ESpecial::None;
	}

	// cores de fachada (multiplicam o material de prédio)
	const FLinearColor Pastel[] = {
		FLinearColor(0.95f, 0.92f, 0.86f), FLinearColor(0.98f, 0.86f, 0.80f), FLinearColor(0.82f, 0.90f, 0.95f), FLinearColor(0.96f, 0.93f, 0.75f),
		FLinearColor(0.88f, 0.95f, 0.86f), FLinearColor(1.f, 1.f, 1.f), FLinearColor(0.93f, 0.84f, 0.92f), FLinearColor(0.85f, 0.82f, 0.78f) };
	const FLinearColor Warm[] = {
		FLinearColor(1.f, 0.95f, 0.9f), FLinearColor(0.9f, 0.8f, 0.7f), FLinearColor(0.78f, 0.7f, 0.62f), FLinearColor(0.95f, 0.85f, 0.72f), FLinearColor(0.7f, 0.66f, 0.62f) };
	const FLinearColor FavelaColors[] = {
		FLinearColor(0.85f, 0.33f, 0.42f), FLinearColor(0.25f, 0.5f, 0.77f), FLinearColor(0.91f, 0.77f, 0.28f), FLinearColor(0.37f, 0.68f, 0.42f),
		FLinearColor(0.89f, 0.48f, 0.21f), FLinearColor(0.6f, 0.35f, 0.23f), FLinearColor(0.94f, 0.92f, 0.89f), FLinearColor(0.55f, 0.37f, 0.75f) };

	struct FSignDef { const TCHAR* Text; FColor Color; };
	const FSignDef SignDefs[] = {
		{ TEXT("AÇAÍ DA ORLA"), FColor(198, 77, 255) }, { TEXT("PADARIA PÃO QUENTE"), FColor(255, 179, 71) }, { TEXT("BAR DO ZÉ"), FColor(255, 77, 77) },
		{ TEXT("OFICINA DE MOTOS"), FColor(77, 210, 255) }, { TEXT("FARMÁCIA 24H"), FColor(77, 255, 136) }, { TEXT("LANCHONETE"), FColor(255, 216, 77) },
		{ TEXT("PASTEL & CALDO DE CANA"), FColor(255, 122, 47) }, { TEXT("BORRACHARIA"), FColor(255, 255, 255) } };

	struct FMountain { FVector2D C; float R; float Z; };
}

AGrauCityGenerator::AGrauCityGenerator()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickInterval = 0.f;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeF(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylF(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphF(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> ConeF(TEXT("/Engine/BasicShapes/Cone.Cone"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneF(TEXT("/Engine/BasicShapes/Plane.Plane"));
	UStaticMesh* Cube = CubeF.Object;

	Road = MakeHism(TEXT("Road"), Cube, true);
	Sidewalk = MakeHism(TEXT("Sidewalk"), Cube, true);
	Calcadao = MakeHism(TEXT("Calcadao"), Cube, true);
	Sand = MakeHism(TEXT("Sand"), Cube, true);
	Sand->ComponentTags.Add(TEXT("Sand"));
	MarkWhite = MakeHism(TEXT("MarkWhite"), Cube, false, false);
	MarkYellow = MakeHism(TEXT("MarkYellow"), Cube, false, false);
	Buildings = MakeHism(TEXT("Buildings"), Cube, true);
	Buildings->NumCustomDataFloats = 4;
	RoofProps = MakeHism(TEXT("RoofProps"), CylF.Object, true);
	LampPoles = MakeHism(TEXT("LampPoles"), CylF.Object, true);
	LampHeads = MakeHism(TEXT("LampHeads"), Cube, false, false);
	Trunks = MakeHism(TEXT("Trunks"), CylF.Object, true);
	Crowns = MakeHism(TEXT("Crowns"), SphF.Object, false);
	Rails = MakeHism(TEXT("Rails"), Cube, true);
	Concrete = MakeHism(TEXT("Concrete"), Cube, true);
	Metal = MakeHism(TEXT("Metal"), Cube, true);
	Orange = MakeHism(TEXT("Orange"), Cube, true);
	LightPanels = MakeHism(TEXT("LightPanels"), Cube, false, false);
	Grass = MakeHism(TEXT("Grass"), Cube, false, false);
	Hills = MakeHism(TEXT("Hills"), SphF.Object, true);
	Favela = MakeHism(TEXT("Favela"), Cube, false);
	Favela->NumCustomDataFloats = 4;
	FarCity = MakeHism(TEXT("FarCity"), Cube, false, false);
	FarCity->NumCustomDataFloats = 4;
	Cones = MakeHism(TEXT("Cones"), ConeF.Object, false);

	Sea = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Sea"));
	Sea->SetupAttachment(Root);
	Sea->SetStaticMesh(PlaneF.Object);
	Sea->SetCollisionProfileName(UCollisionProfile::NoCollision_ProfileName);
	Sea->CastShadow = false;

	PostoLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PostoLight"));
	PostoLight->SetupAttachment(Root);
	PostoLight->IntensityUnits = ELightUnits::Lumens;
	PostoLight->Intensity = 40000.f;
	PostoLight->AttenuationRadius = 4500.f;
	PostoLight->LightColor = FColor(235, 242, 255);
	PostoLight->CastShadows = true;
}

UHierarchicalInstancedStaticMeshComponent* AGrauCityGenerator::MakeHism(const TCHAR* Name, UStaticMesh* Mesh, bool bCollide, bool bShadows)
{
	UHierarchicalInstancedStaticMeshComponent* H = CreateDefaultSubobject<UHierarchicalInstancedStaticMeshComponent>(Name);
	H->SetupAttachment(Root);
	H->SetStaticMesh(Mesh);
	H->SetCollisionProfileName(bCollide ? UCollisionProfile::BlockAll_ProfileName : UCollisionProfile::NoCollision_ProfileName);
	H->CastShadow = bShadows;
	H->SetGenerateOverlapEvents(false);
	return H;
}

FVector AGrauCityGenerator::GetSpawnLocation() const
{
	return GetActorTransform().TransformPosition(FVector(100.f, 6.6f, 0.f) * 100.f + FVector(0.f, 0.f, 50.f));
}

int32 AGrauCityGenerator::Box(UHierarchicalInstancedStaticMeshComponent* H, const FVector& CenterM, const FVector& SizeM, const FRotator& Rot)
{
	return H->AddInstance(FTransform(Rot, CenterM * 100.f, SizeM));
}

void AGrauCityGenerator::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	Rebuild();
}

void AGrauCityGenerator::ClearAll()
{
	TArray<UHierarchicalInstancedStaticMeshComponent*> All = { Road, Sidewalk, Calcadao, Sand, MarkWhite, MarkYellow, Buildings, RoofProps, LampPoles, LampHeads,
		Trunks, Crowns, Rails, Concrete, Metal, Orange, LightPanels, Grass, Hills, Favela, FarCity, Cones };
	for (UHierarchicalInstancedStaticMeshComponent* H : All)
	{
		if (H) H->ClearInstances();
	}
	for (UTextRenderComponent* T : Signs)
	{
		if (IsValid(T)) T->DestroyComponent();
	}
	Signs.Reset();
	for (UHierarchicalInstancedStaticMeshComponent* H : ModelHisms)
	{
		if (IsValid(H)) H->DestroyComponent();
	}
	ModelHisms.Reset();
	Nodes.Reset();
	Roads.Reset();
	Blocks.Reset();
	LampHeadsCm.Reset();
}

void AGrauCityGenerator::Rebuild()
{
	ClearAll();
	FRandomStream Rng(Seed);
	BuildGround();
	BuildMarkings();
	BuildBuildings(Rng);
	BuildLamps();
	BuildPalmsAndTrees(Rng);
	BuildRails();
	BuildPosto();
	BuildPista();
	BuildPraca();
	if (bBuildHills) BuildHills(Rng);
	if (bBuildFarCity) BuildFarCity(Rng);
	BuildGraph();
	ApplyMaterials();
}

void AGrauCityGenerator::BuildGround()
{
	// asfalto da cidade + extensões da orla (topo em z = 0)
	Box(Road, FVector(0.f, (OrlaY1 + CityYMin) * 0.5f, -0.1f), FVector(CityX * 2.f, OrlaY1 - CityYMin, 0.2f));
	for (float S : { -1.f, 1.f })
	{
		Box(Road, FVector(S * (CityX + OrlaX) * 0.5f, (OrlaY0 + OrlaY1) * 0.5f, -0.1f), FVector(OrlaX - CityX, OrlaY1 - OrlaY0, 0.2f));
	}
	// calçadas dos quarteirões (15 cm, a moto sobe o meio-fio)
	for (int32 I = 0; I < NB; ++I)
	{
		for (int32 J = 0; J < NB; ++J)
		{
			const FBox2D R = BlockRect(I, J);
			Blocks.Add(R);
			const FVector2D C = R.GetCenter();
			const FVector2D S = R.GetSize();
			Box(SpecialAt(I, J) == ESpecial::Pista ? Road : Sidewalk, FVector(C.X, C.Y, 0.075f), FVector(S.X, S.Y, 0.15f));
		}
	}
	// calçadão de ondas
	Box(Calcadao, FVector(0.f, (OrlaY1 + CalcY1) * 0.5f, 0.075f), FVector(OrlaX * 2.f, CalcY1 - OrlaY1, 0.15f));
	// areia descendo até o mar (inclinação 1,5%)
	const float Slope = 0.015f;
	const float Len = 75.f;
	const float AngleDeg = FMath::RadiansToDegrees(FMath::Atan(Slope));
	const float TopMid = 0.1f - Slope * Len * 0.5f;
	Box(Sand, FVector(0.f, CalcY1 + Len * 0.5f, TopMid - 0.5f), FVector(3600.f, Len, 1.f), FRotator(0.f, 0.f, AngleDeg));
	// mar
	Sea->SetRelativeLocation(FVector(0.f, 4550.f, -0.35f) * 100.f);
	Sea->SetRelativeScale3D(FVector(16000.f, 9000.f, 1.f));
	Roads.Add({ FVector2D(-OrlaX, OrlaCY), FVector2D(OrlaX, OrlaCY), 10.f });
	for (int32 K = 0; K <= 8; ++K)
	{
		Roads.Add({ FVector2D(RoadX(K), OrlaY0), FVector2D(RoadX(K), RoadY(8)), RW });
	}
	for (int32 M = 1; M <= 8; ++M)
	{
		Roads.Add({ FVector2D(-CityX, RoadY(M)), FVector2D(CityX, RoadY(M)), RW });
	}
}

void AGrauCityGenerator::BuildMarkings()
{
	const float Z = 0.008f;
	auto Q = [&](UHierarchicalInstancedStaticMeshComponent* H, float X0, float Y0, float X1, float Y1)
	{
		Box(H, FVector((X0 + X1) * 0.5f, (Y0 + Y1) * 0.5f, Z), FVector(FMath::Abs(X1 - X0), FMath::Abs(Y1 - Y0), 0.012f));
	};
	auto LaneLines = [&](bool bHoriz, float C, float A0, float A1, float HW)
	{
		auto L = [&](UHierarchicalInstancedStaticMeshComponent* H, float S0, float S1, float O0, float O1)
		{
			if (bHoriz) Q(H, S0, C + O0, S1, C + O1);
			else Q(H, C + O0, S0, C + O1, S1);
		};
		L(MarkYellow, A0, A1, -0.24f, -0.12f);
		L(MarkYellow, A0, A1, 0.12f, 0.24f);
		L(MarkWhite, A0, A1, HW - 0.45f, HW - 0.3f);
		L(MarkWhite, A0, A1, -HW + 0.3f, -HW + 0.45f);
		const float Lane = HW * 0.5f;
		for (float S = A0 + 1.f; S < A1 - 3.f; S += 9.f)
		{
			L(MarkWhite, S, S + 3.f, Lane - 0.07f, Lane + 0.07f);
			L(MarkWhite, S, S + 3.f, -Lane - 0.07f, -Lane + 0.07f);
		}
	};
	auto Crosswalk = [&](bool bHoriz, float C, float At, float Dir, float HW)
	{
		for (float O = -HW + 0.8f; O < HW - 0.6f; O += 1.15f)
		{
			const float S0 = At + Dir * 1.2f, S1 = At + Dir * 4.6f;
			if (bHoriz) Q(MarkWhite, FMath::Min(S0, S1), C + O, FMath::Max(S0, S1), C + O + 0.55f);
			else Q(MarkWhite, C + O, FMath::Min(S0, S1), C + O + 0.55f, FMath::Max(S0, S1));
		}
	};
	for (int32 K = 0; K <= 8; ++K)
	{
		for (int32 M = 0; M < 8; ++M)
		{
			const float YA = RoadY(M) - RoadHW(M), YB = RoadY(M + 1) + RoadHW(M + 1);
			LaneLines(false, RoadX(K), YB + 5.5f, YA - 5.5f, RW);
			Crosswalk(false, RoadX(K), YA, -1.f, RW);
			Crosswalk(false, RoadX(K), YB, 1.f, RW);
		}
	}
	for (int32 M = 0; M <= 8; ++M)
	{
		const float HW = RoadHW(M);
		for (int32 K = 0; K < 8; ++K)
		{
			const float XA = RoadX(K) + RW, XB = RoadX(K + 1) - RW;
			LaneLines(true, RoadY(M), XA + 5.5f, XB - 5.5f, HW);
			Crosswalk(true, RoadY(M), XA, 1.f, HW);
			Crosswalk(true, RoadY(M), XB, -1.f, HW);
		}
	}
	LaneLines(true, OrlaCY, -OrlaX + 2.f, RoadX(0) - RW - 5.5f, 10.f);
	LaneLines(true, OrlaCY, RoadX(8) + RW + 5.5f, OrlaX - 2.f, 10.f);
	Crosswalk(true, OrlaCY, RoadX(0) - RW, -1.f, 10.f);
	Crosswalk(true, OrlaCY, RoadX(8) + RW, 1.f, 10.f);
}

void AGrauCityGenerator::AddSign(const FVector& LocCm, float YawDeg, const FString& Text, const FColor& Color, float SizeCm)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this, NAME_None, RF_Transient);
	T->CreationMethod = EComponentCreationMethod::UserConstructionScript;
	T->SetupAttachment(Root);
	T->SetRelativeLocation(LocCm);
	T->SetRelativeRotation(FRotator(0.f, YawDeg, 0.f));
	T->SetText(FText::FromString(Text));
	T->SetTextRenderColor(Color);
	T->SetWorldSize(SizeCm);
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetCastShadow(false);
	T->RegisterComponent();
	Signs.Add(T);
}

void AGrauCityGenerator::BuildBuildings(FRandomStream& Rng)
{
	const float RowH[8][2] = { { 48, 135 }, { 24, 70 }, { 14, 44 }, { 12, 36 }, { 8, 28 }, { 8, 24 }, { 6, 20 }, { 6, 18 } };
	int32 SignCount = 0;
	for (int32 I = 0; I < NB; ++I)
	{
		for (int32 J = 0; J < NB; ++J)
		{
			if (SpecialAt(I, J) != ESpecial::None) continue;
			const FBox2D Bl = BlockRect(I, J);
			const float IX0 = Bl.Min.X + 5.f, IX1 = Bl.Max.X - 5.f, IY0 = Bl.Min.Y + 5.f, IY1 = Bl.Max.Y - 5.f;
			const int32 NX = J == 0 ? 2 : 2 + Rng.RandRange(0, 1);
			const int32 NY = J == 0 ? 2 : 2 + Rng.RandRange(0, 1);
			const float LW = (IX1 - IX0) / NX, LD = (IY1 - IY0) / NY;
			for (int32 A = 0; A < NX; ++A)
			{
				for (int32 B = 0; B < NY; ++B)
				{
					if (J > 0 && Rng.FRand() < 0.08f) continue;
					const float Gap = 2.f + Rng.FRand() * 3.f;
					float W = LW - Gap, D = LD - Gap;
					if (J == 0) { W = LW - 6.f - Rng.FRand() * 8.f; D = LD - 6.f - Rng.FRand() * 8.f; }
					const float CX = IX0 + LW * (A + 0.5f) + (Rng.FRand() - 0.5f) * (LW - W - Gap) * 0.5f;
					const float CY = IY0 + LD * (B + 0.5f) + (Rng.FRand() - 0.5f) * (LD - D - Gap) * 0.5f;
					const float H = RowH[J][0] + FMath::Pow(Rng.FRand(), 1.4f) * (RowH[J][1] - RowH[J][0]);
					// estilo: 0 vidro, 1 residencial, 2 comercial (o material usa para o desenho das janelas)
					float Style;
					if (H > 42.f) Style = Rng.FRand() < 0.6f ? 0.f : 1.f;
					else if (H > 15.f) Style = Rng.FRand() < 0.7f ? 1.f : 2.f;
					else Style = Rng.FRand() < 0.6f ? 2.f : 1.f;
					const FLinearColor Tint = Style == 0.f ? FLinearColor(0.9f, 0.95f, 1.f) : Style == 1.f ? Pastel[Rng.RandRange(0, 7)] : Warm[Rng.RandRange(0, 4)];
					const int32 Idx = Box(Buildings, FVector(CX, CY, 0.15f + H * 0.5f), FVector(W, D, H));
					Buildings->SetCustomDataValue(Idx, 0, Tint.R, false);
					Buildings->SetCustomDataValue(Idx, 1, Tint.G, false);
					Buildings->SetCustomDataValue(Idx, 2, Tint.B, false);
					Buildings->SetCustomDataValue(Idx, 3, Style + Rng.FRand() * 0.9f, false);
					// caixas d'água azuis no telhado
					if (H < 40.f)
					{
						const int32 N = 1 + Rng.RandRange(0, 2);
						for (int32 K = 0; K < N; ++K)
						{
							Box(RoofProps, FVector(CX + (Rng.FRand() - 0.5f) * (W - 4.f), CY + (Rng.FRand() - 0.5f) * (D - 4.f), 0.15f + H + 0.7f), FVector(2.2f, 2.2f, 1.4f));
						}
					}
					// letreiro de néon na fachada voltada para a rua mais próxima
					if (SignCount < 48 && Rng.FRand() < 0.4f)
					{
						const float DL = (CX - W * 0.5f) - Bl.Min.X, DR = Bl.Max.X - (CX + W * 0.5f);
						const float DB = (CY - D * 0.5f) - Bl.Min.Y, DF = Bl.Max.Y - (CY + D * 0.5f);
						const float MinD = FMath::Min(FMath::Min(DL, DR), FMath::Min(DB, DF));
						FVector Loc(CX, CY, 4.7f);
						float Yaw = 0.f;
						if (MinD == DF) { Loc.Y = CY + D * 0.5f + 0.3f; Yaw = 90.f; }
						else if (MinD == DB) { Loc.Y = CY - D * 0.5f - 0.3f; Yaw = -90.f; }
						else if (MinD == DR) { Loc.X = CX + W * 0.5f + 0.3f; Yaw = 0.f; }
						else { Loc.X = CX - W * 0.5f - 0.3f; Yaw = 180.f; }
						const FSignDef& S = SignDefs[Rng.RandRange(0, (int32)UE_ARRAY_COUNT(SignDefs) - 1)];
						AddSign(Loc * 100.f, Yaw, S.Text, S.Color, 95.f);
						++SignCount;
					}
				}
			}
		}
	}
}

void AGrauCityGenerator::BuildLamps()
{
	struct FLamp { FVector2D P; FVector2D Dir; };
	TArray<FLamp> Lamps;
	auto Row = [&](bool bHoriz, float C, float A0, float A1, float Side, float Off)
	{
		for (float S = A0; S <= A1; S += 30.f)
		{
			FLamp L;
			L.P = bHoriz ? FVector2D(S, C + Side * Off) : FVector2D(C + Side * Off, S);
			L.Dir = bHoriz ? FVector2D(0.f, -Side) : FVector2D(-Side, 0.f);
			Lamps.Add(L);
		}
	};
	for (int32 K = 0; K <= 8; ++K)
	{
		for (int32 M = 0; M < 8; ++M)
		{
			const float YA = RoadY(M) - RoadHW(M) - 14.f, YB = RoadY(M + 1) + RoadHW(M + 1) + 14.f;
			if (K > 0) Row(false, RoadX(K), YB, YA, -1.f, RW + 0.7f);
			if (K < 8) Row(false, RoadX(K), YB, YA, 1.f, RW + 0.7f);
		}
	}
	for (int32 M = 1; M <= 8; ++M)
	{
		for (int32 K = 0; K < 8; ++K)
		{
			const float XA = RoadX(K) + RW + 14.f, XB = RoadX(K + 1) - RW - 14.f;
			Row(true, RoadY(M), XA, XB, 1.f, RW + 0.7f);
			if (M < 8) Row(true, RoadY(M), XA, XB, -1.f, RW + 0.7f);
		}
	}
	for (int32 K = 0; K < 8; ++K)
	{
		Row(true, OrlaCY, RoadX(K) + RW + 14.f, RoadX(K + 1) - RW - 14.f, -1.f, 10.7f);
	}
	Row(true, OrlaCY, -OrlaX + 10.f, OrlaX - 10.f, 1.f, 10.7f);

	UHierarchicalInstancedStaticMeshComponent* Custom = nullptr;
	if (StreetLampMesh)
	{
		Custom = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		Custom->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		Custom->SetupAttachment(Root);
		Custom->SetStaticMesh(StreetLampMesh);
		Custom->RegisterComponent();
		ModelHisms.Add(Custom);
	}
	for (const FLamp& L : Lamps)
	{
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(L.Dir.Y, L.Dir.X));
		const FVector Base(L.P.X, L.P.Y, 0.15f);
		const FVector D(L.Dir.X, L.Dir.Y, 0.f);
		const FVector Head = Base + D * 2.55f + FVector(0.f, 0.f, 8.2f);
		LampHeadsCm.Add(Head * 100.f);
		if (Custom)
		{
			Custom->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Base * 100.f));
			continue;
		}
		Box(LampPoles, Base + FVector(0.f, 0.f, 4.25f), FVector(0.18f, 0.18f, 8.5f));
		Box(Metal, Base + D * 1.3f + FVector(0.f, 0.f, 8.35f), FVector(2.7f, 0.1f, 0.1f), FRotator(0.f, Yaw, 0.f));
		Box(Metal, Head + FVector(0.f, 0.f, 0.08f), FVector(0.65f, 0.36f, 0.14f), FRotator(0.f, Yaw, 0.f));
		Box(LampHeads, Head, FVector(0.52f, 0.27f, 0.04f), FRotator(0.f, Yaw, 0.f));
	}
}

void AGrauCityGenerator::BuildPalmsAndTrees(FRandomStream& Rng)
{
	TArray<FVector> Palms;
	for (float X = -OrlaX + 8.f; X < OrlaX - 8.f; X += 15.f + Rng.FRand() * 4.f)
	{
		Palms.Add(FVector(X, CalcY1 - 1.2f + (Rng.FRand() - 0.5f) * 0.6f, 0.15f));
	}
	for (int32 K = 0; K < 40; ++K)
	{
		const float Y = 26.f + Rng.FRand() * 12.f;
		Palms.Add(FVector((Rng.FRand() - 0.5f) * 3000.f, Y, 0.1f - (Y - CalcY1) * 0.015f));
	}
	TArray<UHierarchicalInstancedStaticMeshComponent*> PalmHisms;
	for (UStaticMesh* M : PalmMeshes)
	{
		if (!M) continue;
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		H->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		H->SetupAttachment(Root);
		H->SetStaticMesh(M);
		H->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		H->RegisterComponent();
		ModelHisms.Add(H);
		PalmHisms.Add(H);
	}
	for (const FVector& Base : Palms)
	{
		const float Yaw = Rng.FRand() * 360.f;
		const float S = 0.85f + Rng.FRand() * 0.4f;
		if (PalmHisms.Num())
		{
			PalmHisms[Rng.RandRange(0, PalmHisms.Num() - 1)]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), Base * 100.f, FVector(S)));
			continue;
		}
		// coqueiro provisório: tronco inclinado + coroa de folhas em estrela
		const FRotator Tilt(Rng.FRandRange(-7.f, 7.f), Yaw, Rng.FRandRange(-7.f, 7.f));
		const float Hgt = 9.f * S;
		const FVector Top = Base + Tilt.RotateVector(FVector(0.f, 0.f, Hgt));
		Box(Trunks, (Base + Top) * 0.5f, FVector(0.34f, 0.34f, Hgt), Tilt);
		for (int32 L = 0; L < 7; ++L)
		{
			const float LeafYaw = Yaw + L * (360.f / 7.f);
			const FRotator LeafRot(-18.f - Rng.FRand() * 12.f, LeafYaw, 0.f);
			Box(Crowns, Top + LeafRot.RotateVector(FVector(2.4f * S, 0.f, 0.f)), FVector(5.2f * S, 0.9f * S, 0.25f), LeafRot);
		}
	}

	// árvores das calçadas e da praça
	TArray<FVector2D> Trees;
	for (int32 K = 0; K <= 8; ++K)
	{
		for (int32 M = 0; M < 8; ++M)
		{
			if (Rng.FRand() < 0.35f) continue;
			const float YA = RoadY(M) - RoadHW(M) - 30.f, YB = RoadY(M + 1) + RoadHW(M + 1) + 16.f;
			for (float Y = YB; Y < YA; Y += 30.f)
			{
				if (K > 0) Trees.Add(FVector2D(RoadX(K) - RW - 2.6f, Y));
				if (K < 8) Trees.Add(FVector2D(RoadX(K) + RW + 2.6f, Y));
			}
		}
	}
	const FBox2D Praca = BlockRect(4, 3);
	for (int32 K = 0; K < 26; ++K)
	{
		const FVector2D T(Praca.Min.X + 8.f + Rng.FRand() * (Praca.GetSize().X - 16.f), Praca.Min.Y + 8.f + Rng.FRand() * (Praca.GetSize().Y - 16.f));
		if (FVector2D::Distance(T, Praca.GetCenter()) < 12.f) continue;
		Trees.Add(T);
	}
	TArray<UHierarchicalInstancedStaticMeshComponent*> TreeHisms;
	for (UStaticMesh* M : TreeMeshes)
	{
		if (!M) continue;
		UHierarchicalInstancedStaticMeshComponent* H = NewObject<UHierarchicalInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
		H->CreationMethod = EComponentCreationMethod::UserConstructionScript;
		H->SetupAttachment(Root);
		H->SetStaticMesh(M);
		H->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
		H->RegisterComponent();
		ModelHisms.Add(H);
		TreeHisms.Add(H);
	}
	for (const FVector2D& T : Trees)
	{
		const float S = 0.8f + Rng.FRand() * 0.5f;
		const float Yaw = Rng.FRand() * 360.f;
		if (TreeHisms.Num())
		{
			TreeHisms[Rng.RandRange(0, TreeHisms.Num() - 1)]->AddInstance(FTransform(FRotator(0.f, Yaw, 0.f), FVector(T.X, T.Y, 0.15f) * 100.f, FVector(S)));
			continue;
		}
		Box(Trunks, FVector(T.X, T.Y, 0.15f + 2.3f * S), FVector(0.4f * S, 0.4f * S, 4.6f * S));
		Box(Crowns, FVector(T.X, T.Y, 0.15f + 5.8f * S), FVector(5.6f * S, 5.2f * S, 4.4f * S), FRotator(0.f, Yaw, 0.f));
	}
}

void AGrauCityGenerator::BuildRails()
{
	auto Rail = [&](float X0, float Y0, float X1, float Y1)
	{
		const FVector2D A(X0, Y0), B(X1, Y1);
		const float Len = FVector2D::Distance(A, B);
		const float Yaw = FMath::RadiansToDegrees(FMath::Atan2(Y1 - Y0, X1 - X0));
		const FVector2D C = (A + B) * 0.5f;
		Box(Rails, FVector(C.X, C.Y, 0.72f), FVector(Len, 0.06f, 0.32f), FRotator(0.f, Yaw, 0.f));
		const int32 N = FMath::Max(1, FMath::FloorToInt(Len / 4.f));
		for (int32 S = 0; S <= N; ++S)
		{
			const FVector2D Pt = FMath::Lerp(A, B, (float)S / N);
			Box(Metal, FVector(Pt.X, Pt.Y, 0.425f), FVector(0.12f, 0.12f, 0.85f));
		}
	};
	Rail(-OrlaX, OrlaY0 - 0.4f, -CityX, OrlaY0 - 0.4f);
	Rail(CityX, OrlaY0 - 0.4f, OrlaX, OrlaY0 - 0.4f);
	Rail(-CityX - 0.4f, CityYMin, -CityX - 0.4f, OrlaY0);
	Rail(CityX + 0.4f, CityYMin, CityX + 0.4f, OrlaY0);
	Rail(-CityX, CityYMin - 0.4f, CityX, CityYMin - 0.4f);
	Rail(-OrlaX - 0.4f, OrlaY0, -OrlaX - 0.4f, CalcY1 + 30.f);
	Rail(OrlaX + 0.4f, OrlaY0, OrlaX + 0.4f, CalcY1 + 30.f);
}

void AGrauCityGenerator::BuildPosto()
{
	const FBox2D Bl = BlockRect(5, 0);
	const float CX = Bl.GetCenter().X;
	const float CanopyY = -32.f;
	PostoCenter = FVector2D(CX, CanopyY);
	Box(Concrete, FVector(CX, CanopyY, 6.0f), FVector(32.f, 17.f, 0.9f));
	for (float S : { -1.f, 1.f })
	{
		Box(Orange, FVector(CX, CanopyY + S * 8.6f, 6.0f), FVector(32.2f, 0.2f, 0.5f));
	}
	for (int32 A = -3; A <= 3; ++A)
	{
		for (int32 B = -1; B <= 1; ++B)
		{
			Box(LightPanels, FVector(CX + A * 4.2f, CanopyY + B * 4.8f, 5.53f), FVector(1.6f, 1.6f, 0.05f));
		}
	}
	for (const FVector2D& O : { FVector2D(-11.f, -4.5f), FVector2D(11.f, -4.5f), FVector2D(-11.f, 4.5f), FVector2D(11.f, 4.5f) })
	{
		Box(Concrete, FVector(CX + O.X, CanopyY + O.Y, 2.95f), FVector(0.7f, 0.7f, 5.6f));
	}
	for (float DX : { -6.f, 6.f })
	{
		Box(Concrete, FVector(CX + DX, CanopyY, 0.27f), FVector(1.4f, 6.f, 0.25f));
		for (float DY : { -1.6f, 1.6f })
		{
			Box(Concrete, FVector(CX + DX, CanopyY + DY, 1.25f), FVector(0.9f, 1.f, 1.7f));
			Box(LightPanels, FVector(CX + DX, CanopyY + DY, 1.7f), FVector(0.92f, 0.6f, 0.35f));
		}
	}
	// loja de conveniência
	const int32 Store = Box(Buildings, FVector(CX, -78.f, 0.15f + 2.3f), FVector(26.f, 12.f, 4.6f));
	Buildings->SetCustomDataValue(Store, 0, 1.f, false);
	Buildings->SetCustomDataValue(Store, 1, 0.95f, false);
	Buildings->SetCustomDataValue(Store, 2, 0.9f, false);
	Buildings->SetCustomDataValue(Store, 3, 2.95f, false);
	Box(Orange, FVector(CX, -71.8f, 5.3f), FVector(14.f, 0.2f, 1.1f));
	AddSign(FVector(CX, -71.6f, 5.3f) * 100.f, 90.f, TEXT("CONVENIÊNCIA"), FColor(255, 255, 255), 80.f);
	// totem de preços
	Box(Orange, FVector(Bl.Max.X - 4.f, Bl.Max.Y - 3.f, 6.4f), FVector(0.5f, 3.2f, 6.4f));
	Box(Concrete, FVector(Bl.Max.X - 4.f, Bl.Max.Y - 3.f, 1.7f), FVector(0.5f, 0.5f, 3.4f));
	AddSign(FVector(Bl.Max.X - 3.7f, Bl.Max.Y - 3.f, 8.6f) * 100.f, 0.f, TEXT("POSTO"), FColor(255, 255, 255), 90.f);
	AddSign(FVector(Bl.Max.X - 3.7f, Bl.Max.Y - 3.f, 7.6f) * 100.f, 0.f, TEXT("BRASA"), FColor(255, 255, 255), 90.f);
	AddSign(FVector(Bl.Max.X - 3.7f, Bl.Max.Y - 3.f, 5.9f) * 100.f, 0.f, TEXT("GASOLINA 6,19"), FColor(255, 207, 58), 45.f);
	AddSign(FVector(Bl.Max.X - 3.7f, Bl.Max.Y - 3.f, 5.2f) * 100.f, 0.f, TEXT("ETANOL 4,29"), FColor(255, 207, 58), 45.f);
	PostoLight->SetRelativeLocation(FVector(CX, CanopyY, 5.2f) * 100.f);
}

void AGrauCityGenerator::BuildPista()
{
	const FBox2D Bl = BlockRect(1, 2);
	const FVector2D C = Bl.GetCenter();
	PistaCenter = C;
	struct FRamp { FVector2D P; float Yaw; float L; float H; };
	const FRamp Ramps[] = { { FVector2D(C.X - 22.f, C.Y + 22.f), 0.f, 7.f, 1.3f }, { FVector2D(C.X + 22.f, C.Y - 24.f), 180.f, 8.f, 1.7f } };
	for (const FRamp& R : Ramps)
	{
		// rampa = placa inclinada; a ponta baixa fica enterrada na pista
		const float Ang = FMath::RadiansToDegrees(FMath::Atan2(R.H, R.L));
		const float Len = FMath::Sqrt(R.L * R.L + R.H * R.H);
		const FVector Dir(FMath::Cos(FMath::DegreesToRadians(R.Yaw)), FMath::Sin(FMath::DegreesToRadians(R.Yaw)), 0.f);
		const FVector Mid = FVector(R.P.X, R.P.Y, 0.15f) + Dir * (R.L * 0.5f) + FVector(0.f, 0.f, R.H * 0.5f - 0.15f);
		Box(Metal, Mid, FVector(Len, 4.4f, 0.3f), FRotator(Ang, R.Yaw, 0.f));
		Box(Orange, FVector(R.P.X, R.P.Y, 0.f) + Dir * (R.L - 0.15f) + FVector(0.f, 0.f, 0.15f + R.H + 0.01f), FVector(0.25f, 4.4f, 0.06f), FRotator(0.f, R.Yaw, 0.f));
	}
	for (int32 K = 0; K < 10; ++K)
	{
		Box(Cones, FVector(Bl.Min.X + 10.f + K * 7.5f, C.Y - 2.f, 0.15f + 0.35f), FVector(0.45f, 0.45f, 0.7f));
	}
}

void AGrauCityGenerator::BuildPraca()
{
	const FBox2D Bl = BlockRect(4, 3);
	const FVector2D C = Bl.GetCenter();
	PracaCenter = C;
	for (int32 K = 0; K < 4; ++K)
	{
		Box(Concrete, FVector(C.X, C.Y, 0.5f), FVector(10.f, 4.2f, 0.7f), FRotator(0.f, K * 45.f, 0.f));
	}
	Box(Concrete, FVector(C.X, C.Y, 1.6f), FVector(1.f, 1.f, 2.2f));
	for (const FVector2D& O : { FVector2D(-24.f, -24.f), FVector2D(24.f, -24.f), FVector2D(-24.f, 24.f), FVector2D(24.f, 24.f) })
	{
		Box(Grass, FVector(C.X + O.X, C.Y + O.Y, 0.16f), FVector(26.f, 26.f, 0.02f));
	}
}

void AGrauCityGenerator::BuildHills(FRandomStream& Rng)
{
	TArray<FMountain> Mounts;
	// morro colado na estrada da orla (como no vídeo) dos dois lados da cidade
	for (float Side : { -1.f, 1.f })
	{
		for (float X = CityX + 80.f; X < OrlaX + 200.f; X += 110.f + Rng.FRand() * 60.f)
		{
			const float R = 120.f + Rng.FRand() * 70.f;
			const float Z = -R * (0.45f + Rng.FRand() * 0.2f);
			const float Half = FMath::Sqrt(FMath::Max(R * R - Z * Z, 1.f));
			Mounts.Add({ FVector2D(Side * X, OrlaY0 - 5.f - Half), R, Z });
		}
	}
	// serra atrás da cidade
	for (float X = -1800.f; X <= 1800.f; X += 220.f + Rng.FRand() * 120.f)
	{
		const float R = 280.f + Rng.FRand() * 260.f;
		Mounts.Add({ FVector2D(X, CityYMin - 120.f - R * 0.55f - Rng.FRand() * 200.f), R, -R * 0.5f });
	}
	for (const FMountain& M : Mounts)
	{
		Box(Hills, FVector(M.C.X, M.C.Y, M.Z), FVector(M.R * 2.f, M.R * 2.f * (0.9f + Rng.FRand() * 0.3f), M.R * 2.f), FRotator(0.f, Rng.FRand() * 360.f, 0.f));
	}
	// morros de granito na baía (pão de açúcar), a sudeste, onde o sol se põe
	const FVector4 Rocks[] = { FVector4(1500.f, 2300.f, 230.f, 300.f), FVector4(1850.f, 2450.f, 150.f, 160.f), FVector4(-1500.f, 2100.f, 300.f, 260.f), FVector4(2600.f, 1200.f, 380.f, 420.f) };
	for (const FVector4& R : Rocks)
	{
		Box(Hills, FVector(R.X, R.Y, -8.f), FVector(R.Z * 2.f, R.Z * 1.8f, R.W * 2.f));
	}
	// comunidade nas encostas atrás da cidade
	auto HeightAt = [&](float X, float Y)
	{
		float Best = -1000.f;
		for (const FMountain& M : Mounts)
		{
			const float D2 = FVector2D::DistSquared(FVector2D(X, Y), M.C);
			if (D2 < M.R * M.R) Best = FMath::Max(Best, M.Z + FMath::Sqrt(M.R * M.R - D2));
		}
		return Best;
	};
	int32 Placed = 0;
	for (int32 K = 0; K < 6000 && Placed < 900; ++K)
	{
		const float X = -760.f + Rng.FRand() * 1520.f;
		const float Y = CityYMin - 40.f - Rng.FRand() * 520.f;
		const float H = HeightAt(X, Y);
		if (H < 6.f || H > 160.f) continue;
		const FVector Size(4.f + Rng.FRand() * 4.f, 4.f + Rng.FRand() * 4.f, 3.f + Rng.FRand() * 6.f);
		const int32 Idx = Box(Favela, FVector(X, Y, H - 1.5f + Size.Z * 0.5f), Size, FRotator(0.f, Rng.FRandRange(-12.f, 12.f), 0.f));
		const bool bRaw = Rng.FRand() < 0.35f;
		const FLinearColor Col = bRaw ? FLinearColor(0.6f, 0.35f, 0.23f) : FavelaColors[Rng.RandRange(0, 7)];
		Favela->SetCustomDataValue(Idx, 0, Col.R, false);
		Favela->SetCustomDataValue(Idx, 1, Col.G, false);
		Favela->SetCustomDataValue(Idx, 2, Col.B, false);
		Favela->SetCustomDataValue(Idx, 3, 1.f + Rng.FRand() * 0.9f, false);
		++Placed;
	}
}

void AGrauCityGenerator::BuildFarCity(FRandomStream& Rng)
{
	const FVector4 Clusters[] = { FVector4(1900.f, 2600.f, 140.f, 900.f), FVector4(3300.f, 1800.f, 70.f, 600.f), FVector4(-200.f, 3100.f, 60.f, 700.f), FVector4(-1800.f, 2600.f, 40.f, 600.f) };
	for (const FVector4& C : Clusters)
	{
		for (int32 K = 0; K < (int32)C.Z; ++K)
		{
			const float Off = (Rng.FRand() - 0.5f) * C.W * 2.f;
			const float Center = 1.f - FMath::Abs(Off) / C.W;
			const float H = 15.f + Rng.FRand() * 45.f + Center * Center * Rng.FRand() * 140.f;
			const FVector Size(18.f + Rng.FRand() * 30.f, 18.f + Rng.FRand() * 30.f, H);
			const int32 Idx = Box(FarCity, FVector(C.X + Off, C.Y + (Rng.FRand() - 0.5f) * 180.f, -1.f + H * 0.5f), Size, FRotator(0.f, Rng.FRand() * 180.f, 0.f));
			FarCity->SetCustomDataValue(Idx, 0, 0.35f, false);
			FarCity->SetCustomDataValue(Idx, 1, 0.36f, false);
			FarCity->SetCustomDataValue(Idx, 2, 0.4f, false);
			FarCity->SetCustomDataValue(Idx, 3, Rng.FRand() * 0.9f, false);
		}
	}
}

void AGrauCityGenerator::BuildGraph()
{
	TMap<int32, int32> Index;
	auto Key = [](int32 K, int32 M) { return K * 100 + M; };
	for (int32 K = 0; K <= 8; ++K)
	{
		for (int32 M = 0; M <= 8; ++M)
		{
			Index.Add(Key(K, M), Nodes.Num());
			FGrauRoadNode N;
			N.P = FVector2D(RoadX(K), RoadY(M));
			Nodes.Add(N);
		}
	}
	const float Ext[] = { -1500.f, -1000.f, 1000.f, 1500.f };
	int32 ExtIdx[4];
	for (int32 E = 0; E < 4; ++E)
	{
		ExtIdx[E] = Nodes.Num();
		FGrauRoadNode N;
		N.P = FVector2D(Ext[E], OrlaCY);
		Nodes.Add(N);
	}
	auto Link = [&](int32 A, int32 B, float HW)
	{
		Nodes[A].Links.Add(B); Nodes[A].HalfWidth.Add(HW);
		Nodes[B].Links.Add(A); Nodes[B].HalfWidth.Add(HW);
	};
	for (int32 K = 0; K <= 8; ++K)
	{
		for (int32 M = 0; M <= 8; ++M)
		{
			const int32 A = Index[Key(K, M)];
			if (K < 8) Link(A, Index[Key(K + 1, M)], RoadHW(M));
			if (M < 8) Link(A, Index[Key(K, M + 1)], RW);
		}
	}
	Link(ExtIdx[0], ExtIdx[1], 10.f);
	Link(ExtIdx[1], Index[Key(0, 0)], 10.f);
	Link(Index[Key(8, 0)], ExtIdx[2], 10.f);
	Link(ExtIdx[2], ExtIdx[3], 10.f);
}

void AGrauCityGenerator::ApplyMaterials()
{
	Mids.Reset();
	auto Set = [&](UPrimitiveComponent* C, UMaterialInstanceDynamic* M)
	{
		if (C && M)
		{
			C->SetMaterial(0, M);
			Mids.Add(M);
		}
	};
	Set(Road, FGrauAssets::Surface(this, FLinearColor(0.035f, 0.035f, 0.038f), 0.55f, 0.f, 0.85f));
	Set(Sidewalk, FGrauAssets::Surface(this, FLinearColor(0.3f, 0.29f, 0.27f), 0.7f, 0.f, 0.4f));
	UMaterialInstanceDynamic* Calc = FGrauAssets::Surface(this, FLinearColor(0.55f, 0.53f, 0.5f), 0.5f, 0.f, 0.35f);
	if (Calc) Calc->SetScalarParameterValue(TEXT("Pattern"), 1.f); // ondas do calçadão
	Set(Calcadao, Calc);
	Set(Sand, FGrauAssets::Surface(this, FLinearColor(0.42f, 0.33f, 0.22f), 0.92f, 0.f, 0.f));
	Set(MarkWhite, FGrauAssets::Surface(this, FLinearColor(0.75f, 0.75f, 0.72f), 0.4f, 0.f, 0.6f));
	Set(MarkYellow, FGrauAssets::Surface(this, FLinearColor(0.8f, 0.48f, 0.04f), 0.4f, 0.f, 0.6f));
	UMaterialInstanceDynamic* Bld = FGrauAssets::Building(this);
	Set(Buildings, Bld);
	Set(Favela, Bld);
	Set(FarCity, Bld);
	Set(RoofProps, FGrauAssets::Surface(this, FLinearColor(0.06f, 0.2f, 0.55f), 0.35f));
	UMaterialInstanceDynamic* Dark = FGrauAssets::Surface(this, FLinearColor(0.06f, 0.06f, 0.065f), 0.4f, 0.8f);
	Set(LampPoles, Dark);
	Set(Metal, Dark);
	Set(LampHeads, FGrauAssets::Emissive(this, FLinearColor(1.f, 0.72f, 0.42f), 60.f));
	Set(Trunks, FGrauAssets::Surface(this, FLinearColor(0.16f, 0.12f, 0.09f), 0.9f));
	Set(Crowns, FGrauAssets::Surface(this, FLinearColor(0.03f, 0.07f, 0.025f), 0.8f));
	Set(Rails, FGrauAssets::Surface(this, FLinearColor(0.55f, 0.57f, 0.6f), 0.35f, 1.f));
	Set(Concrete, FGrauAssets::Surface(this, FLinearColor(0.62f, 0.62f, 0.6f), 0.6f));
	Set(Orange, FGrauAssets::Paint(this, FLinearColor(1.f, 0.22f, 0.02f), 0.2f, 0.35f));
	Set(LightPanels, FGrauAssets::Emissive(this, FLinearColor(0.95f, 0.97f, 1.f), 50.f));
	Set(Grass, FGrauAssets::Surface(this, FLinearColor(0.03f, 0.06f, 0.02f), 0.95f));
	Set(Hills, FGrauAssets::Surface(this, FLinearColor(0.025f, 0.04f, 0.02f), 0.95f));
	Set(Cones, FGrauAssets::Surface(this, FLinearColor(1.f, 0.25f, 0.02f), 0.5f));
	Set(Sea, FGrauAssets::Water(this));
}

void AGrauCityGenerator::BeginPlay()
{
	Super::BeginPlay();
	// poucas luzes reais que acompanham o jogador pelos postes mais próximos
	for (int32 K = 0; K < StreetLightPool; ++K)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(Root);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Lumens);
		L->SetIntensity(9000.f);
		L->SetAttenuationRadius(2600.f);
		L->SetLightColor(FLinearColor(1.f, 0.72f, 0.45f));
		L->SetSourceRadius(15.f);
		L->SetCastShadows(K < 6);
		L->RegisterComponent();
		LightPool.Add(L);
	}
	LightPoolTimer = 0.f;
}

void AGrauCityGenerator::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	LightPoolTimer -= DeltaSeconds;
	if (LightPoolTimer > 0.f || LightPool.Num() == 0 || LampHeadsCm.Num() == 0)
	{
		return;
	}
	LightPoolTimer = 0.4f;
	const APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	if (!Player)
	{
		return;
	}
	const FVector Pp = GetActorTransform().InverseTransformPosition(Player->GetActorLocation());
	TArray<TPair<float, int32>> Near;
	Near.Reserve(LampHeadsCm.Num());
	for (int32 I = 0; I < LampHeadsCm.Num(); ++I)
	{
		Near.Add(TPair<float, int32>(FVector::DistSquared2D(Pp, LampHeadsCm[I]), I));
	}
	const int32 N = FMath::Min(LightPool.Num(), Near.Num());
	Algo::Sort(Near, [](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	for (int32 K = 0; K < N; ++K)
	{
		LightPool[K]->SetRelativeLocation(LampHeadsCm[Near[K].Value] - FVector(0.f, 0.f, 25.f));
	}
}
