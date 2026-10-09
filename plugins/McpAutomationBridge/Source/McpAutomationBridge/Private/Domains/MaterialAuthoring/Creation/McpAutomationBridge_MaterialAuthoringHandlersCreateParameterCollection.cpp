#include "Domains/MaterialAuthoring/McpAutomationBridge_MaterialAuthoringHandlersPrivate.h"
#include "Foundation/BridgeHelpers/Responses/McpAutomationBridgeHelpersJsonFields.h"
#include "Materials/MaterialParameterCollection.h"

// create_parameter_collection: a Material Parameter Collection holds values every material that reads it shares (a
// level-wide wetness, wind strength or time of day), set once at runtime instead of on each material instance; there
// was no way to make one or give it parameters. Sent for a collection that exists, it adds the parameters it lacks and
// sets the defaults of those it has. It never removes one: materials read a parameter by its id.
namespace McpMaterialAuthoringHandlers
{
namespace
{
// One kind's {name, default} rows onto List: a name it lacks is appended (the constructor gives it a fresh id), one it
// has takes the new default. Names are shared by both kinds, so one the Other list holds is refused.
template <typename TParam, typename TOther, typename TSetDefault>
FString UpsertCollectionParameters(TArray<TParam>& List, const TArray<TOther>& Other, const TSharedPtr<FJsonObject>& Payload,
                                   const TCHAR* Field, TSetDefault&& SetDefault, TArray<TSharedPtr<FJsonValue>>& OutAdded)
{
  const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
  if (!Payload->TryGetArrayField(Field, Rows))
  {
    return FString();
  }
  for (const TSharedPtr<FJsonValue>& Row : *Rows)
  {
    const TSharedPtr<FJsonObject>* Item = nullptr;
    FString Name;
    if (!Row.IsValid() || !Row->TryGetObject(Item) || !(*Item)->TryGetStringField(TEXT("name"), Name) || Name.IsEmpty())
    {
      return FString::Printf(TEXT("every %s entry is {name, default}"), Field);
    }
    const FName Key(*Name);
    if (Other.ContainsByPredicate([Key](const TOther& Param) { return Param.ParameterName == Key; }))
    {
      return FString::Printf(TEXT("'%s' is already a parameter of the other kind, and a collection's names are shared"), *Name);
    }
    TParam* Param = List.FindByPredicate([Key](const TParam& Existing) { return Existing.ParameterName == Key; });
    const bool bAdded = Param == nullptr;
    if (bAdded)
    {
      Param = &List.AddDefaulted_GetRef();
      Param->ParameterName = Key;
      OutAdded.Add(MakeShared<FJsonValueString>(Name));
    }
    SetDefault(*Item, *Param, bAdded);
  }
  return FString();
}
}

bool HandleCreateParameterCollection(UMcpAutomationBridgeSubsystem* Bridge, const FString& RequestId, const FString& SubAction,
                                     const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> Socket)
{
  if (SubAction != TEXT("create_parameter_collection"))
  {
    return false;
  }
  const FString RawName = GetJsonStringField(Payload, TEXT("name"));
  FString Folder = GetJsonStringField(Payload, TEXT("path"));
  Folder = Folder.IsEmpty() ? FString(TEXT("/Game/Materials")) : Folder;
  FString PackagePath, PathError;
  UMaterialParameterCollection* Collection = nullptr;
  if (!RawName.IsEmpty() && ValidateAssetCreationPath(Folder, SanitizeAssetName(RawName), PackagePath, PathError))
  {
    Collection = Cast<UMaterialParameterCollection>(McpLoadAsset(PackagePath + TEXT(".") + SanitizeAssetName(RawName)));
  }

  // Worked on copies first, so a refused row leaves the collection as it was and creates nothing.
  TArray<FCollectionScalarParameter> Scalars = Collection ? Collection->ScalarParameters : TArray<FCollectionScalarParameter>();
  TArray<FCollectionVectorParameter> Vectors = Collection ? Collection->VectorParameters : TArray<FCollectionVectorParameter>();
  TArray<TSharedPtr<FJsonValue>> Added;
  FString Error = UpsertCollectionParameters(Scalars, Vectors, Payload, TEXT("scalars"),
      [](const TSharedPtr<FJsonObject>& Item, FCollectionScalarParameter& Param, bool)
      {
        double Value = Param.DefaultValue;
        Item->TryGetNumberField(TEXT("default"), Value);
        Param.DefaultValue = static_cast<float>(Value);
      }, Added);
  if (Error.IsEmpty())
  {
    // A new vector starts opaque, so an [r, g, b] default gets alpha 1 (it starts all zero); one it has keeps its alpha.
    Error = UpsertCollectionParameters(Vectors, Scalars, Payload, TEXT("vectors"),
        [](const TSharedPtr<FJsonObject>& Item, FCollectionVectorParameter& Param, bool bAdded)
        {
          const FLinearColor Base(Param.DefaultValue.R, Param.DefaultValue.G, Param.DefaultValue.B, bAdded ? 1.f : Param.DefaultValue.A);
          Param.DefaultValue = ExtractLinearColorField(Item, TEXT("default"), Base);
        }, Added);
  }
  if (!Error.IsEmpty())
  {
    Bridge->SendAutomationError(Socket, RequestId, FString::Printf(TEXT("%s; nothing was changed."), *Error), TEXT("INVALID_ARGUMENT"));
    return true;
  }

  const bool bCreated = Collection == nullptr;
  if (bCreated)
  {
    FString Name;
    bool bParentFolderCreated = false;
    if (!PrepareNewMaterialAsset(Bridge, RequestId, Socket, RawName, Folder, TEXT("/Game/Materials"),
                                 TEXT("MaterialParameterCollection"), Name, PackagePath, bParentFolderCreated))
    {
      return true;
    }
    Collection = NewObject<UMaterialParameterCollection>(CreatePackage(*PackagePath), FName(*Name),
                                                         RF_Public | RF_Standalone | RF_Transactional);
    FAssetRegistryModule::AssetCreated(Collection);
  }
  Collection->Modify();
  Collection->ScalarParameters = MoveTemp(Scalars);
  Collection->VectorParameters = MoveTemp(Vectors);
  // Rebuilds the collection's uniform buffer and recompiles the materials that read it.
  Collection->PostEditChange();
  Collection->MarkPackageDirty();
  bool bSave = true;
  Payload->TryGetBoolField(TEXT("save"), bSave);
  const bool bSaved = bSave && McpSafeAssetSave(Collection);

  TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
  Result->SetStringField(TEXT("assetPath"), Collection->GetPathName());
  Result->SetBoolField(TEXT("created"), bCreated);
  Result->SetArrayField(TEXT("added"), Added);
  // A float written as a double read 0.30000001192092896; 7 significant digits are all a float holds.
  const auto Clean = [](float Value) { return FCString::Atod(*FString::Printf(TEXT("%.7g"), Value)); };
  TArray<TSharedPtr<FJsonValue>> ScalarRows, VectorRows;
  for (const FCollectionScalarParameter& Param : Collection->ScalarParameters)
  {
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), Param.ParameterName.ToString());
    Row->SetNumberField(TEXT("default"), Clean(Param.DefaultValue));
    ScalarRows.Add(MakeShared<FJsonValueObject>(Row));
  }
  for (const FCollectionVectorParameter& Param : Collection->VectorParameters)
  {
    TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
    Row->SetStringField(TEXT("name"), Param.ParameterName.ToString());
    TArray<TSharedPtr<FJsonValue>> Color;
    for (const float Channel : {Param.DefaultValue.R, Param.DefaultValue.G, Param.DefaultValue.B, Param.DefaultValue.A})
    {
      Color.Add(MakeShared<FJsonValueNumber>(Clean(Channel)));
    }
    Row->SetArrayField(TEXT("default"), Color);
    VectorRows.Add(MakeShared<FJsonValueObject>(Row));
  }
  Result->SetArrayField(TEXT("scalars"), ScalarRows);
  Result->SetArrayField(TEXT("vectors"), VectorRows);
  Result->SetBoolField(TEXT("saved"), bSaved);
  McpHandlerUtils::AddVerification(Result, Collection);
  Bridge->SendAutomationResponse(Socket, RequestId, true,
      FString::Printf(TEXT("Material Parameter Collection '%s' %s (%d scalar, %d vector parameters)."), *Collection->GetName(),
                      bCreated ? TEXT("created") : TEXT("updated"), Collection->ScalarParameters.Num(), Collection->VectorParameters.Num()),
      Result);
  return true;
}
}
