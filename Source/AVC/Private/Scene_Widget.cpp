#include "Scene_Widget.h"
#include "Controller_UI.h"
#include "SlateOptMacros.h"

#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/SLeafWidget.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSpacer.h"
#include "Rendering/DrawElements.h"

namespace
{
    FSlateFontInfo SceneLabelFont(int32 Size = 8)
    {
        FSlateFontInfo Font = FAVCFonts::Get(Size);
        Font.LetterSpacing = 200;
        return Font;
    }

    TSharedRef<SWidget> SceneDivider()
    {
        return SNew(SBox)
            .HeightOverride(1.0f)
            [
                SNew(SBorder)
                .BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
                .BorderBackgroundColor(FLinearColor(FAVCColors::Border))
                .Padding(0.0f)
            ];
    }

    /** Vertical slider: filled from the bottom up to a round handle. */
    class SAVCVerticalSlider : public SLeafWidget
    {
    public:
        SLATE_BEGIN_ARGS(SAVCVerticalSlider)
            : _FillColor(FLinearColor::White)
            , _InitialValue(0.5f)
        {}
            SLATE_ARGUMENT(FLinearColor, FillColor)
            SLATE_ARGUMENT(float, InitialValue)
            SLATE_EVENT(FOnFloatValueChanged, OnValueChanged)
        SLATE_END_ARGS()

        void Construct(const FArguments& InArgs)
        {
            FillColor = InArgs._FillColor;
            Value = FMath::Clamp(InArgs._InitialValue, 0.0f, 1.0f);
            OnValueChanged = InArgs._OnValueChanged;
        }

        virtual FVector2D ComputeDesiredSize(float) const override
        {
            return FVector2D(HandleSize + GlowRadius * 2.0, 60.0f);
        }

        virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
            {
                return FReply::Unhandled();
            }
            bDragging = true;
            UpdateValue(MyGeometry, MouseEvent);
            return FReply::Handled().CaptureMouse(SharedThis(this));
        }

        virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
        {
            if (!bDragging)
            {
                return FReply::Unhandled();
            }
            UpdateValue(MyGeometry, MouseEvent);
            return FReply::Handled();
        }

        virtual FReply OnMouseButtonUp(const FGeometry&, const FPointerEvent& MouseEvent) override
        {
            if (!bDragging || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
            {
                return FReply::Unhandled();
            }
            bDragging = false;
            return FReply::Handled().ReleaseMouseCapture();
        }

        virtual void OnMouseCaptureLost(const FCaptureLostEvent&) override
        {
            bDragging = false;
        }

        virtual int32 OnPaint(const FPaintArgs&, const FGeometry& AllottedGeometry, const FSlateRect&,
            FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle&, bool) const override
        {
            const FSlateBrush* WhiteBrush = FCoreStyle::Get().GetBrush("WhiteBrush");
            const FVector2D Size = AllottedGeometry.GetLocalSize();
            const double TrackLength = FMath::Max(0.0, Size.Y - HandleSize);
            const double TrackX = (Size.X - TrackWidth) * 0.5;
            const double HandleY = HandleSize * 0.5 + TrackLength * (1.0 - Value);

            FSlateDrawElement::MakeBox(OutDrawElements, LayerId,
                AllottedGeometry.ToPaintGeometry(FVector2D(TrackWidth, TrackLength), FSlateLayoutTransform(FVector2D(TrackX, HandleSize * 0.5))),
                WhiteBrush, ESlateDrawEffect::None, FLinearColor(FAVCColors::Border));

            const double FillLength = HandleSize * 0.5 + TrackLength - HandleY;
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 1,
                AllottedGeometry.ToPaintGeometry(FVector2D(TrackWidth, FillLength), FSlateLayoutTransform(FVector2D(TrackX, HandleY))),
                WhiteBrush, ESlateDrawEffect::None, FillColor);

            const FVector2D HandleCenter(Size.X * 0.5, HandleY);
            for (int32 Ring = GlowRings; Ring >= 1; --Ring)
            {
                const double Diameter = HandleSize + Ring * (GlowRadius * 2.0 / GlowRings);
                const FSlateRoundedBoxBrush GlowBrush(FLinearColor::White, Diameter * 0.5);
                FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 2,
                    AllottedGeometry.ToPaintGeometry(FVector2D(Diameter, Diameter), FSlateLayoutTransform(HandleCenter - FVector2D(Diameter, Diameter) * 0.5)),
                    &GlowBrush, ESlateDrawEffect::None, FillColor.CopyWithNewOpacity(0.12f));
            }

