#include "VolumeLoaderBPLibrary.h"

#include "Engine/VolumeTexture.h"
#include "AssetRegistry/AssetRegistryModule.h"

#include <algorithm>
#include <array>
#include <vector>
#include <map>
#include <fstream>
#include <sstream>

#include <DesktopPlatform/Public/DesktopPlatformModule.h>
#include <DesktopPlatform/Public/IDesktopPlatform.h>

template <typename OriTy, typename TrFnTy>
std::vector<uint8_t> loadRawVolAsG8(std::ifstream& is, size_t voxNum, size_t volSz, TrFnTy trFn)
{
	std::vector<OriTy> dat(voxNum);
	std::vector<uint8_t> ret(voxNum);

	is.seekg(0);
	is.read(reinterpret_cast<char*>(dat.data()), volSz);

	std::transform(dat.begin(), dat.end(), ret.begin(), trFn);

	return ret;
}

static inline FString makePkgName(const FString& assetName, FString assetFolder)
{
	if (assetFolder.IsEmpty())
		assetFolder = "GeneratedTextures";
	return "/Game" / assetFolder / assetName;
}

template <typename ObjTy>
ObjTy* createAsset(FString assetName, FString assetFolder, FString assetPrefix = "")
{
	if (!assetPrefix.IsEmpty())
		assetPrefix.AppendChar(TCHAR('_'));
	assetName = assetPrefix + assetName;
	auto pkgName = makePkgName(assetName, assetFolder);
	auto pkg = CreatePackage(*pkgName);
	pkg->FullyLoad();

	auto objPtr = NewObject<ObjTy>(
		static_cast<UObject*>(pkg), FName(*assetName), RF_Public | RF_Standalone | RF_MarkAsRootSet);
	objPtr->AddToRoot();

	return objPtr;
}

void UVolumeLoaderBPLibrary::ConvertRAWVolume2AssetFromFileDialog(
	bool& Success, FString& ErrMsg, EPixelFormat PixelFormat, FIntVector Dimensions,
	const FString& OutAssetName, FString OutAssetFolder)
{
	auto voxSz = [&]() -> uint8_t {
		switch (PixelFormat) {
		case PF_R8:
		case PF_G8:
			return GPixelFormats[PixelFormat].BlockBytes;
		default:
			return 0;
		}
	}();
	auto volSz = voxSz * Dimensions.X * Dimensions.Y * Dimensions.Z;
	if (volSz == 0 || voxSz == 0)
	{
		Success = false;
		ErrMsg = "Volume Size is 0 or Voxel Type is invalid.";
		return;
	}

	TArray<FString> filePaths;
	{
		auto parentWindow = FSlateApplication::Get()
			.FindBestParentWindowForDialogs(TSharedPtr<SWindow>());
		const void* parentWindowHandle =
			(parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid())
			? parentWindow->GetNativeWindow()->GetOSWindowHandle()
			: nullptr;

		auto success =
			FDesktopPlatformModule::Get()
			->OpenFileDialog(parentWindowHandle, "Select a RAW volume file",
				"", "", "", EFileDialogFlags::None, filePaths);
		if (!success)
		{
			Success = false;
			ErrMsg = "No file selected.";
			return;
		}
	}

	auto filePath = std::string(TCHAR_TO_UTF8(*filePaths[0]));
	std::ifstream is(filePath, std::ios::in | std::ios::binary | std::ios::ate);
	if (!is.is_open())
	{
		Success = false;
		ErrMsg = "Opening file failed.";
		return;
	}

	{
		auto fileVolSz = static_cast<size_t>(is.tellg());
		if (fileVolSz != volSz)
		{
			Success = false;
			ErrMsg = "File Size is not equal to Volume Size.";
			return;
		}
	}

	auto dat = loadRawVolAsG8<uint8_t>(
		is, volSz / voxSz, volSz, [](const uint8_t& old) -> uint8_t { return old; });
	is.close();

	auto volTex = createAsset<UVolumeTexture>(OutAssetName, OutAssetFolder, "VOL");
	{
		volTex->SetPlatformData(new FTexturePlatformData);
		volTex->GetPlatformData()->SizeX = Dimensions.X;
		volTex->GetPlatformData()->SizeY = Dimensions.Y;
		volTex->GetPlatformData()->SetNumSlices(Dimensions.Z);
		volTex->GetPlatformData()->PixelFormat = PixelFormat;
		volTex->SRGB = false;
		volTex->NeverStream = true;
	}
	{
		auto mip = new FTexture2DMipMap();
		mip->SizeX = Dimensions.X;
		mip->SizeY = Dimensions.Y;
		mip->SizeZ = Dimensions.Z;
		mip->BulkData.Lock(LOCK_READ_WRITE);

		auto dst = mip->BulkData.Realloc(volSz);
		FMemory::Memcpy(dst, dat.data(), volSz);

		mip->BulkData.Unlock();

		volTex->GetPlatformData()->Mips.Add(mip);
	}
	{
		volTex->MipGenSettings = TMGS_NoMipmaps;
		volTex->CompressionNone = true;
		volTex->Source.Init(Dimensions.X, Dimensions.Y, Dimensions.Z, 1, TSF_G8, dat.data());
		volTex->UpdateResource();
		volTex->MarkPackageDirty();

		FAssetRegistryModule::AssetCreated(volTex);
	}

	Success = true;
	return;
}

