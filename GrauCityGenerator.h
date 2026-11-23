#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrauCityGenerator.generated.h"

class UHierarchicalInstancedStaticMeshComponent;
class UStaticMeshComponent;
class UPointLightComponent;
class UTextRenderComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;

// Layout da cidade em metros (o mesmo da versão web)
namespace GrauLayout
{
	constexpr float P = 106.f;          // passo entre ruas
	constexpr float RW = 8.f;           // meia largura das ruas
	constexpr int32 NB = 8;             // quarteirões por lado
	constexpr float OrlaY0 = -8.f;
	constexpr float OrlaY1 = 12.f;
	constexpr float OrlaCY = 2.f;
	constexpr float OrlaX = 1560.f;
	constexpr float CalcY1 = 20.f;
	constexpr float SeaY = 50.f;
	constexpr float CityX = 4.f * 106.f + 8.f;
	constexpr float CityYMin = -8.f * 106.f - 8.f;

	inline float RoadX(int32 K) { return (K - 4) * P; }
	inline float RoadY(int32 M) { return M == 0 ? OrlaCY : -M * P; }
	inline float RoadHW(int32 M) { return M == 0 ? 10.f : RW; }
	inline FBox2D BlockRect(int32 I, int32 J)
	{
		return FBox2D(FVector2D((I - 4) * P + RW, -(J + 1) * P + RW), FVector2D((I - 3) * P - RW, -J * P - RW));
	}
}

// Nó da malha de ruas (em metros) para o trânsito
struct FGrauRoadNode
{
	FVector2D P = FVector2D::ZeroVector;
	TArray<int32> Links;
	TArray<float> HalfWidth;
};

// Trecho de rua para o minimapa (metros)
struct FGrauRoadSeg
{
	FVector2D A = FVector2D::ZeroVector;
	FVector2D B = FVector2D::ZeroVector;
	float HalfWidth = 8.f;
};

// Gera Porto Brasa: orla com calçadão de ondas, malha de ruas, prédios com janelas acesas,
// posto, pista de grau com rampas, praça, morros, comunidade, mar e a cidade do outro lado da baía.
// Tudo com instâncias (HISM), então roda leve. Coloque modelos realistas nas listas "Grau|Modelos".
// Eixos: X = leste, Y = sul (mar), Z = cima. 1 unidade = 1 cm.
UCLASS()
class REIDOGRAU_API AGrauCityGenerator : public AActor
{
	GENERATED_BODY()

public:
	AGrauCityGenerator();

	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

public:
	UPROPERTY(EditAnywhere, Category = "Grau|Cidade") int32 Seed = 1987;
	UPROPERTY(EditAnywhere, Category = "Grau|Cidade") bool bBuildHills = true;
	UPROPERTY(EditAnywhere, Category = "Grau|Cidade") bool bBuildFarCity = true;
	UPROPERTY(EditAnywhere, Category = "Grau|Cidade", meta = (ClampMin = "0", ClampMax = "64")) int32 StreetLightPool = 32;

	// Modelos realistas opcionais (Fab / Megascans). Vazio = formas provisórias.
	UPROPERTY(EditAnywhere, Category = "Grau|Modelos") TArray<UStaticMesh*> PalmMeshes;
	UPROPERTY(EditAnywhere, Category = "Grau|Modelos") TArray<UStaticMesh*> TreeMeshes;
	UPROPERTY(EditAnywhere, Category = "Grau|Modelos") TArray<UStaticMesh*> RockMeshes;
	UPROPERTY(EditAnywhere, Category = "Grau|Modelos") UStaticMesh* StreetLampMesh = nullptr;

	UPROPERTY(EditAnywhere, Category = "Grau|Partida") float SpawnYaw = 0.f;
	FVector GetSpawnLocation() const;

	// dados para trânsito, minimapa e desafios (metros)
	TArray<FGrauRoadNode> Nodes;
	TArray<FGrauRoadSeg> Roads;
	TArray<FBox2D> Blocks;
	FVector2D PostoCenter = FVector2D::ZeroVector;
	FVector2D PistaCenter = FVector2D::ZeroVector;
	FVector2D PracaCenter = FVector2D::ZeroVector;

private:
	UHierarchicalInstancedStaticMeshComponent* MakeHism(const TCHAR* Name, UStaticMesh* Mesh, bool bCollide, bool bShadows = true);
	void Rebuild();
	void ClearAll();
	void BuildGround();
	void BuildMarkings();
	void BuildBuildings(FRandomStream& Rng);
	void BuildLamps();
	void BuildPalmsAndTrees(FRandomStream& Rng);
	void BuildRails();
	void BuildPosto();
	void BuildPista();
	void BuildPraca();
	void BuildHills(FRandomStream& Rng);
	void BuildFarCity(FRandomStream& Rng);
	void BuildGraph();
	void ApplyMaterials();
	void AddSign(const FVector& LocCm, float YawDeg, const FString& Text, const FColor& Color, float SizeCm);

	// atalho: cubo/cilindro/esfera em metros (centro, tamanho, rotação)
	int32 Box(UHierarchicalInstancedStaticMeshComponent* H, const FVector& CenterM, const FVector& SizeM, const FRotator& Rot = FRotator::ZeroRotator);

	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* Root;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Road;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Sidewalk;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Calcadao;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Sand;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* MarkWhite;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* MarkYellow;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Buildings;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* RoofProps;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* LampPoles;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* LampHeads;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Trunks;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Crowns;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Rails;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Concrete;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Metal;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Orange;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* LightPanels;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Grass;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Hills;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Favela;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* FarCity;
	UPROPERTY() UHierarchicalInstancedStaticMeshComponent* Cones;
	UPROPERTY() UStaticMeshComponent* Sea;
	UPROPERTY() UPointLightComponent* PostoLight;

	UPROPERTY(Transient) TArray<UTextRenderComponent*> Signs;
	UPROPERTY(Transient) TArray<UHierarchicalInstancedStaticMeshComponent*> ModelHisms;
	UPROPERTY(Transient) TArray<UPointLightComponent*> LightPool;
	UPROPERTY(Transient) TArray<UMaterialInstanceDynamic*> Mids;

	TArray<FVector> LampHeadsCm;
	float LightPoolTimer = 0.f;
};
