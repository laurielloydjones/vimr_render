// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundWaveProcedural.h"
#include "Sound/SoundAttenuation.h"
#include "Runtime/Engine/Public/AudioDevice.h"
#include "Engine/Engine.h"
#include "RuntimeAudioSource.generated.h"


UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VOXELS_API URuntimeAudioSource : public USceneComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	URuntimeAudioSource();

	UFUNCTION(BlueprintCallable, Category = "RTAudio")
		void LoadWav(FString wavPath);

	void LoadBuffer(char* data, int numdata);


	UFUNCTION(BlueprintCallable, Category = "RTAudio")
	void Start();

	UFUNCTION(BlueprintCallable, Category = "RTAudio")
	void Stop();

	UFUNCTION(BlueprintCallable, Category = "RTAudio")
	void Pause();

	UFUNCTION(BlueprintCallable, Category = "RTAudio")
	void Resume();

	UFUNCTION(BlueprintCallable, Category = "RTAudio")
	bool IsReady();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		FString RecordingPath;
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
		float StartTimeSec;

	void clear();

	UAudioComponent* GetAudioComponent()
	{
		return AudioComponent;
	}

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

	

	UPROPERTY()
	UAudioComponent* AudioComponent;

	USoundWaveProcedural* SoundWave;

	USoundAttenuation* SoundAttenuation;

	TArray<uint8> audioData;
};
