// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "HealthComponent.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class BMSHOOTER_API UHealthComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UHealthComponent();

	// Property replication 
	void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	UFUNCTION(BlueprintCallable, Category = Health)
	void Heal(float amount);

	UFUNCTION(BlueprintCallable, Category = Health)
	void Damage(float amount);

	UFUNCTION(BlueprintPure, Category = Health)
	float GetMaxHealth() const;

	UFUNCTION(BlueprintPure, Category = Health)
	float GetCurrentHealth() const;

	UFUNCTION(BlueprintPure, Category = Health)
	float GetNormalizedHealth() const;

	// health gets clamped between 0 and maxHealth
	UFUNCTION(BlueprintCallable, Category = Health)
	void SetCurrentHealth(float health);

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	// for changes on currentHealth
	UFUNCTION()
	void OnRep_CurrentHealth();

	// Called from server after currentHealth modification and from clients afetr repNotify 
	void OnCurrentHealthUpdate();

public:

protected:
	
	// Max health of the character
	UPROPERTY(EditDefaultsOnly, Category = Health)
	float maxHealth;

	// Current player's health
	UPROPERTY(ReplicatedUsing = OnRep_CurrentHealth)
	float currentHealth;

private:

	class ABMShooterCharacter* _owner;

};
