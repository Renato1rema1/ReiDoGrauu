#include "GrauPlayerController.h"
#include "GrauBike.h"

AGrauPlayerController::AGrauPlayerController()
{
	PrimaryActorTick.bTickEvenWhenPaused = true;
	bShouldPerformFullTickWhenPaused = true;
}

void AGrauPlayerController::BeginPlay()
{
	Super::BeginPlay();
	SetInputMode(FInputModeGameOnly());
	bShowMouseCursor = false;
}

void AGrauPlayerController::PlayerTick(float DeltaTime)
{
	Super::PlayerTick(DeltaTime);
	if (WasInputKeyJustPressed(EKeys::P) || WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::Gamepad_Special_Right))
	{
		SetPause(!IsPaused());
	}
	if (WasInputKeyJustPressed(EKeys::M))
	{
		bMuted = !bMuted;
		if (AGrauBike* Bike = Cast<AGrauBike>(GetPawn()))
		{
			Bike->SetMuted(bMuted);
		}
	}
}
