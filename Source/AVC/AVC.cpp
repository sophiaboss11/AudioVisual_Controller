// Fill out your copyright notice in the Description page of Project Settings.

#include "AVC.h"
#include "Misc/AssertionMacros.h"
#include "Modules/ModuleManager.h"
#include "Controller_UI.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "GenericPlatform/GenericWindow.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"
#include "UObject/UObjectIterator.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Brushes/SlateDynamicImageBrush.h"
#include "Widgets/SOverlay.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SScaleBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SCompoundWidget.h"

// Wraps content and reports a fixed window zone so Slate can drag the window by its
// custom title bar while still letting the caption buttons receive clicks.
class SAVCWindowZone : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAVCWindowZone) : _Zone(EWindowZone::TitleBar) {}
		SLATE_ARGUMENT(EWindowZone::Type, Zone)
		SLATE_DEFAULT_SLOT(FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		Zone = InArgs._Zone;
		ChildSlot[InArgs._Content.Widget];
	}

	virtual EWindowZone::Type GetWindowZoneOverride() const override
	{
		return Zone;
	}

	// Title bar zones drag the owning window directly.
	virtual FReply OnMouseButtonDown(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (Zone == EWindowZone::TitleBar && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
			if (Window.IsValid())
			{
				bDragging = true;
				DragOffset = MouseEvent.GetScreenSpacePosition() - Window->GetPositionInScreen();
				return FReply::Handled().CaptureMouse(AsShared());
			}
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseMove(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bDragging && HasMouseCapture())
		{
			TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(AsShared());
			if (Window.IsValid())
			{
				if (Window->IsWindowMaximized())
				{
					Window->Restore();
				}
				Window->MoveWindowTo(MouseEvent.GetScreenSpacePosition() - DragOffset);
			}
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual FReply OnMouseButtonUp(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent) override
	{
		if (bDragging && MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
		{
			bDragging = false;
			return FReply::Handled().ReleaseMouseCapture();
		}
		return FReply::Unhandled();
	}

	virtual void OnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override
	{
		bDragging = false;
	}

private:
	EWindowZone::Type Zone = EWindowZone::TitleBar;
	bool bDragging = false;
	FVector2D DragOffset = FVector2D::ZeroVector;
};

// Creates a brush from Source/AVC/avc_logo.png sized to the image's native dimensions.
static TSharedPtr<FSlateDynamicImageBrush> CreateAVCLogoBrush()
{
	const FString LogoPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("AVC/avc_logo.png")));
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *LogoPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AVC - failed to load logo: %s"), *LogoPath);
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	if (!ImageWrapper.IsValid() || !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num()))
	{
		return nullptr;
	}

	return MakeShared<FSlateDynamicImageBrush>(FName(*LogoPath),
		FVector2D(ImageWrapper->GetWidth(), ImageWrapper->GetHeight()));
}

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <dwmapi.h>
#include "Windows/HideWindowsPlatformTypes.h"

static void ApplyAVCLogo(HWND Hwnd)
{
	// No title text
	::SetWindowTextW(Hwnd, L"");

	// Title bar color = FAVCColors::Background (DWMWA_CAPTION_COLOR, Windows 11+).
	const FColor& Bg = FAVCColors::Background;
	const DWORD CaptionColor = static_cast<DWORD>(Bg.R) | (static_cast<DWORD>(Bg.G) << 8) | (static_cast<DWORD>(Bg.B) << 16);
	constexpr DWORD DwmCaptionColorAttribute = 35;
	::DwmSetWindowAttribute(Hwnd, DwmCaptionColorAttribute, &CaptionColor, sizeof(CaptionColor));
}
#endif

class FAVCModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		//UE_LOG(LogTemp, Warning, TEXT("=== AVC game started"));
		PrintScriptCallstack();

		// The game window is created after this module starts up. Tell the engine its
		// final size/position now so it doesn't first appear at the default resolution
		// and then get resized.
		{
			FDisplayMetrics DisplayMetrics;
			FDisplayMetrics::RebuildDisplayMetrics(DisplayMetrics);
			const int32 ScreenWidth = DisplayMetrics.PrimaryDisplayWidth;
			const int32 ScreenHeight = DisplayMetrics.PrimaryDisplayHeight;
			if (ScreenWidth > 0 && ScreenHeight > 0)
			{
				const int32 GameWidth = ScreenWidth / 3;
				const int32 GameHeight = ScreenHeight / 3;
				const int32 GameY = (ScreenHeight - GameHeight) / 2;
				FCommandLine::Append(*FString::Printf(TEXT(" -windowed -ResX=%d -ResY=%d -WinX=0 -WinY=%d"),
					GameWidth, GameHeight, GameY));
			}
		}

		// Defer building the Controller_UI widget and its window until a world's
		// actors have been initialized.
		WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddRaw(this, &FAVCModule::OnWorldInitialized);
	}

	virtual void ShutdownModule() override
	{
		if (WorldInitHandle.IsValid())
		{
			FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
			WorldInitHandle.Reset();
		}

		if (EngineInitHandle.IsValid())
		{
			FCoreDelegates::OnFEngineLoopInitComplete.Remove(EngineInitHandle);
			EngineInitHandle.Reset();
		}

		FDefaultGameModuleImpl::ShutdownModule();
	}

