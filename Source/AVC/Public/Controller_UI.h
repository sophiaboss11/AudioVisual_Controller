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

	/**
	 * Writes DataChannelVal (10) to the ndc_particles Niagara Data Channel,
	 * then reads the value back from the channel and prints it.
	 * Requires a valid game world.
	 */
	void WriteAndReadDataChannel(UWorld* World);

	/** Sets DataChannelVal and immediately writes it to the data channel. */
	void SetDataChannelVal(float NewValue);

	float GetDataChannelVal() const { return DataChannelVal; }

private:
	/** Called by the mouse pad whenever the drawing position changes; stores and prints it in mousePadVal. */
	void OnMousePadPositionChanged(const FVector2D& NewPosition);

	/** Width (X) and height (Y) location of the last click on the mouse pad, in pad-local pixels. */
	FVector2D mousePadVal = FVector2D::ZeroVector;

	/** Value written to the DataChannelVal attribute of the data channel. */
	float DataChannelVal = 10.0f;

	/** World the data channel is written to; set by WriteAndReadDataChannel. */
	TWeakObjectPtr<UWorld> CachedWorld;

	/** Actor created at runtime whose root component owns the spawned particles. */
	TWeakObjectPtr<AActor> OwnerActor;
};
