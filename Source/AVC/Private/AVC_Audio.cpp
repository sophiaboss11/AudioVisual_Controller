#include "AVC_Audio.h"
#include "AVC_Audio.h"

THIRD_PARTY_INCLUDES_START
#include <juce_core/juce_core.h>
THIRD_PARTY_INCLUDES_END

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <mmdeviceapi.h>
#include <audioclient.h>
#include <mmreg.h>
#include <ksmedia.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

#include <memory>
#include <atomic>
#include <vector>

namespace
{
    // Ring buffer of recent mono samples shared between the capture thread and the UI.
    constexpr int32 WaveformCapacity = 4096;
    float GWaveform[WaveformCapacity] = {};
    int32 GWaveformWritePos = 0;
    juce::SpinLock GWaveformLock;
    std::atomic<float> GLevel { 0.0f };

    void PushMonoSamples(const float* Samples, int32 Num)
    {
        const juce::SpinLock::ScopedLockType Lock(GWaveformLock);
        for (int32 i = 0; i < Num; ++i)
        {
            GWaveform[GWaveformWritePos] = Samples[i];
            GWaveformWritePos = (GWaveformWritePos + 1) % WaveformCapacity;
        }
    }

#if PLATFORM_WINDOWS
    // Captures everything played on the default output device (WASAPI loopback) on a JUCE thread.
    class FSystemAudioListener : public juce::Thread
    {
    public:
        FSystemAudioListener() : juce::Thread("AVC System Audio Listener") {}
        ~FSystemAudioListener() override { stopThread(2000); }

        void run() override
        {
            const HRESULT ComHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

            IMMDeviceEnumerator* Enumerator = nullptr;
            IMMDevice* Device = nullptr;
            IAudioClient* AudioClient = nullptr;
            IAudioCaptureClient* CaptureClient = nullptr;
            WAVEFORMATEX* MixFormat = nullptr;

            auto Fail = [](const TCHAR* Step, HRESULT Hr)
            {
                UE_LOG(LogTemp, Error, TEXT("AVC_Audio: %s failed (0x%08x)"), Step, (uint32)Hr);
            };

            HRESULT Hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, __uuidof(IMMDeviceEnumerator), (void**)&Enumerator);
            if (FAILED(Hr)) { Fail(TEXT("CoCreateInstance"), Hr); }

