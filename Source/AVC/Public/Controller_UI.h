// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * 
 */
class AVC_API Controller_UI : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(Controller_UI)
	{}
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	float ControllerValue = 500;
};
