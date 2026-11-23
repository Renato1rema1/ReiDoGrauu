#include "GrauGameMode.h"
#include "ReiDoGrau.h"
#include "GrauBike.h"
#include "GrauCityGenerator.h"
#include "GrauHUD.h"
#include "GrauMissionMarker.h"
#include "GrauPlayerController.h"
#include "GrauScoreComponent.h"
#include "GrauTraffic.h"
#include "GrauTypes.h"
#include "GrauWorldSetup.h"
#include "Engine/DirectionalLight.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const TCHAR* SlotName = TEXT("ReiDoGrau");
	const FLinearColor Ember(1.f, 0.42f, 0.1f);
	const FLinearColor Danger(1.f, 0.23f, 0.3f);
	FVector M2Cm(float X, float Y) { return FVector(X * 100.f, Y * 100.f, 0.f); }
}

AGrauGameMode::AGrauGameMode()
{
	PrimaryActorTick.bCanEverTick = true;
	DefaultPawnClass = AGrauBike::StaticClass();
	PlayerControllerClass = AGrauPlayerController::StaticClass();
	HUDClass = AGrauHUD::StaticClass();
}

void AGrauGameMode::EnsureWorld()
{
	UWorld* World = GetWorld();
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	bool bHasLight = false;
	for (TActorIterator<AGrauWorldSetup> It(World); It; ++It) { bHasLight = true; break; }
	if (!bHasLight)
	{
		for (TActorIterator<ADirectionalLight> It(World); It; ++It) { bHasLight = true; break; }
	}
	if (!bHasLight)
	{
		World->SpawnActor<AGrauWorldSetup>(AGrauWorldSetup::StaticClass(), FTransform::Identity, Params);
		UE_LOG(LogGrau, Log, TEXT("Ambiente do por do sol criado automaticamente."));
	}
	bool bHasCity = false;
	for (TActorIterator<AGrauCityGenerator> It(World); It; ++It) { bHasCity = true; break; }
	if (!bHasCity)
	{
		World->SpawnActor<AGrauCityGenerator>(AGrauCityGenerator::StaticClass(), FTransform::Identity, Params);
		UE_LOG(LogGrau, Log, TEXT("Porto Brasa gerada automaticamente."));
	}
	bool bHasTraffic = false;
	for (TActorIterator<AGrauTrafficManager> It(World); It; ++It) { bHasTraffic = true; break; }
	if (!bHasTraffic)
	{
		World->SpawnActor<AGrauTrafficManager>(AGrauTrafficManager::StaticClass(), FTransform::Identity, Params);
	}
}

void AGrauGameMode::StartPlay()
{
	EnsureWorld();
	for (TActorIterator<AGrauCityGenerator> It(GetWorld()); It; ++It)
	{
		BuildMissions(*It);
		break;
	}
	Super::StartPlay();
}

void AGrauGameMode::BuildMissions(const AGrauCityGenerator* City)
{
	const FVector2D Posto = City->PostoCenter;
	const FVector2D Pista = City->PistaCenter;
	auto Add = [&](const TCHAR* Id, const TCHAR* Title, const TCHAR* Desc, FVector Pos, int32 Reward, float Time, EGrauMissionKind Kind, float Goal)
	{
		FGrauMission M;
		M.Id = FName(Id);
		M.Title = Title;
		M.Desc = Desc;
		M.Pos = Pos;
		M.Reward = Reward;
		M.Time = Time;
		M.Kind = Kind;
		M.Goal = Goal;
		Missions.Add(M);
	};
	Add(TEXT("orla"), TEXT("Orla em Grau"), TEXT("Segure um grau contínuo por 250 metros."), M2Cm(-300.f, 8.6f), 1500, 70.f, EGrauMissionKind::GrauDistance, 250.f);
	Add(TEXT("doze"), TEXT("Grau no 12"), TEXT("Mantenha o grau acima de 70° por 3 segundos."), M2Cm(Pista.X, Pista.Y), 2000, 60.f, EGrauMissionKind::OverSeventy, 3.f);
	Add(TEXT("corrida"), TEXT("Rolê até o Posto"), TEXT("Passe pelos pontos amarelos e chegue no Posto Brasa antes do tempo acabar."), M2Cm(53.f, -318.f), 2500, 55.f, EGrauMissionKind::Checkpoints, 0.f);
	Missions.Last().Checkpoints = { M2Cm(106.f, -360.f), M2Cm(106.f, -424.f), M2Cm(212.f, -424.f), M2Cm(212.f, -212.f), M2Cm(212.f, -106.f), M2Cm(Posto.X, Posto.Y) };
	Add(TEXT("stoppie"), TEXT("Freio de Mão"), TEXT("Faça um stoppie de 1,5 segundo: freie forte segurando CTRL ou X."), M2Cm(380.f, 8.6f), 1200, 45.f, EGrauMissionKind::Stoppie, 1.5f);
	Add(TEXT("rei"), TEXT("Rei da Quebrada"), TEXT("Faça 15.000 pontos em 90 segundos. Vale grau, voo, zerinho e tira fina."), M2Cm(Posto.X + 14.f, Posto.Y + 8.f), 5000, 90.f, EGrauMissionKind::PointsInTime, 15000.f);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	for (FGrauMission& M : Missions)
	{
		AGrauMissionMarker* Mk = GetWorld()->SpawnActor<AGrauMissionMarker>(AGrauMissionMarker::StaticClass(), FTransform(M.Pos), Params);
		if (Mk)
		{
			Mk->SetColor(Ember, 1.8f);
			Markers.Add(Mk);
			M.Marker = Mk;
		}
	}
	CheckpointMarker = GetWorld()->SpawnActor<AGrauMissionMarker>(AGrauMissionMarker::StaticClass(), FTransform::Identity, Params);
	if (CheckpointMarker)
	{
		CheckpointMarker->SetColor(FLinearColor(1.f, 0.82f, 0.23f), 4.5f);
		CheckpointMarker->SetShown(false);
	}
}

