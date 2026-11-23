#include "GrauAssets.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	const TCHAR* PathSurface = TEXT("/Game/ReiDoGrau/Materials/M_Grau_Surface.M_Grau_Surface");
	const TCHAR* PathPaint = TEXT("/Game/ReiDoGrau/Materials/M_Grau_CarPaint.M_Grau_CarPaint");
	const TCHAR* PathBuilding = TEXT("/Game/ReiDoGrau/Materials/M_Grau_Building.M_Grau_Building");
	const TCHAR* PathWater = TEXT("/Game/ReiDoGrau/Materials/M_Grau_Water.M_Grau_Water");
	const TCHAR* PathGlow = TEXT("/Game/ReiDoGrau/Materials/M_Grau_Glow.M_Grau_Glow");
	const TCHAR* PathBasic = TEXT("/Engine/BasicShapes/BasicShapeMaterial.BasicShapeMaterial");

	UMaterialInterface* LoadMat(const TCHAR* Path)
	{
		return LoadObject<UMaterialInterface>(nullptr, Path, nullptr, LOAD_NoWarn | LOAD_Quiet);
	}

	UMaterialInstanceDynamic* BasicFallback(UObject* Outer, const FLinearColor& Color)
	{
		UMaterialInterface* Base = LoadMat(PathBasic);
		if (!Base)
		{
			return nullptr;
		}
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Base, Outer);
		Mid->SetVectorParameterValue(TEXT("Color"), Color);
		return Mid;
	}
}

UStaticMesh* FGrauAssets::Cube() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube")); }
UStaticMesh* FGrauAssets::Cylinder() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cylinder.Cylinder")); }
UStaticMesh* FGrauAssets::Sphere() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")); }
UStaticMesh* FGrauAssets::Cone() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cone.Cone")); }
UStaticMesh* FGrauAssets::Plane() { return LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane")); }

bool FGrauAssets::HasProjectMaterials()
{
	return LoadMat(PathSurface) != nullptr;
}

UMaterialInstanceDynamic* FGrauAssets::Surface(UObject* Outer, const FLinearColor& Color, float Roughness, float Metallic, float Wetness)
{
	if (UMaterialInterface* Master = LoadMat(PathSurface))
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Master, Outer);
		Mid->SetVectorParameterValue(TEXT("BaseColor"), Color);
		Mid->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		Mid->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		Mid->SetScalarParameterValue(TEXT("Wetness"), Wetness);
		Mid->SetVectorParameterValue(TEXT("EmissiveColor"), FLinearColor::Black);
		Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), 0.f);
		return Mid;
	}
	return BasicFallback(Outer, Color);
}

UMaterialInstanceDynamic* FGrauAssets::Emissive(UObject* Outer, const FLinearColor& Color, float Strength)
{
	if (UMaterialInterface* Master = LoadMat(PathSurface))
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Master, Outer);
		Mid->SetVectorParameterValue(TEXT("BaseColor"), FLinearColor(0.02f, 0.02f, 0.02f));
		Mid->SetScalarParameterValue(TEXT("Roughness"), 0.3f);
		Mid->SetScalarParameterValue(TEXT("Metallic"), 0.f);
		Mid->SetScalarParameterValue(TEXT("Wetness"), 0.f);
		Mid->SetVectorParameterValue(TEXT("EmissiveColor"), Color);
		Mid->SetScalarParameterValue(TEXT("EmissiveStrength"), Strength);
		return Mid;
	}
	return BasicFallback(Outer, Color);
}

UMaterialInstanceDynamic* FGrauAssets::Paint(UObject* Outer, const FLinearColor& Color, float Metallic, float Roughness)
{
	if (UMaterialInterface* Master = LoadMat(PathPaint))
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Master, Outer);
		Mid->SetVectorParameterValue(TEXT("PaintColor"), Color);
		Mid->SetScalarParameterValue(TEXT("Metallic"), Metallic);
		Mid->SetScalarParameterValue(TEXT("Roughness"), Roughness);
		return Mid;
	}
	return Surface(Outer, Color, Roughness, Metallic);
}

UMaterialInstanceDynamic* FGrauAssets::Building(UObject* Outer)
{
	if (UMaterialInterface* Master = LoadMat(PathBuilding))
	{
		return UMaterialInstanceDynamic::Create(Master, Outer);
	}
	return Surface(Outer, FLinearColor(0.55f, 0.5f, 0.45f), 0.8f);
}

UMaterialInstanceDynamic* FGrauAssets::Water(UObject* Outer)
{
	if (UMaterialInterface* Master = LoadMat(PathWater))
	{
		return UMaterialInstanceDynamic::Create(Master, Outer);
	}
	return Surface(Outer, FLinearColor(0.01f, 0.03f, 0.04f), 0.05f, 0.f);
}

UMaterialInstanceDynamic* FGrauAssets::Glow(UObject* Outer, const FLinearColor& Color, float Strength)
{
	if (UMaterialInterface* Master = LoadMat(PathGlow))
	{
		UMaterialInstanceDynamic* Mid = UMaterialInstanceDynamic::Create(Master, Outer);
		Mid->SetVectorParameterValue(TEXT("GlowColor"), Color);
		Mid->SetScalarParameterValue(TEXT("GlowStrength"), Strength);
		return Mid;
	}
	return Emissive(Outer, Color, Strength);
}
