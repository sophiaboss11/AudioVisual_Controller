// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller_UI.h"
#include "SlateOptMacros.h"

#include "NiagaraDataChannelFunctionLibrary.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelPublic.h"
#include "NiagaraDataChannelCommon.h"
#include "NiagaraDataChannelAccessContext.h"
#include "NiagaraDataChannel_GameplayBurst.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Framework/Application/SlateApplication.h"

DECLARE_DELEGATE_OneParam(FOnMousePadPositionChanged, const FVector2D&);

/** Drawable pad: tracks the mouse while the left button is held and draws the strokes. */
class SMousePad : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SMousePad) {}
		SLATE_EVENT(FOnMousePadPositionChanged, OnPositionChanged)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		OnPositionChanged = InArgs._OnPositionChanged;
	}

	virtual FVector2D ComputeDesiredSize(float) const override
	{
		return FVector2D(100.0f, 100.0f);
	}

	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		bIsDrawing = true;
		Strokes.AddDefaulted();
		AddPoint(MyGeometry, MouseEvent);

		if (!FadeTimerHandle.IsValid())
		{
			FadeTimerHandle = RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SMousePad::UpdateFade));
		}
		return FReply::Handled().CaptureMouse(SharedThis(this));
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!bIsDrawing)
		{
			return FReply::Unhandled();
		}

		AddPoint(MyGeometry, MouseEvent);
		return FReply::Handled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (!bIsDrawing || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
		{
			return FReply::Unhandled();
		}

		bIsDrawing = false;
		return FReply::Handled().ReleaseMouseCapture();
	}

	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override
	{
		bIsDrawing = false;
	}

	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
	{
		const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
		const FVector2D Size = AllottedGeometry.GetLocalSize();

		// White border, then black fill inset by the border thickness.
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
			WhiteBrush, ESlateDrawEffect::None, FLinearColor::White);

		const FVector2D InnerSize(FMath::Max(0.0, Size.X - 2.0 * BorderThickness), FMath::Max(0.0, Size.Y - 2.0 * BorderThickness));
		FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
			AllottedGeometry.ToPaintGeometry(InnerSize, FSlateLayoutTransform(FVector2D(BorderThickness, BorderThickness))),
			WhiteBrush, ESlateDrawEffect::None, FLinearColor::Black);

		const double Now = FSlateApplication::Get().GetCurrentTime();
		TArray<FVector2D> Segment;
		Segment.SetNum(2);

		for (const TArray<FPadPoint>& Stroke : Strokes)
		{
			for (int32 Index = 1; Index < Stroke.Num(); ++Index)
			{
				const float Brightness = GetBrightness(Now - Stroke[Index].Time);
				if (Brightness <= 0.0f)
				{
					continue;
				}

				Segment[0] = Stroke[Index - 1].Position;
				Segment[1] = Stroke[Index].Position;
				FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(),
					Segment, ESlateDrawEffect::None, FLinearColor(Brightness, Brightness, Brightness, 1.0f), true, 2.0f);
			}
		}

		return LayerId + 2;
	}

