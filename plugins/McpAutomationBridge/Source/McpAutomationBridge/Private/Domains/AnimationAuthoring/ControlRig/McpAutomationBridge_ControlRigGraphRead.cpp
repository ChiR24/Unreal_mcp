#include "Domains/AnimationAuthoring/ControlRig/McpAutomationBridge_ControlRigGraph.h"

#if MCP_HAS_CONTROLRIG_BLUEPRINT
#include "RigVMCore/RigVMStruct.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMLink.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "Rigs/RigHierarchy.h"
#include "UObject/UObjectIterator.h"
#endif

// get_control_rig reads a rig's graph (nodes, pins, links) and hierarchy; list_rig_units finds the unit structs a
// graph can hold. Nothing else could read a Control Rig: a caller wired pins by guessing their names.
namespace McpAnimationAuthoring {
#if MCP_HAS_CONTROLRIG_BLUEPRINT
namespace {
constexpr int32 McpMaxRigElements = 500;

// Units the editor's menu offers: rig structs that are neither abstract, deprecated nor hidden.
bool McpIsListedRigUnit(const UScriptStruct* Struct)
{
    return Struct->IsChildOf(FRigVMStruct::StaticStruct()) && !Struct->HasMetaData(TEXT("Abstract")) &&
           !Struct->HasMetaData(TEXT("Deprecated")) && !Struct->HasMetaData(TEXT("Hidden"));
}

void McpAddRigPin(const URigVMPin* Pin, bool bSubPins, TArray<TSharedPtr<FJsonValue>>& Out)
{
    const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
    Item->SetStringField(TEXT("pin"), Pin->GetPinPath());
    Item->SetStringField(TEXT("direction"), StaticEnum<ERigVMPinDirection>()->GetNameStringByValue(static_cast<int64>(Pin->GetDirection())));
    Item->SetStringField(TEXT("type"), Pin->GetCPPType());
    const FString Default = Pin->GetDefaultValue();
    if (!Default.IsEmpty() && Pin->GetDirection() != ERigVMPinDirection::Output)
    {
        Item->SetStringField(TEXT("default"), Default);
    }
    if (Pin->GetLinks().Num() > 0)
    {
        Item->SetBoolField(TEXT("linked"), true);
    }
    Out.Add(MakeShared<FJsonValueObject>(Item));
    if (bSubPins)
    {
        for (const URigVMPin* Sub : Pin->GetSubPins())
        {
            McpAddRigPin(Sub, true, Out);
        }
    }
}

TSharedPtr<FJsonObject> HandleGetControlRig(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    FString Error;
    UControlRigBlueprint* Rig = McpLoadControlRig(GetJsonStringField(Params, TEXT("assetPath")), Error);
    if (!Rig)
    {
        ANIM_ERROR_RESPONSE(Error, TEXT("NOT_FOUND"));
    }
    const FString Only = GetJsonStringField(Params, TEXT("node"));
    URigVMGraph* Graph = Rig->GetModel();
    TArray<TSharedPtr<FJsonValue>> Nodes;
    TArray<TSharedPtr<FJsonValue>> Names;
    for (URigVMNode* Node : Graph ? Graph->GetNodes() : TArray<URigVMNode*>())
    {
        Names.Add(MakeShared<FJsonValueString>(Node->GetName()));
        if (!Only.IsEmpty() && Node->GetName() != Only)
        {
            continue;
        }
        const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("name"), Node->GetName());
        Item->SetStringField(TEXT("title"), Node->GetNodeTitle());
        const URigVMUnitNode* Unit = Cast<URigVMUnitNode>(Node);
        if (Unit && Unit->GetScriptStruct())
        {
            Item->SetStringField(TEXT("unit"), Unit->GetScriptStruct()->GetPathName());
        }
        const FVector2D Position = Node->GetPosition();
        TArray<TSharedPtr<FJsonValue>> At;
        At.Add(MakeShared<FJsonValueNumber>(Position.X));
        At.Add(MakeShared<FJsonValueNumber>(Position.Y));
        Item->SetArrayField(TEXT("position"), At);
        Item->SetArrayField(TEXT("pins"), McpDescribeRigPins(Node, !Only.IsEmpty()));
        Nodes.Add(MakeShared<FJsonValueObject>(Item));
    }
    if (!Only.IsEmpty() && Nodes.Num() == 0)
    {
        Response->SetArrayField(TEXT("nodeNames"), Names);
        ANIM_ERROR_RESPONSE(FString::Printf(TEXT("node '%s' is not in the rig's graph; nodeNames lists the nodes."), *Only), TEXT("NODE_NOT_FOUND"));
    }
    TArray<TSharedPtr<FJsonValue>> Links;
    for (URigVMLink* Link : Graph && Only.IsEmpty() ? Graph->GetLinks() : TArray<URigVMLink*>())
    {
        if (Link && Link->GetSourcePin() && Link->GetTargetPin())
        {
            const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("from"), Link->GetSourcePin()->GetPinPath());
            Item->SetStringField(TEXT("to"), Link->GetTargetPin()->GetPinPath());
            Links.Add(MakeShared<FJsonValueObject>(Item));
        }
    }
    TArray<TSharedPtr<FJsonValue>> Elements;
    const TArray<FRigElementKey> Keys = Rig->Hierarchy ? Rig->Hierarchy->GetAllKeys(true) : TArray<FRigElementKey>();
    for (const FRigElementKey& Key : Keys)
    {
        if (!Only.IsEmpty() || Elements.Num() == McpMaxRigElements)
        {
            break;
        }
        const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("type"), StaticEnum<ERigElementType>()->GetNameStringByValue(static_cast<int64>(Key.Type)));
        Item->SetStringField(TEXT("name"), Key.Name.ToString());
        const FRigElementKey Parent = Rig->Hierarchy->GetFirstParent(Key);
        if (Parent.IsValid())
        {
            Item->SetStringField(TEXT("parent"), Parent.Name.ToString());
        }
        Elements.Add(MakeShared<FJsonValueObject>(Item));
    }
    Response->SetStringField(TEXT("assetPath"), Rig->GetPathName());
    Response->SetArrayField(TEXT("nodes"), Nodes);
    if (Only.IsEmpty())
    {
        Response->SetArrayField(TEXT("links"), Links);
        Response->SetArrayField(TEXT("elements"), Elements);
        Response->SetNumberField(TEXT("elementCount"), Keys.Num());
    }
    Response->SetBoolField(TEXT("compiled"), Rig->Status == BS_UpToDate || Rig->Status == BS_UpToDateWithWarnings);
    McpHandlerUtils::MarkNoAssetsChanged(Response);
    ANIM_SUCCESS_RESPONSE(Only.IsEmpty()
        ? FString::Printf(TEXT("%s: %d node(s), %d link(s), %d hierarchy element(s)."), *Rig->GetName(), Nodes.Num(), Links.Num(), Keys.Num())
        : FString::Printf(TEXT("%s in %s, with every member pin."), *Only, *Rig->GetName()));
    return Response;
}

