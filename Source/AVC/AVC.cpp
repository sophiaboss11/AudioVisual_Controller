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
		UE_LOG(LogTemp, Warning, TEXT("=== AVC game started"));
		PrintScriptCallstack();


		TSharedRef<Controller_UI> Core = SNew(Controller_UI);

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

	virtual void ShutdownModule() override
	{
		if (EngineInitHandle.IsValid())
		{
			FCoreDelegates::OnFEngineLoopInitComplete.Remove(EngineInitHandle);
			EngineInitHandle.Reset();
		}

		FDefaultGameModuleImpl::ShutdownModule();
	}

private:
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
					GameNativeWindow->ReshapeWindow(0,0,1024,768);
				}
			}
		}
	}

	TWeakPtr<SWindow> AVCWindow;
	FDelegateHandle EngineInitHandle;
};

IMPLEMENT_PRIMARY_GAME_MODULE( FAVCModule, AVC, "AVC" );
