#include "GrauWorldSetup.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/VolumetricCloudComponent.h"
#include "Components/PostProcessComponent.h"

AGrauWorldSetup::AGrauWorldSetup()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));

	Sun = CreateDefaultSubobject<UDirectionalLightComponent>(TEXT("Sun"));
	Sun->SetupAttachment(RootComponent);
	Sun->SetMobility(EComponentMobility::Movable);
	Sun->bAtmosphereSunLight = true;
	Sun->AtmosphereSunLightIndex = 0;
	Sun->Intensity = SunIntensityLux;
	Sun->LightSourceAngle = 0.6f;
	Sun->CastShadows = true;

	Atmosphere = CreateDefaultSubobject<USkyAtmosphereComponent>(TEXT("Atmosphere"));
	Atmosphere->SetupAttachment(RootComponent);

	SkyLight = CreateDefaultSubobject<USkyLightComponent>(TEXT("SkyLight"));
	SkyLight->SetupAttachment(RootComponent);
	SkyLight->SetMobility(EComponentMobility::Movable);
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity = 1.2f;

	Fog = CreateDefaultSubobject<UExponentialHeightFogComponent>(TEXT("Fog"));
	Fog->SetupAttachment(RootComponent);
	Fog->bEnableVolumetricFog = true;
	Fog->FogHeightFalloff = 0.2f;

	Clouds = CreateDefaultSubobject<UVolumetricCloudComponent>(TEXT("Clouds"));
	Clouds->SetupAttachment(RootComponent);

	PostProcess = CreateDefaultSubobject<UPostProcessComponent>(TEXT("PostProcess"));
	PostProcess->SetupAttachment(RootComponent);
	PostProcess->bUnbound = true;
	PostProcess->Priority = 0.f;
}

void AGrauWorldSetup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplySettings();
}

void AGrauWorldSetup::ApplySettings()
{
	// a luz direcional aponta PARA ONDE a luz vai: do sol (sudeste, sobre o mar) para a cidade
	Sun->SetWorldRotation(FRotator(-SunElevation, SunAzimuth + 180.f, 0.f));
	Sun->SetIntensity(SunIntensityLux);
	Fog->SetFogDensity(FogDensity);

	FPostProcessSettings& S = PostProcess->Settings;
	S.bOverride_DynamicGlobalIlluminationMethod = true;
	S.DynamicGlobalIlluminationMethod = EDynamicGlobalIlluminationMethod::Lumen;
	S.bOverride_ReflectionMethod = true;
	S.ReflectionMethod = EReflectionMethod::Lumen;
	S.bOverride_AutoExposureBias = true;
	S.AutoExposureBias = ExposureBias;
	S.bOverride_BloomIntensity = true;
	S.BloomIntensity = 0.9f;
	S.bOverride_MotionBlurAmount = true;
	S.MotionBlurAmount = 0.7f;
	S.bOverride_MotionBlurMax = true;
	S.MotionBlurMax = 8.f;
	S.bOverride_VignetteIntensity = true;
	S.VignetteIntensity = 0.55f;
	S.bOverride_FilmGrainIntensity = true;
	S.FilmGrainIntensity = 0.12f;
	S.bOverride_SceneFringeIntensity = true;
	S.SceneFringeIntensity = 0.7f;
	// gradação: sombras um pouco frias, luzes quentes, saturação e contraste de cinema
	S.bOverride_ColorSaturation = true;
	S.ColorSaturation = FVector4(1.08f, 1.05f, 1.0f, 1.08f);
	S.bOverride_ColorContrast = true;
	S.ColorContrast = FVector4(1.0f, 1.0f, 1.0f, 1.1f);
	S.bOverride_ColorGainShadows = true;
	S.ColorGainShadows = FVector4(0.94f, 1.0f, 1.08f, 1.0f);
	S.bOverride_ColorGainHighlights = true;
	S.ColorGainHighlights = FVector4(1.06f, 1.0f, 0.92f, 1.0f);
}
