// Fill out your copyright notice in the Description page of Project Settings.


#include "Controller_UI.h"
#include "SlateOptMacros.h"

#include "NiagaraDataChannelFunctionLibrary.h"
#include "NiagaraDataChannelAccessor.h"
#include "NiagaraDataChannelAsset.h"
#include "NiagaraDataChannelPublic.h"
#include "NiagaraDataChannelCommon.h"
#include "Engine/Engine.h"

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void Controller_UI::Construct(const FArguments& InArgs)
{
	UE_LOG(LogTemp, Warning, TEXT("=== Controller_UI Construct called"));

	// Load the controllerDraw_dataChannel asset from the project.
	UNiagaraDataChannelAsset* ControllerDrawDataChannel = LoadObject<UNiagaraDataChannelAsset>(nullptr, TEXT("/Game/controllerDraw_dataChannel.controllerDraw_dataChannel"));

	UWorld* World = GEngine ? GEngine->GetCurrentPlayWorld() : nullptr;

	if (ControllerDrawDataChannel && World)
	{
		// Initialize a writer for a single element and set the "AVC_controllerDraw" variable to ControllerValue.
		FNDCAccessContextInst& AccessContext = UNiagaraDataChannelLibrary::GetUsableAccessContextFromNDC(ControllerDrawDataChannel);
		UNiagaraDataChannelWriter* Writer = UNiagaraDataChannelLibrary::WriteToNiagaraDataChannel_WithContext(
			World, ControllerDrawDataChannel, AccessContext, /*Count*/ 1,
			/*bVisibleToBlueprint*/ true, /*bVisibleToNiagaraCPU*/ true, /*bVisibleToNiagaraGPU*/ true,
			TEXT("Controller_UI"));

		if (Writer)
		{
			// Index must be within [0, Count). We requested Count == 1, so write to index 0.
			Writer->WriteFloat(TEXT("AVC_controllerDraw"), 0, ControllerValue);
		}
	}

	/*
	ChildSlot
	[
		// Populate the widget
	];
	*/

}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION
