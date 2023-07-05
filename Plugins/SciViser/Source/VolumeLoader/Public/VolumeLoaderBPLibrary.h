#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"

#include "VolumeLoaderBPLibrary.generated.h"

/*
 *	Blueprint function library for Volume Texture utilities.
 */
UCLASS()
class VOLUMELOADER_API UVolumeLoaderBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, meta = (Keywords = "Convert Raw Volume"), Category = "VolumeLoader")
		static void ConvertRAWVolume2AssetFromFileDialog(
			bool& Success, FString& ErrMsg, EPixelFormat PixelFormat, FIntVector Dimensions,
			const FString& OutAssetName, FString OutAssetFolder = "");

	UFUNCTION(BlueprintCallable, meta = (Keywords = "Convert Transfer Function"), Category = "VolumeLoader")
		static void ConvertTransferFunction2AssetFromFileDialog(
			bool& Success, FString& ErrMsg, FString OutAssetName, FString OutAssetFolder = "");

	UFUNCTION(BlueprintCallable, meta = (Keywords = "Convert Voxel Type"), Category = "VolumeLoader")
		static EPixelFormat ConvertVoxelTypeString2PixelFormat(bool& Success, FString VoxelTypeString = "uint8")
	{
		if (VoxelTypeString == "uint8")
		{
			Success = true;
			return PF_G8;
		}
		if (VoxelTypeString == "float32")
		{
			Success = true;
			return PF_R32_FLOAT;
		}
		Success = false;
		return PF_G8;
	}
};
