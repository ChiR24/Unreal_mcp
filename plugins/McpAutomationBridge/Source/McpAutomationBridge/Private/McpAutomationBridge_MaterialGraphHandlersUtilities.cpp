#include "McpVersionCompatibility.h"

#include "McpAutomationBridge_MaterialGraphHandlersPrivate.h"

#include "McpAutomationBridgeHelpers.h"

#include "Dom/JsonObject.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"

namespace McpMaterialGraphHandlers
{
UMaterialExpression* FindExpressionByIdOrNameOrIndex(
    UMaterial& Material,
    const FString& IdOrName,
    int32 Index)
{
    if (Index >= 0)
    {
        const TArray<UMaterialExpression*>& Expressions = MCP_GET_MATERIAL_EXPRESSIONS((&Material));
        return Index < Expressions.Num() ? Expressions[Index] : nullptr;
    }

    if (IdOrName.IsEmpty())
    {
        return nullptr;
    }

    if (IdOrName.IsNumeric())
    {
        const int32 ParsedIndex = FCString::Atoi(*IdOrName);
        const TArray<UMaterialExpression*>& Expressions = MCP_GET_MATERIAL_EXPRESSIONS((&Material));
        if (ParsedIndex >= 0 && ParsedIndex < Expressions.Num())
        {
            return Expressions[ParsedIndex];
        }
    }

    const FString Needle = IdOrName.TrimStartAndEnd();
    FString ShortNeedle = Needle;
    int32 SeparatorIndex = INDEX_NONE;
    if (ShortNeedle.FindLastChar(TEXT(':'), SeparatorIndex))
    {
        ShortNeedle = ShortNeedle.Mid(SeparatorIndex + 1);
    }
    if (ShortNeedle.FindLastChar(TEXT('.'), SeparatorIndex))
    {
        ShortNeedle = ShortNeedle.Mid(SeparatorIndex + 1);
    }

    for (UMaterialExpression* Expr : MCP_GET_MATERIAL_EXPRESSIONS((&Material)))
    {
        if (!Expr)
        {
            continue;
        }
        if (Expr->MaterialExpressionGuid.ToString() == Needle ||
            Expr->GetName() == Needle ||
            Expr->GetName() == ShortNeedle ||
            Expr->GetPathName() == Needle)
        {
            return Expr;
        }
        if (UMaterialExpressionParameter* ParamExpr = Cast<UMaterialExpressionParameter>(Expr))
        {
            const FString ParamName = ParamExpr->ParameterName.ToString();
            if (ParamName == Needle || ParamName == ShortNeedle)
            {
                return Expr;
            }
        }
    }
    return nullptr;
}

UMaterialExpression* FindExpressionByPayload(
    UMaterial& Material,
    const TSharedPtr<FJsonObject>& Payload,
    const FString& IdField,
    const FString& IndexField)
{
    int32 Index = -1;
    if (Payload->TryGetNumberField(*IndexField, Index) && Index >= 0)
    {
        return FindExpressionByIdOrNameOrIndex(Material, FString(), Index);
    }

    FString IdOrName;
    if (Payload->TryGetStringField(*IdField, IdOrName) && !IdOrName.IsEmpty())
    {
        return FindExpressionByIdOrNameOrIndex(Material, IdOrName);
    }
    if (Payload->TryGetStringField(*IdField, IdOrName) && IdOrName.IsNumeric())
    {
        return FindExpressionByIdOrNameOrIndex(Material, IdOrName);
    }

    return nullptr;
}

UClass* ResolveGraphNodeClass(const FString& NodeType, bool bUseExtendedAliases)
{
    if (NodeType == TEXT("TextureSample")) { return UMaterialExpressionTextureSample::StaticClass(); }
    if (NodeType == TEXT("VectorParameter") ||
        (bUseExtendedAliases && NodeType == TEXT("ConstantVectorParameter")))
    {
        return UMaterialExpressionVectorParameter::StaticClass();
    }
    if (NodeType == TEXT("ScalarParameter") ||
        (bUseExtendedAliases && NodeType == TEXT("ConstantScalarParameter")))
    {
        return UMaterialExpressionScalarParameter::StaticClass();
    }
    if (NodeType == TEXT("Add")) { return UMaterialExpressionAdd::StaticClass(); }
    if (NodeType == TEXT("Multiply")) { return UMaterialExpressionMultiply::StaticClass(); }
    if (NodeType == TEXT("Constant") ||
        (bUseExtendedAliases && (NodeType == TEXT("Float") || NodeType == TEXT("Scalar"))))
    {
        return UMaterialExpressionConstant::StaticClass();
    }
    if (NodeType == TEXT("Constant3Vector") ||
        NodeType == TEXT("Color") ||
        (bUseExtendedAliases && (NodeType == TEXT("ConstantVector") || NodeType == TEXT("Vector3"))))
    {
        return UMaterialExpressionConstant3Vector::StaticClass();
    }

    UClass* ExpressionClass = ResolveClassByName(NodeType);
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = ResolveClassByName(PrefixedName);
    }
    return ExpressionClass && ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())
        ? ExpressionClass
        : nullptr;
}