TSharedPtr<FJsonObject> HandleListRigUnits(const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    const FString Query = GetJsonStringField(Params, TEXT("query")).Replace(TEXT(" "), TEXT(""));
    const int32 Limit = FMath::Clamp(GetJsonIntField(Params, TEXT("limit"), 50), 1, 500);
    TArray<UScriptStruct*> Found;
    for (TObjectIterator<UScriptStruct> It; It; ++It)
    {
        const FString Text = It->GetName() + It->GetMetaData(TEXT("DisplayName")).Replace(TEXT(" "), TEXT("")) +
                             It->GetMetaData(TEXT("Category")) + It->GetMetaData(TEXT("Keywords"));
        if (McpIsListedRigUnit(*It) && (Query.IsEmpty() || Text.Contains(Query, ESearchCase::IgnoreCase)))
        {
            Found.Add(*It);
        }
    }
    Found.Sort([](const UScriptStruct& A, const UScriptStruct& B) { return A.GetName() < B.GetName(); });
    TArray<TSharedPtr<FJsonValue>> Units;
    for (const UScriptStruct* Struct : Found)
    {
        if (Units.Num() == Limit)
        {
            break;
        }
        const TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("unit"), Struct->GetPathName());
        const FString Display = Struct->GetMetaData(TEXT("DisplayName"));
        Item->SetStringField(TEXT("name"), Display.IsEmpty() ? Struct->GetName() : Display);
        if (Struct->HasMetaData(TEXT("Category")))
        {
            Item->SetStringField(TEXT("category"), Struct->GetMetaData(TEXT("Category")));
        }
        Units.Add(MakeShared<FJsonValueObject>(Item));
    }
    Response->SetArrayField(TEXT("units"), Units);
    Response->SetNumberField(TEXT("matched"), Found.Num());
    McpHandlerUtils::MarkNoAssetsChanged(Response);
    ANIM_SUCCESS_RESPONSE(FString::Printf(TEXT("%d rig unit(s) match."), Found.Num()));
    return Response;
}
} // namespace

