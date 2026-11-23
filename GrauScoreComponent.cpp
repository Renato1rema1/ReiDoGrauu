#include "GrauScoreComponent.h"
#include "GrauBike.h"

namespace
{
	const int64 StarAt[5] = { 2000, 10000, 30000, 80000, 200000 };
	const FLinearColor Ember(1.f, 0.42f, 0.1f);
	const FLinearColor Danger(1.f, 0.23f, 0.3f);

	FString CrashText(const FString& Reason)
	{
		if (Reason == TEXT("bateu")) return TEXT("Bateu feio");
		if (Reason == TEXT("carro")) return TEXT("Beijou o para-choque");
		if (Reason == TEXT("passou")) return TEXT("Passou do ponto do grau");
		if (Reason == TEXT("caiu-de-costas")) return TEXT("Caiu de costas");
		if (Reason == TEXT("capotou")) return TEXT("Capotou no stoppie");
		if (Reason == TEXT("pouso")) return TEXT("Pouso torto");
		if (Reason == TEXT("mar")) return TEXT("Foi dar um mergulho");
		return TEXT("Caiu");
	}
}

UGrauScoreComponent::UGrauScoreComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

int32 UGrauScoreComponent::Stars() const
{
	int32 S = 0;
	for (int64 T : StarAt)
	{
		if (Points >= T) ++S;
	}
	return S;
}

void UGrauScoreComponent::ShowToast(const FString& Big, const FString& Small, const FLinearColor& Color, float Seconds)
{
	Toast.Big = Big;
	Toast.Small = Small;
	Toast.Color = Color;
	Toast.TimeLeft = Seconds;
	Toast.Age = 0.f;
}

void UGrauScoreComponent::AddTick(const FString& Text)
{
	FGrauTick T;
	T.Text = Text;
	T.TimeLeft = 1.8f;
	Ticks.Add(T);
	while (Ticks.Num() > 4)
	{
		Ticks.RemoveAt(0);
	}
}

void UGrauScoreComponent::LoseCombo()
{
	Combo = 1.f;
	ComboN = 0;
	ComboTime = 0.f;
}

void UGrauScoreComponent::Bank(float Pts, const FString& Title, const FString& Detail)
{
	const int64 Total = FMath::RoundToInt64(Pts * Combo);
	Points += Total;
	Money += FMath::RoundToInt64(Total * 0.1);
	FString Small = TEXT("+") + GrauFmt::Int(Total);
	if (Combo > 1.f)
	{
		Small += FString::Printf(TEXT(" (x%s)"), *GrauFmt::Dec(Combo, 2));
	}
	if (!Detail.IsEmpty())
	{
		Small += TEXT(" · ") + Detail;
	}
	ShowToast(Title, Small, Ember);
	++ComboN;
	Combo = FMath::Min(3.f, 1.f + ComboN * 0.25f);
	ComboTime = 3.f;
}

void UGrauScoreComponent::AddNearMiss(float Kmh)
{
	const int64 Pts = 150 + FMath::RoundToInt64(Kmh);
	Points += Pts;
	Money += FMath::RoundToInt64(Pts * 0.1);
	ComboTime = FMath::Max(ComboTime, 2.f);
	AddTick(FString::Printf(TEXT("TIRA FINA +%lld"), Pts));
}

