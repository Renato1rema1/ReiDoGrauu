#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "GrauTypes.h"
#include "GrauBike.generated.h"

class UBoxComponent;
class UStaticMeshComponent;
class USkeletalMeshComponent;
class USpotLightComponent;
class USpringArmComponent;
class UCameraComponent;
class UGrauEngineSynth;
class UGrauScoreComponent;
class UStaticMesh;
class USkeletalMesh;
class UAnimationAsset;
class UMaterialInstanceDynamic;

// Peça do modelo provisório (feito com formas básicas da engine)
struct FGrauPart
{
	UStaticMeshComponent* Comp = nullptr;
	uint8 Kind = 0; // 0 pintura, 1 detalhe laranja, 2 preto fosco, 3 metal, 4 pneu, 5 farol, 6 lanterna, 7 roupa, 8 viseira, 9 dourado
};

UENUM(BlueprintType)
enum class EGrauCamera : uint8
{
	Chase UMETA(DisplayName = "Perseguicao"),
	Near UMETA(DisplayName = "Proxima"),
	Cinema UMETA(DisplayName = "Cinema"),
	Helmet UMETA(DisplayName = "Capacete")
};

// A moto do jogador. Física própria (arcade realista) em metros/segundo; a engine só cuida
// de colisão (varredura da caixa), traços de chão, render, som e ragdoll no tombo.
// Convenções: X = frente, Y = direita, Z = cima. Inclinação positiva = deitando pra direita.
UCLASS()
class REIDOGRAU_API AGrauBike : public APawn
{
	GENERATED_BODY()

public:
	AGrauBike();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

protected:
	virtual void BeginPlay() override;

public:
	// ------------------------------------------------------------------ componentes
	UPROPERTY(VisibleAnywhere, Category = "Grau") UBoxComponent* Collision;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* LeanPivot;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* WheeliePivot;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* StoppiePivot;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* ModelRoot;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UBoxComponent* BikeBody;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UBoxComponent* RiderBody;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* RearWheel;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* FrontSteer;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* SteerYaw;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USceneComponent* FrontWheel;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UStaticMeshComponent* BikeMeshComp;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USkeletalMeshComponent* RiderSkeletal;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USpotLightComponent* Headlight;
	UPROPERTY(VisibleAnywhere, Category = "Grau") USpringArmComponent* SpringArm;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UCameraComponent* Camera;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UGrauEngineSynth* EngineSound;
	UPROPERTY(VisibleAnywhere, Category = "Grau") UGrauScoreComponent* Score;

	// ------------------------------------------------------------------ modelos reais (opcionais)
	// Moto importada (Fab / Sketchfab). Frente da moto deve apontar para +X; ajuste em BikeMeshOffset.
	UPROPERTY(EditAnywhere, Category = "Grau|Visual") UStaticMesh* BikeMesh = nullptr;
	UPROPERTY(EditAnywhere, Category = "Grau|Visual") FTransform BikeMeshOffset;
	// Piloto com esqueleto (ex.: Manny do template Third Person) + uma pose/animação sentado na moto.
	// Com isso o tombo vira ragdoll de verdade.
	UPROPERTY(EditAnywhere, Category = "Grau|Visual") USkeletalMesh* RiderMesh = nullptr;
	UPROPERTY(EditAnywhere, Category = "Grau|Visual") UAnimationAsset* RiderPose = nullptr;
	UPROPERTY(EditAnywhere, Category = "Grau|Visual") FTransform RiderMeshOffset;
	UPROPERTY(EditAnywhere, Category = "Grau|Visual") FLinearColor PaintColor = FLinearColor(0.012f, 0.013f, 0.016f);
	UPROPERTY(EditAnywhere, Category = "Grau|Visual") FLinearColor AccentColor = FLinearColor(1.f, 0.16f, 0.01f);

