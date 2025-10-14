#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Misc/ScopeExit.h"
#include "Async/Async.h"
#if WITH_EDITOR
#include "IPythonScriptPlugin.h"
#include "EditorAssetLibrary.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "Factories/Factory.h"
#include "Factories/MaterialFactoryNew.h"
#include "Factories/MaterialInstanceConstantFactoryNew.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceConstant.h"
#if __has_include("MaterialEditingLibrary.h")
#include "MaterialEditingLibrary.h"
#define MCP_HAS_MATERIAL_EDITING_LIBRARY 1
#else
#define MCP_HAS_MATERIAL_EDITING_LIBRARY 0
#endif
#if __has_include("Subsystems/EditorActorSubsystem.h")
#include "Subsystems/EditorActorSubsystem.h"
#elif __has_include("EditorActorSubsystem.h")
#include "EditorActorSubsystem.h"
#endif
#endif

bool UMcpAutomationBridgeSubsystem::HandleAssetAction(const FString& RequestId, const FString& Action, const TSharedPtr<FJsonObject>& Payload, TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();

    // 1) IMPORT ASSET
    if (Lower.Equals(TEXT("import_asset_deferred"), ESearchCase::IgnoreCase) || Lower.StartsWith(TEXT("import_asset")))
    {
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("import_asset_deferred payload missing."), TEXT("INVALID_PAYLOAD")); return true; }
        FString SourcePath; if (!Payload->TryGetStringField(TEXT("sourcePath"), SourcePath) || SourcePath.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("sourcePath required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString Destination; if (!Payload->TryGetStringField(TEXT("destinationPath"), Destination) || Destination.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("destinationPath required"), TEXT("INVALID_ARGUMENT")); return true; }

        // Normalize destination similar to TypeScript helper
        FString CleanDest = Destination;
        CleanDest = CleanDest.Replace(TEXT("\\"), TEXT("/"));
        if (CleanDest.EndsWith(TEXT("/"))) CleanDest = CleanDest.LeftChop(1);
        if (CleanDest.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) CleanDest = FString::Printf(TEXT("/Game%s"), *CleanDest.RightChop(8));
        if (!CleanDest.StartsWith(TEXT("/Game"))) CleanDest = FString::Printf(TEXT("/Game/%s"), *CleanDest.Replace(TEXT("/"), TEXT("")));

        // Build a minimal python script (similar to the server-side TS implementation)
        const FString EscapedSource = SourcePath.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedDest = CleanDest.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));

        const FString Py = FString::Printf(TEXT(R"PY(
import unreal
import json

task = unreal.AssetImportTask()
task.filename = r"%s"
task.destination_path = r"%s"
task.automated = True
task.save = True
task.replace_existing = True

options = unreal.FbxImportUI()
options.import_mesh = True
options.import_as_skeletal = False
options.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
options.static_mesh_import_data.combine_meshes = True
options.static_mesh_import_data.generate_lightmap_u_vs = False
options.static_mesh_import_data.auto_generate_collision = False
task.options = options

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
result = {'success': False, 'error': 'No assets imported', 'source': task.filename}
try:
    asset_tools.import_asset_tasks([task])
    if task.imported_object_paths:
        result = {'success': True, 'imported': len(task.imported_object_paths), 'paths': list(task.imported_object_paths)}
except Exception as e:
    result = {'success': False, 'error': str(e), 'source': task.filename}

print('RESULT:' + json.dumps(result))
)PY"), *EscapedSource, *EscapedDest);

#if WITH_EDITOR
        // Native asset import using AssetTools UAssetImportTask
        AsyncTask(ENamedThreads::GameThread, [this, RequestId, CleanDest, SourcePath, RequestingSocket]() {
            TArray<UAssetImportTask*> Tasks;
            UAssetImportTask* Task = NewObject<UAssetImportTask>();
            Task->Filename = SourcePath;
            Task->DestinationPath = CleanDest;
            Task->bAutomated = true;
            Task->bReplaceExisting = true;
            Task->bSave = true;
            Tasks.Add(Task);

            IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools")).Get();
            AssetTools.ImportAssetTasks(Tasks);

            // Build result
            TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
            if (Task->ImportedObjectPaths.Num() > 0)
            {
                ResObj->SetBoolField(TEXT("success"), true);
                ResObj->SetNumberField(TEXT("imported"), Task->ImportedObjectPaths.Num());
                TArray<TSharedPtr<FJsonValue>> Paths;
                for (const FString& P : Task->ImportedObjectPaths)
                {
                    Paths.Add(MakeShared<FJsonValueString>(P));
                }
                ResObj->SetArrayField(TEXT("paths"), Paths);
                SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Asset import completed"), ResObj, FString());
            }
            else
            {
                ResObj->SetBoolField(TEXT("success"), false);
                ResObj->SetStringField(TEXT("error"), TEXT("No assets imported"));
                SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Asset import failed"), ResObj, TEXT("IMPORT_FAILED"));
            }
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Asset import requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // CREATE MATERIAL
    if (Lower.Equals(TEXT("create_material"), ESearchCase::IgnoreCase))
    {
#if WITH_EDITOR
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("create_material payload missing."), TEXT("INVALID_PAYLOAD")); return true; }
        FString Name; if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString Destination; Payload->TryGetStringField(TEXT("destinationPath"), Destination);
        FString Parent; Payload->TryGetStringField(TEXT("parentMaterial"), Parent);

        if (Destination.IsEmpty()) Destination = TEXT("/Game");
        // Normalize
        if (Destination.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) Destination = FString::Printf(TEXT("/Game%s"), *Destination.RightChop(8));

        const FString EscapedName = Name.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedDest = Destination.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedParent = Parent.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));

        const FString PyCreate = FString::Printf(TEXT(R"PY(
import unreal, json
result = {'success': False, 'message': '', 'path': None, 'error': None}
try:
    name = r"%s"
    dest = r"%s".rstrip('/')
    factory = unreal.MaterialFactoryNew()
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    created = asset_tools.create_asset(name, dest, None, factory)
    if created:
        path = f"{dest}/{name}"
        result['success'] = True
        result['path'] = path
        result['message'] = f'Created material at {path}'
        if r"%s":
            try:
                parent = unreal.EditorAssetLibrary.load_asset(r"%s")
                if parent:
                    created.set_editor_property('parent', parent)
            except Exception as pe:
                result.setdefault('warnings', []).append(f'Failed to set parent: {pe}')
        try:
            unreal.EditorAssetLibrary.save_asset(result['path'])
        except Exception:
            result.setdefault('warnings', []).append('Save failed')
    else:
        result['error'] = 'create_asset returned None'
except Exception as e:
    result['error'] = str(e)

if result.get('error'):
    result['message'] = result['error']
print('RESULT:' + json.dumps(result))
)PY"), *EscapedName, *EscapedDest, *EscapedParent, *EscapedParent);

        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Name, Destination, Parent, RequestingSocket]() {
            // Create material natively via AssetTools and UMaterialFactoryNew
            UMaterialFactoryNew* Factory = NewObject<UMaterialFactoryNew>();
            FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
            UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, Destination, UMaterial::StaticClass(), Factory);
            if (!NewObj)
            {
                TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("CreateAsset returned null")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create material failed"), Err, TEXT("CREATE_MATERIAL_FAILED")); return;
            }
            UMaterial* M = Cast<UMaterial>(NewObj);
            if (!M) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Created asset is not a Material")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create material failed"), Err, TEXT("CREATE_MATERIAL_FAILED")); return; }
            if (!Parent.IsEmpty())
            {
                if (UObject* ParentAsset = LoadObject<UObject>(nullptr, *Parent))
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Warning, TEXT("create_material: parent assignment is not supported in this engine version (parent=%s)."), *Parent);
                }
            }
