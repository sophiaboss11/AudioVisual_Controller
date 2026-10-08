#pragma once

#include "CoreMinimal.h"

/**
 * Audio listening for the controller.
 */
class AVC_API AVC_Audio
{
public:
    /** Starts listening for audio. */
    static void StartListening();

    /** Stops listening for audio. */
    static void StopListening();

    /** Copies the most recent mono system-audio samples (-1..1) into OutSamples, oldest first. */
    static void GetWaveform(TArray<float>& OutSamples, int32 NumSamples);

    /** Current smoothed peak level of system audio (0..1). */
    static float GetLevel();

	static void TestJUCE();
};
