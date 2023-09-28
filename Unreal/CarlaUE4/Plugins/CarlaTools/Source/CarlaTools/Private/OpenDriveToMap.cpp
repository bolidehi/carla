// Copyright (c) 2023 Computer Vision Center (CVC) at the Universitat Autonoma de Barcelona (UAB). This work is licensed under the terms of the MIT license. For a copy, see <https://opensource.org/licenses/MIT>.

#include "OpenDriveToMap.h"
#include "Components/Button.h"
#include "DesktopPlatform/Public/IDesktopPlatform.h"
#include "DesktopPlatform/Public/DesktopPlatformModule.h"
#include "Misc/FileHelper.h"
#include "Engine/LevelBounds.h"
#include "Engine/SceneCapture2D.h"
#include "Runtime/Core/Public/Async/ParallelFor.h"
#include "Kismet/KismetRenderingLibrary.h"
#include "KismetProceduralMeshLibrary.h"

#include "Carla/Game/CarlaStatics.h"
#include "Traffic/TrafficLightManager.h"
#include "Online/CustomFileDownloader.h"
#include "Util/ProceduralCustomMesh.h"

#include "OpenDrive/OpenDriveGenerator.h"

#include <compiler/disable-ue4-macros.h>
#include <carla/opendrive/OpenDriveParser.h>
#include <carla/road/Map.h>
#include <carla/geom/Simplification.h>
#include <carla/road/Deformation.h>
#include <carla/rpc/String.h>
#include <OSM2ODR.h>
#include <compiler/enable-ue4-macros.h>

#include "Engine/Classes/Interfaces/Interface_CollisionDataProvider.h"
#include "Engine/TriggerBox.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "PhysicsCore/Public/BodySetupEnums.h"
#include "RawMesh.h"
#include "AssetRegistryModule.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "MeshDescription.h"
#include "EditorLevelLibrary.h"
#include "ProceduralMeshConversion.h"

#include "ContentBrowserModule.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Math/Vector.h"
#include "GameFramework/Actor.h"

#include "DrawDebugHelpers.h"

FString LaneTypeToFString(carla::road::Lane::LaneType LaneType)
{
  switch (LaneType)
  {
  case carla::road::Lane::LaneType::Driving:
    return FString("Driving");
    break;
  case carla::road::Lane::LaneType::Stop:
    return FString("Stop");
    break;
  case carla::road::Lane::LaneType::Shoulder:
    return FString("Shoulder");
    break;
  case carla::road::Lane::LaneType::Biking:
    return FString("Biking");
    break;
  case carla::road::Lane::LaneType::Sidewalk:
    return FString("Sidewalk");
    break;
  case carla::road::Lane::LaneType::Border:
    return FString("Border");
    break;
  case carla::road::Lane::LaneType::Restricted:
    return FString("Restricted");
    break;
  case carla::road::Lane::LaneType::Parking:
    return FString("Parking");
    break;
  case carla::road::Lane::LaneType::Bidirectional:
    return FString("Bidirectional");
    break;
  case carla::road::Lane::LaneType::Median:
    return FString("Median");
    break;
  case carla::road::Lane::LaneType::Special1:
    return FString("Special1");
    break;
  case carla::road::Lane::LaneType::Special2:
    return FString("Special2");
    break;
  case carla::road::Lane::LaneType::Special3:
    return FString("Special3");
    break;
  case carla::road::Lane::LaneType::RoadWorks:
    return FString("RoadWorks");
    break;
  case carla::road::Lane::LaneType::Tram:
    return FString("Tram");
    break;
  case carla::road::Lane::LaneType::Rail:
    return FString("Rail");
    break;
  case carla::road::Lane::LaneType::Entry:
    return FString("Entry");
    break;
  case carla::road::Lane::LaneType::Exit:
    return FString("Exit");
    break;
  case carla::road::Lane::LaneType::OffRamp:
    return FString("OffRamp");
    break;
  case carla::road::Lane::LaneType::OnRamp:
    return FString("OnRamp");
    break;
  case carla::road::Lane::LaneType::Any:
    return FString("Any");
    break;
  }

  return FString("Empty");
}

void UOpenDriveToMap::ConvertOSMInOpenDrive()
{
  FilePath = FPaths::ProjectContentDir() + "CustomMaps/" + MapName + "/OpenDrive/" + MapName + ".osm";
  FileDownloader->ConvertOSMInOpenDrive( FilePath , OriginGeoCoordinates.X, OriginGeoCoordinates.Y);
  FilePath.RemoveFromEnd(".osm", ESearchCase::Type::IgnoreCase);
  FilePath += ".xodr";

  LoadMap();
}

