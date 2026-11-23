#pragma once

#include "CoreMinimal.h"

// Comandos do piloto já combinados (teclado + controle)
struct FGrauInput
{
	float Throttle = 0.f;   // 0..1
	float Brake = 0.f;      // 0..1
	float Steer = 0.f;      // -1 esquerda .. +1 direita
	bool bPull = false;     // puxar o grau (peso pra trás + embreagem)
	bool bPush = false;     // peso pra frente
	bool bAnalog = false;   // veio de gatilho/analógico
};

enum class EGrauEvent : uint8
{
	Shift,
	Clutch,
	Land,        // a frente do grau voltou ao chão
	Takeoff,
	Landing,     // pouso depois de um salto
	Bump,
	Donut,
	Crash
};

struct FGrauBikeEvent
{
	EGrauEvent Type = EGrauEvent::Bump;
	float Value = 0.f;   // impacto, tempo no ar, etc.
	float Value2 = 0.f;
	bool bFlag = false;
	FString Reason;
};

namespace GrauFmt
{
	// 1234567 -> "1.234.567"
	inline FString Int(int64 N)
	{
		const bool bNeg = N < 0;
		FString Digits = FString::Printf(TEXT("%lld"), bNeg ? -N : N);
		FString Out;
		const int32 Len = Digits.Len();
		for (int32 i = 0; i < Len; ++i)
		{
			Out.AppendChar(Digits[i]);
			const int32 Left = Len - i - 1;
			if (Left > 0 && Left % 3 == 0)
			{
				Out.AppendChar(TEXT('.'));
			}
		}
		return bNeg ? TEXT("-") + Out : Out;
	}

	// 1.5 -> "1,5"
	inline FString Dec(float V, int32 Places = 1)
	{
		FString S = FString::SanitizeFloat(FMath::RoundToFloat(V * FMath::Pow(10.f, (float)Places)) / FMath::Pow(10.f, (float)Places), Places);
		return S.Replace(TEXT("."), TEXT(","));
	}
}
