// Fill out your copyright notice in the Description page of Project Settings.


#include "Brawler.h"

ABrawler::ABrawler()
{
	// Brawler: high HP, high mobility, minimum range
	Health = 40;
	MaxHealth = 40;
	MovementRange = 6;
	AttackRange = 1;      
	MinAttackDamage = 1;  
	MaxAttackDamage = 6;
}