void UOpenDriveToMap::CreateMap()
{
  if( MapName.IsEmpty() )
  {
    UE_LOG(LogCarlaToolsMapGenerator, Error, TEXT("Map Name Is Empty") );
    return;
  }
  if ( !IsValid(FileDownloader) )
  {
    FileDownloader = NewObject<UCustomFileDownloader>();
  }
  FileDownloader->ResultFileName = MapName;
  FileDownloader->Url = Url;

  FileDownloader->DownloadDelegate.BindUObject( this, &UOpenDriveToMap::ConvertOSMInOpenDrive );
  FileDownloader->StartDownload();

  RoadType.Empty();
  RoadMesh.Empty();
  MeshesToSpawn.Empty();
  ActorMeshList.Empty();
}

void UOpenDriveToMap::CreateTerrain( const int MeshGridSize, const float MeshGridSectionSize, const class UTexture2D* HeightmapTexture)
{
  TArray<AActor*> FoundActors;
  UGameplayStatics::GetAllActorsOfClass(GetWorld(), AProceduralMeshActor::StaticClass(), FoundActors);
  FVector BoxOrigin;
  FVector BoxExtent;
  UGameplayStatics::GetActorArrayBounds(FoundActors, false, BoxOrigin, BoxExtent);
  FVector MinBox = BoxOrigin - BoxExtent;

  int NumI = ( BoxExtent.X * 2.0f ) / MeshGridSize;
  int NumJ = ( BoxExtent.Y * 2.0f ) / MeshGridSize;
  ASceneCapture2D* SceneCapture = Cast<ASceneCapture2D>(GetWorld()->SpawnActor(ASceneCapture2D::StaticClass()));
  SceneCapture->SetActorRotation(FRotator(-90,90,0));
  SceneCapture->GetCaptureComponent2D()->ProjectionType = ECameraProjectionMode::Type::Orthographic;
  SceneCapture->GetCaptureComponent2D()->OrthoWidth = MeshGridSize;
  SceneCapture->GetCaptureComponent2D()->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
  SceneCapture->GetCaptureComponent2D()->CompositeMode = ESceneCaptureCompositeMode::SCCM_Overwrite;
  SceneCapture->GetCaptureComponent2D()->bCaptureEveryFrame = false;
  SceneCapture->GetCaptureComponent2D()->bCaptureOnMovement = false;
  //UTextureRenderTarget2D* RenderTarget = UKismetRenderingLibrary::CreateRenderTarget2D(GetWorld(), 256, 256,
  //                                                          ETextureRenderTargetFormat::RTF_RGBA8, FLinearColor(0,0,0), false );
  //SceneCapture->GetCaptureComponent2D()->TextureTarget = RenderTarget;

  /* Blueprint darfted code should be here */
  for( int i = 0; i < NumI; i++ )
  {
    for( int j = 0; j < NumJ; j++ )
    {
      // Offset that each procedural mesh is displaced to accomodate all the tiles
      FVector2D Offset( MinBox.X + i * MeshGridSize, MinBox.Y + j * MeshGridSize);
      SceneCapture->SetActorLocation(FVector(Offset.X + MeshGridSize/2, Offset.Y + MeshGridSize/2, 500));
      //SceneCapture->GetCaptureComponent2D()->CaptureScene();
      CreateTerrainMesh(i * NumJ + j, Offset, MeshGridSize, MeshGridSectionSize, HeightmapTexture, nullptr );
    }
  }
}

void UOpenDriveToMap::CreateTerrainMesh(const int MeshIndex, const FVector2D Offset, const int GridSize, const float GridSectionSize, const UTexture2D* HeightmapTexture, UTextureRenderTarget2D* RoadMask)
{
  // const float GridSectionSize = 100.0f; // In cm
  const float HeightScale = 3.0f;

  UWorld* World = GetWorld();

  // Creation of the procedural mesh
  AProceduralMeshActor* MeshActor = World->SpawnActor<AProceduralMeshActor>();
  MeshActor->SetActorLocation(FVector(Offset.X, Offset.Y, 0));
  UProceduralMeshComponent* Mesh = MeshActor->MeshComponent;

  TArray<FVector> Vertices;
  TArray<int32> Triangles;

  TArray<FVector> Normals;
  TArray<FLinearColor> Colors;
  TArray<FProcMeshTangent> Tangents;
  TArray<FVector2D> UVs;

  int VerticesInLine = (GridSize / GridSectionSize) + 1.0f;
  for( int i = 0; i < VerticesInLine; i++ )
  {
    float X = (i * GridSectionSize);
    const int RoadMapX = i * 255 / VerticesInLine;
    for( int j = 0; j < VerticesInLine; j++ )
    {
      float Y = (j * GridSectionSize);
      const int RoadMapY = j * 255 / VerticesInLine;
      const int CellIndex = RoadMapY + 255 * RoadMapX;
      float HeightValue = GetHeightForLandscape( FVector( (Offset.X + X),
                                                          (Offset.Y + Y),
                                                          0));
      Vertices.Add(FVector( X, Y, HeightValue));
    }
  }

  Normals.Init(FVector(0.0f, 0.0f, 1.0f), Vertices.Num());
  //// Triangles formation. 2 triangles per section.

  for(int i = 0; i < VerticesInLine - 1; i++)
  {
    for(int j = 0; j < VerticesInLine - 1; j++)
    {
      Triangles.Add(   j       + (   i       * VerticesInLine ) );
      Triangles.Add( ( j + 1 ) + (   i       * VerticesInLine ) );
      Triangles.Add(   j       + ( ( i + 1 ) * VerticesInLine ) );

      Triangles.Add( ( j + 1 ) + (   i       * VerticesInLine ) );
      Triangles.Add( ( j + 1 ) + ( ( i + 1 ) * VerticesInLine ) );
      Triangles.Add(   j       + ( ( i + 1 ) * VerticesInLine ) );
    }
  }

  if( DefaultLandscapeMaterial )
  {
    Mesh->SetMaterial(0, DefaultLandscapeMaterial);
  }

  Mesh->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals,
      TArray<FVector2D>(), // UV0
      TArray<FLinearColor>(), // VertexColor
      TArray<FProcMeshTangent>(), // Tangents
      true); // Create collision);

  MeshActor->SetActorLabel("SM_Landscape" + FString::FromInt(MeshIndex) );
  Landscapes.Add(MeshActor);
}