private:
	void OnWorldInitialized(const FActorsInitializedParams& Params)
	{
		// Only build the widget once, for a valid game world.
		if (ControllerWidget.IsValid() || !Params.World || !Params.World->IsGameWorld())
		{
			return;
		}
		TSharedRef<Controller_UI> Core = SNew(Controller_UI)
			.World(Params.World);
		ControllerWidget = Core;
		TWeakPtr<Controller_UI> WeakCore = Core;
		TWeakObjectPtr<UWorld> WeakWorld = Params.World;

		// Write to (and read back from) the Niagara data channel once BeginPlay has run
		// (matching the blueprint's Event BeginPlay). Deferring to the next tick ensures
		// actors have begun play and the data channel handlers are ready.

		// Data channel disabled.
		//Params.World->GetTimerManager().SetTimerForNextTick([WeakCore, WeakWorld]()
		//{
		//	TSharedPtr<Controller_UI> Pinned = WeakCore.Pin();
		//	if (Pinned.IsValid() && WeakWorld.IsValid())
		//	{
		//		Pinned->WriteAndReadDataChannel(WeakWorld.Get());
		//	}
		//});

		if (FSlateApplication::IsInitialized())
		{
			FDisplayMetrics DisplayMetrics;
			FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);
			const int32 ScreenWidth = DisplayMetrics.PrimaryDisplayWidth;
			const int32 ScreenHeight = DisplayMetrics.PrimaryDisplayHeight;
			const FVector2D ControllerSize(ScreenWidth * 2 / 3, ScreenHeight * 2 / 3);
			const FVector2D ControllerPos(ScreenWidth / 3, FMath::Max(0, (ScreenHeight - ScreenHeight * 2 / 3) / 2));

			if (!LogoBrush.IsValid())
			{
				LogoBrush = CreateAVCLogoBrush();
			}

			const float CaptionButtonHeight = 28.0f;
			const float LogoHeight = 29.0f;
			const float LogoInset = 8.0f;
			float LogoWidth = LogoHeight;
			if (LogoBrush.IsValid() && LogoBrush->ImageSize.Y > 0.0)
			{
				LogoWidth = LogoHeight * static_cast<float>(LogoBrush->ImageSize.X / LogoBrush->ImageSize.Y);
			}

			TWeakPtr<SWindow>* WindowRef = &AVCWindow;
			auto MakeCaptionButton = [CaptionButtonHeight](const FString& Label, TFunction<void()> Action) -> TSharedRef<SWidget>
			{
				return SNew(SAVCWindowZone)
					.Zone(EWindowZone::ClientArea)
					[
						SNew(SBox)
						.WidthOverride(48.0f)
						.HeightOverride(CaptionButtonHeight)
						[
						SNew(SButton)
						.ButtonColorAndOpacity(FLinearColor(FAVCColors::Background))
						.ContentPadding(FMargin(0.0f))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.OnClicked_Lambda([Action]() { Action(); return FReply::Handled(); })
						[
							SNew(STextBlock)
							.Font(FCoreStyle::GetDefaultFontStyle("Regular", 14))
							.Text(FText::FromString(Label))
							.ColorAndOpacity(FLinearColor(FAVCColors::Dim))
						]
						]
					];
			};

			// Create the window at its final geometry,
			// game window has been laid out so there is no initial popup/resize.
			// The OS title bar is replaced by a custom one that contains the logo.
			TSharedRef<SWindow> Window = SNew(SWindow)
				.Title(FText::GetEmpty())
				.ClientSize(ControllerSize)
				.ScreenPosition(ControllerPos)
				.AutoCenter(EAutoCenter::None)
				.SizingRule(ESizingRule::UserSized)
				.MinWidth(320.0f)
				.MinHeight(240.0f)
				.UseOSWindowBorder(false)
				.CreateTitleBar(false)
				.HasCloseButton(true)
				.SupportsMaximize(true)
				.SupportsMinimize(true)
				.IsTopmostWindow(false)
				[
					// Custom title bar with the logo in the top-left (one tenth of the window height),
					// with the controller UI below it.
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						SNew(SAVCWindowZone)
						.Zone(EWindowZone::TitleBar)
						[
							SNew(SBox)
							.HeightOverride(LogoHeight + LogoInset * 2.0f)
							[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.BorderBackgroundColor(FLinearColor(FAVCColors::Background))
							.Padding(FMargin(LogoInset, 0.0f, LogoInset, 0.0f))
							[
								SNew(SHorizontalBox)
								+ SHorizontalBox::Slot()
								.FillWidth(1.0f)
								.HAlign(HAlign_Left)
								.VAlign(VAlign_Center)
								[
									// Size the logo explicitly from the image's aspect ratio so it is never stretched.
									SNew(SBox)
									.HeightOverride(LogoHeight)
									.WidthOverride(LogoWidth)
									.Visibility(EVisibility::HitTestInvisible)
									[
										SNew(SImage)
										.Image(LogoBrush.Get())
									]
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 0.0f)
								[
									MakeCaptionButton(TEXT("-"), [WindowRef]() { if (TSharedPtr<SWindow> W = WindowRef->Pin()) { W->Minimize(); } })
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 0.0f)
								[
									MakeCaptionButton(TEXT("\u25A1"), [WindowRef]()
									{
										if (TSharedPtr<SWindow> W = WindowRef->Pin())
										{
											if (W->IsWindowMaximized()) { W->Restore(); } else { W->Maximize(); }
										}
									})
								]
								+ SHorizontalBox::Slot()
								.AutoWidth()
								.VAlign(VAlign_Center)
								.Padding(4.0f, 0.0f)
								[
									MakeCaptionButton(TEXT("\u2715"), []() { FPlatformMisc::RequestExit(false); })
								]
							]
							]
						]
					]
					+ SVerticalBox::Slot()
					.AutoHeight()
					[
						// Thin line below the top bar.
						SNew(SBox)
						.HeightOverride(1.0f)
						[
							SNew(SBorder)
							.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
							.BorderBackgroundColor(FLinearColor(FAVCColors::Border))
							.Padding(0.0f)
						]
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						Core
					]
				];

			FSlateApplication::Get().AddWindow(Window, /*bShowImmediately*/ false);
			AVCWindow = Window;

			// The main game viewport window is created after this module starts up,
			// so re-assert topmost once the engine has finished initializing, otherwise
			// the game window ends up drawn on top of this one.
			EngineInitHandle = FCoreDelegates::OnFEngineLoopInitComplete.AddRaw(this, &FAVCModule::ForceWindowTopmost);
		}
	}

	void ForceWindowTopmost()
	{
		TSharedPtr<SWindow> Window = AVCWindow.Pin();
		if (!Window.IsValid())
		{
			return;
		}

		Window->BringToFront(true);

#if PLATFORM_WINDOWS
		TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
		if (NativeWindow.IsValid())
		{
			HWND Hwnd = static_cast<HWND>(NativeWindow->GetOSWindowHandle());
			if (Hwnd)
			{
				::SetWindowPos(Hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				ApplyAVCLogo(Hwnd);
			}
		}
#endif

		// Reshape the main game viewport window (the one showing the 3D game),
		// which is a separate SWindow owned by the game viewport client.

		if (GEngine && GEngine->GameViewport)
		{
			TSharedPtr<SWindow> GameWindow = GEngine->GameViewport->GetWindow();
			if (GameWindow.IsValid())
			{
				TSharedPtr<FGenericWindow> GameNativeWindow = GameWindow->GetNativeWindow();
				if (GameNativeWindow.IsValid())
				{
					// Game window is 1/3 of the screen; controller window is 2/3 of the screen.
					FDisplayMetrics DisplayMetrics;
					FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);

					const int32 ScreenWidth = DisplayMetrics.PrimaryDisplayWidth;
					const int32 ScreenHeight = DisplayMetrics.PrimaryDisplayHeight;

					const int32 WindowWidth = ScreenWidth / 3;
					const int32 WindowHeight = ScreenHeight / 3;
					const int32 ControllerWidth = ScreenWidth * 2 / 3;
					const int32 ControllerHeight = ScreenHeight * 2 / 3;

					// Place the game and controller windows side by side, centered as a group,
					// with the controller on the right.
					const int32 WindowX = FMath::Max(0, (ScreenWidth - (WindowWidth + ControllerWidth)) / 2);
					const int32 WindowY = (ScreenHeight - WindowHeight) / 2;
					const int32 ControllerY = FMath::Max(0, (ScreenHeight - ControllerHeight) / 2);

					GameNativeWindow->ReshapeWindow(WindowX, WindowY, WindowWidth, WindowHeight);
					Window->ReshapeWindow(FVector2D(WindowX + WindowWidth, ControllerY), FVector2D(ControllerWidth, ControllerHeight));

#if PLATFORM_WINDOWS
					// Make the game window movable and give it an exit button by
					// adding a caption bar and system menu to the native window style.
					HWND GameHwnd = static_cast<HWND>(GameNativeWindow->GetOSWindowHandle());
					if (GameHwnd)
					{
						LONG_PTR Style = ::GetWindowLongPtr(GameHwnd, GWL_STYLE);
						Style |= (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX);
						::SetWindowLongPtr(GameHwnd, GWL_STYLE, Style);
						ApplyAVCLogo(GameHwnd);

						// Force the frame changes to take effect.
						::SetWindowPos(GameHwnd, nullptr, 0, 0, 0, 0,
							SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

						// Keep the controller window directly to the right of the game window's
						// final outer bounds, preserving its own size and vertical position.
						TSharedPtr<FGenericWindow> ControllerNativeWindow = Window->GetNativeWindow();
						HWND ControllerHwnd = ControllerNativeWindow.IsValid()
							? static_cast<HWND>(ControllerNativeWindow->GetOSWindowHandle())
							: nullptr;
						RECT GameRect;
						RECT ControllerRect;
						if (ControllerHwnd && ::GetWindowRect(GameHwnd, &GameRect) && ::GetWindowRect(ControllerHwnd, &ControllerRect))
						{
							// GetWindowRect includes invisible resize borders; use the visible
							// frame bounds so the windows touch with no gap.
							RECT GameVisible = GameRect;
							RECT ControllerVisible = ControllerRect;
							::DwmGetWindowAttribute(GameHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &GameVisible, sizeof(RECT));
							::DwmGetWindowAttribute(ControllerHwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &ControllerVisible, sizeof(RECT));
							const int32 ControllerLeftInset = ControllerVisible.left - ControllerRect.left;

							::SetWindowPos(ControllerHwnd, HWND_TOP,
								GameVisible.right - ControllerLeftInset, ControllerRect.top,
								ControllerRect.right - ControllerRect.left, ControllerRect.bottom - ControllerRect.top,
								SWP_NOACTIVATE);
						}
					}
#endif
				}
			}

			// Don't let the game viewport capture or lock the mouse, so the cursor
			// stays free and defaults to the controller window.
			GEngine->GameViewport->SetMouseCaptureMode(EMouseCaptureMode::NoCapture);
			GEngine->GameViewport->SetMouseLockMode(EMouseLockMode::DoNotLock);
			GEngine->GameViewport->SetHideCursorDuringCapture(false);

			if (UWorld* World = GEngine->GameViewport->GetWorld())
			{
				if (APlayerController* PC = World->GetFirstPlayerController())
				{
					PC->SetShowMouseCursor(true);
				}
			}
		}

		// Reveal the controller now that both windows are in their final positions.
		if (!Window->IsVisible())
		{
			Window->ShowWindow();
		}

		FocusControllerWindow();
	}

	void FocusControllerWindow()
	{
		TSharedPtr<SWindow> Window = AVCWindow.Pin();
		if (!Window.IsValid())
		{
			return;
		}

		FSlateApplication::Get().ReleaseAllPointerCapture();
		Window->BringToFront(true);
		FSlateApplication::Get().SetAllUserFocus(ControllerWidget.Pin(), EFocusCause::SetDirectly);

#if PLATFORM_WINDOWS
		TSharedPtr<FGenericWindow> NativeWindow = Window->GetNativeWindow();
		if (NativeWindow.IsValid())
		{
			if (HWND Hwnd = static_cast<HWND>(NativeWindow->GetOSWindowHandle()))
			{
				::ClipCursor(nullptr);
				::SetForegroundWindow(Hwnd);
				::SetFocus(Hwnd);

				// Start the cursor over the controller window.
				RECT Rect;
				if (::GetWindowRect(Hwnd, &Rect))
				{
					::SetCursorPos((Rect.left + Rect.right) / 2, (Rect.top + Rect.bottom) / 2);
				}
			}
		}
#endif
	}

	TWeakPtr<SWindow> AVCWindow;
	TSharedPtr<FSlateDynamicImageBrush> LogoBrush;
	TWeakPtr<Controller_UI> ControllerWidget;
	TWeakObjectPtr<UNiagaraComponent> BlowingParticlesComponent;
	FDelegateHandle EngineInitHandle;
	FDelegateHandle WorldInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE( FAVCModule, AVC, "AVC" );
