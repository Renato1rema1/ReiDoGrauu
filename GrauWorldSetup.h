#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GrauWorldSetup.generated.h"

class UDirectionalLightComponent;
class USkyAtmosphereComponent;
class USkyLightComponent;
class UExponentialHeightFogComponent;
class UVolumetricCloudComponent;
class UPostProcessComponent;

// Ambiente do pôr do sol num ator só: sol baixo no sudeste (atrás da cidade do outro lado da baía),
// atmosfera física, nuvens volumétricas, neblina com luz volumétrica, céu em tempo real e
// pós-processamento de cinema (Lumen, bloom, motion blur, grão, aberração cromática).
UCLASS()
class REIDOGRAU_API AGrauWorldSetup : public AActor
{
	GENERATED_BODY()

public:
	AGrauWorldSetup();
	virtual void OnConstruction(const FTransform& Transform) override;

	// Altura do sol em graus. Entre 3 e 7 dá o laranja do vídeo; abaixo de 0 vira noite.
	UPROPERTY(EditAnywhere, Category = "Grau|Ambiente", meta = (ClampMin = "-20", ClampMax = "60")) float SunElevation = 5.f;
	// Direção do sol (graus, 0 = leste, 90 = sul/mar)
	UPROPERTY(EditAnywhere, Category = "Grau|Ambiente") float SunAzimuth = 53.f;
	UPROPERTY(EditAnywhere, Category = "Grau|Ambiente") float SunIntensityLux = 8.f;
	UPROPERTY(EditAnywhere, Category = "Grau|Ambiente") float FogDensity = 0.012f;
	UPROPERTY(EditAnywhere, Category = "Grau|Ambiente") float ExposureBias = 0.4f;

	UPROPERTY(VisibleAnywhere, Category = "Grau") UDirectionalLightComponent* Sun;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USkyAtmosphereComponent* Atmosphere;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USkyLightComponent* SkyLight;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UExponentialHeightFogComponent* Fog;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UVolumetricCloudComponent* Clouds;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UPostProcessComponent* PostProcess;

private:
	void ApplySettings();
};