void UOpenDriveToMap::OpenFileDialog()
{
  TArray<FString> OutFileNames;
  void* ParentWindowPtr = FSlateApplication::Get().GetActiveTopLevelWindow()->GetNativeWindow()->GetOSWindowHandle();
  IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
  if (DesktopPlatform)
  {
    DesktopPlatform->OpenFileDialog(ParentWindowPtr, "Select xodr file", FPaths::ProjectDir(), FString(""), ".xodr", 1, OutFileNames);
  }
  for(FString& CurrentString : OutFileNames)
  {
    FilePath = CurrentString;
    UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT("FileObtained %s"), *CurrentString );
  }
}

void UOpenDriveToMap::LoadMap()
{
  FString FileContent;
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT("UOpenDriveToMap::LoadMap(): File to load %s"), *FilePath );
  FFileHelper::LoadFileToString(FileContent, *FilePath);
  std::string opendrive_xml = carla::rpc::FromLongFString(FileContent);
  CarlaMap = carla::opendrive::OpenDriveParser::Load(opendrive_xml);

  if (!CarlaMap.has_value())
  {
    UE_LOG(LogCarlaToolsMapGenerator, Error, TEXT("Invalid Map"));
  }
  else
  {
    UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT("Valid Map loaded"));
  }
  MapName = FPaths::GetCleanFilename(FilePath);
  MapName.RemoveFromEnd(".xodr", ESearchCase::Type::IgnoreCase);
  UE_LOG(LogCarlaToolsMapGenerator, Warning, TEXT("MapName %s"), *MapName);

  GenerateAll(CarlaMap);
  GenerationFinished();
}

TArray<AActor*> UOpenDriveToMap::GenerateMiscActors(float Offset)
{
  std::vector<std::pair<carla::geom::Transform, std::string>>
    Locations = CarlaMap->GetTreesTransform(DistanceBetweenTrees, DistanceFromRoadEdge, Offset);
  TArray<AActor*> Returning;
  int i = 0;
  for (auto& cl : Locations)
  {
    const FVector scale{ 1.0f, 1.0f, 1.0f };
    cl.first.location.z = GetHeight(cl.first.location.x, cl.first.location.y) + 0.3f;
    FTransform NewTransform ( FRotator(cl.first.rotation), FVector(cl.first.location), scale );

    NewTransform = GetSnappedPosition(NewTransform);

    AActor* Spawner = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
      NewTransform.GetLocation(), NewTransform.Rotator());
    Spawner->Tags.Add(FName("MiscSpawnPosition"));
    Spawner->Tags.Add(FName(cl.second.c_str()));
    Spawner->SetActorLabel("MiscSpawnPosition" + FString::FromInt(i));
    ++i;
    Returning.Add(Spawner);
  }
  return Returning;
}
void UOpenDriveToMap::GenerateAll(const boost::optional<carla::road::Map>& CarlaMap )
{
  if (!CarlaMap.has_value())
  {
    UE_LOG(LogCarlaToolsMapGenerator, Error, TEXT("Invalid Map"));
  }else
  {
    if(DefaultHeightmap && !Heightmap){
      Heightmap = DefaultHeightmap;
    }

    GenerateRoadMesh(CarlaMap);
    GenerateLaneMarks(CarlaMap);
    GenerateSpawnPoints(CarlaMap);
    CreateTerrain(12800, 256, nullptr);
    GenerateTreePositions(CarlaMap);
  }
}

