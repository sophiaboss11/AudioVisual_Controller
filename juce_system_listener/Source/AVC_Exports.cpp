#include <JuceHeader.h>

#include <JuceHeader.h>

// Simple test export callable from the AVC Unreal project; uses juce::jmax from juce_core.
extern "C" __declspec(dllexport) int AVC_Max(int A, int B)
{
    return juce::jmax(A, B);
}