            const FSlateRoundedBoxBrush HandleBrush(FLinearColor::White, HandleSize * 0.5, FLinearColor(FAVCColors::Background), 2.0f);
            FSlateDrawElement::MakeBox(OutDrawElements, LayerId + 3,
                AllottedGeometry.ToPaintGeometry(FVector2D(HandleSize, HandleSize), FSlateLayoutTransform(HandleCenter - FVector2D(HandleSize, HandleSize) * 0.5)),
                &HandleBrush, ESlateDrawEffect::None, FillColor);

            return LayerId + 3;
        }

    private:
        void UpdateValue(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
        {
            const FVector2D Local = MyGeometry.AbsoluteToLocal(MouseEvent.GetScreenSpacePosition());
            const double TrackLength = FMath::Max(1.0, MyGeometry.GetLocalSize().Y - HandleSize);
            Value = FMath::Clamp(1.0f - static_cast<float>((Local.Y - HandleSize * 0.5) / TrackLength), 0.0f, 1.0f);
            OnValueChanged.ExecuteIfBound(Value);
        }

        static constexpr double HandleSize = 14.0;
        static constexpr double TrackWidth = 3.0;
        static constexpr double GlowRadius = 4.0;
        static constexpr int32 GlowRings = 4;

        FLinearColor FillColor;
        float Value = 0.5f;
        bool bDragging = false;
        FOnFloatValueChanged OnValueChanged;
    };
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void Scene_Widget::Construct(const FArguments& InArgs)
{
    static const TCHAR* Names[] = { TEXT("Scene 1"), TEXT("Scene 2"), TEXT("Scene 3") };

    static const FSlateRoundedBoxBrush NormalBrush(FLinearColor::Transparent, 6.0f);
    static const FSlateRoundedBoxBrush SelectedBrush(FLinearColor(FAVCColors::Card), 6.0f);

    TSharedRef<SVerticalBox> List = SNew(SVerticalBox);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Names); ++Index)
    {
        List->AddSlot()
        .AutoHeight()
        .Padding(0.0f, 2.0f)
        [
            SNew(SButton)
            .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
            .ContentPadding(0.0f)
            .OnClicked_Lambda([this, Index]()
            {
                SelectedScene = Index;
                return FReply::Handled();
            })
            [
                SNew(SBorder)
                .BorderImage_Lambda([this, Index]() -> const FSlateBrush*
                {
                    return SelectedScene == Index ? &SelectedBrush : &NormalBrush;
                })
                .Padding(FMargin(8.0f, 6.0f))
                [
                    SNew(SHorizontalBox)
                    + SHorizontalBox::Slot()
                    .AutoWidth()
                    .VAlign(VAlign_Center)
                    .Padding(0.0f, 0.0f, 10.0f, 0.0f)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(FString::Printf(TEXT("%02d"), Index + 1)))
                        .ColorAndOpacity(FLinearColor(FAVCColors::Dimmer))
                    ]
                    + SHorizontalBox::Slot()
                    .FillWidth(1.0f)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Text(FText::FromString(Names[Index]))
                        .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
                    ]
                ]
            ]
        ];
    }

    ChildSlot
    [
        SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(10.0f, 8.0f, 10.0f, 0.0f)
        [
            List
        ]
        + SVerticalBox::Slot()
        .FillHeight(1.0f)
        [
            SNew(SSpacer)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SceneDivider()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            BuildSwitchSection()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SceneDivider()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            BuildButtonSection(TEXT("Camera"), { TEXT("CAM 1"), TEXT("CAM 2"), TEXT("CAM 3"), TEXT("CAM 4") }, &SelectedCamera, 44.0f)
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            SceneDivider()
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        [
            BuildButtonSection(TEXT("Light"), { TEXT("LT 1"), TEXT("LT 2"), TEXT("LT 3"), TEXT("LT 4") }, &SelectedLight, 76.0f,
                SNew(SAVCVerticalSlider)
                .FillColor(FLinearColor(FAVCColors::Fuchsia))
                .InitialValue(LightSliderValue)
                .OnValueChanged_Lambda([this](float NewValue) { LightSliderValue = NewValue; }))
        ]
    ];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

namespace
{
    const FSlateRoundedBoxBrush& OutlinedBrush(bool bSelected)
    {
        static const FSlateRoundedBoxBrush Normal(FLinearColor(FAVCColors::Background), 4.0f, FLinearColor(FAVCColors::Border), 1.0f);
        static const FSlateRoundedBoxBrush Selected(FLinearColor(FAVCColors::Card), 4.0f, FLinearColor(FAVCColors::Purple), 1.0f);
        return bSelected ? Selected : Normal;
    }

    TSharedRef<SWidget> MakeOutlinedButton(const FString& Label, int32* SelectedIndex, int32 Index, float Height)
    {
        return SNew(SBox)
            .HeightOverride(Height)
            [
                SNew(SButton)
                .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
                .ContentPadding(0.0f)
                .OnClicked_Lambda([SelectedIndex, Index]() { *SelectedIndex = Index; return FReply::Handled(); })
                [
                    SNew(SBorder)
                    .BorderImage_Lambda([SelectedIndex, Index]() -> const FSlateBrush* { return &OutlinedBrush(*SelectedIndex == Index); })
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(SceneLabelFont())
                        .Text(FText::FromString(Label))
                        .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
                    ]
                ]
            ];
    }
}

