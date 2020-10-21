// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BMShooterCharacter.h"
#include "BMShooterProjectile.h"
#include "Animation/AnimInstance.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/Engine.h"
#include "DrawDebugHelpers.h"
#include "Net/UnrealNetwork.h"
#include "Components/HealthComponent.h"
#include "TimerManager.h"
#include "NavigationSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogFPChar, Warning, All);

//////////////////////////////////////////////////////////////////////////
// ABMShooterCharacter

ABMShooterCharacter::ABMShooterCharacter()
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(55.f, 96.0f);

	// set our turn rates for input
	BaseTurnRate = 45.f;
	BaseLookUpRate = 45.f;

	// Create a CameraComponent	
	FirstPersonCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("FirstPersonCamera"));
	FirstPersonCameraComponent->SetupAttachment(GetCapsuleComponent());
	FirstPersonCameraComponent->SetRelativeLocation(FVector(-39.56f, 1.75f, 64.f)); // Position the camera
	FirstPersonCameraComponent->bUsePawnControlRotation = true;

	// Create a mesh component that will be used when being viewed from a '1st person' view (when controlling this pawn)
	FPMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("CharacterMesh1P"));
	FPMesh->SetOnlyOwnerSee(true);
	FPMesh->SetupAttachment(FirstPersonCameraComponent);
	FPMesh->bCastDynamicShadow = false;
	FPMesh->CastShadow = false;
	FPMesh->SetRelativeRotation(FRotator(1.9f, -19.19f, 5.2f));
	FPMesh->SetRelativeLocation(FVector(-0.5f, -4.4f, -155.7f));
	FPMesh->SetCollisionObjectType(ECC_Pawn);

	// Create a gun mesh component
	FPGun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FPGun"));
	FPGun->SetOnlyOwnerSee(true);			// only the owning player will see this mesh
	FPGun->bCastDynamicShadow = false;
	FPGun->CastShadow = false;
	FPGun->SetupAttachment(RootComponent);

	FPMuzzleLocation = CreateDefaultSubobject<USceneComponent>(TEXT("MuzzleLocation"));
	FPMuzzleLocation->SetupAttachment(FPGun);
	FPMuzzleLocation->SetRelativeLocation(FVector(0.2f, 48.4f, -10.6f));

	// Default offset from the character location for projectiles to spawn
	GunOffset = FVector(100.0f, 0.0f, 10.0f);

	// third person gun mesh seen by others
	TPGun = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("TPGun"));
	TPGun->SetOwnerNoSee(true);
	TPGun->SetupAttachment(GetMesh(), TEXT("RightHand"));

	healthComponent = CreateDefaultSubobject<UHealthComponent>(TEXT("HealthComponent"));
	if (healthComponent) {
		healthComponent->SetIsReplicated(true);

		// bind healthmodifieddelegate to local function
		healthComponent->OnHealthModifiedDelegate.AddDynamic(this, &ABMShooterCharacter::HealthModified);
	}

	respawnTime = 10.0f;
	characterDead = false;
}

void ABMShooterCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();
	
	//Attach gun mesh component to Skeleton, doing it here because the skeleton is not yet created in the constructor
	FPGun->AttachToComponent(FPMesh, FAttachmentTransformRules(EAttachmentRule::SnapToTarget, true), TEXT("GripPoint"));
	FPGun->SetHiddenInGame(false);

	TPGun->SetOwnerNoSee(true);
	
	// hide third person mesh
	GetMesh()->SetOwnerNoSee(true);

}

//////////////////////////////////////////////////////////////////////////
// Input

void ABMShooterCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// set up gameplay key bindings
	check(PlayerInputComponent);

	// Bind jump events
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	// Bind fire event
	PlayerInputComponent->BindAction("Fire", IE_Pressed, this, &ABMShooterCharacter::OnFire);

	// Bind movement events
	PlayerInputComponent->BindAxis("MoveForward", this, &ABMShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("MoveRight", this, &ABMShooterCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("TurnRate", this, &ABMShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("LookUp", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("LookUpRate", this, &ABMShooterCharacter::LookUpAtRate);
}

void ABMShooterCharacter::OnFire()
{
	// try and fire a projectile
	if (ProjectileClass != NULL)
	{
		// call bp event
		Fire();
	}
}

void ABMShooterCharacter::MoveForward(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorForwardVector(), Value);
	}
}

void ABMShooterCharacter::MoveRight(float Value)
{
	if (Value != 0.0f)
	{
		// add movement in that direction
		AddMovementInput(GetActorRightVector(), Value);
	}
}

void ABMShooterCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * BaseTurnRate * GetWorld()->GetDeltaSeconds());
}

void ABMShooterCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * BaseLookUpRate * GetWorld()->GetDeltaSeconds());

	FRotator rotation = FirstPersonCameraComponent->GetComponentRotation();
	CorrectPitchServer(rotation);
}

void ABMShooterCharacter::OnRep_CharacterDead()
{
	if (characterDead)
	{
		
		ActivateRagdoll();
	}
	else
	{
		// If respawn restore character
		ResetCharacter();
	}
}

void ABMShooterCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const {
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	//Replicate current health.
	DOREPLIFETIME(ABMShooterCharacter, characterDead);
}

void ABMShooterCharacter::RespawnCharacter() {
	// spawn on server
	if (GetLocalRole() == ROLE_Authority) {
		UNavigationSystemV1* navigationSystem = UNavigationSystemV1::GetCurrent(GetWorld());
		FNavLocation navLocation;
		
		navigationSystem->GetRandomPoint(navLocation); // get random point on nav mesh
		SetActorLocation(navLocation);
		SetActorLocation(GetActorLocation() + FVector(0.0f, 0.0f, 180.0f)); // it appeared on the floor otherwise
		
		healthComponent->ResetHealth();
		characterDead = false;
	}
}

void ABMShooterCharacter::ActivateRagdoll() {
	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetCollisionProfileName("Ragdoll");

	if (IsLocallyControlled()) {
		// hide fp meshes and show tp meshes
		FPMesh->SetOwnerNoSee(true);
		FPGun->SetOwnerNoSee(true);
		GetMesh()->SetOwnerNoSee(false);
		TPGun->SetOwnerNoSee(false);
		
		// disable input
		DisableInput(Cast<APlayerController>(GetController()));

		// to see the ragdoll when dead
		FirstPersonCameraComponent->bUsePawnControlRotation = false;
		FirstPersonCameraComponent->SetRelativeRotation(FRotator(-90.0f, 0.0f, 0.0f));
	}
}

void ABMShooterCharacter::ResetCharacter() {
	GetMesh()->AttachTo(GetCapsuleComponent(), NAME_None, EAttachLocation::SnapToTarget, true);
	GetMesh()->SetSimulatePhysics(false);
	GetMesh()->SetCollisionProfileName("CharacterMesh");

	if (IsLocallyControlled()) {
		// hide tp meshes and show fp meshes
		GetMesh()->SetOwnerNoSee(true);
		TPGun->SetOwnerNoSee(true);
		FPMesh->SetOwnerNoSee(false);
		FPGun->SetOwnerNoSee(false);

		// enable input 
		EnableInput(Cast<APlayerController>(GetController()));

		//reset camera values
		
		FirstPersonCameraComponent->SetRelativeRotation(FRotator(90.0f, 0.0f, 0.0f));
		FirstPersonCameraComponent->bUsePawnControlRotation = true;
	}
}

void ABMShooterCharacter::HealthModified() {
	if (GetLocalRole() == ROLE_Authority) {

		// check on server if the character is dead
		if (healthComponent->GetCurrentHealth() == 0 && !characterDead) {
			characterDead = true;
			FString text = "should respawn";
			GEngine->AddOnScreenDebugMessage(-1, 5.f, FColor::Blue, text);
			// Respawn character using unreal timer
			FTimerHandle respawnTimer;
			GetWorldTimerManager().SetTimer<ABMShooterCharacter>(respawnTimer, this, 
				&ABMShooterCharacter::RespawnCharacter, respawnTime, false);
		}
	}


	// modify widget
	/*if (IsLocallyControlled())
	{
		OnHealthModification();
	} */
}

void ABMShooterCharacter::CorrectPitchServer_Implementation(FRotator rotation) {
	CorrectPitchMulticast(rotation);
}

void ABMShooterCharacter::CorrectPitchMulticast_Implementation(FRotator rotation) {
	correctedRotation = rotation;
}