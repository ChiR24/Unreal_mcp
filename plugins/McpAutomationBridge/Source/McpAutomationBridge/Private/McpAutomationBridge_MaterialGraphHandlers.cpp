#include "McpAutomationBridgeSubsystem.h"
#include "McpAutomationBridgeHelpers.h"
#include "McpAutomationBridgeGlobals.h"
#include "Async/Async.h"

#if WITH_EDITOR
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionMultiply.h"
#include "Materials/MaterialExpressionAdd.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Engine/Texture2D.h"
#include "EditorAssetLibrary.h"
#include "MaterialEditingLibrary.h"
#endif

bool UMcpAutomationBridgeSubsystem::HandleCreateMaterialNodes(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("create_material_nodes"), ESearchCase::IgnoreCase)) { return false; }
#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("create_material_nodes payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    FString MaterialPath; if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty()) { SendAutomationError(RequestingSocket, RequestId, TEXT("materialPath required"), TEXT("INVALID_ARGUMENT")); return true; }

    // Operation: scalarParameter or vectorParameter
    const TSharedPtr<FJsonObject>* ScalarParam = nullptr;
    const TSharedPtr<FJsonObject>* VectorParam = nullptr;
    Payload->TryGetObjectField(TEXT("scalarParameter"), ScalarParam);
    Payload->TryGetObjectField(TEXT("vectorParameter"), VectorParam);

    TSharedPtr<FJsonObject> ScalarParamCopy = ScalarParam ? *ScalarParam : TSharedPtr<FJsonObject>();
    TSharedPtr<FJsonObject> VectorParamCopy = VectorParam ? *VectorParam : TSharedPtr<FJsonObject>();

    UMaterial* Mat = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Mat)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load material"), TEXT("LOAD_FAILED"));
        return true;
    }
    Mat->Modify();

    const bool bScalarRequested = ScalarParamCopy.IsValid();
    const bool bVectorRequested = VectorParamCopy.IsValid();
    bool bScalarCreated = false;
    bool bVectorCreated = false;
    TSharedPtr<FJsonObject> Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("materialPath"), MaterialPath);

    auto ConnectToProperty = [&](UMaterialExpression* Expr, const FString& PropertyName)
    {
        const FString P = PropertyName.ToLower();
        if (P == TEXT("basecolor")) { UMaterialEditingLibrary::ConnectMaterialProperty(Expr, FString(), EMaterialProperty::MP_BaseColor); return true; }
        if (P == TEXT("emissivecolor")) { UMaterialEditingLibrary::ConnectMaterialProperty(Expr, FString(), EMaterialProperty::MP_EmissiveColor); return true; }
        if (P == TEXT("roughness")) { UMaterialEditingLibrary::ConnectMaterialProperty(Expr, FString(), EMaterialProperty::MP_Roughness); return true; }
        if (P == TEXT("metallic")) { UMaterialEditingLibrary::ConnectMaterialProperty(Expr, FString(), EMaterialProperty::MP_Metallic); return true; }
        if (P == TEXT("opacity")) { UMaterialEditingLibrary::ConnectMaterialProperty(Expr, FString(), EMaterialProperty::MP_Opacity); return true; }
        if (P == TEXT("normal")) { UMaterialEditingLibrary::ConnectMaterialProperty(Expr, FString(), EMaterialProperty::MP_Normal); return true; }
        return false;
    };

    if (bScalarRequested)
    {
        FString Name;
        ScalarParamCopy->TryGetStringField(TEXT("name"), Name);
        double DefaultValue = 0.0;
        ScalarParamCopy->TryGetNumberField(TEXT("default"), DefaultValue);
        FString ConnectTo;
        ScalarParamCopy->TryGetStringField(TEXT("connectTo"), ConnectTo);

        UMaterialExpressionScalarParameter* Node = Cast<UMaterialExpressionScalarParameter>(
            UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionScalarParameter::StaticClass(), 100, 100));
        if (Node)
        {
            Node->ParameterName = FName(*Name);
            Node->DefaultValue = static_cast<float>(DefaultValue);
            if (!ConnectTo.IsEmpty())
            {
                ConnectToProperty(Node, ConnectTo);
            }
            bScalarCreated = true;
        }
        else
        {
            Out->SetStringField(TEXT("scalarError"), TEXT("Failed to create scalar parameter expression"));
        }
    }
    if (bVectorRequested)
    {
        FString Name;
        VectorParamCopy->TryGetStringField(TEXT("name"), Name);
        const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
        VectorParamCopy->TryGetArrayField(TEXT("default"), Arr);
        FLinearColor Color(0, 0, 0, 1);
        if (Arr && Arr->Num() >= 3)
        {
            Color.R = static_cast<float>((*Arr)[0]->AsNumber());
            Color.G = static_cast<float>((*Arr)[1]->AsNumber());
            Color.B = static_cast<float>((*Arr)[2]->AsNumber());
            if (Arr->Num() > 3)
            {
                Color.A = static_cast<float>((*Arr)[3]->AsNumber());
            }
        }
        FString ConnectTo;
        VectorParamCopy->TryGetStringField(TEXT("connectTo"), ConnectTo);

        UMaterialExpressionVectorParameter* Node = Cast<UMaterialExpressionVectorParameter>(
            UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionVectorParameter::StaticClass(), 100, 200));
        if (Node)
        {
            Node->ParameterName = FName(*Name);
            Node->DefaultValue = Color;
            if (!ConnectTo.IsEmpty())
            {
                ConnectToProperty(Node, ConnectTo);
            }
            bVectorCreated = true;
        }
        else
        {
            Out->SetStringField(TEXT("vectorError"), TEXT("Failed to create vector parameter expression"));
        }
    }

    if (bScalarRequested)
    {
        Out->SetBoolField(TEXT("scalarCreated"), bScalarCreated);
    }
    if (bVectorRequested)
    {
        Out->SetBoolField(TEXT("vectorCreated"), bVectorCreated);
    }

    const bool bDidWork = bScalarCreated || bVectorCreated;

    if (bDidWork)
    {
        Mat->PostEditChange();
        Mat->MarkPackageDirty();
    }

    if (!bDidWork)
    {
        const bool bRequestedAny = bScalarRequested || bVectorRequested;
        SendAutomationResponse(RequestingSocket, RequestId, false, bRequestedAny ? TEXT("Failed to create requested material nodes") : TEXT("No operations performed"), Out, bRequestedAny ? TEXT("CREATE_MATERIAL_NODES_FAILED") : TEXT("NO_OP"));
        return true;
    }

    Out->SetBoolField(TEXT("success"), true);
    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Material nodes created"), Out, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("create_material_nodes requires editor build."), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialTextureSample(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_material_texture_sample"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("add_material_texture_sample payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("materialPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString TexturePath;
    if (!Payload->TryGetStringField(TEXT("texturePath"), TexturePath) || TexturePath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("texturePath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ConnectTo;
    Payload->TryGetStringField(TEXT("connectTo"), ConnectTo);

    UMaterial* Mat = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Mat)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load material"), TEXT("LOAD_FAILED"));
        return true;
    }

    UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *TexturePath);
    if (!Texture)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load texture"), TEXT("TEXTURE_LOAD_FAILED"));
        return true;
    }

    Mat->Modify();

    UMaterialExpressionTextureSample* TexSample = Cast<UMaterialExpressionTextureSample>(
        UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionTextureSample::StaticClass(), 300, 100));
    if (!TexSample)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to create texture sample expression"), TEXT("CREATE_EXPRESSION_FAILED"));
        return true;
    }

    TexSample->Texture = Texture;

    bool bConnected = false;
    if (!ConnectTo.IsEmpty())
    {
        const FString P = ConnectTo.ToLower();
        if (P == TEXT("basecolor")) { UMaterialEditingLibrary::ConnectMaterialProperty(TexSample, FString(), EMaterialProperty::MP_BaseColor); bConnected = true; }
        else if (P == TEXT("emissivecolor")) { UMaterialEditingLibrary::ConnectMaterialProperty(TexSample, FString(), EMaterialProperty::MP_EmissiveColor); bConnected = true; }
        else if (P == TEXT("roughness")) { UMaterialEditingLibrary::ConnectMaterialProperty(TexSample, FString(), EMaterialProperty::MP_Roughness); bConnected = true; }
        else if (P == TEXT("metallic")) { UMaterialEditingLibrary::ConnectMaterialProperty(TexSample, FString(), EMaterialProperty::MP_Metallic); bConnected = true; }
        else if (P == TEXT("normal")) { UMaterialEditingLibrary::ConnectMaterialProperty(TexSample, FString(), EMaterialProperty::MP_Normal); bConnected = true; }
    }

    Mat->PostEditChange();
    Mat->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("materialPath"), MaterialPath);
    Resp->SetStringField(TEXT("texturePath"), TexturePath);
    Resp->SetBoolField(TEXT("connected"), bConnected);
    if (bConnected)
    {
        Resp->SetStringField(TEXT("connectedTo"), ConnectTo);
    }

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Texture sample node added"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("add_material_texture_sample requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}