private:
	struct FPadPoint
	{
		FVector2D Position;
		double Time;
	};

	/** How long a drawn pixel stays fully white, in seconds. */
	static constexpr double HoldDuration = 1.0;
	/** How long it then takes to fade from white to black, in seconds. */
	static constexpr double FadeDuration = 1.0;
	static constexpr double BorderThickness = 1.0;

	static float GetBrightness(double Age)
	{
		if (Age <= HoldDuration)
		{
			return 1.0f;
		}
		return (float)FMath::Clamp(1.0 - (Age - HoldDuration) / FadeDuration, 0.0, 1.0);
	}

	EActiveTimerReturnType UpdateFade(double InCurrentTime, float InDeltaTime)
	{
		// Drop points that have fully faded (keeping each stroke's last point while drawing).
		const double Now = FSlateApplication::Get().GetCurrentTime();
		const double MaxAge = HoldDuration + FadeDuration;
		for (int32 StrokeIndex = Strokes.Num() - 1; StrokeIndex >= 0; --StrokeIndex)
		{
			TArray<FPadPoint>& Stroke = Strokes[StrokeIndex];
			int32 NumExpired = 0;
			while (NumExpired + 1 < Stroke.Num() && Now - Stroke[NumExpired + 1].Time > MaxAge)
			{
				++NumExpired;
			}
			Stroke.RemoveAt(0, NumExpired);

			const bool bIsActiveStroke = bIsDrawing && StrokeIndex == Strokes.Num() - 1;
			if (!bIsActiveStroke && (Stroke.Num() == 0 || Now - Stroke.Last().Time > MaxAge))
			{
				Strokes.RemoveAt(StrokeIndex);
			}
		}

		Invalidate(EInvalidateWidgetReason::Paint);

		if (Strokes.Num() == 0 && !bIsDrawing)
		{
			FadeTimerHandle.Reset();
			return EActiveTimerReturnType::Stop;
		}
		return EActiveTimerReturnType::Continue;
	}
	void AddPoint(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		const FVector2D Size = MyGeometry.GetLocalSize();
		FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
		Local.X = FMath::Clamp(Local.X, 0.0, (double)Size.X);
		Local.Y = FMath::Clamp(Local.Y, 0.0, (double)Size.Y);

		TArray<FPadPoint>& Stroke = Strokes.Last();
		if (Stroke.Num() > 0 && Stroke.Last().Position.Equals(Local))
		{
			return;
		}

		Stroke.Add({ Local, FSlateApplication::Get().GetCurrentTime() });
		OnPositionChanged.ExecuteIfBound(Local);
	}

	FOnMousePadPositionChanged OnPositionChanged;
	TArray<TArray<FPadPoint>> Strokes;
	TSharedPtr<FActiveTimerHandle> FadeTimerHandle;
	bool bIsDrawing = false;
};
#include "Widgets/SBoxPanel.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void Controller_UI::Construct(const FArguments& InArgs)
{
	UE_LOG(LogTemp, Warning, TEXT("=== Controller_UI Construct called"));

	ChildSlot
	[
		SNew(SVerticalBox)
		// Upper half: controls
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
		SNew(SBox)
		.Padding(10.0f)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(TEXT("Particle Size")))
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SSpinBox<float>)
				.MinSliderValue(0.0f)
				.MaxSliderValue(100.0f)
				.Delta(0.1f)
				.Value(this, &Controller_UI::GetDataChannelVal)
				.OnValueChanged(this, &Controller_UI::SetDataChannelVal)
				.OnValueCommitted_Lambda([this](float NewValue, ETextCommit::Type) { SetDataChannelVal(NewValue); })
			]
		]
		]
		// Lower half: mouse input pad
		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		.Padding(10.0f)
		[
			SNew(SMousePad)
			.OnPositionChanged(this, &Controller_UI::OnMousePadPositionChanged)
		]
	];

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void Controller_UI::OnMousePadPositionChanged(const FVector2D& NewPosition)
{
	mousePadVal = NewPosition;

	UE_LOG(LogTemp, Warning, TEXT("Controller_UI - mousePadVal = (%f, %f)"), mousePadVal.X, mousePadVal.Y);
	if (GEngine)
	{
		// Fixed key so the on-screen message updates in place instead of stacking.
		GEngine->AddOnScreenDebugMessage(1001, 2.0f, FColor::Cyan,
			FString::Printf(TEXT("mousePadVal = (%.1f, %.1f)"), mousePadVal.X, mousePadVal.Y));
	}
}

void Controller_UI::SetDataChannelVal(float NewValue)
{
	DataChannelVal = NewValue;
	if (UWorld* World = CachedWorld.Get())
	{
		WriteAndReadDataChannel(World);
	}
}

