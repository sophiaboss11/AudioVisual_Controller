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

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include <dwmapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// Builds a window icon from Source/AVC/avc_logo.png (cached after first load).
static HICON LoadAVCLogoIcon()
{
	static HICON CachedIcon = nullptr;
	if (CachedIcon)
	{
		return CachedIcon;
	}

	const FString LogoPath = FPaths::Combine(FPaths::GameSourceDir(), TEXT("AVC/avc_logo.png"));
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *LogoPath))
	{
		UE_LOG(LogTemp, Warning, TEXT("AVC - failed to load logo: %s"), *LogoPath);
		return nullptr;
	}

	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(TEXT("ImageWrapper"));
	TSharedPtr<IImageWrapper> ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::PNG);
	TArray<uint8> RawData;
	if (!ImageWrapper.IsValid()
		|| !ImageWrapper->SetCompressed(FileData.GetData(), FileData.Num())
		|| !ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, RawData))
	{
		return nullptr;
	}

	const int32 Width = ImageWrapper->GetWidth();
	const int32 Height = ImageWrapper->GetHeight();

	BITMAPINFO BitmapInfo = {};
	BitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	BitmapInfo.bmiHeader.biWidth = Width;
	BitmapInfo.bmiHeader.biHeight = -Height; // top-down
	BitmapInfo.bmiHeader.biPlanes = 1;
	BitmapInfo.bmiHeader.biBitCount = 32;
	BitmapInfo.bmiHeader.biCompression = BI_RGB;

	void* Bits = nullptr;
	HDC ScreenDC = ::GetDC(nullptr);
	HBITMAP ColorBitmap = ::CreateDIBSection(ScreenDC, &BitmapInfo, DIB_RGB_COLORS, &Bits, nullptr, 0);
	::ReleaseDC(nullptr, ScreenDC);
	if (!ColorBitmap || !Bits)
	{
		return nullptr;
	}
	FMemory::Memcpy(Bits, RawData.GetData(), Width * Height * 4);

	HBITMAP MaskBitmap = ::CreateBitmap(Width, Height, 1, 1, nullptr);
	ICONINFO IconInfo = {};
	IconInfo.fIcon = 1;
	IconInfo.hbmMask = MaskBitmap;
	IconInfo.hbmColor = ColorBitmap;
	CachedIcon = ::CreateIconIndirect(&IconInfo);

	::DeleteObject(ColorBitmap);
	::DeleteObject(MaskBitmap);
	return CachedIcon;
}

static void ApplyAVCLogo(HWND Hwnd)
{
	if (HICON Icon = LoadAVCLogoIcon())
	{
		::SendMessageW(Hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(Icon));
		::SendMessageW(Hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(Icon));
	}

	// No title text in the window bar.
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

			// Create the window at its final geometry, but keep it hidden until the
			// game window has been laid out so there is no initial popup/resize.
			TSharedRef<SWindow> Window = SNew(SWindow)
				.Title(FText::GetEmpty())
				.ClientSize(ControllerSize)
				.ScreenPosition(ControllerPos)
				.AutoCenter(EAutoCenter::None)
				.SizingRule(ESizingRule::UserSized)
				.UseOSWindowBorder(true)
				.HasCloseButton(true)
				.SupportsMaximize(true)
				.SupportsMinimize(true)
				.IsTopmostWindow(true)
				[
					Core
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
				::SetWindowPos(Hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
						Style |= (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
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

							::SetWindowPos(ControllerHwnd, HWND_TOPMOST,
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
	TWeakPtr<Controller_UI> ControllerWidget;
	TWeakObjectPtr<UNiagaraComponent> BlowingParticlesComponent;
	FDelegateHandle EngineInitHandle;
	FDelegateHandle WorldInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE( FAVCModule, AVC, "AVC" );
