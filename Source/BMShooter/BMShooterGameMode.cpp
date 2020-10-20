// Copyright 1998-2019 Epic Games, Inc. All Rights Reserved.

#include "BMShooterGameMode.h"
#include "BMShooterHUD.h"
#include "BMShooterCharacter.h"
#include "UObject/ConstructorHelpers.h"

ABMShooterGameMode::ABMShooterGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ABMShooterHUD::StaticClass();
}
