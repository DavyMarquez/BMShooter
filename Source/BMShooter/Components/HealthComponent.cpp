// Fill out your copyright notice in the Description page of Project Settings.


#include "HealthComponent.h"
#include "Net/UnrealNetwork.h"
#include "Engine/Engine.h"

// Sets default values for this component's properties
UHealthComponent::UHealthComponent() {
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = false;

	maxHealth = 100.0f;
	currentHealth = 100.0f;
}


// Called when the game starts
void UHealthComponent::BeginPlay() {
	Super::BeginPlay();

	if (GetOwner()) {
		GetOwner()->OnTakeAnyDamage.AddDynamic(this, &UHealthComponent::ReceiveDamage);
	}
}
 
void UHealthComponent::OnRep_CurrentHealth() {
	//OnCurrentHealthUpdate();
}

void UHealthComponent::OnCurrentHealthUpdate() {
	// Server
	if (GetOwnerRole() == ROLE_Authority)
	{
		FString healthMessage = FString::Printf(TEXT("%s now has %f health remaining."), *GetFName().ToString(), currentHealth);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Red, healthMessage);
	}

	// Client
	if (GetOwnerRole() == ROLE_AutonomousProxy && IsNetMode(NM_Client))
	{
		FString healthMessage = FString::Printf(TEXT("You now have %f health remaining."), currentHealth);
		GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, healthMessage);
	}

	// broadcast health modified event
	OnHealthModifiedDelegate.Broadcast();
}

void UHealthComponent::ReceiveDamage(AActor* DamagedActor, float Damage, const class UDamageType* DamageType, class AController* InstigatedBy, AActor* DamageCauser) {
	UHealthComponent::Damage(Damage);
}

void UHealthComponent::GetLifetimeReplicatedProps(TArray <FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//Replicate current health.
	DOREPLIFETIME(UHealthComponent, currentHealth);
}

float UHealthComponent::GetMaxHealth() const {
	return maxHealth;
}

float UHealthComponent::GetCurrentHealth() const {
	return currentHealth;
}

float UHealthComponent::GetNormalizedHealth() const {
	return currentHealth / maxHealth; //normalize result
}

void UHealthComponent::SetCurrentHealth(float healthValue) {
	if (GetOwnerRole() == ROLE_Authority) {
		currentHealth = FMath::Clamp(healthValue, 0.f, maxHealth);

		// notify server of the update
		OnCurrentHealthUpdate();  	
	}
}

void UHealthComponent::Heal(float amount) {
	SetCurrentHealth(currentHealth + amount);
}

void UHealthComponent::Damage(float amount) {
	SetCurrentHealth(currentHealth - amount);
}

void UHealthComponent::ResetHealth() {
	SetCurrentHealth(maxHealth);
}