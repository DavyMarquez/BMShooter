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
	}
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
		FVector mousePos;
		FVector mouseDir;

		APlayerController* playerController = Cast<APlayerController>(GetController());
		FVector2D screenPos = GEngine->GameViewport->Viewport->GetSizeXY();
		playerController->DeprojectScreenPositionToWorld(screenPos.X / 2.0f, screenPos.Y / 2.0f, mousePos, mouseDir);
		
		mouseDir *= 100000.0f;

		ServerFire(mousePos, mouseDir);
	}

	// try and play a firing animation if specified
	if (FPFireAnimation != NULL)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = FPMesh->GetAnimInstance();
		if (AnimInstance != NULL)
		{
			AnimInstance->Montage_Play(FPFireAnimation, 1.f);
		}
	}
}

void ABMShooterCharacter::Fire(const FVector pos, const FVector dir) {
	//GEngine->AddOnScreenDebugMessage(-1, 3, FColor::Blue, FString(TEXT("Fire")));
	//DrawDebugLine(GetWorld(), pos, dir, FColor::Red, true, 100, 0, 5.0f);

	FVector muzzlePosition = FPMuzzleLocation->GetComponentLocation();
	if (GetWorld()) {
		FActorSpawnParameters SpawnInfo;
		GetWorld()->SpawnActor< ABMShooterProjectile>(ProjectileClass, muzzlePosition, dir.Rotation(), SpawnInfo);
	}
}

void ABMShooterCharacter::ServerFire_Implementation(const FVector pos, const FVector dir) {
	Fire(pos, dir);
	MultiCastShootEffects();
}

void ABMShooterCharacter::MultiCastShootEffects_Implementation(){
	if (TPFireAnimation != NULL) {
		UAnimInstance* animInstance = GetMesh()->GetAnimInstance();
		if (animInstance != NULL) {
			animInstance->Montage_Play(TPFireAnimation, 1.0f);
		}
	}
	if (FireSound != NULL)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, GetActorLocation());
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
}

