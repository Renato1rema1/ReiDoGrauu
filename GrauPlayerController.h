#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GrauPlayerController.generated.h"

// Teclas globais: pausar (P / Esc / Start) e som (M). A pilotagem é lida direto pela moto.
UCLASS()
class REIDOGRAU_API AGrauPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGrauPlayerController();
	virtual void PlayerTick(float DeltaTime) override;

protected:
	virtual void BeginPlay() override;

private:
	bool bMuted = false;
};