UControlRigBlueprint* McpLoadControlRig(const FString& AssetPath, FString& OutError)
{
    const FString SafePath = SanitizeProjectRelativePath(AssetPath);
    UControlRigBlueprint* Rig = SafePath.IsEmpty() ? nullptr : Cast<UControlRigBlueprint>(McpLoadAsset(SafePath));
    if (!Rig)
    {
        OutError = FString::Printf(TEXT("assetPath '%s' is not a Control Rig."), *AssetPath);
    }
    return Rig;
}

TArray<TSharedPtr<FJsonValue>> McpDescribeRigPins(const URigVMNode* Node, bool bSubPins)
{
    TArray<TSharedPtr<FJsonValue>> Pins;
    for (const URigVMPin* Pin : Node->GetPins())
    {
        McpAddRigPin(Pin, bSubPins, Pins);
    }
    return Pins;
}

UScriptStruct* McpFindRigUnitStruct(const FString& Name)
{
    if (Name.StartsWith(TEXT("/")))
    {
        UScriptStruct* Struct = FindObject<UScriptStruct>(nullptr, *Name);
        return Struct && Struct->IsChildOf(FRigVMStruct::StaticStruct()) ? Struct : nullptr;
    }
    const FString Bare = Name.Replace(TEXT(" "), TEXT(""));
    for (TObjectIterator<UScriptStruct> It; It; ++It)
    {
        const FString StructName = It->GetName();
        if (McpIsListedRigUnit(*It) && (StructName.Equals(Bare, ESearchCase::IgnoreCase) ||
                                        StructName.Equals(TEXT("RigUnit_") + Bare, ESearchCase::IgnoreCase) ||
                                        StructName.Equals(TEXT("RigVMFunction_") + Bare, ESearchCase::IgnoreCase) ||
                                        It->GetMetaData(TEXT("DisplayName")).Replace(TEXT(" "), TEXT("")).Equals(Bare, ESearchCase::IgnoreCase)))
        {
            return *It;
        }
    }
    return nullptr;
}
#endif

TSharedPtr<FJsonObject> HandleControlRigGraphActions(const FString& SubAction, const TSharedPtr<FJsonObject>& Params, TSharedPtr<FJsonObject> Response)
{
    if (SubAction != TEXT("edit_control_rig") && SubAction != TEXT("get_control_rig") && SubAction != TEXT("list_rig_units"))
    {
        return nullptr;
    }
#if MCP_HAS_CONTROLRIG_BLUEPRINT
    if (SubAction == TEXT("edit_control_rig"))
    {
        return HandleEditControlRig(Params, Response);
    }
    return SubAction == TEXT("get_control_rig") ? HandleGetControlRig(Params, Response) : HandleListRigUnits(Params, Response);
#else
    ANIM_ERROR_RESPONSE(TEXT("Control Rig is not available in this editor (the Control Rig plugin is off)."), TEXT("NOT_SUPPORTED"));
#endif
}

} // namespace McpAnimationAuthoring
