#include "IO_Widget.h"
#include "AVC_Audio.h"
#include "Controller_UI.h"
#include "SlateOptMacros.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SLeafWidget.h"
#include "Rendering/DrawElements.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"


#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
    FSlateFontInfo IOLabelFont(int32 Size = 8)
    {
        FSlateFontInfo Font = FAVCFonts::Get(Size);
        Font.LetterSpacing = 200;
        return Font;
    }

    /** Thin horizontal level meter with a rounded track; a negative level draws only the track. */
    class SAVCLevelBar : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SAVCLevelBar)
            : _FillColor(FLinearColor::White)
            , _Level(0.0f)
        {}
            SLATE_ARGUMENT(FLinearColor, FillColor)
            SLATE_ATTRIBUTE(float, Level)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            FillColor = InArgs._FillColor;
            Level = InArgs._Level;
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            return FVector2D(60.0f, 4.0f);
        }

        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
        {
            static const FSlateRoundedBoxBrush TrackBrush(FLinearColor(FAVCColors::Card), 2.0f);
            static const FSlateRoundedBoxBrush FillBrush(FLinearColor::White, 2.0f);

            const FVector2D Size = AllottedGeometry.GetLocalSize();
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
                &TrackBrush, ESlateDrawEffect::None, FLinearColor(FAVCColors::Card));

            const float Value = Level.Get();
            if (Value > 0.0f)
            {
                const FVector2D FillSize(Size.X * FMath::Clamp(Value, 0.0f, 1.0f), Size.Y);
                FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
                    AllottedGeometry.ToPaintGeometry(FillSize, FSlateLayoutTransform()),
                    &FillBrush, ESlateDrawEffect::None, FillColor);
            }
            return LayerId + 1;
        }

    private:
        FLinearColor FillColor;
        TAttribute<float> Level;
    };

    /** Scrolling oscilloscope of the system audio captured by AVC_Audio. */
    class SAVCWaveformGraph : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SAVCWaveformGraph)
            : _LineColor(FLinearColor::White)
            , _NumSamples(1024)
            , _Gain(1.0f)
        {}
            SLATE_ARGUMENT(FLinearColor, LineColor)
            SLATE_ARGUMENT(int32, NumSamples)
            SLATE_ARGUMENT(float, Gain)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            LineColor = InArgs._LineColor;
            NumSamples = InArgs._NumSamples;
            Gain = InArgs._Gain;
            RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SAVCWaveformGraph::Animate));
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            return FVector2D(120.0f, 60.0f);
        }

        virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override
        {
            static const FSlateRoundedBoxBrush BackgroundBrush(FLinearColor(FAVCColors::Card), 3.0f);

            const FVector2D Size = AllottedGeometry.GetLocalSize();
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId, AllottedGeometry.ToPaintGeometry(),
                &BackgroundBrush, ESlateDrawEffect::None, FLinearColor(FAVCColors::Card));

            const float MidY = Size.Y * 0.5f;

            // Center line.
            TArray<FVector2D> AxisPoints;
            AxisPoints.Add(FVector2D(0.0f, MidY));
            AxisPoints.Add(FVector2D(Size.X, MidY));
            FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 1, AllottedGeometry.ToPaintGeometry(),
                AxisPoints, ESlateDrawEffect::None, FLinearColor(FAVCColors::Border), true, 1.0f);

            AVC_Audio::GetWaveform(Samples, NumSamples);
            const int32 Count = Samples.Num();
            if (Count < 2 || Size.X < 2.0f)
            {
                return LayerId + 1;
            }

            // One point per pixel column, using the peak of the samples that fall in that column.
            const int32 NumPoints = FMath::Clamp(FMath::FloorToInt(Size.X), 2, Count);
            const float SamplesPerPoint = (float)Count / NumPoints;
            const float HalfHeight = MidY - 1.0f;

            Points.Reset(NumPoints);
            for (int32 p = 0; p < NumPoints; ++p)
            {
                const int32 Start = FMath::FloorToInt(p * SamplesPerPoint);
                const int32 End = FMath::Min(Count, FMath::Max(Start + 1, FMath::FloorToInt((p + 1) * SamplesPerPoint)));
                float Value = 0.0f;
                for (int32 s = Start; s < End; ++s)
                {
                    if (FMath::Abs(Samples[s]) > FMath::Abs(Value))
                    {
                        Value = Samples[s];
                    }
                }
                const float X = Size.X * p / (NumPoints - 1);
                const float Y = MidY - FMath::Clamp(Value * Gain, -1.0f, 1.0f) * HalfHeight;
                Points.Add(FVector2D(X, Y));
            }

            FSlateDrawElement::MakeLines(OutDrawElements, LayerId + 2, AllottedGeometry.ToPaintGeometry(),
                Points, ESlateDrawEffect::None, LineColor, true, 1.5f);

            return LayerId + 2;
        }

    private:
        EActiveTimerReturnType Animate(double InCurrentTime, float InDeltaTime)
        {
            Invalidate(EInvalidateWidgetReason::Paint);
            return EActiveTimerReturnType::Continue;
        }

        FLinearColor LineColor;
        int32 NumSamples = 1024;
        float Gain = 1.0f;
        mutable TArray<float> Samples;
        mutable TArray<FVector2D> Points;
    };
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void IO_Widget::Construct(const FArguments& InArgs)
{
    AVC_Audio::StartListening();

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SNew(STextBlock)
            .Font(IOLabelFont())
            .TransformPolicy(ETextTransformPolicy::ToUpper)
            .Text(FText::FromString(TEXT("Input Output")))
            .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
        ]
        // Video output dock zone at the top of the panel.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 16.0f, 0.0f, 0.0f)
        [
            BuildVideoOutputSection()
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSpacer)
        ]
        // Audio input section at the bottom of the panel.
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            BuildAudioInputSection()
        ]
    ];

    RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &IO_Widget::TickVideoDock));
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

