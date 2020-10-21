// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BMShooterCharacter.generated.h"

class UInputComponent;

UCLASS(config=Game)
class ABMShooterCharacter : public ACharacter
{
	GENERATED_BODY()


public: // Public functions
	ABMShooterCharacter();

	/** Returns Mesh1P subobject **/
	FORCEINLINE class USkeletalMeshComponent* GetMesh1P() const { return FPMesh; }

	/** Returns FirstPersonCameraComponent subobject **/
	FORCEINLINE class UCameraComponent* GetFirstPersonCameraComponent() const { return FirstPersonCameraComponent; }

	// Property replication
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay();

	/** Fires a projectile. */
	void OnFire();


	UFUNCTION(BlueprintImplementableEvent)
	void Fire();

	/** Handles moving forward/backward */
	void MoveForward(float Val);

	/** Handles stafing movement, left and right */
	void MoveRight(float Val);

	/**
	 * Called via input to turn at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void TurnAtRate(float Rate);

	/**
	 * Called via input to turn look up/down at a given rate.
	 * @param Rate	This is a normalized rate, i.e. 1.0 means 100% of desired turn rate
	 */
	void LookUpAtRate(float Rate);

	// APawn interface
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;
	// End of APawn interface

	// For when the character dies
	UFUNCTION()
	void OnRep_CharacterDead();

	void RespawnCharacter();

	void ActivateRagdoll();

	void ResetCharacter();

	UFUNCTION()
	void HealthModified();

	UFUNCTION(Server, Unreliable)
	void CorrectPitchServer(FRotator rotation);

	UFUNCTION(NetMultiCast, Unreliable)
		void CorrectPitchMulticast(FRotator rotation);

public: // Public variables

	/** Pawn mesh: 1st person view (arms; seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Mesh)
		class USkeletalMeshComponent* FPMesh;

	/** Gun mesh: 1st person view (seen only by self) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
		class USkeletalMeshComponent* FPGun;

	/** Location on gun mesh where projectiles should spawn. (first person)*/
	UPROPERTY(VisibleDefaultsOnly, BlueprintReadOnly, Category = Mesh)
		class USceneComponent* FPMuzzleLocation;

	/** AnimMontage to play each time we fire (first person) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Gameplay)
		class UAnimMontage* FPFireAnimation;

	/** First person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera, meta = (AllowPrivateAccess = "true"))
		class UCameraComponent* FirstPersonCameraComponent;

	/** Gun mesh: 3rd person view (seen by others) */
	UPROPERTY(VisibleDefaultsOnly, Category = Mesh)
		class USkeletalMeshComponent* TPGun;

	/** AnimMontage to play each time we fire seen by others*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
		class UAnimMontage* TPFireAnimation;

	/** Base turn rate, in deg/sec. Other scaling may affect final turn rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
		float BaseTurnRate;

	/** Base look up/down rate, in deg/sec. Other scaling may affect final rate. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = Camera)
		float BaseLookUpRate;

	/** Gun muzzle's offset from the characters location */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
		FVector GunOffset;

	/** Projectile class to spawn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Projectile)
		TSubclassOf<class ABMShooterProjectile> ProjectileClass;

	/** Sound to play each time we fire */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Gameplay)
		class USoundBase* FireSound;
	// Health component
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = Health)
		class UHealthComponent* healthComponent = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = Gameplay)
		float respawnTime;

	UPROPERTY(ReplicatedUsing = OnRep_CharacterDead, BlueprintReadOnly)
		bool characterDead;
	
	UPROPERTY(BlueprintReadOnly)
		FRotator correctedRotation;
};

