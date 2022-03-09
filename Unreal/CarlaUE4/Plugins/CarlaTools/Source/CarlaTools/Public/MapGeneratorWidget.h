// Copyright (c) 2022 Computer Vision Center (CVC) at the Universitat Autonoma
// de Barcelona (UAB).
//
// This work is licensed under the terms of the MIT license.
// For a copy, see <https://opensource.org/licenses/MIT>.

#pragma once 

#include "CoreMinimal.h"

#include "EditorUtilityWidget.h"
#include "Engine/TextureRenderTarget2D.h"
#include "UnrealString.h"

#include "MapGeneratorWidget.generated.h"

USTRUCT(BlueprintType)
struct CARLATOOLS_API FMapGeneratorMetaInfo
{
    GENERATED_USTRUCT_BODY();

    UPROPERTY(BlueprintReadWrite)
    FString DestinationPath;

    UPROPERTY(BlueprintReadWrite)
    FString MapName;

    UPROPERTY(BlueprintReadWrite)   
    int SizeX;

    UPROPERTY(BlueprintReadWrite)
    int SizeY;
};

USTRUCT(BlueprintType)
struct CARLATOOLS_API FMapGeneratorTileMetaInfo
{
    GENERATED_USTRUCT_BODY();

    UPROPERTY(BlueprintReadWrite)   
    bool bIsTiled = true;

    UPROPERTY(BlueprintReadWrite)   
    int IndexX;

    UPROPERTY(BlueprintReadWrite)
    int IndexY;
};

/// Class UMapGeneratorWidget extends the functionality of UEditorUtilityWidget
/// to be able to generate and manage maps and largemaps tiles for procedural
/// map generation
UCLASS(BlueprintType)
class CARLATOOLS_API UMapGeneratorWidget : public UEditorUtilityWidget
{
    GENERATED_BODY()


private:
    // UPROPERTY()
    // UObjectLibrary *MapObjectLibrary;

public:
    /// This function invokes a blueprint event defined in widget blueprint 
    /// event graph, which sets a heightmap to the @a Landscape using
    /// ALandscapeProxy::LandscapeImportHeightMapFromRenderTarget(...)
    /// function, which is not exposed to be used in C++ code, only blueprints
    /// @a metaTileInfo contains some useful info to execute this function
    UFUNCTION(BlueprintImplementableEvent)
    void AssignLandscapeHeightMap(ALandscape* Landscape, FMapGeneratorTileMetaInfo TileMetaInfo);

    /// Function called by Widget Blueprint which generates all tiles of map
    /// @a mapName, and saves them in @a destinationPath
    /// Returns a void string is success and an error message if the process failed
    UFUNCTION(Category="Map Generator",BlueprintCallable)
    FString GenerateMapFiles(const FMapGeneratorMetaInfo& MetaInfo);

private:    
    /// Loads the base tile map and stores it in @a WorldAssetData
    /// The funtions return true is success, otherwise false
    UFUNCTION()
    bool LoadBaseTileWorld(FAssetData& WorldAssetData);

    /// Loads the base large map and stores it in @a WorldAssetData
    /// The funtions return true is success, otherwise false
    UFUNCTION()
    bool LoadBaseLargeMapWorld(FAssetData& WorldAssetData);

    /// Loads the base template UWorld object from @a BaseMapPath and returns 
    /// it in @a WorldAssetData
    /// The funtions return true is success, otherwise false
    UFUNCTION()
    bool LoadWorld(FAssetData& WorldAssetData, const FString& BaseMapPath);

    /// Saves a world contained in @a WorldToBeSaved, in the path defined in @a DestinationPath
    /// named as @a WorldName, as a package .umap
    UFUNCTION()
    bool SaveWorld(FAssetData& WorldToBeSaved, const FString& DestinationPath, const FString& WorldName);

    /// Takes the name of the map from @a MetaInfo and created the main map
    /// including all the actors needed by large map system
    UFUNCTION()
    bool CreateMainLargeMap(const FMapGeneratorMetaInfo& MetaInfo);

    /// Takes @a MetaInfo as input and generates all tiles based on the
    /// dimensions specified for the map
    /// The funtions return true is success, otherwise false
    UFUNCTION()
    bool CreateTilesMaps(const FMapGeneratorMetaInfo& MetaInfo);

    /// Gets the Landscape from the input world @a WorldAssetData and
    /// applies the heightmap to it. The tile index is indexX and indexY in
    /// @a TileMetaInfo argument
    /// The funtions return true is success, otherwise false
    UFUNCTION()
    bool ApplyHeightMapToLandscape(FAssetData& WorldAssetData, FMapGeneratorTileMetaInfo TileMetaInfo);
};
// #endif