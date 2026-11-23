#pragma once

#include "CoreMinimal.h"
#include "Components/SynthComponent.h"
#include <atomic>
#include "GrauEngineSynth.generated.h"

// Ronco de 4 cilindros sintetizado em tempo real (não precisa de nenhum arquivo de som).
// Se você importar um som de motor de verdade, dá para trocar por um UAudioComponent com pitch pelo giro.
UCLASS(ClassGroup = (Audio), meta = (BlueprintSpawnableComponent))
class REIDOGRAU_API UGrauEngineSynth : public USynthComponent
{
	GENERATED_BODY()

public:
	UGrauEngineSynth(const FObjectInitializer& ObjectInitializer);

	// Chamado pela moto a cada quadro (thread do jogo)
	void SetEngineState(float InRpm, float InThrottle, float InSpeedMS, bool bInActive, bool bInScrape, float InSkid);

	UPROPERTY(EditAnywhere, Category = "Grau|Som", meta = (ClampMin = "0", ClampMax = "2"))
	float MasterGain = 0.9f;

protected:
	virtual bool Init(int32& SampleRate) override;
	virtual int32 OnGenerateAudio(float* OutAudio, int32 NumSamples) override;

private:
	float NextNoise();

	std::atomic<float> TargetRpm{ 1400.f };
	std::atomic<float> TargetThrottle{ 0.f };
	std::atomic<float> TargetSpeed{ 0.f };
	std::atomic<float> TargetActive{ 0.f };
	std::atomic<float> TargetScrape{ 0.f };
	std::atomic<float> TargetSkid{ 0.f };

	// estado da thread de áudio
	float SampleRateF = 48000.f;
	float Rpm = 1400.f;
	float Throttle = 0.f;
	float Speed = 0.f;
	float Active = 0.f;
	float Scrape = 0.f;
	float Skid = 0.f;
	double Phase[4] = { 0.0, 0.0, 0.0, 0.0 };
	double AmPhase = 0.0;
	float EngineLP = 0.f;
	float IntakeLP = 0.f;
	float IntakeLP2 = 0.f;
	float WindLP = 0.f;
	float ScrapeLP = 0.f;
	float SkidLP = 0.f;
	float PopTimer = 0.f;
	float PopLevel = 0.f;
	uint32 Seed = 0x2545F491u;
};
