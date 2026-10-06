// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UNiagaraComponent;

/** AVC color palette. */
class AVC_API FAVCColors
{
public:
	static constexpr const TCHAR* Label = TEXT("AVC");

	static inline const FColor Background = FColor(0x08, 0x08, 0x10);
	static inline const FColor Card       = FColor(0x0F, 0x0F, 0x1A);
	static inline const FColor Border     = FColor(0x1E, 0x1E, 0x32);

	static inline const FColor Foreground = FColor(0xE8, 0xE8, 0xF0);
	static inline const FColor Dim        = FColor(0x98, 0x98, 0xC8);
	static inline const FColor Dimmer     = FColor(0x5A, 0x5A, 0x90);

	static inline const FColor Cyan       = FColor(0x00, 0xD0, 0xFF);
	static inline const FColor Blue       = FColor(0x40, 0x60, 0xF0);
	static inline const FColor Purple     = FColor(0x70, 0x30, 0xF0);
	static inline const FColor Fuchsia    = FColor(0xE0, 0x18, 0xC8);
	static inline const FColor Green      = FColor(0x00, 0xFF, 0x88);
};

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
	/** Builds the effect type tab bar (Particle, Fluid, Rigid Body, Volumetric, Pyro). */
	TSharedRef<SWidget> BuildEffectTabs();

	/** Index of the selected effect tab; 0 = Particle. */
	int32 SelectedEffectTab = 0;

	/** Builds the overlay trigger buttons (Flash, Strobe, Blackout, Freeze). */
	TSharedRef<SWidget> BuildOverlayTriggers();

	/** Toggle state of each overlay trigger button. */
	bool OverlayTriggerSelected[4] = { false, false, false, false };

	/** Overlay color toggle: false = White, true = Black. */
	bool bOverlayBlack = false;

	/** Time the Flash trigger was last clicked; its outline highlights briefly afterwards. */
	double FlashTriggerTime = -1000.0;

	/** Number box value next to the text overlay (0.1 - 4). */
	float OverlayValue = 1.0f;

	/** Time the Overlay button was last clicked; drives its white flash. */
	double OverlayFlashStartTime = -1000.0;

	/** Text typed into the text overlay box. */
	FString OverlayText;

	/** Builds the 3x2 grid of effect sliders. */
	TSharedRef<SWidget> BuildEffectSliders();

	/** Normalized (0..1) values of the effect sliders. */
	float EffectSliderValues[6] = { 0.5f, 0.5f, 0.5f, 0.5f, 0.5f, 0.5f };

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
