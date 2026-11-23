#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GrauHUD.generated.h"

class UFont;

// HUD desenhado direto no Canvas (sem assets): velocímetro, giro, marcador do grau,
// dinheiro/respeito, mensagens, desafio e minimapa que gira com a moto.
UCLASS()
class REIDOGRAU_API AGrauHUD : public AHUD
{
	GENERATED_BODY()

public:
	virtual void DrawHUD() override;

private:
	void Txt(const FString& S, float X, float Y, UFont* Font, float Scale, const FLinearColor& Color, int32 Align = 0, bool bShadow = true);
	void Panel(float X, float Y, float W, float H, float Alpha = 0.6f);
	void Arc(float CX, float CY, float R, float A0, float A1, const FLinearColor& Color, float Thick);
	void DrawMinimap(class AGrauBike* Bike, class AGrauGameMode* GM, float U);

	float FirstDrawTime = -1.f;
};
