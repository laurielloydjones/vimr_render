// Fill out your copyright notice in the Description page of Project Settings.

#include "RuntimeAudioSource.h"
#include "AudioDevice.h"
#include "Engine/Engine.h"
#include "Sound/SoundWaveProcedural.h"
#include "Runtime/Core/Public/Misc/FileHelper.h"

// Sets default values for this component's properties
URuntimeAudioSource::URuntimeAudioSource()
{
	PrimaryComponentTick.bCanEverTick = false;
}


void URuntimeAudioSource::LoadWav(FString wavPath)
{
	// Sound Attenuation Settings
	SoundAttenuation = NewObject<USoundAttenuation>();
	SoundAttenuation->Attenuation.bAttenuate = 1;
	SoundAttenuation->Attenuation.bSpatialize = 1;
	SoundAttenuation->Attenuation.DistanceAlgorithm = EAttenuationDistanceModel::NaturalSound;
	SoundAttenuation->Attenuation.FalloffDistance = 2500; // 2.5m, this depends on the environment and the playback sound loudness.

	SoundWave = NewObject<USoundWaveProcedural>();
	SoundWave->NumChannels = 1;
	SoundWave->SetSampleRate(44100);
	SoundWave->bLooping = false;
	SoundWave->bProcedural = true;
	SoundWave->Volume = 1.0f;
	SoundWave->Duration = INDEFINITELY_LOOPING_DURATION;
	SoundWave->SoundGroup = SOUNDGROUP_Voice;
	SoundWave->bCanProcessAsync = true;

	FAudioDevice::FCreateComponentParams Params = FAudioDevice::FCreateComponentParams(GetWorld(), GetAttachmentRootActor());
	Params.bPlay = false;
	Params.AttenuationSettings = SoundAttenuation;
	FAudioDeviceHandle AudioDevice = GEngine->GetActiveAudioDevice();
	AudioComponent = AudioDevice->CreateComponent(SoundWave, Params);

	if (AudioComponent)
	{
		AudioComponent->RegisterComponent();
		AudioComponent->AttachToComponent(this, FAttachmentTransformRules(EAttachmentRule::KeepRelative, false));
		AudioComponent->bAutoActivate = true;
		AudioComponent->bAlwaysPlay = true;
		AudioComponent->VolumeMultiplier = 1.2f;
		AudioComponent->bIsUISound = true;
		AudioComponent->bAllowSpatialization = true;
		AudioComponent->bAutoDestroy = false;
		AudioComponent->OnUpdateTransform(EUpdateTransformFlags::PropagateFromParent);
		//AudioComponent->SetFlags(RF_MarkAsNative);
	}
	else
	{
	}
	FFileHelper::LoadFileToArray(audioData, wavPath.GetCharArray().GetData());
	SoundWave->ResetAudio();
	SoundWave->QueueAudio(audioData.GetData(), audioData.Num());

	SetIsReplicated(true);
}

void URuntimeAudioSource::Start()
{
	if (AudioComponent) {
		AudioComponent->Play();//FIXME: This is broken in Unreal  AudioComponent->Play(StartTimeSec);
		AudioComponent->SetPaused(false);
	}
}

void URuntimeAudioSource::Stop()
{
	if (AudioComponent) {
		Pause();
		SoundWave->ResetAudio();
		SoundWave->QueueAudio(audioData.GetData(), audioData.Num());
	}
}

void URuntimeAudioSource::Pause()
{
	if (AudioComponent) {
		AudioComponent->SetPaused(true);
	}
}

void URuntimeAudioSource::Resume()
{
	if (AudioComponent) {
		AudioComponent->SetPaused(false);
	}
}

bool URuntimeAudioSource::IsReady()
{
	return false;
}

// Called when the game starts
void URuntimeAudioSource::BeginPlay()
{
	Super::BeginPlay();
}

void URuntimeAudioSource::clear()
{
	SoundWave->ResetAudio();
}