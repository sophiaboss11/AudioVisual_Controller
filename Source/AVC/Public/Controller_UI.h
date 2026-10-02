// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UNiagaraComponent;

/**
 * 
 */
class AVC_API Controller_UI : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(Controller_UI)
		: _World(nullptr)
	{}
		/** Game world in which the particle owning component is created. */
		SLATE_ARGUMENT(UWorld*, World)
	SLATE_END_ARGS()

	/** Constructs this widget with InArgs */
	void Construct(const FArguments& InArgs);

	/**
	 * Writes DataChannelVal (10) to the ndc_particles Niagara Data Channel,
	 * then reads the value back from the channel and prints it.
	 * Requires a valid game world.
	 */
	void WriteAndReadDataChannel(UWorld* World, bool bWriteDataChannelVal = true, bool bWriteWindSpeed = true);

	/** Sets DataChannelVal and immediately writes it to the data channel. */
	void SetDataChannelVal(float NewValue);

	float GetDataChannelVal() const { return DataChannelVal; }

	FVector GetWindSpeed() const { return myWindSpeed; }

	/** Stores the Niagara component of the BlowingParticles actor. */
	void SetBlowingParticles(UNiagaraComponent* InComp) { BlowingParticlesComponent = InComp; }

	UNiagaraComponent* GetBlowingParticles() const { return BlowingParticlesComponent.Get(); }

private:
	/** Called by the mouse pad whenever the drawing position changes; stores and prints it in mousePadVal. */
	void OnMousePadPositionChanged(const FVector2D& NewPosition, const FVector2D& NormalizedPosition);

	/** Wind speed magnitude at the edges of the mouse pad (Y and Z axes). */
	static constexpr double MaxPadWindSpeed = 1000.0;

	/** Width (X) and height (Y) location of the last click on the mouse pad, in pad-local pixels. */
	FVector2D mousePadVal = FVector2D::ZeroVector;

	/** Value written to the DataChannelVal attribute of the data channel. */
	float DataChannelVal = 10.0f;

	/** Value written to the "Wind Speed" user parameter of the BlowingParticles system. */
	FVector myWindSpeed = FVector(10.0, 0.0, 0.0);

	/** World the data channel is written to; set by WriteAndReadDataChannel. */
	TWeakObjectPtr<UWorld> CachedWorld;

	/** Actor created at runtime whose root component owns the spawned particles. */
	TWeakObjectPtr<AActor> OwnerActor;

	/** Scene component (root of OwnerActor) that owns the spawned particles; created in Construct. */
	TWeakObjectPtr<USceneComponent> OwningComponent;

	/** Niagara component of the BlowingParticles actor in the level. */
	TWeakObjectPtr<UNiagaraComponent> BlowingParticlesComponent;

	/** World location of the particle owning component. */
	const FVector ParticleLocation = FVector(-9810.0, 7830.0, -14130.0);
};
