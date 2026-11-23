#include "GrauHUD.h"
#include "GrauBike.h"
#include "GrauCityGenerator.h"
#include "GrauGameMode.h"
#include "GrauScoreComponent.h"
#include "GrauTypes.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Engine.h"
#include "Engine/Font.h"
#include "Engine/World.h"
#include "EngineUtils.h"

namespace
{
	const FLinearColor Paper(0.95f, 0.92f, 0.88f);
	const FLinearColor Ember(1.f, 0.42f, 0.1f);
	const FLinearColor EmberHot(1.f, 0.7f, 0.36f);
	const FLinearColor Go(0.55f, 1.f, 0.68f);
	const FLinearColor Danger(1.f, 0.23f, 0.3f);
	const FLinearColor Muted(0.95f, 0.92f, 0.88f, 0.25f);

	const TCHAR* Tips[] = {
		TEXT("W acelera · A/D inclina · S freia · mouse olha em volta"),
		TEXT("Para puxar o grau: em 1ª ou 2ª, segure W e aperte SHIFT"),
		TEXT("No grau, dose o W para subir e o S para descer. A faixa verde é o equilíbrio"),
		TEXT("Acima de 74° a rabeta raspa no chão: vale ponto, mas não passe do ponto"),
		TEXT("Passe raspando nos carros acima de 55 km/h: TIRA FINA"),
		TEXT("Colunas laranja são desafios: chegue perto e aperte E · C troca a câmera"),
	};

	// recorte de linha num retângulo (Liang–Barsky)
	bool ClipLine(FVector2D& A, FVector2D& B, const FBox2D& R)
	{
		float T0 = 0.f, T1 = 1.f;
		const FVector2D D = B - A;
		const float P[4] = { -D.X, D.X, -D.Y, D.Y };
		const float Q[4] = { A.X - R.Min.X, R.Max.X - A.X, A.Y - R.Min.Y, R.Max.Y - A.Y };
		for (int32 I = 0; I < 4; ++I)
		{
			if (FMath::IsNearlyZero(P[I]))
			{
				if (Q[I] < 0.f) return false;
				continue;
			}
			const float T = Q[I] / P[I];
			if (P[I] < 0.f) { if (T > T1) return false; T0 = FMath::Max(T0, T); }
			else { if (T < T0) return false; T1 = FMath::Min(T1, T); }
		}
		const FVector2D A0 = A;
		A = A0 + D * T0;
		B = A0 + D * T1;
		return true;
	}
}

void AGrauHUD::Txt(const FString& S, float X, float Y, UFont* Font, float Scale, const FLinearColor& Color, int32 Align, bool bShadow)
{
	if (!Canvas || !Font || S.IsEmpty())
	{
		return;
	}
	const float TW = Font->GetStringSize(*S) * Scale;
	const float DX = Align == 1 ? X - TW * 0.5f : (Align == 2 ? X - TW : X);
	FCanvasTextItem Item(FVector2D(DX, Y), FText::FromString(S), Font, Color);
	Item.Scale = FVector2D(Scale, Scale);
	if (bShadow)
	{
		Item.EnableShadow(FLinearColor(0.f, 0.f, 0.f, 0.85f), FVector2D(2.f, 2.f));
	}
	Canvas->DrawItem(Item);
}

void AGrauHUD::Panel(float X, float Y, float W, float H, float Alpha)
{
	DrawRect(FLinearColor(0.055f, 0.043f, 0.063f, Alpha), X, Y, W, H);
}

void AGrauHUD::Arc(float CX, float CY, float R, float A0, float A1, const FLinearColor& Color, float Thick)
{
	const int32 Steps = FMath::Max(2, FMath::CeilToInt((A1 - A0) / 4.f));
	for (int32 I = 0; I < Steps; ++I)
	{
		const float Ta = FMath::DegreesToRadians(A0 + (A1 - A0) * I / Steps);
		const float Tb = FMath::DegreesToRadians(A0 + (A1 - A0) * (I + 1) / Steps);
		DrawLine(CX - FMath::Cos(Ta) * R, CY - FMath::Sin(Ta) * R, CX - FMath::Cos(Tb) * R, CY - FMath::Sin(Tb) * R, Color, Thick);
	}
}