bool UMcpAutomationBridgeSubsystem::HandleAddMaterialExpression(
    const FString& RequestId,
    const FString& Action,
    const TSharedPtr<FJsonObject>& Payload,
    TSharedPtr<FMcpBridgeWebSocket> RequestingSocket)
{
    const FString Lower = Action.ToLower();
    if (!Lower.Equals(TEXT("add_material_expression"), ESearchCase::IgnoreCase)) { return false; }

#if WITH_EDITOR
    if (!Payload.IsValid()) { SendAutomationError(RequestingSocket, RequestId, TEXT("add_material_expression payload missing"), TEXT("INVALID_PAYLOAD")); return true; }
    
    FString MaterialPath;
    if (!Payload->TryGetStringField(TEXT("materialPath"), MaterialPath) || MaterialPath.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("materialPath required"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    FString ExpressionType;
    if (!Payload->TryGetStringField(TEXT("expressionType"), ExpressionType) || ExpressionType.IsEmpty())
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("expressionType required (Multiply, Add, Lerp)"), TEXT("INVALID_ARGUMENT"));
        return true;
    }

    UMaterial* Mat = LoadObject<UMaterial>(nullptr, *MaterialPath);
    if (!Mat)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Failed to load material"), TEXT("LOAD_FAILED"));
        return true;
    }

    Mat->Modify();

    UMaterialExpression* NewExpression = nullptr;
    const FString Type = ExpressionType.ToLower();

    if (Type == TEXT("multiply"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionMultiply::StaticClass(), 500, 200);
    }
    else if (Type == TEXT("add"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionAdd::StaticClass(), 500, 200);
    }
    else if (Type == TEXT("lerp") || Type == TEXT("linearinterpolate"))
    {
        NewExpression = UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionLinearInterpolate::StaticClass(), 500, 200);
    }
    else if (Type == TEXT("constant"))
    {
        UMaterialExpressionConstant* Const = Cast<UMaterialExpressionConstant>(UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant::StaticClass(), 500, 200));
        if (Const)
        {
            Const->R = 1.0f;
            NewExpression = Const;
        }
    }
    else if (Type == TEXT("constant3vector"))
    {
        UMaterialExpressionConstant3Vector* Vec = Cast<UMaterialExpressionConstant3Vector>(UMaterialEditingLibrary::CreateMaterialExpression(Mat, UMaterialExpressionConstant3Vector::StaticClass(), 500, 200));
        if (Vec)
        {
            Vec->Constant = FLinearColor(1.0f, 1.0f, 1.0f);
            NewExpression = Vec;
        }
    }

    if (!NewExpression)
    {
        SendAutomationError(RequestingSocket, RequestId, TEXT("Unsupported expression type"), TEXT("UNSUPPORTED_TYPE"));
        return true;
    }

    Mat->PostEditChange();
    Mat->MarkPackageDirty();

    TSharedPtr<FJsonObject> Resp = MakeShared<FJsonObject>();
    Resp->SetBoolField(TEXT("success"), true);
    Resp->SetStringField(TEXT("materialPath"), MaterialPath);
    Resp->SetStringField(TEXT("expressionType"), ExpressionType);
    Resp->SetStringField(TEXT("expressionGuid"), NewExpression->MaterialExpressionGuid.ToString());

    SendAutomationResponse(RequestingSocket, RequestId, true, TEXT("Material expression node added"), Resp, FString());
    return true;
#else
    SendAutomationResponse(RequestingSocket, RequestId, false, TEXT("add_material_expression requires editor build"), nullptr, TEXT("NOT_IMPLEMENTED"));
    return true;
#endif
}
