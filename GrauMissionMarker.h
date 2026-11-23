#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrauMissionMarker.generated.h"

class UStaticMeshComponent;

// Coluna de luz + anel no chão, estilo GTA, marcando desafios e pontos de corrida.
UCLASS()
class REIDOGRAU_API AGrauMissionMarker : public AActor
{
	GENERATED_BODY()

public:
	AGrauMissionMarker();
	virtual void Tick(float DeltaSeconds) override;

	void SetColor(const FLinearColor& Color, float RadiusM);
	void SetShown(bool bShow);

	UPROPERTY(VisibleAnywhere, Category = "Grau") UStaticMeshComponent* Beam;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UStaticMeshComponent* Ring;

private:
	float BaseRadius = 1.8f;
};