void UVolumeLoaderBPLibrary::ConvertTransferFunction2AssetFromFileDialog(
	bool& Success, FString& ErrMsg, FString OutAssetName, FString OutAssetFolder)
{
	TArray<FString> filePaths;
	{
		auto parentWindow = FSlateApplication::Get()
			.FindBestParentWindowForDialogs(TSharedPtr<SWindow>());
		const void* parentWindowHandle =
			(parentWindow.IsValid() && parentWindow->GetNativeWindow().IsValid())
			? parentWindow->GetNativeWindow()->GetOSWindowHandle()
			: nullptr;

		auto success =
			FDesktopPlatformModule::Get()
			->OpenFileDialog(parentWindowHandle, "Select a transfer function file",
				"", "", "", EFileDialogFlags::None, filePaths);
		if (!success)
		{
			Success = false;
			ErrMsg = "No file selected.";
			return;
		}
	}

	auto str = std::string(TCHAR_TO_UTF8(*filePaths[0]));
	std::ifstream is(str, std::ios::in);
	if (!is.is_open())
	{
		Success = false;
		ErrMsg = "Opening file failed.";
		return;
	}

	str.clear();
	std::map<float, std::array<float, 4>> index2Samples;
	while (std::getline(is, str))
	{
		float vals[5];
		auto readNum = sscanf(str.data(), "%f%f%f%f%f", vals, vals + 1, vals + 2, vals + 3, vals + 4);
		if (readNum != 5)
			continue;

		index2Samples.emplace(std::piecewise_construct,
			std::forward_as_tuple(vals[0]), std::forward_as_tuple(vals[1], vals[2], vals[3], vals[4]));
	}
	is.close();

	constexpr auto TexW = 256;
	constexpr auto TexH = 16;

	std::vector<std::array<uint8_t, 4>> samples(TexW * TexH);
	size_t prevIdx = 0, currIdx = 0;
	std::array<float, 4> prevCol{0};
	for (const auto& [idx, col] : index2Samples)
	{
		auto rng = static_cast<float>(idx) - static_cast<float>(prevIdx);
		if (rng == 0.f)
			rng = 1.f;
		while (currIdx <= std::floor(idx)) {
			auto K = (static_cast<float>(currIdx) - static_cast<float>(prevIdx)) / rng;
			auto OneMinusK = 1.f - K;

			// RGBA -> BGRA
			samples[currIdx][0] = std::floor(OneMinusK * prevCol[2] + K * col[2]);
			samples[currIdx][1] = std::floor(OneMinusK * prevCol[1] + K * col[1]);
			samples[currIdx][2] = std::floor(OneMinusK * prevCol[0] + K * col[0]);
			samples[currIdx][3] = std::floor(OneMinusK * prevCol[3] + K * col[3]);
			++currIdx;
		}
		if (currIdx >= TexW)
			break;

		prevIdx = std::floor(idx);
		prevCol = col;
	}

	for (size_t h = 1; h < TexH; ++h)
	{
		auto dst = samples.begin() + h * TexW;
		std::copy(samples.begin(), samples.begin() + TexW, dst);
	}

	auto tex = createAsset<UTexture2D>(OutAssetName, OutAssetFolder, "TF");
	{
		tex->SetPlatformData(new FTexturePlatformData);
		tex->GetPlatformData()->SizeX = TexW;
		tex->GetPlatformData()->SizeY = TexH;
		tex->GetPlatformData()->SetNumSlices(1);
		tex->GetPlatformData()->PixelFormat = PF_B8G8R8A8;
		tex->SRGB = false;
		tex->NeverStream = true;
	}
	{
		auto mip = new FTexture2DMipMap();
		mip->SizeX = TexW;
		mip->SizeY = TexH;
		mip->SizeZ = 1;
		mip->BulkData.Lock(LOCK_READ_WRITE);

		auto sz = sizeof(uint8_t) * 4 * samples.size();
		auto dst = mip->BulkData.Realloc(sz);
		FMemory::Memcpy(dst, samples.data(), sz);

		mip->BulkData.Unlock();

		tex->GetPlatformData()->Mips.Add(mip);
	}
	{
		tex->MipGenSettings = TMGS_NoMipmaps;
		tex->CompressionNone = true;
		tex->Source.Init(TexW, TexH, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8_t*>(samples.data()));
		tex->UpdateResource();
		tex->MarkPackageDirty();

		FAssetRegistryModule::AssetCreated(tex);
	}

	Success = true;
	return;
}