	// ------------------------------------------------------------------ ajustes
	UPROPERTY(EditAnywhere, Category = "Grau|Fisica") bool bAssist = true;
	UPROPERTY(EditAnywhere, Category = "Grau|Fisica") float MassKg = 285.f;
	UPROPERTY(EditAnywhere, Category = "Grau|Fisica") float PowerWatts = 150000.f;
	UPROPERTY(EditAnywhere, Category = "Grau|Fisica") float CrashImpactMS = 8.5f;
	UPROPERTY(EditAnywhere, Category = "Grau|Fisica") float SeaLevelCm = -35.f;
	UPROPERTY(EditAnywhere, Category = "Grau|Camera") EGrauCamera CameraMode = EGrauCamera::Chase;

	// ------------------------------------------------------------------ estado (HUD / pontuação)
	float SpeedMS = 0.f;
	float Lean = 0.f;       // rad
	float Pitch = 0.f;      // rad (+ grau / - stoppie)
	float Omega = 0.f;
	float Rpm = 1400.f;
	int32 Gear = 1;
	float Throttle = 0.f;
	float Brake = 0.f;
	float SteerS = 0.f;
	float Wheelspin = 0.f;
	float AirTime = 0.f;
	float CrashTime = 0.f;
	bool bAir = false;
	bool bCrashed = false;
	bool bScraping = false;
	bool bBurnout = false;
	bool bPullHeld = false;
	bool bPushHeld = false;
	FString CrashReason;
	TArray<FGrauBikeEvent> FrameEvents;

	float KmH() const { return FMath::Abs(SpeedMS) * 3.6f; }
	float PitchDeg() const { return FMath::RadiansToDegrees(Pitch); }
	FVector Forward() const;
	FVector RearContact() const { return Pos; }
	FVector FocusPoint() const;
	float GetYawDeg() const { return YawDeg; }

	void CycleCamera();
	void Respawn();
	void PlaceAt(const FVector& Location, float InYawDeg);
	void SetMuted(bool bMute);

private:
	// física
	void StepPhysics(float Dt, const FGrauInput& In);
	bool MoveAndCollide(float Dt);
	bool TraceGround(float X, float Y, float FromZ, float& OutZ, float* OutGrip = nullptr) const;
	FGrauInput ReadInput() const;
	void Crash(const FString& Reason, float Impact);
	void UpdateCrash(float Dt);
	void SyncVisual(float Dt);
	void UpdateCamera(float Dt);
	void AddEvent(EGrauEvent Type, float Value = 0.f, float Value2 = 0.f, bool bFlag = false, const FString& Reason = FString());
	void ApplyMaterials();
	void BuildPlaceholder();
	UStaticMeshComponent* AddPart(USceneComponent* Parent, UStaticMesh* Mesh, const FVector& LocCm, const FRotator& Rot, const FVector& SizeCm, uint8 Kind);

	FVector Pos = FVector::ZeroVector;   // contato traseiro (cm)
	float YawDeg = 0.f;
	float Vy = 0.f;                       // m/s vertical
	float LastGroundZ = 0.f;
	float GroundVy = 0.f;
	float GroundPitch = 0.f;
	float ShiftTimer = 0.f;
	float ReverseTimer = 0.f;
	float RearAngle = 0.f;
	float FrontAngle = 0.f;
	float Susp = 0.f;
	float SuspVel = 0.f;
	float FootDown = 1.f;
	float SafeTimer = 0.f;
	bool bPrevPull = false;
	FVector SafePos = FVector::ZeroVector;
	float SafeYaw = 0.f;
	float Accum = 0.f;
	float Grip = 1.05f;
	float SlowMoTimer = 0.f;
	bool bSkeletalRider = false;

	// câmera
	float CamYaw = 0.f;
	float CamYawOff = 0.f;
	float CamPitchOff = 0.f;
	float CamFov = 80.f;
	float CamShake = 0.f;
	float CamKick = 0.f;
	float LastMouseTime = -10.f;
	FVector CrashCamPos = FVector::ZeroVector;
	bool bCrashCamSet = false;

	// tombo
	FTransform BikeBodyRel;
	FTransform RiderBodyRel;
	bool bRagdoll = false;
	bool bMuted = false;

	TArray<FGrauPart> Parts;
	UPROPERTY(Transient) TArray<UMaterialInstanceDynamic*> Mids;
};
