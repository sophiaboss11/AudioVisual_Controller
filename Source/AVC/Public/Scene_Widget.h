#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

/**
 * Scene list shown in the left section of the controller UI.
 * Clicking a scene selects it (Card background); only one scene is selected at a time.
 */
class AVC_API Scene_Widget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(Scene_Widget) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

    int32 GetSelectedScene() const { return SelectedScene; }

private:
    /** Index of the selected scene; INDEX_NONE when nothing is selected. */
    int32 SelectedScene = INDEX_NONE;

    /** Builds the Switch Scene / T1-T3 section. */
    TSharedRef<SWidget> BuildSwitchSection();

    /** Builds a labeled section with a row of outlined buttons; only the button at *SelectedIndex is selected. */
    TSharedRef<SWidget> BuildButtonSection(const TCHAR* Title, const TArray<FString>& Labels, int32* SelectedIndex, float ButtonHeight, TSharedPtr<SWidget> TrailingWidget = nullptr);

    /** Index of the selected T1-T3 button; INDEX_NONE when nothing is selected. */
    int32 SelectedTransition = INDEX_NONE;

    /** Index of the selected camera button; INDEX_NONE when nothing is selected. */
    int32 SelectedCamera = INDEX_NONE;

    /** Index of the selected light button; INDEX_NONE when nothing is selected. */
    int32 SelectedLight = INDEX_NONE;

    /** Normalized (0..1) value of the light vertical slider. */
    float LightSliderValue = 0.75f;
};