            if (SUCCEEDED(Hr)) { Hr = Enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &Device); if (FAILED(Hr)) Fail(TEXT("GetDefaultAudioEndpoint"), Hr); }
            if (SUCCEEDED(Hr)) { Hr = Device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, (void**)&AudioClient); if (FAILED(Hr)) Fail(TEXT("Activate"), Hr); }
            if (SUCCEEDED(Hr)) { Hr = AudioClient->GetMixFormat(&MixFormat); if (FAILED(Hr)) Fail(TEXT("GetMixFormat"), Hr); }
            if (SUCCEEDED(Hr))
            {
                Hr = AudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED, AUDCLNT_STREAMFLAGS_LOOPBACK, 10000000, 0, MixFormat, nullptr);
                if (FAILED(Hr)) Fail(TEXT("IAudioClient::Initialize"), Hr);
            }
            if (SUCCEEDED(Hr)) { Hr = AudioClient->GetService(__uuidof(IAudioCaptureClient), (void**)&CaptureClient); if (FAILED(Hr)) Fail(TEXT("GetService"), Hr); }
            if (SUCCEEDED(Hr)) { Hr = AudioClient->Start(); if (FAILED(Hr)) Fail(TEXT("Start"), Hr); }

            if (SUCCEEDED(Hr))
            {
                const int NumChannels = MixFormat->nChannels;
                const int BitsPerSample = MixFormat->wBitsPerSample;
                bool bIsFloat = MixFormat->wFormatTag == WAVE_FORMAT_IEEE_FLOAT;
                if (MixFormat->wFormatTag == WAVE_FORMAT_EXTENSIBLE)
                {
                    bIsFloat = reinterpret_cast<WAVEFORMATEXTENSIBLE*>(MixFormat)->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT;
                }

                UE_LOG(LogTemp, Warning, TEXT("AVC_Audio: WASAPI loopback started (%d Hz, %d ch, %d bit, %s)"),
                    (int32)MixFormat->nSamplesPerSec, NumChannels, BitsPerSample, bIsFloat ? TEXT("float") : TEXT("pcm"));

                float WindowPeak = 0.0f;
                std::vector<float> Mono;
                bool bWasReceiving = false;
                juce::uint32 LastLogTime = juce::Time::getMillisecondCounter();

                while (!threadShouldExit())
                {
                    wait(10);

                    UINT32 PacketSize = 0;
                    if (FAILED(CaptureClient->GetNextPacketSize(&PacketSize)))
                        break;

                    if (PacketSize == 0)
                    {
                        GLevel.store(GLevel.load() * 0.9f);
                    }

                    while (PacketSize > 0)
                    {
                        BYTE* Data = nullptr;
                        UINT32 NumFrames = 0;
                        DWORD Flags = 0;
                        if (FAILED(CaptureClient->GetBuffer(&Data, &NumFrames, &Flags, nullptr, nullptr)))
                            break;

                        const bool bSilent = (Flags & AUDCLNT_BUFFERFLAGS_SILENT) != 0 || Data == nullptr;
                        Mono.assign(NumFrames, 0.0f);
                        float PacketPeak = 0.0f;
                        if (!bSilent)
                        {
                            const int NumSamples = (int)NumFrames * NumChannels;
                            if (bIsFloat && BitsPerSample == 32)
                            {
                                const float* Samples = reinterpret_cast<const float*>(Data);
                                for (int i = 0; i < NumSamples; ++i)
                                {
                                    PacketPeak = juce::jmax(PacketPeak, std::abs(Samples[i]));
                                    Mono[i / NumChannels] += Samples[i] / NumChannels;
                                }
                            }
                            else if (BitsPerSample == 16)
                            {
                                const int16* Samples = reinterpret_cast<const int16*>(Data);
                                for (int i = 0; i < NumSamples; ++i)
                                {
                                    const float S = Samples[i] / 32768.0f;
                                    PacketPeak = juce::jmax(PacketPeak, std::abs(S));
                                    Mono[i / NumChannels] += S / NumChannels;
                                }
                            }
                        }
                        WindowPeak = juce::jmax(WindowPeak, PacketPeak);
                        PushMonoSamples(Mono.data(), (int32)NumFrames);

                        // Fast attack, slow release.
                        const float Prev = GLevel.load();
                        GLevel.store(PacketPeak > Prev ? PacketPeak : Prev * 0.9f + PacketPeak * 0.1f);

                        CaptureClient->ReleaseBuffer(NumFrames);
                        if (FAILED(CaptureClient->GetNextPacketSize(&PacketSize)))
                            break;
                    }

                    const juce::uint32 Now = juce::Time::getMillisecondCounter();
                    if (Now - LastLogTime >= 500)
                    {
                        const bool bReceiving = WindowPeak > 0.001f;
                        if (bReceiving)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("AVC_Audio: system audio input received (peak %.3f)"), WindowPeak);
                        }
                        else if (bWasReceiving)
                        {
                            UE_LOG(LogTemp, Warning, TEXT("AVC_Audio: system audio is silent"));
                        }
                        bWasReceiving = bReceiving;
                        WindowPeak = 0.0f;
                        LastLogTime = Now;
                    }
                }

                AudioClient->Stop();
                UE_LOG(LogTemp, Warning, TEXT("AVC_Audio: WASAPI loopback stopped"));
            }

            if (MixFormat) CoTaskMemFree(MixFormat);
            if (CaptureClient) CaptureClient->Release();
            if (AudioClient) AudioClient->Release();
            if (Device) Device->Release();
            if (Enumerator) Enumerator->Release();
            if (SUCCEEDED(ComHr)) CoUninitialize();
        }
    };

    std::unique_ptr<FSystemAudioListener> GListener;
#endif
}

void AVC_Audio::StartListening()
{
    UE_LOG(LogTemp, Warning, TEXT("===listening..."));
    TestJUCE();

#if PLATFORM_WINDOWS
    if (!GListener)
    {
        GListener = std::make_unique<FSystemAudioListener>();
        GListener->startThread();
    }
#endif
}

void AVC_Audio::StopListening()
{
#if PLATFORM_WINDOWS
    GListener.reset();
#endif
    GLevel.store(0.0f);
}

void AVC_Audio::GetWaveform(TArray<float>& OutSamples, int32 NumSamples)
{
    NumSamples = FMath::Clamp(NumSamples, 0, WaveformCapacity);
    OutSamples.SetNumUninitialized(NumSamples);

    const juce::SpinLock::ScopedLockType Lock(GWaveformLock);
    int32 ReadPos = (GWaveformWritePos - NumSamples + WaveformCapacity) % WaveformCapacity;
    for (int32 i = 0; i < NumSamples; ++i)
    {
        OutSamples[i] = GWaveform[ReadPos];
        ReadPos = (ReadPos + 1) % WaveformCapacity;
    }
}

float AVC_Audio::GetLevel()
{
    return FMath::Clamp(GLevel.load(), 0.0f, 1.0f);
}

void AVC_Audio::TestJUCE()
{ 
    //FloatType myPi = juce_MathsFunctions::pi
	float maxValue = juce::jmax(1.0f, 2.0f);

    juce::String message = "JUCE is working!2";

	UE_LOG(LogTemp, Warning, TEXT("%s %f"), UTF8_TO_TCHAR(message.toRawUTF8()), maxValue);
}
