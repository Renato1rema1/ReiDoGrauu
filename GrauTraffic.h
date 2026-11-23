#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrauTraffic.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USpotLightComponent;
class UStaticMesh;
class UMaterialInstanceDynamic;
class AGrauCityGenerator;
class AGrauBike;

struct FGrauCarHit
{
	float Impact = 0.f;                       // velocidade relativa contra a moto (m/s)
	FVector2D Normal = FVector2D::ZeroVector; // empurra a moto para fora do carro
	float PenCm = 0.f;
};

// Um carro do trânsito. A lógica fica no AGrauTrafficManager; aqui só visual e colisão.
UCLASS()
class REIDOGRAU_API AGrauTrafficCar : public AActor
{
	GENERATED_BODY()

public:
	AGrauTrafficCar();

	void SetupLook(FRandomStream& Rng, UStaticMesh* OverrideMesh);
	FVector GetCarVelocityMS() const { return FVector(FMath::Cos(Yaw), FMath::Sin(Yaw), 0.f) * Speed; }

	UPROPERTY(VisibleAnywhere, Category = "Grau") UBoxComponent* Box;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USpotLightComponent* Beam;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UStaticMeshComponent* OverrideMeshComp;

	// estado de direção (metros / radianos, Yaw 0 = leste)
	FVector2D Pos = FVector2D::ZeroVector;
	float Yaw = 0.f;
	float Speed = 0.f;
	float Cruise = 13.f;
	int32 From = 0;
	int32 To = 0;
	float HW = 8.f;
	int32 Lane = 0;
	FVector2D Start = FVector2D::ZeroVector;
	FVector2D End = FVector2D::ZeroVector;
	FVector2D Dir = FVector2D(1.f, 0.f);
	float NearT = 0.f;

private:
	UStaticMeshComponent* AddPart(UStaticMesh* Mesh, const FVector& LocCm, const FVector& SizeCm, uint8 Kind);

	TArray<UStaticMeshComponent*> Parts;
	TArray<uint8> Kinds;
	UPROPERTY(Transient) TArray<UMaterialInstanceDynamic*> Mids;
};

// Trânsito da cidade: carros que seguem as faixas, param atrás de outros e da moto.
UCLASS()
class REIDOGRAU_API AGrauTrafficManager : public AActor
{
	GENERATED_BODY()

public:
	AGrauTrafficManager();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(EditAnywhere, Category = "Grau|Transito", meta = (ClampMin = "0", ClampMax = "80")) int32 CarCount = 26;
	// Modelos de carro (frente apontando para +X, origem no chão). Vazio = carros provisórios.
	UPROPERTY(EditAnywhere, Category = "Grau|Transito") TArray<UStaticMesh*> CarMeshes;

	static AGrauTrafficManager* Get(const UWorld* World);
	bool HitBike(const FVector& RearCm, const FVector& Fwd, float SpeedMS, FGrauCarHit& Out) const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void PickEdge(AGrauTrafficCar* Car, int32 FromNode, int32 Avoid);
	void ComputeWaypoints(AGrauTrafficCar* Car) const;
	void SpawnCars();

	UPROPERTY(Transient) TArray<AGrauTrafficCar*> Cars;
	TWeakObjectPtr<AGrauCityGenerator> City;
	FRandomStream Rng;
};