void AGrauHUD::DrawHUD()
{
	Super::DrawHUD();
	if (!Canvas)
	{
		return;
	}
	AGrauBike* Bike = Cast<AGrauBike>(GetOwningPawn());
	if (!Bike || !Bike->Score)
	{
		return;
	}
	AGrauGameMode* GM = GetWorld()->GetAuthGameMode<AGrauGameMode>();
	UGrauScoreComponent* S = Bike->Score;
	const float W = Canvas->ClipX, H = Canvas->ClipY;
	const float U = H / 1080.f;
	UFont* Big = GEngine->GetLargeFont();
	UFont* Med = GEngine->GetMediumFont();
	const float Now = GetWorld()->GetRealTimeSeconds();
	if (FirstDrawTime < 0.f) FirstDrawTime = Now;

	// barras de cinema
	if (Bike->CameraMode == EGrauCamera::Cinema)
	{
		DrawRect(FLinearColor::Black, 0.f, 0.f, W, H * 0.1f);
		DrawRect(FLinearColor::Black, 0.f, H * 0.9f, W, H * 0.1f);
	}

	// ---- velocímetro (canto inferior direito) ----
	const float Kmh = Bike->bCrashed ? 0.f : Bike->KmH();
	Txt(FString::FromInt(FMath::RoundToInt(Kmh)), W - 150.f * U, H - 175.f * U, Big, 4.4f * U, Paper, 2);
	Txt(TEXT("KM/H"), W - 50.f * U, H - 92.f * U, Med, 1.4f * U, Paper, 2);
	const FString GearTxt = (Kmh < 1.f && Bike->Throttle < 0.05f) ? TEXT("N") : FString::FromInt(Bike->Gear);
	const float Gx = W - 95.f * U, Gy = H - 250.f * U, Gs = 46.f * U;
	DrawLine(Gx, Gy, Gx + Gs, Gy, Paper, 2.f);
	DrawLine(Gx + Gs, Gy, Gx + Gs, Gy + Gs, Paper, 2.f);
	DrawLine(Gx + Gs, Gy + Gs, Gx, Gy + Gs, Paper, 2.f);
	DrawLine(Gx, Gy + Gs, Gx, Gy, Paper, 2.f);
	Txt(GearTxt, Gx + Gs * 0.5f, Gy + 2.f * U, Big, 1.5f * U, Paper, 1);
	const int32 Lit = FMath::RoundToInt(FMath::Clamp(Bike->Rpm / 14000.f, 0.f, 1.f) * 14.f);
	for (int32 I = 0; I < 14; ++I)
	{
		const float Sh = (10.f + I * 1.6f) * U;
		const FLinearColor C = I < Lit ? (I >= 12 ? Danger : Paper) : Muted;
		DrawRect(C, Gx - 20.f * U - (14 - I) * 14.f * U, Gy + Gs - Sh, 10.f * U, Sh);
	}

	// ---- dinheiro e respeito (canto superior direito) ----
	Txt(TEXT("R$ ") + GrauFmt::Int(S->Money), W - 40.f * U, 30.f * U, Big, 2.3f * U, Go, 2);
	const int32 Stars = S->Stars();
	for (int32 I = 0; I < 5; ++I)
	{
		const float Sx = W - 40.f * U - (5 - I) * 30.f * U;
		DrawRect(I < Stars ? EmberHot : Muted, Sx, 100.f * U, 20.f * U, 20.f * U);
	}
	Txt(GrauFmt::Int(S->Points) + TEXT(" PTS · RESPEITO"), W - 40.f * U, 128.f * U, Med, 1.1f * U, Paper, 2);

	// ---- marcador do grau (topo, centro) ----
	if (S->bGrauOn)
	{
		const float Cx = W * 0.5f, Cy = 150.f * U, R = 95.f * U;
		Arc(Cx, Cy, R, 0.f, 180.f, FLinearColor(0.95f, 0.92f, 0.88f, 0.18f), 9.f * U);
		Arc(Cx, Cy, R, 40.f, 66.f, FLinearColor(0.55f, 1.f, 0.68f, 0.75f), 9.f * U);
		Arc(Cx, Cy, R, 74.5f, 90.f, FLinearColor(1.f, 0.23f, 0.3f, 0.85f), 9.f * U);
		const float Deg = FMath::Clamp(FMath::Abs(Bike->PitchDeg()), 0.f, 90.f);
		const float A = FMath::DegreesToRadians(Bike->Pitch >= 0.f ? Deg : 180.f - Deg);
		DrawLine(Cx, Cy, Cx - FMath::Cos(A) * R, Cy - FMath::Sin(A) * R, Paper, 4.f * U);
		Txt(FString::Printf(TEXT("%d°"), FMath::RoundToInt(Deg)), Cx, Cy + 8.f * U, Big, 1.7f * U, Paper, 1);
		Txt((S->bInStoppie ? TEXT("STOPPIE +") : TEXT("+")) + GrauFmt::Int(FMath::RoundToInt64(S->Pending)), Cx, Cy + 58.f * U, Big, 1.6f * U, EmberHot, 1);
		FString Chips;
		for (const FString& C : S->Chips)
		{
			Chips += (Chips.IsEmpty() ? TEXT("") : TEXT("   ")) + C;
		}
		Txt(Chips, Cx, Cy + 100.f * U, Med, 1.1f * U, EmberHot, 1);
	}

	// ---- mensagens grandes ----
	if (S->Toast.TimeLeft > 0.f)
	{
		const float Pop = FMath::Clamp(S->Toast.Age / 0.18f, 0.f, 1.f);
		const float Sc = (3.6f - 1.f * (1.f - Pop)) * U;
		Txt(S->Toast.Big, W * 0.5f, H * 0.28f, Big, Sc, S->Toast.Color, 1);
		Txt(S->Toast.Small, W * 0.5f, H * 0.28f + 90.f * U, Med, 1.6f * U, S->Toast.Color.Equals(Danger, 0.05f) ? FLinearColor(1.f, 0.76f, 0.78f) : Go, 1);
	}
	for (int32 I = 0; I < S->Ticks.Num(); ++I)
	{
		const FGrauTick& T = S->Ticks[I];
		const float Alpha = FMath::Clamp(T.TimeLeft / 0.4f, 0.f, 1.f);
		Txt(T.Text, W * 0.5f, H * 0.7f - (S->Ticks.Num() - 1 - I) * 34.f * U, Med, 1.5f * U, FLinearColor(1.f, 0.82f, 0.6f, Alpha), 1);
	}

	// ---- combo ----
	const float MapSize = 300.f * U;
	const float BarY = H - MapSize - 70.f * U;
	Txt(FString::Printf(TEXT("COMBO x%s"), *GrauFmt::Dec(S->Combo, 2)), 40.f * U, BarY - 30.f * U, Med, 1.1f * U, Paper, 0);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), 40.f * U, BarY, MapSize, 7.f * U);
	DrawRect(Ember, 40.f * U, BarY, MapSize * FMath::Clamp(S->ComboTime / 3.f, 0.f, 1.f), 7.f * U);

	// ---- desafio e aviso de interação ----
	if (GM)
	{
		if (GM->bMissionActive)
		{
			const float Pw = 560.f * U;
			Panel(W * 0.5f - Pw * 0.5f, 300.f * U, Pw, 112.f * U);
			Txt(GM->MissionTitle.ToUpper(), W * 0.5f, 308.f * U, Big, 1.2f * U, EmberHot, 1);
			Txt(GM->MissionObjective, W * 0.5f, 345.f * U, Med, 1.2f * U, Paper, 1);
			Txt(FString::Printf(TEXT("%d s"), FMath::Max(0, FMath::CeilToInt(GM->MissionTimeLeft))), W * 0.5f, 372.f * U, Big, 1.2f * U, Paper, 1);
		}
		if (!GM->PromptText.IsEmpty())
		{
			TArray<FString> Lines;
			GM->PromptText.ParseIntoArrayLines(Lines);
			const float Pw = 900.f * U;
			Panel(W * 0.5f - Pw * 0.5f, H - 300.f * U, Pw, (20.f + Lines.Num() * 34.f) * U);
			for (int32 I = 0; I < Lines.Num(); ++I)
			{
				Txt(Lines[I], W * 0.5f, H - 290.f * U + I * 34.f * U, Med, 1.3f * U, I == 0 ? EmberHot : Paper, 1);
			}
		}
	}

	// ---- dicas dos primeiros segundos ----
	const int32 TipIdx = FMath::FloorToInt((Now - FirstDrawTime) / 7.f);
	if (TipIdx >= 0 && TipIdx < (int32)UE_ARRAY_COUNT(Tips))
	{
		Panel(30.f * U, 30.f * U, 700.f * U, 54.f * U);
		Txt(Tips[TipIdx], 46.f * U, 42.f * U, Med, 1.25f * U, Paper, 0);
	}

	DrawMinimap(Bike, GM, U);

	if (GetWorld()->IsPaused())
	{
		DrawRect(FLinearColor(0.03f, 0.02f, 0.04f, 0.55f), 0.f, 0.f, W, H);
		Txt(TEXT("PAUSADO"), W * 0.5f, H * 0.4f, Big, 4.f * U, Paper, 1);
		Txt(TEXT("P continua · C câmera · R volta pra pista · M som"), W * 0.5f, H * 0.4f + 110.f * U, Med, 1.4f * U, Paper, 1);
	}
}