void Controller_UI::WriteAndReadDataChannel(UWorld* World)
{
	// Only act on a valid game world; editor/preview worlds are ignored.
	if (!World || !World->IsGameWorld())
	{
		return;
	}
	CachedWorld = World;

	// Load the ndc_particles
	UNiagaraDataChannelAsset* DataChannel = LoadObject<UNiagaraDataChannelAsset>(nullptr, TEXT("/Game/avc_Particles/ndc_particles.ndc_particles"));
	if (!DataChannel)
	{
		UE_LOG(LogTemp, Warning, TEXT("Controller_UI::WriteAndReadDataChannel - failed to load ndc_particles asset."));
		return;
	}


	// --- Make (NDCAccess Context Gameplay Burst) Instance ---
	// ndc_particles is a Gameplay Burst data channel, so it requires a matching access context.
	FNDCAccessContextInst AccessContext{ TNDCAccessContextType(FNDCAccessContext_GameplayBurst::StaticStruct()) };
	if (!AccessContext.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("Controller_UI::WriteAndReadDataChannel - failed to create Gameplay Burst access context."));
		return;
	}

	// Create a dedicated actor with its own scene component to own the spawned particles.
	const FVector ParticleLocation(-9810.0, 7830.0, -14130.0);
	USceneComponent* OwningComponent = nullptr;
	if (AActor* Owner = OwnerActor.Get())
	{
		OwningComponent = Owner->GetRootComponent();
	}
	else
	{
		FActorSpawnParameters SpawnParams;
		SpawnParams.Name = MakeUniqueObjectName(World, AActor::StaticClass(), TEXT("AVC_ParticleOwner"));
		SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

		AActor* NewOwner = World->SpawnActor<AActor>(AActor::StaticClass(), FTransform::Identity, SpawnParams);
		if (NewOwner)
		{
			OwningComponent = NewObject<USceneComponent>(NewOwner, TEXT("ParticleRoot"));
			NewOwner->SetRootComponent(OwningComponent);
			OwningComponent->RegisterComponent();
			OwningComponent->SetWorldLocation(ParticleLocation);
			OwnerActor = NewOwner;
		}
	}

	if (!OwningComponent)
	{
		UE_LOG(LogTemp, Warning, TEXT("Controller_UI::WriteAndReadDataChannel - failed to create owning component."));
		return;
	}

	FNDCAccessContext_GameplayBurst& BurstContext = AccessContext.GetChecked<FNDCAccessContext_GameplayBurst>();
	BurstContext.OwningComponent = OwningComponent;
	BurstContext.bForceAttachToOwningComponent = true;
	BurstContext.Location = ParticleLocation;
	BurstContext.bOverrideLocation = false;

	// --- Write to the Niagara Data Channel ---
	// Request a writer for a single element (Count == 1), visible to Blueprint/CPU/GPU
	// to match the blueprint's "Write To Niagara Data Channel" node settings.
	UNiagaraDataChannelWriter* Writer = UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel_WithContext(
		World, DataChannel, AccessContext,
		/*Count*/ 1,
		/*bVisibleToBlueprint*/ true, /*bVisibleToNiagaraCPU*/ true, /*bVisibleToNiagaraGPU*/ true,
		TEXT("Controller_UI"));

	if (!Writer)
	{
		UE_LOG(LogTemp, Warning, TEXT("Controller_UI::WriteAndReadDataChannel - failed to obtain data channel writer."));
		return;
	}

	// Overwrite DataChannelVal at index 0.
	Writer->WriteFloat(TEXT("DataChannelVal"), 0, DataChannelVal);
	UE_LOG(LogTemp, Warning, TEXT("Controller_UI::WriteAndReadDataChannel - wrote DataChannelVal = %f"), DataChannelVal);

	// --- Read the value back from the updated data channel and print it ---
	UNiagaraDataChannelReader* Reader = UNiagaraDataChannelLibrary::ReadFromNiagaraDataChannel_WithContext(
		World, DataChannel, AccessContext, /*bReadPreviousFrame*/ false);
	if (Reader)
	{
		const int32 Count = Reader->Num();
		for (int32 Index = 0; Index < Count; ++Index)
		{
			bool bSuccess = false;
			const float ReadValue = Reader->ReadFloat(TEXT("DataChannelVal"), Index, bSuccess);
			if (bSuccess)
			{
				UE_LOG(LogTemp, Warning, TEXT("Controller_UI::WriteAndReadDataChannel - read DataChannelVal = %f"), ReadValue);
			}
		}
	}
}