#if WITH_EDITOR
            UEditorAssetLibrary::SaveLoadedAsset(M);
#endif
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("path"), M->GetPathName()); Out->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Material created"), Out, FString());
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_material requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // CREATE MATERIAL INSTANCE
    if (Lower.Equals(TEXT("create_material_instance"), ESearchCase::IgnoreCase))
    {
#if WITH_EDITOR
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("create_material_instance payload missing."), TEXT("INVALID_PAYLOAD")); return true; }
        FString Name; if (!Payload->TryGetStringField(TEXT("name"), Name) || Name.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("name required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString Destination; Payload->TryGetStringField(TEXT("destinationPath"), Destination);
        FString Parent; if (!Payload->TryGetStringField(TEXT("parentMaterial"), Parent) || Parent.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("parentMaterial required"), TEXT("INVALID_ARGUMENT")); return true; }

        if (Destination.IsEmpty()) Destination = TEXT("/Game");
        if (Destination.StartsWith(TEXT("/Content"), ESearchCase::IgnoreCase)) Destination = FString::Printf(TEXT("/Game%s"), *Destination.RightChop(8));

        const FString EscapedNameInst = Name.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedDestInst = Destination.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedParentInst = Parent.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));

        // Serialize optional parameters object from payload so Python can apply them
        FString ParamsJson;
        const TSharedPtr<FJsonObject>* ParamsObjPtr = nullptr;
        if (Payload->TryGetObjectField(TEXT("parameters"), ParamsObjPtr) && ParamsObjPtr && (*ParamsObjPtr).IsValid())
        {
            const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ParamsJson);
            FJsonSerializer::Serialize((*ParamsObjPtr).ToSharedRef(), Writer);
        }

        const FString PyInst = FString::Printf(TEXT(R"PY(
import unreal, json
result = {'success': False, 'message': '', 'path': None, 'error': None}
try:
    name = r"%s"
    dest = r"%s".rstrip('/')
    parent_path = r"%s"
    params_json = r'''%s'''
    params = {}
    try:
        if params_json and params_json.strip():
            params = json.loads(params_json)
    except Exception as pe:
        result.setdefault('warnings', []).append(f'Failed to parse parameters JSON: {pe}')

    parent = unreal.EditorAssetLibrary.load_asset(parent_path)
    if not parent:
        result['error'] = f'Parent material not found: {parent_path}'
    else:
        factory = unreal.MaterialInstanceConstantFactoryNew()
        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        created = asset_tools.create_asset(name, dest, None, factory)
        if created:
            created.set_editor_property('parent', parent)
            # Apply parameters if provided
            if params:
                melib = getattr(unreal, 'MaterialEditingLibrary', None)
                for k, v in params.items():
                    try:
                        # Scalars / numerics
                        if isinstance(v, bool) or isinstance(v, int) or isinstance(v, float):
                            try:
                                if melib and hasattr(melib, 'set_material_instance_scalar_parameter_value'):
                                    melib.set_material_instance_scalar_parameter_value(created, k, float(v))
                                else:
                                    result.setdefault('warnings', []).append(f'No scalar setter available for {k}')
                            except Exception as se:
                                result.setdefault('warnings', []).append(f'Failed to set scalar {k}: {se}')
                        # Vectors / colors
                        elif isinstance(v, (list, tuple)):
                            try:
                                if len(v) >= 3:
                                    r = float(v[0]); g = float(v[1]); b = float(v[2]); a = float(v[3]) if len(v) > 3 else 1.0
                                    lc = unreal.LinearColor(r, g, b, a)
                                    if melib and hasattr(melib, 'set_material_instance_vector_parameter_value'):
                                        melib.set_material_instance_vector_parameter_value(created, k, lc)
                                    else:
                                        result.setdefault('warnings', []).append(f'No vector setter available for {k}')
                                else:
                                    result.setdefault('warnings', []).append(f'Invalid vector length for {k}: {v}')
                            except Exception as ve:
                                result.setdefault('warnings', []).append(f'Failed to set vector {k}: {ve}')
                        # Texture / asset path
                        elif isinstance(v, str):
                            try:
                                maybe_asset = unreal.EditorAssetLibrary.load_asset(v)
                                if maybe_asset:
                                    if melib and hasattr(melib, 'set_material_instance_texture_parameter_value'):
                                        melib.set_material_instance_texture_parameter_value(created, k, maybe_asset)
                                    else:
                                        result.setdefault('warnings', []).append(f'No texture setter available for {k}')
                                else:
                                    result.setdefault('warnings', []).append(f'Failed to load asset for texture parameter {k}: {v}')
                            except Exception as te:
                                result.setdefault('warnings', []).append(f'Failed to set texture {k}: {te}')
                        else:
                            result.setdefault('warnings', []).append(f'Unsupported parameter type for {k}: {type(v)}')
                    except Exception as e:
                        result.setdefault('warnings', []).append(f'Error applying parameter {k}: {e}')

            try:
                unreal.EditorAssetLibrary.save_asset(f"{dest}/{name}")
            except Exception:
                result.setdefault('warnings', []).append('Save failed')
            result['success'] = True
            result['path'] = f"{dest}/{name}"
            result['message'] = f'Created material instance at {dest}/{name}'
        else:
            result['error'] = 'Material instance creation returned None'
except Exception as e:
    result['error'] = str(e)

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

print('RESULT:' + json.dumps(result))
)PY"), *EscapedNameInst, *EscapedDestInst, *EscapedParentInst, *ParamsJson);

        AsyncTask(ENamedThreads::GameThread, [this, RequestId, Name, Destination, Parent, ParamsJson, RequestingSocket]() {
            // Create material instance via native APIs
            UMaterialInstanceConstantFactoryNew* Factory = NewObject<UMaterialInstanceConstantFactoryNew>();
            FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
            UObject* NewObj = AssetToolsModule.Get().CreateAsset(Name, Destination, UMaterialInstanceConstant::StaticClass(), Factory);
            if (!NewObj) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("CreateAsset returned null")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create material instance failed"), Err, TEXT("CREATE_MATERIAL_INSTANCE_FAILED")); return; }
            UMaterialInstanceConstant* MIC = Cast<UMaterialInstanceConstant>(NewObj);
            if (!MIC) { TSharedPtr<FJsonObject> Err = MakeShared<FJsonObject>(); Err->SetStringField(TEXT("error"), TEXT("Created asset is not a MaterialInstanceConstant")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Create material instance failed"), Err, TEXT("CREATE_MATERIAL_INSTANCE_FAILED")); return; }
            // Load parent material
            UObject* ParentAsset = LoadObject<UObject>(nullptr, *Parent);
            if (ParentAsset && ParentAsset->IsA<UMaterialInterface>())
            {
#if MCP_HAS_MATERIAL_EDITING_LIBRARY
                UMaterialEditingLibrary::SetMaterialInstanceParent(MIC, Cast<UMaterialInterface>(ParentAsset));
#else
                MIC->SetEditorProperty(TEXT("Parent"), Cast<UMaterialInterface>(ParentAsset));
#endif
            }
            // Apply params JSON when present using MaterialEditingLibrary when available
            if (!ParamsJson.IsEmpty())
            {
                TSharedPtr<FJsonObject> ParsedParams;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(ParamsJson);
                if (FJsonSerializer::Deserialize(Reader, ParsedParams) && ParsedParams.IsValid())
                {
                    // Iterate parameters and attempt to set via native APIs
                    for (const auto& Pair : ParsedParams->Values)
                    {
                        const FString& Key = Pair.Key;
                        const TSharedPtr<FJsonValue>& Val = Pair.Value;
                        if (!Val.IsValid()) continue;
#if MCP_HAS_MATERIAL_EDITING_LIBRARY
                        if (Val->Type == EJson::Number)
                        {
                            const float Num = static_cast<float>(Val->AsNumber());
                            UMaterialEditingLibrary::SetMaterialInstanceScalarParameterValue(MIC, FName(*Key), Num);
                        }
                        else if (Val->Type == EJson::Array)
                        {
                            const TArray<TSharedPtr<FJsonValue>>& Arr = Val->AsArray();
                            if (Arr.Num() >= 3)
                            {
                                const float R = static_cast<float>(Arr[0]->AsNumber());
                                const float G = static_cast<float>(Arr[1]->AsNumber());
                                const float B = static_cast<float>(Arr[2]->AsNumber());
                                const float A = Arr.Num() > 3 ? static_cast<float>(Arr[3]->AsNumber()) : 1.0f;
                                UMaterialEditingLibrary::SetMaterialInstanceVectorParameterValue(MIC, FName(*Key), FLinearColor(R, G, B, A));
                            }
                        }
                        else if (Val->Type == EJson::String)
                        {
                            UObject* Maybe = LoadObject<UObject>(nullptr, *Val->AsString());
                            if (Maybe)
                            {
                                if (UTexture* AsTexture = Cast<UTexture>(Maybe))
                                {
                                    UMaterialEditingLibrary::SetMaterialInstanceTextureParameterValue(MIC, FName(*Key), AsTexture);
                                }
                            }
                        }
#endif
                    }
                }
            }
#if WITH_EDITOR
            UEditorAssetLibrary::SaveLoadedAsset(MIC);
#endif
            TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("path"), MIC->GetPathName()); Out->SetBoolField(TEXT("success"), true); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Material instance created"), Out, FString());
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_material_instance requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // 2) DUPLICATE ASSET
    if (Lower.Equals(TEXT("duplicate_asset"), ESearchCase::IgnoreCase))
    {
#if WITH_EDITOR
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("duplicate_asset payload missing."), TEXT("INVALID_PAYLOAD")); return true; }
        FString SourcePathDup; if (!Payload->TryGetStringField(TEXT("sourcePath"), SourcePathDup) || SourcePathDup.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("sourcePath required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString DestinationDup; if (!Payload->TryGetStringField(TEXT("destinationPath"), DestinationDup) || DestinationDup.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("destinationPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString NewNameDup; Payload->TryGetStringField(TEXT("newName"), NewNameDup);
        bool bOverwrite = false; Payload->TryGetBoolField(TEXT("overwrite"), bOverwrite);
        bool bSave = false; Payload->TryGetBoolField(TEXT("save"), bSave);

        const FString EscapedSourceDup = SourcePathDup.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedDestDup = DestinationDup.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedNewNameDup = NewNameDup.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));

        const FString PyDup = FString::Printf(TEXT(R"PY(
import unreal, json

source_path = r"%s"
target_folder = r"%s".rstrip('/')
requested_name = r"%s"
overwrite_existing = %s
save_new_asset = %s

result = {
    'success': False,
    'message': '',
    'error': '',
    'source': source_path,
    'path': ''
}

asset_lib = unreal.EditorAssetLibrary

if not asset_lib.does_asset_exist(source_path):
  result['error'] = f"Source asset not found: {source_path}"
else:
  original_name = source_path.split('/')[-1]
  asset_name = requested_name.strip() or original_name
  if not asset_name:
    result['error'] = 'Unable to determine asset name'
  else:
    folder = target_folder or source_path.rsplit('/', 1)[0]
    folder = folder.rstrip('/')
    if not folder:
      result['error'] = 'Destination path is empty'
    else:
      new_path = f"{folder}/{asset_name}"
      if not overwrite_existing and asset_lib.does_asset_exist(new_path):
        result['error'] = f"Asset already exists at {new_path}"
        result['conflictPath'] = new_path
      else:
        overwritten = False
        if overwrite_existing and asset_lib.does_asset_exist(new_path):
          if not asset_lib.delete_asset(new_path):
            result['error'] = f"Failed to remove existing asset at {new_path}"
          else:
            overwritten = True
        if not result['error']:
          duplicated = asset_lib.duplicate_asset(source_path, new_path)
          if duplicated:
            result['success'] = True
            result['path'] = new_path
            result['message'] = f"Duplicated asset to {new_path}"
            result['overwritten'] = overwritten
            if save_new_asset:
              try:
                asset_lib.save_asset(new_path, False)
              except Exception as save_err:
                result.setdefault('warnings', []).append(f"Save failed for {new_path}: {save_err}")
          else:
            result['error'] = 'DuplicateAsset returned False'

if not result['success'] and not result['error']:
  result['error'] = 'Duplicate operation failed'

if result.get('error'):
  result['message'] = result['error']
else:
  result['error'] = None

if 'warnings' in result and not result['warnings']:
  result.pop('warnings')

print('RESULT:' + json.dumps(result))
)PY"), *EscapedSourceDup, *EscapedDestDup, *EscapedNewNameDup, bOverwrite ? TEXT("True") : TEXT("False"), bSave ? TEXT("True") : TEXT("False"));

        AsyncTask(ENamedThreads::GameThread, [this, RequestId, SourcePathDup, DestinationDup, NewNameDup, bOverwrite, bSave, RequestingSocket]() {
            TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
            bool bResult = false;
            FString ResultPath;
            // Prefer EditorAssetLibrary duplication; fall back to AssetTools if necessary
            if (UEditorAssetLibrary::DoesAssetExist(SourcePathDup))
            {
                FString Dest = DestinationDup;
                if (!NewNameDup.IsEmpty()) Dest = FString::Printf(TEXT("%s/%s"), *DestinationDup, *NewNameDup);
                // If not overwrite and dest exists -> error
                if (!bOverwrite && UEditorAssetLibrary::DoesAssetExist(Dest))
                {
                    ResObj->SetBoolField(TEXT("success"), false);
                    ResObj->SetStringField(TEXT("error"), TEXT("Asset already exists at destination"));
                    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Duplicate failed: destination exists"), ResObj, TEXT("DUPLICATE_FAILED"));
                    return;
                }

                UObject* DuplicatedAsset = UEditorAssetLibrary::DuplicateAsset(SourcePathDup, Dest);
                const bool bDupOk = (DuplicatedAsset != nullptr);
                if (bDupOk)
                {
                    bResult = true; ResultPath = Dest;
                }
            }

            ResObj->SetBoolField(TEXT("success"), bResult);
            if (bResult) { ResObj->SetStringField(TEXT("path"), ResultPath); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Asset duplicated"), ResObj, FString()); }
            else { ResObj->SetStringField(TEXT("error"), TEXT("Duplicate failed")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Duplicate failed"), ResObj, TEXT("DUPLICATE_FAILED")); }
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Duplicate asset requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // 3) RENAME ASSET
    if (Lower.Equals(TEXT("rename_asset"), ESearchCase::IgnoreCase))
    {
#if WITH_EDITOR
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("rename_asset payload missing."), TEXT("INVALID_PAYLOAD")); return true; }
        FString AssetPath; if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString NewName; if (!Payload->TryGetStringField(TEXT("newName"), NewName) || NewName.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("newName required"), TEXT("INVALID_ARGUMENT")); return true; }

        const FString EscapedAsset = AssetPath.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedNew = NewName.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));

        const FString PyRename = FString::Printf(TEXT(R"PY(
import unreal, json

asset_path = r"%s"
new_name = r"%s".strip()

result = {
    'success': False,
    'message': '',
    'error': '',
    'path': asset_path
}

asset_lib = unreal.EditorAssetLibrary

if not new_name:
    result['error'] = 'New asset name must not be empty'
elif not asset_lib.does_asset_exist(asset_path):
    result['error'] = f"Asset not found: {asset_path}"
else:
    parent_path, _ = asset_path.rsplit('/', 1)
    destination = f"{parent_path}/{new_name}"
    if asset_lib.does_asset_exist(destination):
        result['error'] = f"Asset already exists at {destination}"
        result['conflictPath'] = destination
    else:
        if asset_lib.rename_asset(asset_path, destination):
            result['success'] = True
            result['path'] = destination
            result['message'] = f"Renamed asset to {destination}"
        else:
            result['error'] = 'RenameAsset returned False'

if not result['success'] and not result['error']:
    result['error'] = 'Rename operation failed'

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

print('RESULT:' + json.dumps(result))
)PY"), *EscapedAsset, *EscapedNew);

        AsyncTask(ENamedThreads::GameThread, [this, RequestId, AssetPath, NewName, RequestingSocket]() {
            TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
            FString ParentPath, OldName;
            if (!AssetPath.Split(TEXT("/"), &ParentPath, &OldName, ESearchCase::IgnoreCase, ESearchDir::FromEnd)) ParentPath = AssetPath;
            const FString Destination = FString::Printf(TEXT("%s/%s"), *ParentPath, *NewName);
            bool bOk = false;
            if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
            {
                bOk = UEditorAssetLibrary::RenameAsset(AssetPath, Destination);
                if (!bOk)
                {
                    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
                    TArray<FAssetRenameData> Renames;
                    Renames.Emplace(FSoftObjectPath(AssetPath), FSoftObjectPath(Destination));
                    bool bRenameOk = AssetToolsModule.Get().RenameAssets(Renames);
                    if (bRenameOk)
                    {
                        bOk = true;
                    }
                    else
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("rename_asset: fallback to AssetTools failed for %s -> %s"), *AssetPath, *Destination);
                    }
                }
            }
            ResObj->SetBoolField(TEXT("success"), bOk);
            if (bOk) { ResObj->SetStringField(TEXT("path"), Destination); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Asset renamed"), ResObj, FString()); }
            else { ResObj->SetStringField(TEXT("error"), TEXT("Rename failed")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Rename failed"), ResObj, TEXT("RENAME_FAILED")); }
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Rename asset requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // 4) MOVE ASSET
    if (Lower.Equals(TEXT("move_asset"), ESearchCase::IgnoreCase))
    {
#if WITH_EDITOR
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("move_asset payload missing."), TEXT("INVALID_PAYLOAD")); return true; }
        FString AssetPath; if (!Payload->TryGetStringField(TEXT("assetPath"), AssetPath) || AssetPath.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("assetPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString DestinationMove; if (!Payload->TryGetStringField(TEXT("destinationPath"), DestinationMove) || DestinationMove.TrimStartAndEnd().IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("destinationPath required"), TEXT("INVALID_ARGUMENT")); return true; }
        FString NewNameMove; Payload->TryGetStringField(TEXT("newName"), NewNameMove);
        bool bFixup = true; if (Payload->HasField(TEXT("fixupRedirectors"))) { Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixup); }

        const FString EscapedAssetMove = AssetPath.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedDestMove = DestinationMove.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
        const FString EscapedNewNameMove = NewNameMove.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));

        const FString PyMove = FString::Printf(TEXT(R"PY(
import unreal, json

asset_path = r"%s"
target_folder = r"%s".rstrip('/')
requested_name = r"%s"
fixup_redirectors = %s

result = {
    'success': False,
    'message': '',
    'error': '',
    'path': asset_path
}

asset_lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

if not asset_lib.does_asset_exist(asset_path):
    result['error'] = f"Asset not found: {asset_path}"
else:
    current_name = asset_path.split('/')[-1]
    asset_name = requested_name.strip() or current_name
    folder = target_folder or asset_path.rsplit('/', 1)[0]
    folder = folder.rstrip('/')
    if not folder:
        result['error'] = 'Destination path is empty'
    else:
        destination = f"{folder}/{asset_name}"
        if destination == asset_path:
            result['success'] = True
            result['path'] = destination
            result['message'] = 'Asset already resides at the requested path'
        elif asset_lib.does_asset_exist(destination):
            result['error'] = f"Asset already exists at {destination}"
            result['conflictPath'] = destination
        else:
            if asset_lib.rename_asset(asset_path, destination):
                result['success'] = True
                result['path'] = destination
                result['message'] = f"Moved asset to {destination}"
                if fixup_redirectors and asset_tools:
                    try:
                        asset_tools.fixup_redirectors([])
                        asset_tools.fixup_redirectors([folder])
                    except Exception as fix_err:
                        result.setdefault('warnings', []).append(f"Fix redirectors failed for {folder}: {fix_err}")
            else:
                result['error'] = 'RenameAsset returned False'

if not result['success'] and not result['error']:
    result['error'] = 'Move operation failed'

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

if 'warnings' in result and not result['warnings']:
    result.pop('warnings')

print('RESULT:' + json.dumps(result))
)PY"), *EscapedAssetMove, *EscapedDestMove, *EscapedNewNameMove, bFixup ? TEXT("True") : TEXT("False"));

        AsyncTask(ENamedThreads::GameThread, [this, RequestId, AssetPath, DestinationMove, NewNameMove, bFixup, RequestingSocket]() {
            TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
            FString Dest = DestinationMove;
            if (!NewNameMove.IsEmpty()) Dest = FString::Printf(TEXT("%s/%s"), *DestinationMove, *NewNameMove);
            bool bOk = false;
            if (UEditorAssetLibrary::DoesAssetExist(AssetPath))
            {
                bOk = UEditorAssetLibrary::RenameAsset(AssetPath, Dest);
                if (!bOk)
                {
                    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
                    TArray<FAssetRenameData> Renames;
                    Renames.Emplace(FSoftObjectPath(AssetPath), FSoftObjectPath(Dest));
                    if (AssetToolsModule.Get().RenameAssets(Renames))
                    {
                        bOk = true;
                    }
                }
                if (bOk && bFixup)
                {
                    UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("move_asset: redirector fixup requested for %s but skipped (unsupported API)."), *DestinationMove);
                }
            }
            ResObj->SetBoolField(TEXT("success"), bOk);
            if (bOk) { ResObj->SetStringField(TEXT("path"), Dest); SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Asset moved"), ResObj, FString()); }
            else { ResObj->SetStringField(TEXT("error"), TEXT("Move failed")); SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Move failed"), ResObj, TEXT("MOVE_FAILED")); }
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Move asset requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    // 5) DELETE ASSETS
    if (Lower.Equals(TEXT("delete_assets"), ESearchCase::IgnoreCase))
    {
#if WITH_EDITOR
        if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("delete_assets payload missing."), TEXT("INVALID_PAYLOAD")); return true; }
        const TArray<TSharedPtr<FJsonValue>>* PathsArr = nullptr;
        if (!Payload->TryGetArrayField(TEXT("paths"), PathsArr) || PathsArr == nullptr) { SendAutomationError(RequestingSocket, RequestId, TEXT("paths array required"), TEXT("INVALID_ARGUMENT")); return true; }
        bool bFixup = true; if (Payload->HasField(TEXT("fixupRedirectors"))) { Payload->TryGetBoolField(TEXT("fixupRedirectors"), bFixup); }

        // Build Python list literal
        FString JsonList = TEXT("[");
        for (int32 i = 0; i < PathsArr->Num(); ++i)
        {
            FString P = (*PathsArr)[i]->AsString();
            FString Esc = P.Replace(TEXT("\\"), TEXT("\\\\")).Replace(TEXT("\""), TEXT("\\\""));
            JsonList += FString::Printf(TEXT("r\"%s\""), *Esc);
            if (i != PathsArr->Num() - 1) JsonList += TEXT(", ");
        }
        JsonList += TEXT("]");

    const FString PyDeleteScript = FString::Printf(TEXT(R"PY(
import unreal, json

asset_paths = %s
fixup_redirectors = %s

result = {
    'success': False,
    'message': '',
    'error': '',
    'deleted': [],
    'missing': [],
    'failed': []
}

asset_lib = unreal.EditorAssetLibrary
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

        for path in asset_paths:
    normalized = path.rstrip('/')
    if not asset_lib.does_asset_exist(normalized):
        result['missing'].append(normalized)
        continue
    try:
        if asset_lib.delete_asset(normalized):
            result['deleted'].append(normalized)
        else:
                    result['failed'].append(normalized)
    except Exception as delete_err:
                # Use safer formatting to avoid C++ printf collisions when
                # embedding Python code inside FString::Printf format strings.
                result['failed'].append(f"{normalized}:: {delete_err}")

if result['failed']:
    result['error'] = f"Failed to delete {len(result['failed'])} asset(s)"
elif not result['deleted']:
    result['error'] = 'No assets were deleted'
else:
    result['success'] = True

if result['deleted'] and fixup_redirectors and asset_tools:
    try:
        folders = sorted({ path.rsplit('/', 1)[0] for path in result['deleted'] if '/' in path })
        if folders:
            asset_tools.fixup_redirectors(folders)
    except Exception as fix_err:
        result.setdefault('warnings', []).append(f"Fix redirectors failed: {fix_err}")

if result.get('error'):
    result['message'] = result['error']
else:
    result['error'] = None

if 'warnings' in result and not result['warnings']:
    result.pop('warnings')

print('RESULT:' + json.dumps(result))
)PY"), *JsonList, bFixup ? TEXT("True") : TEXT("False"));

    AsyncTask(ENamedThreads::GameThread, [this, RequestId, PathsArr = *PathsArr, bFixup, RequestingSocket]() {
            TSharedPtr<FJsonObject> ResObj = MakeShared<FJsonObject>();
            TArray<FString> Deleted; TArray<FString> Missing; TArray<FString> Failed;
            FAssetToolsModule& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
            for (const TSharedPtr<FJsonValue>& V : PathsArr)
            {
                if (!V.IsValid() || V->Type != EJson::String) continue;
                const FString Path = V->AsString();
                if (!UEditorAssetLibrary::DoesAssetExist(Path)) { Missing.Add(Path); continue; }
                bool bDel = UEditorAssetLibrary::DeleteAsset(Path);
                if (bDel) Deleted.Add(Path); else Failed.Add(Path);
            }
            ResObj->SetArrayField(TEXT("deleted"), TArray<TSharedPtr<FJsonValue>>());
            TArray<TSharedPtr<FJsonValue>> DelVals; for (const FString& P : Deleted) DelVals.Add(MakeShared<FJsonValueString>(P)); ResObj->SetArrayField(TEXT("deleted"), DelVals);
            TArray<TSharedPtr<FJsonValue>> MissVals; for (const FString& P : Missing) MissVals.Add(MakeShared<FJsonValueString>(P)); ResObj->SetArrayField(TEXT("missing"), MissVals);
            TArray<TSharedPtr<FJsonValue>> FailVals; for (const FString& P : Failed) FailVals.Add(MakeShared<FJsonValueString>(P)); ResObj->SetArrayField(TEXT("failed"), FailVals);
            if (Deleted.Num() > 0 && bFixup)
            {
                // Fix redirectors for folders touched
                TSet<FString> Folders;
                for (const FString& P : Deleted) if (P.Contains(TEXT("/"))) Folders.Add(P.Left(P.Find(TEXT("/"), ESearchCase::IgnoreCase, ESearchDir::FromEnd)));
                if (Folders.Num() > 0)
                {
                    for (const FString& Folder : Folders)
                    {
                        UE_LOG(LogMcpAutomationBridgeSubsystem, Log, TEXT("FixUpRedirectors: skipped redirector cleanup for %s (unsupported API)."), *Folder);
                    }
                }
            }
            const bool bSuccess = Failed.Num() == 0 && Deleted.Num() > 0;
            ResObj->SetBoolField(TEXT("success"), bSuccess);
            if (bSuccess) SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Assets deleted"), ResObj, FString());
            else SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Delete failed"), ResObj, TEXT("DELETE_FAILED"));
        });
        return true;
#else
        SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("Delete assets requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
        return true;
#endif
    }

    return false;
}
