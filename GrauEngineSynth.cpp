#include "GrauEngineSynth.h"
#include <cmath>

UGrauEngineSynth::UGrauEngineSynth(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NumChannels = 1;
	bAutoActivate = true;
}

void UGrauEngineSynth::SetEngineState(float InRpm, float InThrottle, float InSpeedMS, bool bInActive, bool bInScrape, float InSkid)
{
	TargetRpm.store(InRpm, std::memory_order_relaxed);
	TargetThrottle.store(InThrottle, std::memory_order_relaxed);
	TargetSpeed.store(InSpeedMS, std::memory_order_relaxed);
	TargetActive.store(bInActive ? 1.f : 0.f, std::memory_order_relaxed);
	TargetScrape.store(bInScrape ? 1.f : 0.f, std::memory_order_relaxed);
	TargetSkid.store(InSkid, std::memory_order_relaxed);
}

bool UGrauEngineSynth::Init(int32& SampleRate)
{
	NumChannels = 1;
	SampleRateF = SampleRate > 0 ? (float)SampleRate : 48000.f;
	return true;
}

float UGrauEngineSynth::NextNoise()
{
	// xorshift: ruído branco barato e estável na thread de áudio
	Seed ^= Seed << 13;
	Seed ^= Seed >> 17;
	Seed ^= Seed << 5;
	return (float)(Seed & 0xFFFFFF) / (float)0x7FFFFF - 1.f;
}

int32 UGrauEngineSynth::OnGenerateAudio(float* OutAudio, int32 NumSamples)
{
	const float TRpm = TargetRpm.load(std::memory_order_relaxed);
	const float TThr = TargetThrottle.load(std::memory_order_relaxed);
	const float TSpd = TargetSpeed.load(std::memory_order_relaxed);
	const float TAct = TargetActive.load(std::memory_order_relaxed);
	const float TScr = TargetScrape.load(std::memory_order_relaxed);
	const float TSkd = TargetSkid.load(std::memory_order_relaxed);

	const float Sr = SampleRateF;
	const float KFast = 1.f - FMath::Exp(-1.f / (Sr * 0.02f));
	const float KSlow = 1.f - FMath::Exp(-1.f / (Sr * 0.08f));
	const float Mul[4] = { 1.f, 0.5f, 2.f, 4.f };
	const float TwoPi = 2.f * UE_PI;

	for (int32 i = 0; i < NumSamples; ++i)
	{
		Rpm += (TRpm - Rpm) * KFast;
		Throttle += (TThr - Throttle) * KFast;
		Speed += (TSpd - Speed) * KSlow;
		Active += (TAct - Active) * KSlow;
		Scrape += (TScr - Scrape) * KFast;
		Skid += (TSkd - Skid) * KFast;

		// frequência de ignição de um 4 cilindros 4 tempos: 2 explosões por volta
		const float Fire = Rpm / 60.f * 2.f;
		float Osc[4];
		for (int32 k = 0; k < 4; ++k)
		{
			Phase[k] += (double)(Fire * Mul[k] / Sr);
			Phase[k] -= FMath::FloorToDouble(Phase[k]);
			const float P = (float)Phase[k];
			switch (k)
			{
			case 0: Osc[k] = 2.f * P - 1.f; break;                       // dente de serra
			case 1: Osc[k] = P < 0.5f ? 1.f : -1.f; break;                 // quadrada (ronco grave)
			case 2: Osc[k] = 2.f * P - 1.f; break;                       // serra uma oitava acima
			default: Osc[k] = 4.f * FMath::Abs(P - 0.5f) - 1.f; break;    // triângulo (agudo)
			}
		}
		AmPhase += (double)(Fire * 0.5f / Sr);
		AmPhase -= FMath::FloorToDouble(AmPhase);
		const float Am = 0.72f + 0.28f * FMath::Sin(TwoPi * (float)AmPhase);
		const float Mix = (0.5f * Osc[0] + 0.35f * Osc[1] + 0.22f * Osc[2] + 0.12f * Osc[3]) * Am;
		const float Driven = tanhf(Mix * 1.6f);

		const float Cutoff = 500.f + Throttle * 2600.f + (Rpm / 14000.f) * 2400.f;
		const float A = 1.f - FMath::Exp(-TwoPi * Cutoff / Sr);
		EngineLP += A * (Driven - EngineLP);
		float Out = EngineLP * (0.35f + 0.65f * Throttle) * 0.45f * Active;

		const float N = NextNoise();
		// admissão: ruído passa-faixa grosseiro
		const float Ai = 1.f - FMath::Exp(-TwoPi * (300.f + Rpm * 0.12f) / Sr);
		IntakeLP += Ai * (N - IntakeLP);
		IntakeLP2 += Ai * 0.25f * (IntakeLP - IntakeLP2);
		Out += (IntakeLP - IntakeLP2) * Throttle * 0.35f * Active;

		// vento cresce com a velocidade
		const float Sp = FMath::Clamp(Speed / 80.f, 0.f, 1.f);
		const float Aw = 1.f - FMath::Exp(-TwoPi * (300.f + Sp * 2200.f) / Sr);
		WindLP += Aw * (N - WindLP);
		Out += WindLP * Sp * Sp * 0.5f;

		// rabeta raspando (agudo) e pneu cantando
		ScrapeLP += 0.35f * (N - ScrapeLP);
		Out += (N - ScrapeLP) * Scrape * 0.22f;
		SkidLP += 0.18f * (N - SkidLP);
		Out += (SkidLP - ScrapeLP * 0.5f) * Skid * 0.18f;

		// estouros de escape quando corta o giro alto
		PopTimer -= 1.f / Sr;
		if (Active > 0.5f && Throttle < 0.1f && Rpm > 7000.f && PopTimer <= 0.f && NextNoise() > 0.9996f)
		{
			PopLevel = 0.6f;
			PopTimer = 0.05f;
		}
		PopLevel *= 0.9985f;
		Out += N * PopLevel * 0.5f;

		OutAudio[i] = FMath::Clamp(Out * MasterGain, -1.f, 1.f);
	}
	return NumSamples;
}