void UOpenDriveToMap::GenerateRoadMesh( const boost::optional<carla::road::Map>& CarlaMap )
{
  opg_parameters.vertex_distance = 0.5f;
  opg_parameters.vertex_width_resolution = 8.0f;
  opg_parameters.simplification_percentage = 50.0f;

  double start = FPlatformTime::Seconds();
  const auto Meshes = CarlaMap->GenerateOrderedChunkedMesh(opg_parameters);
  double end = FPlatformTime::Seconds();
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" GenerateOrderedChunkedMesh code executed in %f seconds. Simplification percentage is %f"), end - start, opg_parameters.simplification_percentage);

  start = FPlatformTime::Seconds();
  int index = 0;
  for (const auto &PairMap : Meshes)
  {
    for( auto& Mesh : PairMap.second )
    {
      if (!Mesh->GetVertices().size())
      {
        continue;
      }
      if (!Mesh->IsValid()) {
        continue;
      }

      if(PairMap.first == carla::road::Lane::LaneType::Driving)
      {
        for( auto& Vertex : Mesh->GetVertices() )
        {
          FVector VertexFVector = Vertex.ToFVector();
          Vertex.z += GetHeight(Vertex.x, Vertex.y, DistanceToLaneBorder(CarlaMap,VertexFVector) > 65.0f );
        }
        carla::geom::Simplification Simplify(0.15);
        Simplify.Simplificate(Mesh);
      }else{
        for( auto& Vertex : Mesh->GetVertices() )
        {
          Vertex.z += GetHeight(Vertex.x, Vertex.y, false) + 0.10;
        }
      }

      AProceduralMeshActor* TempActor = GetWorld()->SpawnActor<AProceduralMeshActor>();

      TempActor->SetActorLabel(FString("SM_Lane_") + FString::FromInt(index));

      UProceduralMeshComponent *TempPMC = TempActor->MeshComponent;
      TempPMC->bUseAsyncCooking = true;
      TempPMC->bUseComplexAsSimpleCollision = true;
      TempPMC->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);

      if(DefaultRoadMaterial && PairMap.first == carla::road::Lane::LaneType::Driving)
      {
        TempPMC->SetMaterial(0, DefaultRoadMaterial);
        TempActor->SetActorLabel(FString("SM_DrivingLane_") + FString::FromInt(index));
      }
      if(DefaultSidewalksMaterial && PairMap.first == carla::road::Lane::LaneType::Sidewalk)
      {
        TempPMC->SetMaterial(0, DefaultSidewalksMaterial);
        TempActor->SetActorLabel(FString("SM_Sidewalk_") + FString::FromInt(index));
      }
      FVector MeshCentroid = FVector(0,0,0);
      for( auto Vertex : Mesh->GetVertices() )
      {
        MeshCentroid += Vertex.ToFVector();
      }

      MeshCentroid /= Mesh->GetVertices().size();

      for( auto& Vertex : Mesh->GetVertices() )
      {
       Vertex.x -= MeshCentroid.X;
       Vertex.y -= MeshCentroid.Y;
       Vertex.z -= MeshCentroid.Z;
      }

      const FProceduralCustomMesh MeshData = *Mesh;
      TArray<FVector> Normals;
      TArray<FProcMeshTangent> Tangents;

      UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
        MeshData.Vertices,
        MeshData.Triangles,
        MeshData.UV0,
        Normals,
        Tangents
      );

      TempPMC->CreateMeshSection_LinearColor(
          0,
          MeshData.Vertices,
          MeshData.Triangles,
          MeshData.Normals,
          MeshData.UV0, // UV0
          TArray<FLinearColor>(), // VertexColor
          Tangents, // Tangents
          true); // Create collision
      TempActor->SetActorLocation(MeshCentroid * 100);
      // ActorMeshList.Add(TempActor);

      RoadType.Add(LaneTypeToFString(PairMap.first));
      // RoadMesh.Add(TempPMC);
      index++;
    }
  }

  end = FPlatformTime::Seconds();
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT("Mesh spawnning and translation code executed in %f seconds."), end - start);
}

