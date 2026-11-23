#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/SaveGame.h"
#include "GrauGameMode.generated.h"

class AGrauBike;
class AGrauMissionMarker;
class AGrauCityGenerator;

UCLASS()
class REIDOGRAU_API UGrauSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UPROPERTY() int64 Points = 0;
	UPROPERTY() int64 Money = 0;
	UPROPERTY() float BestGrau = 0.f;
	UPROPERTY() TArray<FName> Completed;
};

enum class EGrauMissionKind : uint8
{
	GrauDistance,   // segurar um grau por X metros
	OverSeventy,    // segundos acima de 70°
	Checkpoints,    // corrida por pontos
	Stoppie,        // segundos de stoppie
	PointsInTime    // pontos dentro do tempo
};

struct FGrauMission
{
	FName Id;
	FString Title;
	FString Desc;
	FVector Pos = FVector::ZeroVector;   // cm
	int32 Reward = 1000;
	float Time = 60.f;
	EGrauMissionKind Kind = EGrauMissionKind::GrauDistance;
	float Goal = 0.f;
	TArray<FVector> Checkpoints;          // cm
	AGrauMissionMarker* Marker = nullptr;
};

// Regras do jogo: monta o mundo se o mapa estiver vazio, desafios, salvamento.
UCLASS()
class REIDOGRAU_API AGrauGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGrauGameMode();

	virtual void StartPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	void SaveProgress();

	// lidos pelo HUD
	FString PromptText;
	bool bMissionActive = false;
	FString MissionTitle;
	FString MissionObjective;
	float MissionTimeLeft = 0.f;
	TArray<FVector> MapBlips;     // cm
	TArray<bool> MapBlipDone;
	int32 MapBlipActive = -1;

private:
	void EnsureWorld();
	void BuildMissions(const AGrauCityGenerator* City);
	void StartMission(int32 Index);
	void StopMission();
	void UpdateMissions(float Dt, AGrauBike* Bike);
	void LoadProgress(AGrauBike* Bike);

	TArray<FGrauMission> Missions;
	UPROPERTY(Transient) TArray<AGrauMissionMarker*> Markers;
	UPROPERTY(Transient) AGrauMissionMarker* CheckpointMarker = nullptr;
	UPROPERTY(Transient) UGrauSaveGame* Save = nullptr;

	int32 Active = -1;
	float Best = 0.f;
	int32 CheckpointIdx = 0;
	int64 PointsAtStart = 0;
	float Cooldown = 0.f;
	float SaveTimer = 0.f;
	bool bLoaded = false;
};
