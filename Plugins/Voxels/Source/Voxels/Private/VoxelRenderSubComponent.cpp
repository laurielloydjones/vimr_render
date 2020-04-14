#include "VoxelRenderSubComponent.h"
#include "Voxels.h"
#include "Engine.h"

// Fix conflict between Windows.h macros and Unreal function name
#undef UpdateResource


UVoxelRenderSubComponent::UVoxelRenderSubComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	FString VoxelAsset = FString::Printf(TEXT("StaticMesh'/Voxels/UnitCubesOffset.UnitCubesOffset'"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> MeshFinder(*VoxelAsset);
	if(MeshFinder.Object != nullptr)
	{
		SetStaticMesh(MeshFinder.Object);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Couldn't load asset: %s"), *VoxelAsset);
	}

	static ConstructorHelpers::FObjectFinder<UMaterial> MaterialFinder(TEXT("Material'/Voxels/VertexMoveMaterial.VertexMoveMaterial'"));
	if(MaterialFinder.Object != nullptr)
	{
		StaticMaterial = MaterialFinder.Object;
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Couldn't load /Voxels/VertexMoveMaterial"));
	}

	UpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, SUB_VOXEL_COUNT_SQR, SUB_VOXEL_COUNT_SQR);

	EmptyData = new uint8[SUB_VOXEL_COUNT * VOXEL_TEXTURE_BPP]();
}

void UVoxelRenderSubComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	if(UpdateTextureRegion != nullptr)
	{
		delete UpdateTextureRegion;
		UpdateTextureRegion = nullptr;
	}

	delete[] EmptyData;

	Super::OnComponentDestroyed(bDestroyingHierarchy);
}

void UVoxelRenderSubComponent::BeginPlay()
{
	Super::BeginPlay();

	SetBoundsScale(10000.0f); // TODO: Automatically match this to voxel mat properties

	if(StaticMaterial)
	{
		CoarsePositionTexture = UTexture2D::CreateTransient(SUB_VOXEL_COUNT_SQR, SUB_VOXEL_COUNT_SQR);
		CoarsePositionTexture->UpdateResource();

		PositionTexture = UTexture2D::CreateTransient(SUB_VOXEL_COUNT_SQR, SUB_VOXEL_COUNT_SQR);
		PositionTexture->UpdateResource();

		ColourTexture = UTexture2D::CreateTransient(SUB_VOXEL_COUNT_SQR, SUB_VOXEL_COUNT_SQR);
		ColourTexture->UpdateResource();

		Material = CreateDynamicMaterialInstance(0, StaticMaterial);
		Material->SetTextureParameterValue(FName("CoarsePositionTexture"), CoarsePositionTexture);
		Material->SetTextureParameterValue(FName("PositionTexture"), PositionTexture);
		Material->SetTextureParameterValue(FName("ColourTexture"), ColourTexture);
		SetMaterial(0, Material);
		// GEngine->AddOnScreenDebugMessage(-1, 10.0f, FColor::Green, FString("Added Textures to Material"));

		// Scale was set before Material was available (e.g. in a parent constructor)
		if (bQueueScale)
		{
			Material->SetScalarParameterValue(FName("Scale"), Scale);
			bQueueScale = false;
		}
		if (bQueueLocation)
		{
			Material->SetVectorParameterValue(FName("Location"), Location);
			bQueueLocation = false;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Tried UVoxelRenderSubComponent::BeginPlay without Material ready"));
	}
}

void UVoxelRenderSubComponent::SetData(uint8* CoarsePositionData, uint8* PositionData, uint8* ColourData)
{
	if(CoarsePositionTexture && PositionTexture && ColourTexture)
	{
		// Take heap allocated copy of input data to hand to render thread, which will be responsible for cleanup in lambda
		// TODO: Restructure so that caller moves memory responsibility to here, avoiding extra allocation, remove delete in callback once done
		size_t DataSize = SUB_VOXEL_COUNT * VOXEL_TEXTURE_BPP;
		uint8* PositionDataRT = new uint8[DataSize];
		memcpy(PositionDataRT, PositionData, DataSize);
		uint8* ColourDataRT = new uint8[DataSize];
		memcpy(ColourDataRT, ColourData, DataSize);
		uint8* CoarseDataRT = new uint8[DataSize];
		memcpy(CoarseDataRT, CoarsePositionData, DataSize);

		// Region is safely reused across calls, it isn't modified and doesn't need to be deleted
		CoarsePositionTexture->UpdateTextureRegions(0, 1, UpdateTextureRegion, SUB_VOXEL_COUNT_SQR * VOXEL_TEXTURE_BPP, VOXEL_TEXTURE_BPP, CoarseDataRT, [](uint8 *Data, const FUpdateTextureRegion2D *Region) {
			delete[] Data;
		});
		PositionTexture->UpdateTextureRegions(0, 1, UpdateTextureRegion, SUB_VOXEL_COUNT_SQR * VOXEL_TEXTURE_BPP, VOXEL_TEXTURE_BPP, PositionDataRT, [](uint8 *Data, const FUpdateTextureRegion2D *Region) {
			delete[] Data;
		});
		ColourTexture->UpdateTextureRegions(0, 1, UpdateTextureRegion, SUB_VOXEL_COUNT_SQR * VOXEL_TEXTURE_BPP, VOXEL_TEXTURE_BPP, ColourDataRT, [](uint8 *Data, const FUpdateTextureRegion2D *Region) {
			delete[] Data;
		});
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Tried UVoxelRenderSubComponent::SetData without Textures initalised"));
	}
}

void UVoxelRenderSubComponent::ZeroData()
{
	if (CoarsePositionTexture && PositionTexture)
	{
		CoarsePositionTexture->UpdateTextureRegions(0, 1, UpdateTextureRegion, SUB_VOXEL_COUNT_SQR * VOXEL_TEXTURE_BPP, VOXEL_TEXTURE_BPP, EmptyData, [](uint8 *Data, const FUpdateTextureRegion2D *Region) {});
		PositionTexture->UpdateTextureRegions(0, 1, UpdateTextureRegion, SUB_VOXEL_COUNT_SQR * VOXEL_TEXTURE_BPP, VOXEL_TEXTURE_BPP, EmptyData, [](uint8 *Data, const FUpdateTextureRegion2D *Region) {});
	}
}

void UVoxelRenderSubComponent::SetScale(float Scale)
{
	if (Scale != this->Scale) {
		this->Scale = Scale;
		if (Material != nullptr)
		{
			Material->SetScalarParameterValue(FName("Scale"), Scale);
		}
		else
		{
			bQueueScale = true;
		}
	}
}

void UVoxelRenderSubComponent::SetLocation(FVector Location)
{
	this->Location = Location;
	if (Material != nullptr)
	{
		Material->SetVectorParameterValue(FName("Location"), Location);
	}
	else
	{
		bQueueLocation = true;
	}
}