void UOpenDriveToMap::GenerateLaneMarks(const boost::optional<carla::road::Map>& CarlaMap)
{
  opg_parameters.vertex_distance = 0.5f;
  opg_parameters.vertex_width_resolution = 8.0f;
  opg_parameters.simplification_percentage = 15.0f;
  std::vector<std::string> lanemarkinfo;
  auto MarkingMeshes = CarlaMap->GenerateLineMarkings(opg_parameters, lanemarkinfo);

  int index = 0;
  for (const auto& Mesh : MarkingMeshes)
  {
    if ( !Mesh->GetVertices().size() )
    {
      index++;
      continue;
    }
    if ( !Mesh->IsValid() ) {
      index++;
      continue;
    }

    FVector MeshCentroid = FVector(0, 0, 0);
    for (auto& Vertex : Mesh->GetVertices())
    {
      FVector VertexFVector = Vertex.ToFVector();
      Vertex.z += GetHeight(Vertex.x, Vertex.y, DistanceToLaneBorder(CarlaMap,VertexFVector) > 65.0f ) + 0.0001f;
      MeshCentroid += Vertex.ToFVector();
    }

    MeshCentroid /= Mesh->GetVertices().size();

    for (auto& Vertex : Mesh->GetVertices())
    {
      Vertex.x -= MeshCentroid.X;
      Vertex.y -= MeshCentroid.Y;
      Vertex.z -= MeshCentroid.Z;
    }

    // TODO: Improve this code
    float MinDistance = 99999999.9f;
    for(auto SpawnedActor : LaneMarkerActorList)
    {
      float VectorDistance = FVector::Distance(MeshCentroid*100, SpawnedActor->GetActorLocation());
      if(VectorDistance < MinDistance)
      {
        MinDistance = VectorDistance;
      }
    }

    if(MinDistance < 250)
    {
      UE_LOG(LogCarlaToolsMapGenerator, Warning, TEXT("Skkipped is %f."), MinDistance);
      index++;
      continue;
    }

    AProceduralMeshActor* TempActor = GetWorld()->SpawnActor<AProceduralMeshActor>();
    TempActor->SetActorLabel(FString("SM_LaneMark_") + FString::FromInt(index));
    UProceduralMeshComponent* TempPMC = TempActor->MeshComponent;
    TempPMC->bUseAsyncCooking = true;
    TempPMC->bUseComplexAsSimpleCollision = true;
    TempPMC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    TempPMC->CastShadow = false;
    if (lanemarkinfo[index].find("yellow") != std::string::npos) {
      if(DefaultLaneMarksYellowMaterial)
        TempPMC->SetMaterial(0, DefaultLaneMarksYellowMaterial);
    }else{
      if(DefaultLaneMarksWhiteMaterial)
        TempPMC->SetMaterial(0, DefaultLaneMarksWhiteMaterial);

    }

    const FProceduralCustomMesh MeshData = *Mesh;
    TArray<FVector> Normals;
    TArray<FProcMeshTangent> Tangents;
    UKismetProceduralMeshLibrary::CalculateTangentsForMesh(
      MeshData.Vertices,
      MeshData.Triangles,
      MeshData.UV0,
      Normals,
      Tangents
    );
    TempPMC->CreateMeshSection_LinearColor(
      0,
      MeshData.Vertices,
      MeshData.Triangles,
      Normals,
      MeshData.UV0, // UV0
      TArray<FLinearColor>(), // VertexColor
      Tangents, // Tangents
      true); // Create collision
    TempActor->SetActorLocation(MeshCentroid * 100);
    TempActor->Tags.Add(*FString(lanemarkinfo[index].c_str()));
    LaneMarkerActorList.Add(TempActor);
    index++;
  }
}

void UOpenDriveToMap::GenerateSpawnPoints( const boost::optional<carla::road::Map>& CarlaMap )
{
  float SpawnersHeight = 300.f;
  const auto Waypoints = CarlaMap->GenerateWaypointsOnRoadEntries();
  for (const auto &Wp : Waypoints)
  {
    const FTransform Trans = CarlaMap->ComputeTransform(Wp);
    AVehicleSpawnPoint *Spawner = GetWorld()->SpawnActor<AVehicleSpawnPoint>();
    Spawner->SetActorRotation(Trans.GetRotation());
    Spawner->SetActorLocation(Trans.GetTranslation() + FVector(0.f, 0.f, SpawnersHeight));
  }
}