void AGrauGameMode::LoadProgress(AGrauBike* Bike)
{
	bLoaded = true;
	if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
	{
		Save = Cast<UGrauSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
	}
	if (!Save)
	{
		Save = Cast<UGrauSaveGame>(UGameplayStatics::CreateSaveGameObject(UGrauSaveGame::StaticClass()));
	}
	if (Save && Bike && Bike->Score)
	{
		Bike->Score->Points = Save->Points;
		Bike->Score->Money = Save->Money;
		Bike->Score->BestGrau = Save->BestGrau;
	}
}

void AGrauGameMode::SaveProgress()
{
	const AGrauBike* Bike = Cast<AGrauBike>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Save || !Bike || !Bike->Score)
	{
		return;
	}
	Save->Points = Bike->Score->Points;
	Save->Money = Bike->Score->Money;
	Save->BestGrau = Bike->Score->BestGrau;
	UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

void AGrauGameMode::EndPlay(const EEndPlayReason::Type Reason)
{
	SaveProgress();
	Super::EndPlay(Reason);
}

void AGrauGameMode::StartMission(int32 Index)
{
	Active = Index;
	Best = 0.f;
	CheckpointIdx = 0;
	const FGrauMission& M = Missions[Index];
	MissionTimeLeft = M.Time;
	if (const AGrauBike* Bike = Cast<AGrauBike>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		PointsAtStart = Bike->Score ? Bike->Score->Points : 0;
		if (Bike->Score) Bike->Score->ShowToast(M.Title.ToUpper(), M.Desc, FLinearColor::White, 3.2f);
	}
	for (AGrauMissionMarker* Mk : Markers)
	{
		Mk->SetShown(false);
	}
	PromptText.Empty();
}

void AGrauGameMode::StopMission()
{
	Active = -1;
	Cooldown = 4.f;
	bMissionActive = false;
	for (AGrauMissionMarker* Mk : Markers)
	{
		Mk->SetShown(true);
	}
	if (CheckpointMarker) CheckpointMarker->SetShown(false);
}

void AGrauGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AGrauBike* Bike = Cast<AGrauBike>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!Bike)
	{
		return;
	}
	if (!bLoaded)
	{
		LoadProgress(Bike);
	}
	UpdateMissions(DeltaSeconds, Bike);
	SaveTimer += DeltaSeconds;
	if (SaveTimer > 15.f)
	{
		SaveTimer = 0.f;
		SaveProgress();
	}
}

