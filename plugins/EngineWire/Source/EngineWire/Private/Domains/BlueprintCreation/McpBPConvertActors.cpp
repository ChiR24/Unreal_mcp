// manage_blueprint convert_actors_to_blueprint: placed actors turned into a Blueprint class, as the editor's Convert
// Selection to Blueprint Class does (a prefab). harvest (default) copies every component of every actor into one
// Blueprint; child_actors keeps each actor whole as a Child Actor component; subclass makes one actor's class a
// Blueprint subclass that keeps its property values. With replace (default) the actors in the level give way to one
// instance of the new Blueprint where they stood, the engine's one undo step. FKismetEditorUtilities does the work (the
// same calls in every 5.x) and never opens the Blueprint editor; the asset is saved through McpSafeAssetSave.

#include "Domains/Blueprint/McpBPActionContext.h"

#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SimpleConstructionScript.h"
#include "EngineWireSubsystem.h"
#include "Foundation/BridgeHelpers/McpHelpers.h"
#include "Foundation/HandlerUtils/McpHandlerUtils.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsActionsPaths.h"
#include "Foundation/HandlerUtils/McpHandlerUtilsJson.h"
#include "GameFramework/Actor.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Safety/McpSafeOperationsAssetSave.h"
#include "Selection.h"

namespace
{
// The engine's own three conversions; Path is the new package, the actors already checked.
UBlueprint* Convert(const FString& Mode, const FString& Path, const TArray<AActor*>& Actors, bool bReplace)
{
    if (Mode == TEXT("subclass"))
    {
        FKismetEditorUtilities::FCreateBlueprintFromActorParams Params;
        Params.bReplaceActor = bReplace;
        Params.bKeepMobility = true;
        Params.bOpenBlueprint = false;
        return FKismetEditorUtilities::CreateBlueprintFromActor(Path, Actors[0], Params);
    }
    if (Mode == TEXT("child_actors"))
    {
        FKismetEditorUtilities::FCreateBlueprintFromActorsParams Params(Actors);
        Params.bReplaceActors = bReplace;
        Params.bOpenBlueprint = false;
        return FKismetEditorUtilities::CreateBlueprintFromActors(Path, Params);
    }
    FKismetEditorUtilities::FHarvestBlueprintFromActorsParams Params;
    Params.bReplaceActors = bReplace;
    Params.bOpenBlueprint = false;
    return FKismetEditorUtilities::HarvestBlueprintFromActors(Path, Actors, Params);
}
} // namespace

namespace McpBlueprintHandlers
{
bool HandleBlueprintConvertActors(const FBlueprintActionContext& Context)
{
    MCP_BLUEPRINT_ACTION_LOCALS(Context);
    if (!ActionMatchesPattern(TEXT("convert_actors_to_blueprint")))
    {
        return false;
    }
    const auto Refuse = [&](const FString& Message, const TCHAR* Code)
    {
        Bridge.SendAutomationResponse(RequestingSocket, RequestId, false, Message + TEXT(" Nothing was created."), nullptr, Code);
        return true;
    };
    const TArray<FString> Names = McpHandlerUtils::GetStringArrayField(Payload, TEXT("actorNames"));
    const FString Name = GetJsonStringField(Payload, TEXT("name")).TrimStartAndEnd();
    const FString SaveAsGiven = GetJsonStringField(Payload, TEXT("savePath"), TEXT("/Game"));
    const FString SavePath = SanitizeProjectRelativePath(SaveAsGiven);
    const FString Mode = GetJsonStringField(Payload, TEXT("mode"), TEXT("harvest")).ToLower();
    const bool bReplace = GetJsonBoolField(Payload, TEXT("replace"), true);
    if (SavePath.IsEmpty())
    {
        return Refuse(McpPathRefusalMessage(TEXT("savePath"), SaveAsGiven), TEXT("INVALID_PATH"));
    }
    const FString Path = SavePath / Name;
    if (Name.IsEmpty() || Name.Contains(TEXT("/")) || !FPackageName::IsValidLongPackageName(Path))
    {
        return Refuse(FString::Printf(TEXT("name '%s' is not a valid asset name."), *Name), TEXT("INVALID_ARGUMENT"));
    }
    if (FPackageName::DoesPackageExist(Path) || FindObject<UPackage>(nullptr, *Path))
    {
        return Refuse(FString::Printf(TEXT("%s already exists; give the Blueprint another name."), *Path), TEXT("ASSET_ALREADY_EXISTS"));
    }
    if (!(Mode == TEXT("harvest") || Mode == TEXT("child_actors") || Mode == TEXT("subclass")))
    {
        return Refuse(TEXT("mode is harvest, child_actors or subclass."), TEXT("INVALID_ARGUMENT"));
    }
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    TArray<AActor*> Actors;
    TArray<FString> Labels;
    for (const FString& ActorName : Names)
    {
        AActor* Actor = World ? FindActorByNameInWorldForMcp(World, ActorName, true) : nullptr;
        if (!Actor)
        {
            return Refuse(FString::Printf(TEXT("No actor is labelled or named %s."), *ActorName), TEXT("ACTOR_NOT_FOUND"));
        }
        if (!Actors.Contains(Actor))
        {
            Actors.Add(Actor);
            Labels.Add(Actor->GetActorLabel());
        }
    }
    if (Mode == TEXT("subclass") && Actors.Num() != 1)
    {
        return Refuse(TEXT("subclass makes a Blueprint of one actor's class: name exactly one actor (harvest and child_actors take several)."),
            TEXT("INVALID_ARGUMENT"));
    }
    TSharedPtr<FJsonObject> Result = McpHandlerUtils::CreateResultObject();
    // No transaction around the call: one open while the Blueprint is made records its construction, and undoing that
    // half-reverts a compiled Blueprint (the compiler ensures). The engine replaces the actors under a transaction of its
    // own, after the Blueprint exists, which is the undo step the reply's undo block names, as in the editor.
    UBlueprint* Blueprint = Convert(Mode, Path, Actors, bReplace);
    if (!Blueprint)
    {
        return Refuse(TEXT("The engine could not make a Blueprint from these actors."), TEXT("CREATE_FAILED"));
    }
    // The engine selects the instance it placed where the actors stood.
    AActor* Instance = nullptr;
    for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); bReplace && It && !Instance; ++It)
    {
        AActor* Selected = Cast<AActor>(*It);
        Instance = Selected && Selected->GetClass() == Blueprint->GeneratedClass ? Selected : nullptr;
    }
    const bool bSaved = McpSafeOperations::McpSafeAssetSave(Blueprint);
    Result->SetStringField(TEXT("blueprintPath"), Blueprint->GetOutermost()->GetName());
    Result->SetStringField(TEXT("mode"), Mode);
    Result->SetArrayField(TEXT("actors"), McpHandlerUtils::ToJsonStringArray(Labels));
    Result->SetNumberField(TEXT("components"),
        Blueprint->SimpleConstructionScript ? Blueprint->SimpleConstructionScript->GetAllNodes().Num() : 0);
    if (Instance)
    {
        Result->SetStringField(TEXT("replacedWith"), Instance->GetActorLabel());
    }
    Result->SetBoolField(TEXT("saved"), bSaved);
    const FString Where = Instance ? FString::Printf(TEXT("; they are now one instance, %s"), *Instance->GetActorLabel()) : FString();
    Bridge.SendAutomationResponse(RequestingSocket, RequestId, true,
        FString::Printf(TEXT("%d actor(s) made into %s%s."), Labels.Num(), *Blueprint->GetOutermost()->GetName(), *Where), Result);
    return true;
}
} // namespace McpBlueprintHandlers