void UOpenDriveToMap::GenerateTreePositions( const boost::optional<carla::road::Map>& CarlaMap )
{
  std::vector<std::pair<carla::geom::Transform, std::string>> Locations =
    CarlaMap->GetTreesTransform(DistanceBetweenTrees, DistanceFromRoadEdge );
  int i = 0;
  for (auto &cl : Locations)
  {
    const FVector scale{ 1.0f, 1.0f, 1.0f };
    cl.first.location.z  = GetHeight(cl.first.location.x, cl.first.location.y) + 0.3f;
    FTransform NewTransform ( FRotator(cl.first.rotation), FVector(cl.first.location), scale );
    NewTransform = GetSnappedPosition(NewTransform);

    AActor* Spawner = GetWorld()->SpawnActor<AStaticMeshActor>(AStaticMeshActor::StaticClass(),
      NewTransform.GetLocation(), NewTransform.Rotator());

    Spawner->Tags.Add(FName("TreeSpawnPosition"));
    Spawner->Tags.Add(FName(cl.second.c_str()));
    Spawner->SetActorLabel("TreeSpawnPosition" + FString::FromInt(i) );
    ++i;
  }
}
UStaticMesh* UOpenDriveToMap::CreateStaticMeshAsset( UProceduralMeshComponent* ProcMeshComp, int32 MeshIndex, FString FolderName )
{
  FMeshDescription MeshDescription = BuildMeshDescription(ProcMeshComp);

  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

  // If we got some valid data.
  if (MeshDescription.Polygons().Num() > 0)
  {
    FString MeshName = *(FolderName + FString::FromInt(MeshIndex) );
    FString PackageName = "/Game/CustomMaps/" + MapName + "/Static/" + FolderName + "/" + MeshName;

    if( !PlatformFile.DirectoryExists(*PackageName) )
    {
      PlatformFile.CreateDirectory(*PackageName);
    }

    // Then find/create it.
    UPackage* Package = CreatePackage(*PackageName);
    check(Package);
    // Create StaticMesh object
    UStaticMesh* StaticMesh = NewObject<UStaticMesh>(Package, *MeshName, RF_Public | RF_Standalone);
    StaticMesh->InitResources();

    StaticMesh->LightingGuid = FGuid::NewGuid();

    // Add source to new StaticMesh
    FStaticMeshSourceModel& SrcModel = StaticMesh->AddSourceModel();
    SrcModel.BuildSettings.bRecomputeNormals = false;
    SrcModel.BuildSettings.bRecomputeTangents = false;
    SrcModel.BuildSettings.bRemoveDegenerates = false;
    SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
    SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
    SrcModel.BuildSettings.bGenerateLightmapUVs = true;
    SrcModel.BuildSettings.SrcLightmapIndex = 0;
    SrcModel.BuildSettings.DstLightmapIndex = 1;
    SrcModel.BuildSettings.DistanceFieldResolutionScale = 0;
    StaticMesh->CreateMeshDescription(0, MoveTemp(MeshDescription));
    StaticMesh->CommitMeshDescription(0);

    //// SIMPLE COLLISION
    if (!ProcMeshComp->bUseComplexAsSimpleCollision )
    {
      StaticMesh->CreateBodySetup();
      UBodySetup* NewBodySetup = StaticMesh->BodySetup;
      NewBodySetup->BodySetupGuid = FGuid::NewGuid();
      NewBodySetup->AggGeom.ConvexElems = ProcMeshComp->ProcMeshBodySetup->AggGeom.ConvexElems;
      NewBodySetup->bGenerateMirroredCollision = false;
      NewBodySetup->bDoubleSidedGeometry = true;
      NewBodySetup->CollisionTraceFlag = CTF_UseDefault;
      NewBodySetup->CreatePhysicsMeshes();
    }

    //// MATERIALS
    TSet<UMaterialInterface*> UniqueMaterials;
    const int32 NumSections = ProcMeshComp->GetNumSections();
    for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
    {
      FProcMeshSection *ProcSection =
        ProcMeshComp->GetProcMeshSection(SectionIdx);
      UMaterialInterface *Material = ProcMeshComp->GetMaterial(SectionIdx);
      UniqueMaterials.Add(Material);
    }
    // Copy materials to new mesh
    for (auto* Material : UniqueMaterials)
    {
      StaticMesh->StaticMaterials.Add(FStaticMaterial(Material));
    }

    //Set the Imported version before calling the build
    StaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
    StaticMesh->Build(false);
    StaticMesh->PostEditChange();

    // Notify asset registry of new asset
    FAssetRegistryModule::AssetCreated(StaticMesh);
    UPackage::SavePackage(Package, StaticMesh, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *MeshName, GError, nullptr, true, true, SAVE_NoError);
    return StaticMesh;
  }
  return nullptr;
}