TSharedRef<SWidget> IO_Widget::BuildVideoOutputSection()
{
    static const FSlateRoundedBoxBrush ZoneBrush(FLinearColor::Transparent, 3.0f, FLinearColor::White, 1.0f);

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 10.0f)
        [
            SNew(SHorizontalBox)
            + SHorizontalBox::Slot()
            .FillWidth(1.0f)
            .VAlign(VAlign_Center)
            [
                SNew(STextBlock)
                .Font(IOLabelFont())
                .TransformPolicy(ETextTransformPolicy::ToUpper)
                .Text(FText::FromString(TEXT("Video Output")))
                .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
            ]
            + SHorizontalBox::Slot()
            .AutoWidth()
            .VAlign(VAlign_Center)
            [
                SNew(SButton)
                .ButtonColorAndOpacity(FLinearColor(FAVCColors::Card))
                .Visibility_Lambda([this]() { return bGameDocked ? EVisibility::Visible : EVisibility::Collapsed; })
                .OnClicked_Lambda([this]() { UndockGameWindow(); return FReply::Handled(); })
                [
                    SNew(STextBlock)
                    .Font(IOLabelFont(7))
                    .TransformPolicy(ETextTransformPolicy::ToUpper)
                    .Text(FText::FromString(TEXT("Undock")))
                    .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
                ]
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            // 16:9 outlined zone spanning the widget width.
            SAssignNew(VideoZone, SBox)
            .MinAspectRatio(16.0f / 9.0f)
            .MaxAspectRatio(16.0f / 9.0f)
            [
                SNew(SBorder)
                .BorderImage(&ZoneBrush)
                .BorderBackgroundColor(FLinearColor(FAVCColors::Border))
                .HAlign(HAlign_Center)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Font(IOLabelFont(7))
                    .TransformPolicy(ETextTransformPolicy::ToUpper)
                    .Text(FText::FromString(TEXT("Drag game window here")))
                    .ColorAndOpacity(FLinearColor(FAVCColors::Dimmer))
                    .Visibility_Lambda([this]() { return bGameDocked ? EVisibility::Hidden : EVisibility::HitTestInvisible; })
                ]
            ]
        ];
}

