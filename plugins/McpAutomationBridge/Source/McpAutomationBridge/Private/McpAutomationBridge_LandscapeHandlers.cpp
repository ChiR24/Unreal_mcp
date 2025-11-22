#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"

#if WITH_EDITOR
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "LandscapeInfo.h"
#include "LandscapeStreamingProxy.h"
#include "LandscapeComponent.h"
#include "LandscapeEditorObject.h"
#include "LandscapeEditorUtils.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeGrassType.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Engine/World.h"
#include "EditorAssetLibrary.h"
#include "UObject/SavePackage.h"
#include "Async/Async.h"
#include "Misc/ScopedSlowTask.h"
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleEditLandscape(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    // Dispatch to specific edit operations implemented below
    if (HandleModifyHeightmap(RequestId, Action, Payload, RequestingSocket)) return true;
    if (HandlePaintLandscapeLayer(RequestId, Action, Payload, RequestingSocket)) return true;
    return false;
}

bool UMcpAutomationBridgeSubsystem::HandleCreateLandscape(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_landscape"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("create_landscape payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    // Parse inputs (accept multiple shapes)
    double X = 0.0, Y = 0.0, Z = 0.0;
    if (!Payload->TryGetNumberField(TEXT("x"), X) || !Payload->TryGetNumberField(TEXT("y"), Y) || !Payload->TryGetNumberField(TEXT("z"), Z))
    {
        // Try location object { x, y, z }
        const TSharedPtr<FJsonObject>* LocObj = nullptr;
        if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
        {
            (*LocObj)->TryGetNumberField(TEXT("x"), X);
            (*LocObj)->TryGetNumberField(TEXT("y"), Y);
            (*LocObj)->TryGetNumberField(TEXT("z"), Z);
        }
        else
        {
            // Try location as array [x,y,z]
            const TArray<TSharedPtr<FJsonValue>>* LocArr = nullptr;
            if (Payload->TryGetArrayField(TEXT("location"), LocArr) && LocArr && LocArr->Num() >= 3)
            {
                X = (*LocArr)[0]->AsNumber();
                Y = (*LocArr)[1]->AsNumber();
                Z = (*LocArr)[2]->AsNumber();
            }
        }
    }
    
    int32 ComponentsX = 8, ComponentsY = 8;
    bool bHasCX = Payload->TryGetNumberField(TEXT("componentsX"), ComponentsX);
    bool bHasCY = Payload->TryGetNumberField(TEXT("componentsY"), ComponentsY);

    int32 ComponentCount = 0;
    Payload->TryGetNumberField(TEXT("componentCount"), ComponentCount);
    if (!bHasCX && ComponentCount > 0) { ComponentsX = ComponentCount; }
    if (!bHasCY && ComponentCount > 0) { ComponentsY = ComponentCount; }

    // If sizeX/sizeY provided (world units), derive a coarse components estimate
    double SizeXUnits = 0.0, SizeYUnits = 0.0;
    if (Payload->TryGetNumberField(TEXT("sizeX"), SizeXUnits) && SizeXUnits > 0 && !bHasCX)
    {
        ComponentsX = FMath::Max(1, static_cast<int32>(FMath::Floor(SizeXUnits / 1000.0)));
    }
    if (Payload->TryGetNumberField(TEXT("sizeY"), SizeYUnits) && SizeYUnits > 0 && !bHasCY)
    {
        ComponentsY = FMath::Max(1, static_cast<int32>(FMath::Floor(SizeYUnits / 1000.0)));
    }
    
    int32 QuadsPerComponent = 63;
    if (!Payload->TryGetNumberField(TEXT("quadsPerComponent"), QuadsPerComponent))
    {
        // Accept quadsPerSection synonym from some clients
        Payload->TryGetNumberField(TEXT("quadsPerSection"), QuadsPerComponent);
    }
    
    int32 SectionsPerComponent = 1;
    Payload->TryGetNumberField(TEXT("sectionsPerComponent"), SectionsPerComponent);
    
    FString MaterialPath;
    Payload->TryGetStringField(TEXT("materialPath"), MaterialPath);

    if (!GEditor || !GEditor->GetEditorWorldContext().World())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Editor world not available"), TEXT("EDITOR_NOT_AVAILABLE"));
        return true;
    }

    UWorld* World = GEditor->GetEditorWorldContext().World();
    FVector Location(X, Y, Z);

    if (ComponentsX < 1 || ComponentsX > 32 || ComponentsY < 1 || ComponentsY > 32)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("ComponentsX/Y must be between 1 and 32"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    if (QuadsPerComponent != 7 && QuadsPerComponent != 15 && QuadsPerComponent != 31 && QuadsPerComponent != 63 && QuadsPerComponent != 127 && QuadsPerComponent != 255)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("QuadsPerComponent must be 7, 15, 31, 63, 127, or 255"), TEXT("INVALID_ARGUMENT"));
        return true;
    }





    ALandscape* Landscape = World->SpawnActor<ALandscape>(ALandscape::StaticClass(), Location, FRotator::ZeroRotator);
    if (!Landscape)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to spawn landscape actor"), TEXT("SPAWN_FAILED"));
        return true;
    }

    Landscape->SetActorLabel(FString::Printf(TEXT("Landscape_%dx%d"), ComponentsX, ComponentsY));
    Landscape->ComponentSizeQuads = QuadsPerComponent;
    Landscape->SubsectionSizeQuads = QuadsPerComponent / SectionsPerComponent;
    Landscape->NumSubsections = SectionsPerComponent;

    if (!MaterialPath.IsEmpty())
    {
        UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
        if (Mat)
        {
            Landscape->LandscapeMaterial = Mat;
        }
    }



    const int32 VertX = ComponentsX * QuadsPerComponent + 1;
    const int32 VertY = ComponentsY * QuadsPerComponent + 1;

    TArray<uint16> HeightArray;
    HeightArray.SetNumUninitialized(VertX * VertY);
    for (int32 YIdx = 0; YIdx < VertY; ++YIdx)
    {
        for (int32 XIdx = 0; XIdx < VertX; ++XIdx)
        {
            HeightArray[YIdx * VertX + XIdx] = 32768;
        }
    }

    const int32 InMinX = 0;
    const int32 InMinY = 0;
    const int32 InMaxX = ComponentsX * QuadsPerComponent;
    const int32 InMaxY = ComponentsY * QuadsPerComponent;
    const int32 NumSubsections = SectionsPerComponent;
    const int32 SubsectionSizeQuads = QuadsPerComponent / FMath::Max(1, SectionsPerComponent);

    FGuid HeightmapGuid = FGuid::NewGuid();
    TMap<FGuid, TArray<uint16>> ImportHeightData;
    ImportHeightData.Add(HeightmapGuid, MoveTemp(HeightArray));

    TMap<FGuid, TArray<FLandscapeImportLayerInfo>> ImportLayerInfos;
    TArray<FLandscapeLayer> EditLayers;

    Landscape->Import(
        HeightmapGuid,
        InMinX, InMinY,
        InMaxX, InMaxY,
        NumSubsections,
        SubsectionSizeQuads,
        ImportHeightData,
        nullptr,
        ImportLayerInfos,
        ELandscapeImportAlphamapType::Additive,
        TArrayView<const FLandscapeLayer>(EditLayers)
    );

    // Rely on PostEditChange to update components; avoid direct collision rebuild to reduce crash risk
    Landscape->PostEditChange();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPathName());
    Resp->SetStringField(TEXT("actorLabel"), Landscape->GetActorLabel());
    Resp->SetNumberField(TEXT("componentsX"), ComponentsX);
    Resp->SetNumberField(TEXT("componentsY"), ComponentsY);
    Resp->SetNumberField(TEXT("quadsPerComponent"), QuadsPerComponent);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Landscape created successfully"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_landscape requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleModifyHeightmap(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("modify_heightmap"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("modify_heightmap payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString LandscapePath;
    Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
    FString LandscapeName;
    Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

    const TArray<TSharedPtr<FJsonValue>>* HeightDataArray = nullptr;
    if (!Payload->TryGetArrayField(TEXT("heightData"), HeightDataArray) || !HeightDataArray || HeightDataArray->Num() == 0)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("heightData array required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Copy height data for async task
    TArray<uint16> HeightValues;
    for (const TSharedPtr<FJsonValue>& Val : *HeightDataArray)
    {
        if (Val.IsValid() && Val->Type == EJson::Number)
        {
            HeightValues.Add(static_cast<uint16>(FMath::Clamp(Val->AsNumber(), 0.0, 65535.0)));
        }
    }

    ALandscape* Landscape = nullptr;
    if (!LandscapePath.IsEmpty())
    {
        Landscape = Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape && !LandscapeName.IsEmpty())
    {
        if (!GEditor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
        {
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors)
            {
                if (A && A->IsA<ALandscape>() && A->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
                {
                    Landscape = Cast<ALandscape>(A);
                    break;
                }
            }
        }
    }
    if (!Landscape)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to find landscape"), TEXT("LOAD_FAILED"));
        return true;
    }

    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Landscape has no info"), TEXT("INVALID_LANDSCAPE"));
        return true;
    }

    FScopedSlowTask SlowTask(2.0f, FText::FromString(TEXT("Modifying heightmap...")));
    SlowTask.MakeDialog();

    int32 MinX, MinY, MaxX, MaxY;
    if (!LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to get landscape extent"), TEXT("INVALID_LANDSCAPE"));
        return true;
    }

    SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Writing heightmap data")));

    const int32 SizeX = (MaxX - MinX + 1);
    const int32 SizeY = (MaxY - MinY + 1);

    if (HeightValues.Num() != SizeX * SizeY)
    {
        SendAutomationError(
            RequestingSocket,
            RequestId,
            FString::Printf(TEXT("Height data size mismatch. Expected %d x %d = %d values, got %d"), SizeX, SizeY, SizeX * SizeY, HeightValues.Num()),
            TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
    LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightValues.GetData(), SizeX, true);

    SlowTask.EnterProgressFrame(1.0f, FText::FromString(TEXT("Rebuilding collision")));
    LandscapeEdit.Flush();
    // Avoid explicit collision rebuild here; PostEditChange is sufficient for editor updates
    Landscape->PostEditChange();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), LandscapePath);
    Resp->SetNumberField(TEXT("modifiedVertices"), HeightValues.Num());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Heightmap modified successfully"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("modify_heightmap requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandlePaintLandscapeLayer(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("paint_landscape_layer"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("paint_landscape_layer payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString LandscapePath; Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
    FString LandscapeName; Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);

    FString LayerName;
    if (!Payload->TryGetStringField(TEXT("layerName"), LayerName) || LayerName.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("layerName required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    // Paint region (optional - if not specified, paint entire landscape)
    int32 MinX = -1, MinY = -1, MaxX = -1, MaxY = -1;
    const TSharedPtr<FJsonObject>* RegionObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("region"), RegionObj) && RegionObj)
    {
        (*RegionObj)->TryGetNumberField(TEXT("minX"), MinX);
        (*RegionObj)->TryGetNumberField(TEXT("minY"), MinY);
        (*RegionObj)->TryGetNumberField(TEXT("maxX"), MaxX);
        (*RegionObj)->TryGetNumberField(TEXT("maxY"), MaxY);
    }

    double Strength = 1.0;
    Payload->TryGetNumberField(TEXT("strength"), Strength);
    Strength = FMath::Clamp(Strength, 0.0, 1.0);

    ALandscape* Landscape = nullptr;
    if (!LandscapePath.IsEmpty())
    {
        Landscape = Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape && !LandscapeName.IsEmpty())
    {
        if (!GEditor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
        {
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors)
            {
                if (A && A->IsA<ALandscape>() && A->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
                {
                    Landscape = Cast<ALandscape>(A);
                    break;
                }
            }
        }
    }
    if (!Landscape)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to find landscape"), TEXT("LOAD_FAILED"));
        return true;
    }

    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Landscape has no info"), TEXT("INVALID_LANDSCAPE"));
        return true;
    }

    ULandscapeLayerInfoObject* LayerInfo = nullptr;
    for (const FLandscapeInfoLayerSettings& Layer : LandscapeInfo->Layers)
    {
        if (Layer.LayerName == FName(*LayerName))
        {
            LayerInfo = Layer.LayerInfoObj;
            break;
        }
    }

    if (!LayerInfo)
    {
        SendAutomationError(
            RequestingSocket,
            RequestId,
            FString::Printf(TEXT("Layer '%s' not found. Create layer first using landscape editor."), *LayerName),
            TEXT("LAYER_NOT_FOUND"));
        return true;
    }

    FScopedSlowTask SlowTask(1.0f, FText::FromString(TEXT("Painting landscape layer...")));
    SlowTask.MakeDialog();

    int32 PaintMinX = MinX;
    int32 PaintMinY = MinY;
    int32 PaintMaxX = MaxX;
    int32 PaintMaxY = MaxY;
    if (PaintMinX < 0 || PaintMaxX < 0)
    {
        LandscapeInfo->GetLandscapeExtent(PaintMinX, PaintMinY, PaintMaxX, PaintMaxY);
    }

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
    const uint8 PaintValue = static_cast<uint8>(Strength * 255.0);
    const int32 RegionSizeX = (PaintMaxX - PaintMinX + 1);
    const int32 RegionSizeY = (PaintMaxY - PaintMinY + 1);

    TArray<uint8> AlphaData;
    AlphaData.Init(PaintValue, RegionSizeX * RegionSizeY);

    LandscapeEdit.SetAlphaData(LayerInfo, PaintMinX, PaintMinY, PaintMaxX, PaintMaxY, AlphaData.GetData(), RegionSizeX);
    LandscapeEdit.Flush();
    Landscape->PostEditChange();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), LandscapePath);
    Resp->SetStringField(TEXT("layerName"), LayerName);
    Resp->SetNumberField(TEXT("strength"), Strength);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Layer painted successfully"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("paint_landscape_layer requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSculptLandscape(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("sculpt_landscape"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("sculpt_landscape payload missing"), TEXT("INVALID_PAYLOAD")); return true; }

    FString LandscapePath; Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
    FString LandscapeName; Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);
    
    double LocX = 0, LocY = 0, LocZ = 0;
    const TSharedPtr<FJsonObject>* LocObj = nullptr;
    if (Payload->TryGetObjectField(TEXT("location"), LocObj) && LocObj)
    {
        (*LocObj)->TryGetNumberField(TEXT("x"), LocX);
        (*LocObj)->TryGetNumberField(TEXT("y"), LocY);
        (*LocObj)->TryGetNumberField(TEXT("z"), LocZ);
    }
    else
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("location required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }
    FVector TargetLocation(LocX, LocY, LocZ);

    FString ToolMode = TEXT("Raise");
    Payload->TryGetStringField(TEXT("toolMode"), ToolMode);

    double BrushRadius = 1000.0;
    Payload->TryGetNumberField(TEXT("brushRadius"), BrushRadius);

    double BrushFalloff = 0.5;
    Payload->TryGetNumberField(TEXT("brushFalloff"), BrushFalloff);

    double Strength = 0.1;
    Payload->TryGetNumberField(TEXT("strength"), Strength);

    ALandscape* Landscape = nullptr;
    if (!LandscapePath.IsEmpty())
    {
        Landscape = Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape && !LandscapeName.IsEmpty())
    {
        if (!GEditor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
        {
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors)
            {
                if (A && A->IsA<ALandscape>() && A->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
                {
                    Landscape = Cast<ALandscape>(A);
                    break;
                }
            }
        }
    }
    if (!Landscape)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to find landscape"), TEXT("LOAD_FAILED"));
        return true;
    }

    ULandscapeInfo* LandscapeInfo = Landscape->GetLandscapeInfo();
    if (!LandscapeInfo)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Landscape has no info"), TEXT("INVALID_LANDSCAPE"));
        return true;
    }

    // Convert World Location to Landscape Local Space
    FVector LocalPos = Landscape->GetActorTransform().InverseTransformPosition(TargetLocation);
    int32 CenterX = FMath::RoundToInt(LocalPos.X);
    int32 CenterY = FMath::RoundToInt(LocalPos.Y);

    // Convert Brush Radius to Vertex Units (assuming uniform scale for simplicity, or use X)
    float ScaleX = Landscape->GetActorScale3D().X;
    int32 RadiusVerts = FMath::Max(1, FMath::RoundToInt(BrushRadius / ScaleX));
    int32 FalloffVerts = FMath::RoundToInt(RadiusVerts * BrushFalloff);

    int32 MinX = CenterX - RadiusVerts;
    int32 MaxX = CenterX + RadiusVerts;
    int32 MinY = CenterY - RadiusVerts;
    int32 MaxY = CenterY + RadiusVerts;

    // Clamp to landscape extents
    int32 LMinX, LMinY, LMaxX, LMaxY;
    if (LandscapeInfo->GetLandscapeExtent(LMinX, LMinY, LMaxX, LMaxY))
    {
        MinX = FMath::Max(MinX, LMinX);
        MinY = FMath::Max(MinY, LMinY);
        MaxX = FMath::Min(MaxX, LMaxX);
        MaxY = FMath::Min(MaxY, LMaxY);
    }

    if (MinX > MaxX || MinY > MaxY)
    {
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Brush outside landscape bounds"), nullptr, TEXT("OUT_OF_BOUNDS"));
        return true;
    }

    int32 SizeX = MaxX - MinX + 1;
    int32 SizeY = MaxY - MinY + 1;
    TArray<uint16> HeightData;
    HeightData.AddZeroed(SizeX * SizeY);

    FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
    LandscapeEdit.GetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0);

    bool bModified = false;
    for (int32 Y = MinY; Y <= MaxY; ++Y)
    {
        for (int32 X = MinX; X <= MaxX; ++X)
        {
            float Dist = FMath::Sqrt(FMath::Square((float)(X - CenterX)) + FMath::Square((float)(Y - CenterY)));
            if (Dist > RadiusVerts) continue;

            float Alpha = 1.0f;
            if (Dist > (RadiusVerts - FalloffVerts))
            {
                Alpha = 1.0f - ((Dist - (RadiusVerts - FalloffVerts)) / (float)FalloffVerts);
            }
            Alpha = FMath::Clamp(Alpha, 0.0f, 1.0f);

            int32 Index = (Y - MinY) * SizeX + (X - MinX);
            uint16 CurrentHeight = HeightData[Index];
            
            // Landscape height is 0..65535, where 32768 is 0 Z (usually).
            // 1 unit of uint16 corresponds to Scale.Z / 128.0 (approx, depends on encoding).
            // Standard: Height = (Value - 32768) * Scale.Z / 128.0
            // So DeltaValue = DeltaWorldZ * 128.0 / Scale.Z
            
            float ScaleZ = Landscape->GetActorScale3D().Z;
            float HeightScale = 128.0f / ScaleZ; // Conversion factor from World Z to uint16
            
            float Delta = 0.0f;
            if (ToolMode.Equals(TEXT("Raise"), ESearchCase::IgnoreCase))
            {
                Delta = Strength * Alpha * 100.0f * HeightScale; // Arbitrary strength multiplier
            }
            else if (ToolMode.Equals(TEXT("Lower"), ESearchCase::IgnoreCase))
            {
                Delta = -Strength * Alpha * 100.0f * HeightScale;
            }
            else if (ToolMode.Equals(TEXT("Flatten"), ESearchCase::IgnoreCase))
            {
                // Target Z is TargetLocation.Z
                // Target Value = (TargetLocation.Z / ScaleZ * 128.0) + 32768
                float TargetVal = (LocalPos.Z * 128.0f) + 32768.0f; // LocalPos.Z is already in local space relative to actor? 
                // No, LocalPos is relative to actor transform. Landscape Z is usually 0 in local space at 32768.
                // Let's assume Flatten to the Z of the hit location.
                
                float CurrentVal = (float)CurrentHeight;
                float Target = (TargetLocation.Z - Landscape->GetActorLocation().Z) / ScaleZ * 128.0f + 32768.0f;
                
                Delta = (Target - CurrentVal) * Strength * Alpha;
            }

            int32 NewHeight = FMath::Clamp((int32)(CurrentHeight + Delta), 0, 65535);
            if (NewHeight != CurrentHeight)
            {
                HeightData[Index] = (uint16)NewHeight;
                bModified = true;
            }
        }
    }

    if (bModified)
    {
        LandscapeEdit.SetHeightData(MinX, MinY, MaxX, MaxY, HeightData.GetData(), 0, true);
        LandscapeEdit.Flush();
        // Avoid explicit collision rebuild to prevent potential crashes; PostEditChange will refresh rendering
        Landscape->PostEditChange();
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("toolMode"), ToolMode);
    Resp->SetNumberField(TEXT("modifiedVertices"), bModified ? HeightData.Num() : 0);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Landscape sculpted"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("sculpt_landscape requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleSetLandscapeMaterial(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("set_landscape_material"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("set_landscape_material payload missing"), TEXT("INVALID_PAYLOAD")); return true; }

    FString LandscapePath; Payload->TryGetStringField(TEXT("landscapePath"), LandscapePath);
    FString LandscapeName; Payload->TryGetStringField(TEXT("landscapeName"), LandscapeName);
    FString MaterialPath; if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("materialPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    ALandscape* Landscape = nullptr;
    if (!LandscapePath.IsEmpty())
    {
        Landscape = Cast<ALandscape>(StaticLoadObject(ALandscape::StaticClass(), nullptr, *LandscapePath));
    }
    if (!Landscape && !LandscapeName.IsEmpty())
    {
        if (!GEditor)
        {
            SendAutomationError(RequestingSocket, RequestId, TEXT("Editor not available"), TEXT("EDITOR_NOT_AVAILABLE"));
            return true;
        }
        if (UEditorActorSubsystem* ActorSS = GEditor->GetEditorSubsystem<UEditorActorSubsystem>())
        {
            TArray<AActor*> AllActors = ActorSS->GetAllLevelActors();
            for (AActor* A : AllActors)
            {
                if (A && A->IsA<ALandscape>() && A->GetActorLabel().Equals(LandscapeName, ESearchCase::IgnoreCase))
                {
                    Landscape = Cast<ALandscape>(A);
                    break;
                }
            }
        }
    }
    if (!Landscape)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to find landscape"), TEXT("LOAD_FAILED"));
        return true;
    }

    UMaterialInterface* Mat = LoadObject<UMaterialInterface>(nullptr, *MaterialPath);
    if (!Mat)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load material"), TEXT("LOAD_FAILED"));
        return true;
    }

    Landscape->LandscapeMaterial = Mat;
    Landscape->PostEditChange();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("landscapePath"), Landscape->GetPathName());
    Resp->SetStringField(TEXT("materialPath"), MaterialPath);

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Landscape material set"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("set_landscape_material requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleCreateLandscapeGrassType(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_landscape_grass_type"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("create_landscape_grass_type payload missing"), TEXT("INVALID_PAYLOAD")); return true; }

    FString Name;
    if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString MeshPath;
    if (!Payload->TryGetStringField(TEXT("meshPath"), MeshPath) || MeshPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("meshPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    double Density = 1.0;
    Payload->TryGetNumberField(TEXT("density"), Density);

    double MinScale = 0.8;
    Payload->TryGetNumberField(TEXT("minScale"), MinScale);

    double MaxScale = 1.2;
    Payload->TryGetNumberField(TEXT("maxScale"), MaxScale);

    UStaticMesh* StaticMesh = LoadObject<UStaticMesh>(nullptr, *MeshPath);
    if (!StaticMesh)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load static mesh"), TEXT("LOAD_FAILED"));
        return true;
    }

    FString PackagePath = TEXT("/Game/Landscape");
    FString AssetName = Name;
    FString FullPackagePath = FString::Printf(TEXT("%s/%s"), *PackagePath, *AssetName);

    UPackage* Package = CreatePackage(*FullPackagePath);
    ULandscapeGrassType* GrassType = NewObject<ULandscapeGrassType>(Package, FName(*AssetName), RF_Public | RF_Standalone);
    if (!GrassType)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create grass type asset"), TEXT("CREATION_FAILED"));
        return true;
    }

    FGrassVariety Variety;
    Variety.GrassMesh = StaticMesh;
    // Variety.GrassDensity.Name = FName(TEXT("Density")); // Default parameter name?
    // Actually GrassDensity is FPerPlatformFloat.
    Variety.GrassDensity.Default = static_cast<float>(Density);
    
    Variety.ScaleX = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
    Variety.ScaleY = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
    Variety.ScaleZ = FFloatInterval(static_cast<float>(MinScale), static_cast<float>(MaxScale));
    
    Variety.RandomRotation = true;
    Variety.AlignToSurface = true;

    GrassType->GrassVarieties.Add(Variety);

    Package->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(GrassType);

    FString PackageFileName = FPackageName::LongPackageNameToFilename(FullPackagePath, FPackageName::GetAssetPackageExtension());
    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.Error = GError;
    SaveArgs.SaveFlags = SAVE_NoError;
    bool bSaved = UPackage::SavePackage(Package, GrassType, *PackageFileName, SaveArgs);

    if (!bSaved)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to save grass type asset"), TEXT("SAVE_FAILED"));
        return true;
    }

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("asset_path"), GrassType->GetPathName());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Landscape grass type created"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_landscape_grass_type requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