TArray<UStaticMesh*> UOpenDriveToMap::CreateStaticMeshAssets()
{
  double start = FPlatformTime::Seconds();
  double end = FPlatformTime::Seconds();

  IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
  TArray<UStaticMesh*> StaticMeshes;

  double BuildMeshDescriptionTime = 0.0f;
  double PackgaesCreatingTime = 0.0f;
  double MeshInitTime = 0.0f;
  double MatAndCollInitTime = 0.0f;
  double MeshBuildTime = 0.0f;
  double PackSaveTime = 0.0f;


  for (int i = 0; i < RoadMesh.Num(); ++i)
  {
    FString MeshName = RoadType[i] + FString::FromInt(i);
    FString PackageName = "/Game/CustomMaps/" + MapName + "/Static/" + RoadType[i] + "/" + MeshName;

    if (!PlatformFile.DirectoryExists(*PackageName))
    {
      PlatformFile.CreateDirectory(*PackageName);
    }

    UProceduralMeshComponent* ProcMeshComp = RoadMesh[i];
    start = FPlatformTime::Seconds();
    FMeshDescription MeshDescription = BuildMeshDescription(ProcMeshComp);
    end = FPlatformTime::Seconds();
    BuildMeshDescriptionTime += end - start;
    // If we got some valid data.
    if (MeshDescription.Polygons().Num() > 0)
    {
      start = FPlatformTime::Seconds();
      // Then find/create it.
      UPackage* Package = CreatePackage(*PackageName);
      check(Package);
      end = FPlatformTime::Seconds();
      PackgaesCreatingTime += end - start;

      start = FPlatformTime::Seconds();

      // Create StaticMesh object
      UStaticMesh* CurrentStaticMesh = NewObject<UStaticMesh>(Package, *MeshName, RF_Public | RF_Standalone);
      CurrentStaticMesh->InitResources();

      CurrentStaticMesh->LightingGuid = FGuid::NewGuid();

      // Add source to new StaticMesh
      FStaticMeshSourceModel& SrcModel = CurrentStaticMesh->AddSourceModel();
      SrcModel.BuildSettings.bRecomputeNormals = false;
      SrcModel.BuildSettings.bRecomputeTangents = false;
      SrcModel.BuildSettings.bRemoveDegenerates = false;
      SrcModel.BuildSettings.bUseHighPrecisionTangentBasis = false;
      SrcModel.BuildSettings.bUseFullPrecisionUVs = false;
      SrcModel.BuildSettings.bGenerateLightmapUVs = true;
      SrcModel.BuildSettings.SrcLightmapIndex = 0;
      SrcModel.BuildSettings.DstLightmapIndex = 1;
      CurrentStaticMesh->CreateMeshDescription(0, MoveTemp(MeshDescription));
      CurrentStaticMesh->CommitMeshDescription(0);
      end = FPlatformTime::Seconds();
      MeshInitTime += end - start;
      start = FPlatformTime::Seconds();

      //// SIMPLE COLLISION
      if (!ProcMeshComp->bUseComplexAsSimpleCollision)
      {
        CurrentStaticMesh->CreateBodySetup();
        UBodySetup* NewBodySetup = CurrentStaticMesh->BodySetup;
        NewBodySetup->BodySetupGuid = FGuid::NewGuid();
        NewBodySetup->AggGeom.ConvexElems = ProcMeshComp->ProcMeshBodySetup->AggGeom.ConvexElems;
        NewBodySetup->bGenerateMirroredCollision = false;
        NewBodySetup->bDoubleSidedGeometry = true;
        NewBodySetup->CollisionTraceFlag = CTF_UseDefault;
        NewBodySetup->CreatePhysicsMeshes();
      }

      //// MATERIALS
      TSet<UMaterialInterface*> UniqueMaterials;
      const int32 NumSections = ProcMeshComp->GetNumSections();
      for (int32 SectionIdx = 0; SectionIdx < NumSections; SectionIdx++)
      {
        FProcMeshSection* ProcSection =
          ProcMeshComp->GetProcMeshSection(SectionIdx);
        UMaterialInterface* Material = ProcMeshComp->GetMaterial(SectionIdx);
        UniqueMaterials.Add(Material);
      }
      // Copy materials to new mesh
      for (auto* Material : UniqueMaterials)
      {
        CurrentStaticMesh->StaticMaterials.Add(FStaticMaterial(Material));
      }

      end = FPlatformTime::Seconds();
      MatAndCollInitTime += end - start;
      start = FPlatformTime::Seconds();
      //Set the Imported version before calling the build
      CurrentStaticMesh->ImportVersion = EImportStaticMeshVersion::LastVersion;
      CurrentStaticMesh->Build(false);
      CurrentStaticMesh->PostEditChange();

      end = FPlatformTime::Seconds();
      MeshBuildTime += end - start;
      start = FPlatformTime::Seconds();

      FString RoadName = *(RoadType[i] + FString::FromInt(i));
      // Notify asset registry of new asset
      FAssetRegistryModule::AssetCreated(CurrentStaticMesh);
      UPackage::SavePackage(Package, CurrentStaticMesh, EObjectFlags::RF_Public | EObjectFlags::RF_Standalone, *RoadName, GError, nullptr, true, true, SAVE_NoError);
      end = FPlatformTime::Seconds();
      PackSaveTime += end - start;


      StaticMeshes.Add( CurrentStaticMesh );
    }
  }
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" UOpenDriveToMap::CreateStaticMeshAssets total time in BuildMeshDescriptionTime %f. Time per mesh %f"), BuildMeshDescriptionTime, BuildMeshDescriptionTime/ RoadMesh.Num());
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" UOpenDriveToMap::CreateStaticMeshAssets total time in PackgaesCreatingTime %f. Time per mesh %f"), PackgaesCreatingTime, PackgaesCreatingTime / RoadMesh.Num());
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" UOpenDriveToMap::CreateStaticMeshAssets total time in MeshInitTime %f. Time per mesh %f"), MeshInitTime, MeshInitTime / RoadMesh.Num());
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" UOpenDriveToMap::CreateStaticMeshAssets total time in MatAndCollInitTime %f. Time per mesh %f"), MatAndCollInitTime, MatAndCollInitTime / RoadMesh.Num());
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" UOpenDriveToMap::CreateStaticMeshAssets total time in MeshBuildTime %f. Time per mesh %f"), MeshBuildTime, MeshBuildTime / RoadMesh.Num());
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" UOpenDriveToMap::CreateStaticMeshAssets total time in PackSaveTime %f. Time per mesh %f"), PackSaveTime, PackSaveTime / RoadMesh.Num());
  return StaticMeshes;
}

