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
#include "Windows/HideWindowsPlatformTypes.h"
#endif

class FAVCModule : public FDefaultGameModuleImpl
{
public:
	virtual void StartupModule() override
	{
		FDefaultGameModuleImpl::StartupModule();
		//UE_LOG(LogTemp, Warning, TEXT("=== AVC game started"));
		PrintScriptCallstack();

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

		// Find the Niagara component in this world that uses the "blowing_particles" system asset.
		//for (TActorIterator<AActor> It(Params.World); It; ++It)
		//{
		//	AActor* Actor = *It;
		//	if (Actor)
		//	{
		//		UE_LOG(LogTemp, Warning, TEXT("=== Actor: %s (Label: %s)"), *Actor->GetName(), *Actor->GetActorNameOrLabel());
		//	}
		//}




		// Write to (and read back from) the Niagara data channel once BeginPlay has run
		// (matching the blueprint's Event BeginPlay). Deferring to the next tick ensures
		// actors have begun play and the data channel handlers are ready.

		Params.World->GetTimerManager().SetTimerForNextTick([WeakCore, WeakWorld]()
		{
			TSharedPtr<Controller_UI> Pinned = WeakCore.Pin();
			if (Pinned.IsValid() && WeakWorld.IsValid())
			{
				Pinned->WriteAndReadDataChannel(WeakWorld.Get());
			}
		});

		if (FSlateApplication::IsInitialized())
		{
			TSharedRef<SWindow> Window = SNew(SWindow)
				.Title(FText::FromString(TEXT("AVC")))
				.ClientSize(FVector2D(400, 300))
				.SizingRule(ESizingRule::UserSized)
				.UseOSWindowBorder(true)
				.HasCloseButton(true)
				.SupportsMaximize(true)
				.SupportsMinimize(true)
				.IsTopmostWindow(true)
				[
					Core
				];

			FSlateApplication::Get().AddWindow(Window);
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
					// Size the window to half the monitor's resolution and center it.
					FDisplayMetrics DisplayMetrics;
					FSlateApplication::Get().GetInitialDisplayMetrics(DisplayMetrics);

					const int32 ScreenWidth = DisplayMetrics.PrimaryDisplayWidth;
					const int32 ScreenHeight = DisplayMetrics.PrimaryDisplayHeight;

					const int32 WindowWidth = ScreenWidth / 2;
					const int32 WindowHeight = ScreenHeight / 2;
					const FVector2D ControllerSize = Window->GetSizeInScreen();
					const int32 ControllerWidth = FMath::RoundToInt(ControllerSize.X);

					// Place the game and controller windows side by side, centered as a group,
					// with the controller on the right.
					const int32 WindowX = FMath::Max(0, (ScreenWidth - (WindowWidth + ControllerWidth)) / 2);
					const int32 WindowY = (ScreenHeight - WindowHeight) / 2;

					GameNativeWindow->ReshapeWindow(WindowX, WindowY, WindowWidth, WindowHeight);
					Window->ReshapeWindow(FVector2D(WindowX + WindowWidth, WindowY), FVector2D(ControllerWidth, WindowHeight));

#if PLATFORM_WINDOWS
					// Make the game window movable and give it an exit button by
					// adding a caption bar and system menu to the native window style.
					HWND GameHwnd = static_cast<HWND>(GameNativeWindow->GetOSWindowHandle());
					if (GameHwnd)
					{
						LONG_PTR Style = ::GetWindowLongPtr(GameHwnd, GWL_STYLE);
						Style |= (WS_CAPTION | WS_SYSMENU | WS_THICKFRAME);
						::SetWindowLongPtr(GameHwnd, GWL_STYLE, Style);

						// Force the frame changes to take effect.
						::SetWindowPos(GameHwnd, nullptr, 0, 0, 0, 0,
							SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

						// Align the controller window with the game window's final outer
						// bounds so their top and bottom edges match exactly.
						TSharedPtr<FGenericWindow> ControllerNativeWindow = Window->GetNativeWindow();
						HWND ControllerHwnd = ControllerNativeWindow.IsValid()
							? static_cast<HWND>(ControllerNativeWindow->GetOSWindowHandle())
							: nullptr;
						RECT GameRect;
						RECT ControllerRect;
						if (ControllerHwnd && ::GetWindowRect(GameHwnd, &GameRect) && ::GetWindowRect(ControllerHwnd, &ControllerRect))
						{
							const int32 ControllerOuterWidth = ControllerRect.right - ControllerRect.left;
							::SetWindowPos(ControllerHwnd, HWND_TOPMOST,
								GameRect.right, GameRect.top,
								ControllerOuterWidth, GameRect.bottom - GameRect.top,
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
