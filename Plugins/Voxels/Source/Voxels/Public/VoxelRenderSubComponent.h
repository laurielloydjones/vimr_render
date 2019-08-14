#pragma once

#include "CoreMinimal.h"
#include "Components/StaticMeshComponent.h"
#include "VoxelRenderSubComponent.generated.h"


#define SUB_VOXEL_COUNT 65536
#define SUB_VOXEL_COUNT_SQR 256
#define VOXEL_TEXTURE_BPP 4

UCLASS()
class VOXELS_API UVoxelRenderSubComponent : public UStaticMeshComponent
{
	GENERATED_BODY()

public:
	UVoxelRenderSubComponent();

	void SetData(uint8* CoarsePositionData, uint8* PositionData, uint8* ColourData);

	void ZeroData();

	void BeginPlay() override;

	void OnComponentDestroyed(bool bDestroyingHierarchy) override;

	void SetScale(float Scale);

	void SetLocation(FVector Location);

private:
	UPROPERTY()
	UMaterialInterface* StaticMaterial;

	UPROPERTY()
	UMaterialInstanceDynamic* Material;

	UPROPERTY()
	UTexture2D* CoarsePositionTexture;

	UPROPERTY()
	UTexture2D* PositionTexture;

	UPROPERTY()
	UTexture2D* ColourTexture;

	FUpdateTextureRegion2D *UpdateTextureRegion;
	uint8* EmptyData;

	bool bQueueScale = false;
	bool bQueueLocation = false;
	float Scale = 1.0f;
	FVector Location = FVector(0.0f);
};