TSharedRef<SWidget> Scene_Widget::BuildSwitchSection()
{
    static const FSlateRoundedBoxBrush SwitchBrush(FLinearColor(FAVCColors::Blue), 4.0f);

    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox)
        + SHorizontalBox::Slot()
        .AutoWidth()
        .Padding(0.0f, 0.0f, 6.0f, 0.0f)
        [
            SNew(SBox)
            .HeightOverride(36.0f)
            [
                SNew(SButton)
                .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
                .ContentPadding(0.0f)
                .OnClicked_Lambda([]() { return FReply::Handled(); })
                [
                    SNew(SBorder)
                    .BorderImage(&SwitchBrush)
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    .Padding(FMargin(14.0f, 0.0f))
                    [
                        SNew(STextBlock)
                        .Font(SceneLabelFont())
                        .Text(FText::FromString(TEXT("SWITCH SCENE")))
                        .ColorAndOpacity(FLinearColor(FAVCColors::Foreground))
                    ]
                ]
            ]
        ];

    static const FSlateRoundedBoxBrush TNormalBrush(FLinearColor(FAVCColors::Background), 4.0f, FLinearColor(FAVCColors::Border), 1.0f);
    static const FSlateRoundedBoxBrush TSelectedBrush(FLinearColor(FAVCColors::Card), 4.0f, FLinearColor(FAVCColors::Purple), 1.0f);

    static const TCHAR* Labels[] = { TEXT("T1"), TEXT("T2"), TEXT("T3") };
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Labels); ++Index)
    {
        Row->AddSlot()
        .FillWidth(1.0f)
        .Padding(6.0f, 0.0f, Index == UE_ARRAY_COUNT(Labels) - 1 ? 0.0f : 6.0f, 0.0f)
        [
            SNew(SBox)
            .HeightOverride(36.0f)
            [
                SNew(SButton)
                .ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder"))
                .ContentPadding(0.0f)
                .OnClicked_Lambda([this, Index]() { SelectedTransition = Index; return FReply::Handled(); })
                [
                    SNew(SBorder)
                    .BorderImage_Lambda([this, Index]() -> const FSlateBrush* { return SelectedTransition == Index ? &TSelectedBrush : &TNormalBrush; })
                    .HAlign(HAlign_Center)
                    .VAlign(VAlign_Center)
                    [
                        SNew(STextBlock)
                        .Font(SceneLabelFont())
                        .Text(FText::FromString(Labels[Index]))
                        .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
                    ]
                ]
            ]
        ];
    }

    return SNew(SBox)
        .Padding(FMargin(14.0f, 12.0f))
        [
            Row
        ];
}
TSharedRef<SWidget> Scene_Widget::BuildButtonSection(const TCHAR* Title, const TArray<FString>& Labels, int32* SelectedIndex, float ButtonHeight, TSharedPtr<SWidget> TrailingWidget)
{
    TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
    for (int32 Index = 0; Index < Labels.Num(); ++Index)
    {
        Row->AddSlot()
        .FillWidth(1.0f)
        .Padding(Index == 0 ? 0.0f : 4.0f, 0.0f, Index == Labels.Num() - 1 ? 0.0f : 4.0f, 0.0f)
        [
            MakeOutlinedButton(Labels[Index], SelectedIndex, Index, ButtonHeight)
        ];
    }

    if (TrailingWidget.IsValid())
    {
        Row->AddSlot()
        .AutoWidth()
        .Padding(14.0f, 0.0f, 0.0f, 0.0f)
        [
            SNew(SBox)
            .HeightOverride(ButtonHeight)
            [
                TrailingWidget.ToSharedRef()
            ]
        ];
    }

    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(14.0f, 14.0f, 14.0f, 10.0f)
        [
            SNew(STextBlock)
            .Font(SceneLabelFont())
            .TransformPolicy(ETextTransformPolicy::ToUpper)
            .Text(FText::FromString(Title))
            .ColorAndOpacity(FLinearColor(FAVCColors::Dim))
        ]
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(14.0f, 0.0f, 14.0f, 14.0f)
        [
            Row
        ];
}