void UOpenDriveToMap::SaveMap()
{
  double start = FPlatformTime::Seconds();

  MeshesToSpawn = CreateStaticMeshAssets();

  double end = FPlatformTime::Seconds();
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" Meshes created static mesh code executed in %f seconds."), end - start);

  start = FPlatformTime::Seconds();

  for (int i = 0; i < MeshesToSpawn.Num(); ++i)
  {
    AStaticMeshActor* TempActor = GetWorld()->SpawnActor<AStaticMeshActor>();
    // Build mesh from source
    TempActor->GetStaticMeshComponent()->SetStaticMesh(MeshesToSpawn[i]);
    TempActor->SetActorLabel(FString("SM_") + MeshesToSpawn[i]->GetName());
    TempActor->SetActorTransform(ActorMeshList[i]->GetActorTransform());
  }

  for (auto CurrentActor : ActorMeshList)
  {
    CurrentActor->Destroy();
  }

  end = FPlatformTime::Seconds();
  UE_LOG(LogCarlaToolsMapGenerator, Log, TEXT(" Spawning Static Meshes code executed in %f seconds."), end - start);
}

float UOpenDriveToMap::GetHeight(float PosX, float PosY, bool bDrivingLane){
  if( bDrivingLane ){
    return carla::geom::deformation::GetZPosInDeformation(PosX, PosY) -
      carla::geom::deformation::GetBumpDeformation(PosX,PosY);
  }else{
    return carla::geom::deformation::GetZPosInDeformation(PosX, PosY) +
      (carla::geom::deformation::GetZPosInDeformation(PosX, PosY) * -0.15f);
  }
}

FTransform UOpenDriveToMap::GetSnappedPosition( FTransform Origin ){
  FTransform ToReturn = Origin;
  FVector Start = Origin.GetLocation() + FVector( 0, 0, 1000);
  FVector End = Origin.GetLocation() - FVector( 0, 0, 1000);
  FHitResult HitResult;
  FCollisionQueryParams CollisionQuery;
  FCollisionResponseParams CollisionParams;

  if( GetWorld()->LineTraceSingleByChannel(
    HitResult,
    Start,
    End,
    ECollisionChannel::ECC_WorldStatic,
    CollisionQuery,
    CollisionParams
  ) )
  {
    ToReturn.SetLocation(HitResult.Location);
  }
  return ToReturn;
}

float UOpenDriveToMap::GetHeightForLandscape( FVector Origin ){
  FVector Start = Origin + FVector( 0, 0, 10000);
  FVector End = Origin - FVector( 0, 0, 10000);
  FHitResult HitResult;
  FCollisionQueryParams CollisionQuery;
  CollisionQuery.AddIgnoredActors(Landscapes);
  FCollisionResponseParams CollisionParams;

  if( GetWorld()->LineTraceSingleByChannel(
    HitResult,
    Start,
    End,
    ECollisionChannel::ECC_WorldStatic,
    CollisionQuery,
    CollisionParams
  ) )
  {
    return GetHeight(Origin.X * 0.01f, Origin.Y * 0.01f, true) * 100.0f - 25.0f;
  }else{
    return GetHeight(Origin.X * 0.01f, Origin.Y * 0.01f, true) * 100.0f;
  }
  return 0.0f;
}

float UOpenDriveToMap::DistanceToLaneBorder(const boost::optional<carla::road::Map>& CarlaMap,
        FVector &location, int32_t lane_type ) const
{
  carla::geom::Location cl(location);
  //wp = GetClosestWaypoint(pos). if distance wp - pos == lane_width --> estas al borde de la carretera
  auto wp = CarlaMap->GetClosestWaypointOnRoad(cl, lane_type);
  if(wp)
  {
    carla::geom::Transform ct = CarlaMap->ComputeTransform(*wp);
    double LaneWidth = CarlaMap->GetLaneWidth(*wp);
    return cl.Distance(ct.location) - LaneWidth;
  }
  return 100000.0f;
}

