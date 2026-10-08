#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Input/output panel shown in the right section of the controller UI.
 */
class AVC_API IO_Widget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(IO_Widget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    /** Sets the audio input level (0..1). */
    void SetAudioLevel(float InLevel) { AudioLevel = FMath::Clamp(InLevel, 0.0f, 1.0f); }

    /** Sets the game controller input level (0..1), or a negative value when no controller is connected. */
    void SetControllerLevel(float InLevel) { ControllerLevel = InLevel; }

private:
    /** Builds the Video Output section: an outlined drop zone the game window can be docked into. */
    TSharedRef<SWidget> BuildVideoOutputSection();

    /** Polls the game window each frame to dock it when dropped on the zone and keep it aligned while docked. */
    EActiveTimerReturnType TickVideoDock(double InCurrentTime, float InDeltaTime);

    /** Docks the game window into the zone. */
    void DockGameWindow();

    /** Restores the game window to a normal floating window. */
    void UndockGameWindow();

    /** Native handle of the game viewport window, or null. */
    void* GetGameWindowHandle() const;

    /** The outlined zone the game window docks into. */
    TSharedPtr<SWidget> VideoZone;

    bool bGameDocked = false;
    bool bHasLastGameRect = false;
    int32 LastGameRect[4] = { 0, 0, 0, 0 };

    /** Game window style/bounds/owner before docking, restored on undock. */
    int64 SavedGameStyle = 0;
    int64 SavedGameOwner = 0;
    int32 SavedGameRect[4] = { 0, 0, 0, 0 };

    /** Builds the Audio Input section (audio in and game controller in meters). */
    TSharedRef<SWidget> BuildAudioInputSection();

    /** Builds one input row: status dot, label, level bar and value. */
    TSharedRef<SWidget> BuildInputRow(const FString& Label, const FColor& Color, TAttribute<float> Level);

    float AudioLevel = 0.72f;

    /** Negative when no game controller is connected. */
    float ControllerLevel = -1.0f;
};
