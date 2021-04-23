#include "VoxelRenderComponent.h"
#include "Voxels.h"
#include "VoxelRenderSubComponent.h"
#include "Engine.h"


UVoxelRenderComponent::UVoxelRenderComponent(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;

	for(int i = 0; i < TOTAL_VOXELS / SUB_VOXEL_COUNT; i++)
	{
		UVoxelRenderSubComponent* VRSC = ObjectInitializer.CreateDefaultSubobject<UVoxelRenderSubComponent>(this, FName(*(FString::Printf(TEXT("VoxelSubRender%d"), i))));
		VRSC->SetupAttachment(GetAttachmentRoot());
		VoxelRenderers.Add(VRSC);
	}

	//SetIsReplicated(true);
}


void UVoxelRenderComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	//TArray<FColor> OutColours; // Used only for saving to PNG

	if(VoxelSource != nullptr)
	{
		int VoxelCount;
		uint8 Voxelmm;
		uint8* CoarsePositionData = nullptr;
		uint8* PositionData = nullptr;
		uint8* ColourData = nullptr;
		//double startRead = FPlatformTime::Seconds();
		VoxelSource->GetFramePointers(VoxelCount, CoarsePositionData, PositionData, ColourData, Voxelmm);
		SetScale(((float)Voxelmm) / 10.0);// convert mm to cm
		//double endRead = FPlatformTime::Seconds();
		//GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("GetNextFrame time: %.4f ms"), (endRead - startRead) * 1000.0));

		// Throw away any data beyond the number of pregenerated voxels
		if(VoxelCount > TOTAL_VOXELS)
		{
			GEngine->AddOnScreenDebugMessage(VoxelWarningKey, 5.0f, FColor::Red, FString::Printf(TEXT("Too man voxels! %d / %d"), VoxelCount, TOTAL_VOXELS));
			VoxelCount = TOTAL_VOXELS;
		}

		/*if(saveFrame)
		{
			uint8* cPointer = ColourData;
			for(int i = 0; i < VoxelCount; i++)
			{
				OutColours.Add(FColor(cPointer[2], cPointer[1], cPointer[0]));
				cPointer += VOXEL_TEXTURE_BPP;
			}
		}*/

		int offset = 0;
		int RenderedVoxels = 0;
		bool bZero = false;
		for(auto& VRSC : VoxelRenderers)
		{
			if (bZero)
			{
				VRSC->ZeroData();
			}
			else {
				if (RenderedVoxels + SUB_VOXEL_COUNT > VoxelCount) {
					bZero = true; // Zero out further sub-renderers
					// Zero out the remainder of this one
					int SRVoxels = VoxelCount - RenderedVoxels;
					int EndOffset = offset + SRVoxels * VOXEL_TEXTURE_BPP;
					memset(CoarsePositionData + EndOffset, 0, (SUB_VOXEL_COUNT - SRVoxels) * VOXEL_TEXTURE_BPP);
					memset(PositionData + EndOffset, 0, (SUB_VOXEL_COUNT - SRVoxels) * VOXEL_TEXTURE_BPP);
				}
				// Set Data takes copies of required slices for render thread
				VRSC->SetData(CoarsePositionData + offset, PositionData + offset, ColourData + offset);
				offset += SUB_VOXEL_COUNT * VOXEL_TEXTURE_BPP;
				RenderedVoxels += SUB_VOXEL_COUNT;
			}
		}

		// For debugging, save texture map to PNG
		/*if(saveFrame)
		{
			// Position->Depth
			TArray<FColor> OutPositions;
			for(int i = 0; i < OutColours.Num(); i++)
			{
				uint8* p = PositionData + i * VOXEL_TEXTURE_BPP;
				OutPositions.Add(FColor(p[0], p[1], p[2]));
			}
			// Pad with 0s
			int diff = TOTAL_VOXELS - OutColours.Num();
			for(int i = 0; i < diff; i++)
			{
				OutColours.Add(FColor(0, 0, 0));
				OutPositions.Add(FColor(0, 0, 0));
			}
			TArray<uint8> CompressedPNG;
			FImageUtils::CompressImageArray(
				512,
				TOTAL_VOXELS / 512,
				OutColours,
				CompressedPNG
			);
			FFileHelper::SaveArrayToFile(CompressedPNG, TEXT("VoxelColourMap.png"));
			CompressedPNG.Reset();
			FImageUtils::CompressImageArray(
				512,
				TOTAL_VOXELS / 512,
				OutPositions,
				CompressedPNG
			);
			FFileHelper::SaveArrayToFile(CompressedPNG, TEXT("VoxelPositionMap.png"));
			GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Yellow, FString::Printf(TEXT("Saved voxel maps")));
		}*/
	}
}

void UVoxelRenderComponent::SetScale(float Scale)
{
	for(auto& VRSC : VoxelRenderers)
	{
		VRSC->SetScale(Scale);
	}
}

void UVoxelRenderComponent::SetLocation(FVector Location)
{
	for (auto& VRSC : VoxelRenderers)
	{
		VRSC->SetLocation(Location);
	}
}

void UVoxelRenderComponent::SetRotation(FVector Rotation)
{
	for (auto& VRSC : VoxelRenderers)
	{
		VRSC->SetRotation(Rotation);
	}
}