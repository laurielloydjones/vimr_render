#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "VoxelSourceInterface.h"
#include "VoxelRenderComponent.generated.h"

#define TOTAL_VOXELS 196608

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class VOXELS_API UVoxelRenderComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable)
		void SetScale(float Scale);

	UFUNCTION(BlueprintCallable)
		void SetLocation(FVector Location);

	UFUNCTION(BlueprintCallable)
		void SetRotation(FVector Rotation);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Default)
	TScriptInterface<IVoxelSourceInterface> VoxelSource;

private:
	UPROPERTY()
	TArray<class UVoxelRenderSubComponent*> VoxelRenderers;
};