void UGrauScoreComponent::UpdateScore(float Dt, const AGrauBike* Bike, const TArray<FGrauBikeEvent>& Events)
{
	Chips.Reset();
	Toast.TimeLeft -= Dt;
	Toast.Age += Dt;
	for (int32 i = Ticks.Num() - 1; i >= 0; --i)
	{
		Ticks[i].TimeLeft -= Dt;
		if (Ticks[i].TimeLeft <= 0.f) Ticks.RemoveAt(i);
	}
	if (ComboTime > 0.f)
	{
		ComboTime -= Dt;
		if (ComboTime <= 0.f) LoseCombo();
	}
	if (!Bike)
	{
		return;
	}
	const float Deg = Bike->PitchDeg();
	const float V = FMath::Abs(Bike->SpeedMS);

	// ---- grau ----
	if (!Bike->bCrashed && Bike->Pitch > 0.12f && !Bike->bAir)
	{
		if (!bInGrau)
		{
			bInGrau = true;
			GrauDist = GrauPts = GrauMaxDeg = GrauOver70 = GrauScrape = 0.f;
		}
		GrauDist += V * Dt;
		GrauMaxDeg = FMath::Max(GrauMaxDeg, Deg);
		float Mult = 1.f;
		if (Deg >= 40.f && Deg <= 66.f) { Mult += 1.f; Chips.Add(TEXT("EQUILÍBRIO x2")); }
		if (Deg > 70.f) { Mult += 2.f; Chips.Add(TEXT("NO 12 x3")); GrauOver70 += Dt; }
		if (FMath::Abs(Bike->Lean) > 0.22f) { Mult += 0.5f; Chips.Add(TEXT("NA CURVA")); }
		if (Bike->KmH() > 80.f) { Mult += 0.5f; Chips.Add(TEXT("VELOZ")); }
		if (Bike->bScraping) { Mult += 1.f; Chips.Add(TEXT("RASPANDO")); GrauScrape += Dt; }
		GrauPts += V * Dt * 10.f * Mult;
	}
	else if (bInGrau && !Bike->bCrashed && Bike->Pitch <= 0.02f)
	{
		bInGrau = false;
		if (GrauDist > 8.f)
		{
			FString Title = GrauDist < 40.f ? TEXT("GRAU") : GrauDist < 120.f ? TEXT("GRAU LONGO") : GrauDist < 300.f ? TEXT("GRAU DE RESPEITO") : TEXT("GRAU INFINITO");
			if (GrauMaxDeg >= 70.f) Title = GrauDist < 120.f ? TEXT("GRAU NO 12") : TEXT("NO 12 E LONGE");
			if (GrauScrape > 0.6f) Title = TEXT("RASPOU A RABETA");
			Bank(GrauPts, Title, FString::Printf(TEXT("%d m · %d°"), FMath::RoundToInt(GrauDist), FMath::RoundToInt(GrauMaxDeg)));
			if (GrauDist > BestGrau)
			{
				BestGrau = GrauDist;
				AddTick(FString::Printf(TEXT("NOVO RECORDE %d m"), FMath::RoundToInt(GrauDist)));
			}
		}
	}

	// ---- stoppie ----
	if (!Bike->bCrashed && Bike->Pitch < -0.1f)
	{
		if (!bInStoppie)
		{
			bInStoppie = true;
			StoppieTime = StoppiePts = 0.f;
		}
		StoppieTime += Dt;
		StoppiePts += (V * Dt * 15.f + Dt * 60.f) * (Bike->Pitch < -0.35f ? 2.f : 1.f);
		if (Bike->Pitch < -0.35f) Chips.Add(TEXT("NARIZ NO CHÃO x2"));
	}
	else if (bInStoppie && !Bike->bCrashed && Bike->Pitch >= 0.f)
	{
		bInStoppie = false;
		if (StoppieTime > 0.35f)
		{
			Bank(StoppiePts, TEXT("STOPPIE"), GrauFmt::Dec(StoppieTime) + TEXT(" s"));
		}
	}
	Pending = (bInGrau ? GrauPts : 0.f) + (bInStoppie ? StoppiePts : 0.f);
	bGrauOn = !Bike->bCrashed && FMath::Abs(Bike->Pitch) > 0.05f;
	if (Bike->bBurnout) Chips.Add(TEXT("QUEIMANDO PNEU"));

	// ---- eventos da moto ----
	for (const FGrauBikeEvent& E : Events)
	{
		switch (E.Type)
		{
		case EGrauEvent::Landing:
			if (E.Value > 0.45f)
			{
				const bool bInWheelie = Bike->Pitch > 0.12f;
				const float Pts = E.Value * 450.f * (E.bFlag ? 1.f : 0.5f) + (bInWheelie ? 300.f : 0.f);
				Bank(Pts, bInWheelie ? TEXT("POUSO NO GRAU") : TEXT("VOOU!"), GrauFmt::Dec(E.Value) + TEXT(" s no ar"));
			}
			break;
		case EGrauEvent::Donut:
			DonutAngle += 2.1f * E.Value;
			if (DonutAngle > 2.f * UE_PI)
			{
				DonutAngle = 0.f;
				Bank(300.f, TEXT("ZERINHO"), FString());
			}
			break;
		case EGrauEvent::Crash:
		{
			const float Lost = Pending;
			bInGrau = bInStoppie = false;
			Pending = 0.f;
			LoseCombo();
			FString Small = CrashText(E.Reason);
			if (Lost > 0.f)
			{
				Small += TEXT(" · -") + GrauFmt::Int(FMath::RoundToInt64(Lost));
			}
			ShowToast(TEXT("PERDEU!"), Small, Danger, 3.f);
			break;
		}
		default:
			break;
		}
	}
}