UClass* ResolveBatchNodeClass(const FString& NodeType)
{
    if (NodeType == TEXT("TextureSample")) { return UMaterialExpressionTextureSample::StaticClass(); }
    if (NodeType == TEXT("VectorParameter")) { return UMaterialExpressionVectorParameter::StaticClass(); }
    if (NodeType == TEXT("ScalarParameter")) { return UMaterialExpressionScalarParameter::StaticClass(); }
    if (NodeType == TEXT("Add")) { return UMaterialExpressionAdd::StaticClass(); }
    if (NodeType == TEXT("Multiply")) { return UMaterialExpressionMultiply::StaticClass(); }
    if (NodeType == TEXT("Constant")) { return UMaterialExpressionConstant::StaticClass(); }
    if (NodeType == TEXT("Constant3Vector") || NodeType == TEXT("Color"))
    {
        return UMaterialExpressionConstant3Vector::StaticClass();
    }

    UClass* ExpressionClass = FindObject<UClass>(
        nullptr,
        *FString::Printf(TEXT("/Script/Engine.%s"), *NodeType));
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *NodeType);
        ExpressionClass = FindObject<UClass>(
            nullptr,
            *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
    }
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        ExpressionClass = ResolveClassByName(NodeType);
    }
    return ExpressionClass && ExpressionClass->IsChildOf(UMaterialExpression::StaticClass())
        ? ExpressionClass
        : nullptr;
}

UClass* ResolveExpressionClass(const FString& ExpressionClassName)
{
    UClass* ExpressionClass = FindObject<UClass>(
        nullptr,
        *FString::Printf(TEXT("/Script/Engine.%s"), *ExpressionClassName));

    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
        ExpressionClass = FindObject<UClass>(
            nullptr,
            *FString::Printf(TEXT("/Script/Engine.%s"), *PrefixedName));
    }
    if (!ExpressionClass || !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
    {
        ExpressionClass = ResolveClassByName(ExpressionClassName);
        if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
        {
            ExpressionClass = nullptr;
        }
    }
    if (!ExpressionClass)
    {
        const FString PrefixedName = FString::Printf(TEXT("MaterialExpression%s"), *ExpressionClassName);
        ExpressionClass = ResolveClassByName(PrefixedName);
        if (ExpressionClass && !ExpressionClass->IsChildOf(UMaterialExpression::StaticClass()))
        {
            ExpressionClass = nullptr;
        }
    }
    return ExpressionClass;
}

void AddExpressionToMaterial(UMaterial& Material, UMaterialExpression& Expression)
{
#if WITH_EDITORONLY_DATA
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 1
    if (Material.GetEditorOnlyData())
    {
        MCP_GET_MATERIAL_EXPRESSIONS((&Material)).Add(&Expression);
    }
#else
    Material.Expressions.Add(&Expression);
#endif
#endif
}

bool SetMainMaterialInputExpression(
    UMaterial& Material,
    const FString& InputName,
    UMaterialExpression* Expression)
{
#if WITH_EDITORONLY_DATA
    if (InputName == TEXT("BaseColor")) { MCP_GET_MATERIAL_INPUT((&Material), BaseColor).Expression = Expression; return true; }
    if (InputName == TEXT("EmissiveColor")) { MCP_GET_MATERIAL_INPUT((&Material), EmissiveColor).Expression = Expression; return true; }
    if (InputName == TEXT("Roughness")) { MCP_GET_MATERIAL_INPUT((&Material), Roughness).Expression = Expression; return true; }
    if (InputName == TEXT("Metallic")) { MCP_GET_MATERIAL_INPUT((&Material), Metallic).Expression = Expression; return true; }
    if (InputName == TEXT("Specular")) { MCP_GET_MATERIAL_INPUT((&Material), Specular).Expression = Expression; return true; }
    if (InputName == TEXT("Normal")) { MCP_GET_MATERIAL_INPUT((&Material), Normal).Expression = Expression; return true; }
    if (InputName == TEXT("Opacity")) { MCP_GET_MATERIAL_INPUT((&Material), Opacity).Expression = Expression; return true; }
    if (InputName == TEXT("OpacityMask")) { MCP_GET_MATERIAL_INPUT((&Material), OpacityMask).Expression = Expression; return true; }
    if (InputName == TEXT("AmbientOcclusion")) { MCP_GET_MATERIAL_INPUT((&Material), AmbientOcclusion).Expression = Expression; return true; }
    if (InputName == TEXT("SubsurfaceColor")) { MCP_GET_MATERIAL_INPUT((&Material), SubsurfaceColor).Expression = Expression; return true; }
#endif
    return false;
}
}
#endif
