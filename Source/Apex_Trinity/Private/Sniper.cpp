// Fill out your copyright notice in the Description page of Project Settings.


#include "Sniper.h"

ASniper::ASniper()
{
	// Sniper: low HP, low mobility, high damage and long range
	Health = 20;
	MaxHealth = 20;
	MovementRange = 4;
	AttackRange = 10;      
	MinAttackDamage = 4;  
	MaxAttackDamage = 8;
}