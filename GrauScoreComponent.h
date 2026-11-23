#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GrauTypes.h"
#include "GrauScoreComponent.generated.h"

class AGrauBike;

struct FGrauToast
{
	FString Big;
	FString Small;
	FLinearColor Color = FLinearColor::White;
	float TimeLeft = 0.f;
	float Age = 0.f;
};

struct FGrauTick
{
	FString Text;
	float TimeLeft = 0.f;
};

// Pontuação do grau: distância empinada, ângulo, combo, stoppie, voo, zerinho e "tira fina".
UCLASS(ClassGroup = (Grau), meta = (BlueprintSpawnableComponent))
class REIDOGRAU_API UGrauScoreComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UGrauScoreComponent();

	void UpdateScore(float Dt, const AGrauBike* Bike, const TArray<FGrauBikeEvent>& Events);
	void AddNearMiss(float Kmh);
	void ShowToast(const FString& Big, const FString& Small, const FLinearColor& Color, float Seconds = 2.2f);
	void AddTick(const FString& Text);
	void LoseCombo();
	int32 Stars() const;

	// valores salvos
	int64 Points = 0;
	int64 Money = 0;
	float BestGrau = 0.f;

	// estado atual
	float Combo = 1.f;
	int32 ComboN = 0;
	float ComboTime = 0.f;
	float Pending = 0.f;
	TArray<FString> Chips;
	bool bGrauOn = false;

	bool bInGrau = false;
	float GrauDist = 0.f;
	float GrauPts = 0.f;
	float GrauMaxDeg = 0.f;
	float GrauOver70 = 0.f;
	float GrauScrape = 0.f;

	bool bInStoppie = false;
	float StoppieTime = 0.f;
	float StoppiePts = 0.f;

	float DonutAngle = 0.f;

	FGrauToast Toast;
	TArray<FGrauTick> Ticks;

private:
	void Bank(float Pts, const FString& Title, const FString& Detail);
};
