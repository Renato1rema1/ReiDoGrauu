#pragma once

#include "CoreMinimal.h"

class UStaticMesh;
class UMaterialInterface;
class UMaterialInstanceDynamic;

// Acesso centralizado às formas básicas da engine e aos materiais-mestre criados pelo script
// Content/Python/setup_portobrasa.py. Se o script ainda não rodou, cai no material básico da engine.
struct REIDOGRAU_API FGrauAssets
{
	static UStaticMesh* Cube();
	static UStaticMesh* Cylinder();
	static UStaticMesh* Sphere();
	static UStaticMesh* Cone();
	static UStaticMesh* Plane();

	// Superfície PBR genérica (asfalto, calçada, plástico, metal...). Wetness liga as poças.
	static UMaterialInstanceDynamic* Surface(UObject* Outer, const FLinearColor& Color, float Roughness = 0.5f, float Metallic = 0.f, float Wetness = 0.f);
	// Luz emissiva (faróis, postes, letreiros)
	static UMaterialInstanceDynamic* Emissive(UObject* Outer, const FLinearColor& Color, float Strength);
	// Pintura com verniz (moto e carros)
	static UMaterialInstanceDynamic* Paint(UObject* Outer, const FLinearColor& Color, float Metallic = 0.55f, float Roughness = 0.3f);
	// Fachadas com janelas procedurais (cor e chance de luz por instância)
	static UMaterialInstanceDynamic* Building(UObject* Outer);
	// Mar
	static UMaterialInstanceDynamic* Water(UObject* Outer);
	// Brilho translúcido aditivo (marcadores de desafio)
	static UMaterialInstanceDynamic* Glow(UObject* Outer, const FLinearColor& Color, float Strength);

	static bool HasProjectMaterials();
};