void* IO_Widget::GetGameWindowHandle() const
{
    if (GEngine && GEngine->GameViewport)
    {
        TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
        if (GameWindow.IsValid() && GameWindow->GetNativeWindow().IsValid())
        {
            return GameWindow->GetNativeWindow()->GetOSWindowHandle();
        }
    }
    return nullptr;
}

EActiveTimerReturnType IO_Widget::TickVideoDock(double InCurrentTime, float InDeltaTime)
{
#if PLATFORM_WINDOWS
    HWND GameHwnd = static_cast<HWND>(GetGameWindowHandle());
    if (!GameHwnd || !VideoZone.IsValid())
    {
        return EActiveTimerReturnType::Continue;
    }

    const FGeometry& ZoneGeometry = VideoZone->GetTickSpaceGeometry();
    const FVector2D ZonePos = ZoneGeometry.GetAbsolutePosition();
    const FVector2D ZoneSize = ZoneGeometry.GetAbsoluteSize();
    if (ZoneSize.X < 1.0 || ZoneSize.Y < 1.0)
    {
        return EActiveTimerReturnType::Continue;
    }

    if (bGameDocked)
    {
        // Keep the docked game window aligned inside the zone's outline.
        const int32 Inset = 1;
        ::SetWindowPos(GameHwnd, nullptr,
            FMath::RoundToInt(ZonePos.X) + Inset, FMath::RoundToInt(ZonePos.Y) + Inset,
            FMath::RoundToInt(ZoneSize.X) - Inset * 2, FMath::RoundToInt(ZoneSize.Y) - Inset * 2,
            SWP_NOZORDER | SWP_NOACTIVATE);
        return EActiveTimerReturnType::Continue;
    }

    RECT Rect;
    if (!::GetWindowRect(GameHwnd, &Rect))
    {
        return EActiveTimerReturnType::Continue;
    }

    // Dock when the game window was just moved (dragged) and dropped with the cursor over the zone.
    const bool bMoved = bHasLastGameRect && (Rect.left != LastGameRect[0] || Rect.top != LastGameRect[1]);
    LastGameRect[0] = Rect.left; LastGameRect[1] = Rect.top; LastGameRect[2] = Rect.right; LastGameRect[3] = Rect.bottom;
    bHasLastGameRect = true;

    const bool bMouseDown = (::GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    if (bMoved && !bMouseDown)
    {
        POINT Cursor;
        ::GetCursorPos(&Cursor);
        const FSlateRect ZoneRect(ZonePos, ZonePos + ZoneSize);
        if (ZoneRect.ContainsPoint(FVector2D(Cursor.x, Cursor.y)))
        {
            DockGameWindow();
        }
    }
#endif
    return EActiveTimerReturnType::Continue;
}

void IO_Widget::DockGameWindow()
{
#if PLATFORM_WINDOWS
    HWND GameHwnd = static_cast<HWND>(GetGameWindowHandle());
    TSharedPtr<SWindow> MyWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
    if (!GameHwnd || !MyWindow.IsValid() || !MyWindow->GetNativeWindow().IsValid())
    {
        return;
    }
    HWND ControllerHwnd = static_cast<HWND>(MyWindow->GetNativeWindow()->GetOSWindowHandle());

    RECT Rect;
    ::GetWindowRect(GameHwnd, &Rect);
    SavedGameRect[0] = Rect.left; SavedGameRect[1] = Rect.top; SavedGameRect[2] = Rect.right; SavedGameRect[3] = Rect.bottom;
    SavedGameStyle = ::GetWindowLongPtr(GameHwnd, GWL_STYLE);
    SavedGameOwner = ::GetWindowLongPtr(GameHwnd, GWLP_HWNDPARENT);

    // Frameless, and owned by the controller so it stays above it and moves/minimizes with it.
    ::SetWindowLongPtr(GameHwnd, GWL_STYLE, SavedGameStyle & ~(WS_CAPTION | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
    ::SetWindowLongPtr(GameHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(ControllerHwnd));
    ::SetWindowPos(GameHwnd, nullptr, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);

    bGameDocked = true;
#endif
}

void IO_Widget::UndockGameWindow()
{
#if PLATFORM_WINDOWS
    HWND GameHwnd = static_cast<HWND>(GetGameWindowHandle());
    bGameDocked = false;
    bHasLastGameRect = false;
    if (!GameHwnd)
    {
        return;
    }

    ::SetWindowLongPtr(GameHwnd, GWL_STYLE, SavedGameStyle);
    ::SetWindowLongPtr(GameHwnd, GWLP_HWNDPARENT, SavedGameOwner);
    ::SetWindowPos(GameHwnd, HWND_TOP, SavedGameRect[0], SavedGameRect[1],
        SavedGameRect[2] - SavedGameRect[0], SavedGameRect[3] - SavedGameRect[1],
        SWP_FRAMECHANGED | SWP_SHOWWINDOW);
#endif
}

TSharedRef<SWidget> IO_Widget::BuildAudioInputSection()
{
    return SNew(SVerticalBox)
        // Live waveform of the system audio output.
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 12.0f)
        [
            SNew(SBox)
            .HeightOverride(60.0f)
            [
                SNew(SAVCWaveformGraph)
                .LineColor(FLinearColor(FAVCColors::Cyan))
                .NumSamples(256)
                .Gain(4.0f)
            ]
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 16.0f)
        [
            SNew(STextBlock)
            .Font(IOLabelFont())
            .TransformPolicy(ETextTransformPolicy::ToUpper)
            .Text(FText::FromString(TEXT("Audio Input")))
            .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(0.0f, 0.0f, 0.0f, 14.0f)
        [
            BuildInputRow(TEXT("Audio In"), FAVCColors::Cyan,
                TAttribute<float>::CreateLambda([]() { return AVC_Audio::GetLevel(); }))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            BuildInputRow(TEXT("Game\nController In"), FAVCColors::Cyan,
                TAttribute<float>::CreateLambda([this]() { return ControllerLevel; }))
        ];
}

TSharedRef<SWidget> IO_Widget::BuildInputRow(const FString& Label, const FColor& Color, TAttribute<float> Level)
{
    static const FSlateRoundedBoxBrush DotBrush(FLinearColor::White, 4.0f);

    // Active inputs (level >= 0) use the accent color; inactive ones are dimmed.
    auto IsActive = [Level]() { return Level.Get() >= 0.0f; };
    const FLinearColor Accent(Color);

    return SNew(SHorizontalBox)
        // Status dot
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        .Padding(0.0f, 0.0f, 14.0f, 0.0f)
        [
            SNew(SBox)
            .WidthOverride(8.0f)
            .HeightOverride(8.0f)
            [
                SNew(SBorder)
                .BorderImage(&DotBrush)
                .BorderBackgroundColor_Lambda([IsActive, Accent]()
                {
                    return IsActive() ? Accent : FLinearColor(FAVCColors::Border);
                })
            ]
        ]
        // Label
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(STextBlock)
            .Font(IOLabelFont(7))
            .TransformPolicy(ETextTransformPolicy::ToUpper)
            .Text(FText::FromString(Label))
            .ColorAndOpacity(FLinearColor(FAVCColors::Dimmer))
        ]
        // Level bar
        + SHorizontalBox::Slot()
        .FillWidth(1.0f)
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .HeightOverride(4.0f)
            [
                SNew(SAVCLevelBar)
                .FillColor(Accent)
                .Level(Level)
            ]
        ]
        // Value
        + SHorizontalBox::Slot()
        .AutoWidth()
        .VAlign(VAlign_Center)
        [
            SNew(SBox)
            .WidthOverride(40.0f)
            .HAlign(HAlign_Right)
            [
                SNew(STextBlock)
                .Font(FAVCFonts::Get(7))
                .Text_Lambda([Level]()
                {
                    const float Value = Level.Get();
                    return Value >= 0.0f
                        ? FText::AsNumber(FMath::RoundToInt(Value * 100.0f))
                        : FText::FromString(TEXT("-"));
                })
                .ColorAndOpacity_Lambda([IsActive, Accent]()
                {
                    return FSlateColor(IsActive() ? Accent : FLinearColor(FAVCColors::Dimmer));
                })
            ]
        ];
}