void AGrauGameMode::UpdateMissions(float Dt, AGrauBike* Bike)
{
	UGrauScoreComponent* S = Bike->Score;
	if (!S)
	{
		return;
	}
	if (Cooldown > 0.f) Cooldown -= Dt;
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const bool bInteract = PC && (PC->WasInputKeyJustPressed(EKeys::E) || PC->WasInputKeyJustPressed(EKeys::Gamepad_FaceButton_Left));
	const FVector BikePos = Bike->RearContact();

	MapBlips.Reset();
	MapBlipDone.Reset();
	MapBlipActive = -1;

	if (Active < 0)
	{
		bMissionActive = false;
		PromptText.Empty();
		for (int32 I = 0; I < Missions.Num(); ++I)
		{
			const FGrauMission& M = Missions[I];
			const bool bDone = Save && Save->Completed.Contains(M.Id);
			MapBlips.Add(M.Pos);
			MapBlipDone.Add(bDone);
			if (FVector::Dist2D(BikePos, M.Pos) < 600.f && !Bike->bCrashed && Cooldown <= 0.f)
			{
				PromptText = FString::Printf(TEXT("[E] %s%s\n%s"), *M.Title, bDone ? TEXT(" · já concluído") : TEXT(""), *M.Desc);
				if (bInteract && Bike->KmH() < 40.f)
				{
					StartMission(I);
					return;
				}
			}
		}
		return;
	}

	FGrauMission& M = Missions[Active];
	bMissionActive = true;
	MissionTitle = M.Title;
	MissionTimeLeft -= Dt;
	bool bOk = false;
	switch (M.Kind)
	{
	case EGrauMissionKind::GrauDistance:
	{
		const float D = S->bInGrau ? S->GrauDist : 0.f;
		Best = FMath::Max(Best, D);
		MissionObjective = FString::Printf(TEXT("Grau atual %d m · melhor %d / %d m"), FMath::RoundToInt(D), FMath::RoundToInt(Best), FMath::RoundToInt(M.Goal));
		bOk = Best >= M.Goal;
		break;
	}
	case EGrauMissionKind::OverSeventy:
	{
		const float T = S->bInGrau ? S->GrauOver70 : 0.f;
		Best = FMath::Max(Best, T);
		MissionObjective = FString::Printf(TEXT("Acima de 70°: %s s · melhor %s / %s s"), *GrauFmt::Dec(T), *GrauFmt::Dec(Best), *GrauFmt::Dec(M.Goal));
		bOk = Best >= M.Goal;
		break;
	}
	case EGrauMissionKind::Checkpoints:
	{
		if (M.Checkpoints.IsValidIndex(CheckpointIdx))
		{
			const FVector Cp = M.Checkpoints[CheckpointIdx];
			if (FVector::Dist2D(BikePos, Cp) < 900.f)
			{
				++CheckpointIdx;
				S->AddTick(FString::Printf(TEXT("PONTO %d/%d"), CheckpointIdx, M.Checkpoints.Num()));
			}
		}
		if (M.Checkpoints.IsValidIndex(CheckpointIdx))
		{
			const FVector Next = M.Checkpoints[CheckpointIdx];
			if (CheckpointMarker)
			{
				CheckpointMarker->SetActorLocation(Next);
				CheckpointMarker->SetShown(true);
			}
			MapBlips.Add(Next);
			MapBlipDone.Add(false);
			MapBlipActive = 0;
		}
		MissionObjective = FString::Printf(TEXT("Ponto %d de %d"), FMath::Min(CheckpointIdx + 1, M.Checkpoints.Num()), M.Checkpoints.Num());
		bOk = CheckpointIdx >= M.Checkpoints.Num();
		break;
	}
	case EGrauMissionKind::Stoppie:
	{
		const float T = S->bInStoppie ? S->StoppieTime : 0.f;
		Best = FMath::Max(Best, T);
		MissionObjective = FString::Printf(TEXT("Stoppie %s s · melhor %s / %s s"), *GrauFmt::Dec(T), *GrauFmt::Dec(Best), *GrauFmt::Dec(M.Goal));
		bOk = Best >= M.Goal;
		break;
	}
	case EGrauMissionKind::PointsInTime:
	{
		const int64 Got = FMath::Max<int64>(0, S->Points - PointsAtStart);
		MissionObjective = FString::Printf(TEXT("%s / %s pts"), *GrauFmt::Int(Got), *GrauFmt::Int((int64)M.Goal));
		bOk = Got >= (int64)M.Goal;
		break;
	}
	}

	if (bOk)
	{
		const bool bFirst = Save && !Save->Completed.Contains(M.Id);
		const int32 Reward = bFirst ? M.Reward : M.Reward / 4;
		if (Save) Save->Completed.AddUnique(M.Id);
		S->Money += Reward;
		S->ShowToast(TEXT("DESAFIO CONCLUÍDO"), FString::Printf(TEXT("%s · +R$ %s"), *M.Title, *GrauFmt::Int(Reward)), Ember, 3.5f);
		StopMission();
		SaveProgress();
	}
	else if (MissionTimeLeft <= 0.f || Bike->bCrashed)
	{
		if (Bike->bCrashed) S->AddTick(TEXT("DESAFIO FALHOU"));
		else S->ShowToast(TEXT("DESAFIO FALHOU"), TEXT("O tempo acabou"), Danger, 3.f);
		StopMission();
	}
}
