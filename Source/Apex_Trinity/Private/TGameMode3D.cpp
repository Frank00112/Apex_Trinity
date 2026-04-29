// Fill out your copyright notice in the Description page of Project Settings.


#include "TGameMode3D.h"
#include "TacticalCamera3D.h"

ATGameMode3D::ATGameMode3D()
{
	// Forcing the 3D camera
	DefaultPawnClass = ATacticalCamera3D::StaticClass();
}