void AGrauHUD::DrawMinimap(AGrauBike* Bike, AGrauGameMode* GM, float U)
{
	const float Size = 300.f * U;
	const float X0 = 40.f * U, Y0 = Canvas->ClipY - Size - 40.f * U;
	const FVector2D C(X0 + Size * 0.5f, Y0 + Size * 0.5f);
	const FBox2D Rect(FVector2D(X0, Y0), FVector2D(X0 + Size, Y0 + Size));
	Panel(X0, Y0, Size, Size, 0.78f);

	const float ViewM = 170.f + FMath::Min(Bike->KmH(), 200.f) * 0.9f;
	const float K = (Size * 0.5f) / ViewM;
	const float Yaw = FMath::DegreesToRadians(Bike->GetYawDeg());
	const float Cs = FMath::Cos(Yaw), Sn = FMath::Sin(Yaw);
	const FVector2D Me(Bike->RearContact().X / 100.f, Bike->RearContact().Y / 100.f);
	auto ToScreen = [&](const FVector2D& WorldM)
	{
		const FVector2D D = WorldM - Me;
		const float Fwd = D.X * Cs + D.Y * Sn;
		const float Right = -D.X * Sn + D.Y * Cs;
		return FVector2D(C.X + Right * K, C.Y - Fwd * K);
	};

	// mar e orla
	{
		FVector2D A = ToScreen(FVector2D(-3000.f, GrauLayout::SeaY)), B = ToScreen(FVector2D(3000.f, GrauLayout::SeaY));
		if (ClipLine(A, B, Rect)) DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.1f, 0.3f, 0.42f), FMath::Max(3.f, 60.f * K));
	}
	for (TActorIterator<AGrauCityGenerator> It(GetWorld()); It; ++It)
	{
		for (const FGrauRoadSeg& R : It->Roads)
		{
			FVector2D A = ToScreen(R.A), B = ToScreen(R.B);
			if (ClipLine(A, B, Rect))
			{
				DrawLine(A.X, A.Y, B.X, B.Y, FLinearColor(0.45f, 0.44f, 0.48f), FMath::Max(2.f, R.HalfWidth * 2.f * K));
			}
		}
		break;
	}
	// desafios (presos na borda quando longe)
	if (GM)
	{
		for (int32 I = 0; I < GM->MapBlips.Num(); ++I)
		{
			FVector2D P = ToScreen(FVector2D(GM->MapBlips[I].X, GM->MapBlips[I].Y) / 100.f);
			P.X = FMath::Clamp(P.X, X0 + 8.f * U, X0 + Size - 8.f * U);
			P.Y = FMath::Clamp(P.Y, Y0 + 8.f * U, Y0 + Size - 8.f * U);
			const bool bDone = GM->MapBlipDone.IsValidIndex(I) && GM->MapBlipDone[I];
			const FLinearColor Col = GM->MapBlipActive == I ? FLinearColor(1.f, 0.82f, 0.23f) : (bDone ? FLinearColor(0.78f, 0.64f, 0.5f) : Ember);
			DrawRect(FLinearColor::Black, P.X - 9.f * U, P.Y - 9.f * U, 18.f * U, 18.f * U);
			DrawRect(Col, P.X - 7.f * U, P.Y - 7.f * U, 14.f * U, 14.f * U);
		}
	}
	// jogador (seta para cima) e norte
	const float A = 14.f * U;
	DrawLine(C.X, C.Y - A, C.X + A * 0.75f, C.Y + A * 0.8f, Ember, 3.f);
	DrawLine(C.X + A * 0.75f, C.Y + A * 0.8f, C.X, C.Y + A * 0.35f, Ember, 3.f);
	DrawLine(C.X, C.Y + A * 0.35f, C.X - A * 0.75f, C.Y + A * 0.8f, Ember, 3.f);
	DrawLine(C.X - A * 0.75f, C.Y + A * 0.8f, C.X, C.Y - A, Ember, 3.f);
	FVector2D N = ToScreen(Me + FVector2D(0.f, -ViewM * 0.85f));
	N.X = FMath::Clamp(N.X, X0 + 12.f * U, X0 + Size - 24.f * U);
	N.Y = FMath::Clamp(N.Y, Y0 + 6.f * U, Y0 + Size - 30.f * U);
	Txt(TEXT("N"), N.X, N.Y, GEngine->GetMediumFont(), 1.2f * U, Paper, 1);
}
