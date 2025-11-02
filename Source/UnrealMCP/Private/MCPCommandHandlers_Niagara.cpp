#include "MCPCommandHandlers_Niagara.h"

#include "MCPFileLogger.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Factories/NiagaraSystemFactoryNew.h"
#include "NiagaraEmitter.h"
#include "NiagaraEmitterHandle.h"
#include "NiagaraParameterStore.h"
#include "NiagaraSystem.h"
#include "NiagaraTypes.h"
#include "UObject/SavePackage.h"

#include "Modules/ModuleManager.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"

namespace
{
    FString BuildObjectPath(const FString& PackagePath, const FString& AssetName)
    {
        if (PackagePath.EndsWith(TEXT("/")))
        {
            return PackagePath + AssetName + TEXT(".") + AssetName;
        }
        return PackagePath + TEXT("/") + AssetName + TEXT(".") + AssetName;
    }

    bool EnsureParameterExists(FNiagaraParameterStore& ParameterStore, const FNiagaraVariable& Variable)
    {
        if (!ParameterStore.HasParameter(Variable))
        {
            if (!ParameterStore.AddParameter(Variable))
            {
                return false;
            }
        }
        return true;
    }

    bool ApplyUserParameters(UNiagaraSystem* NiagaraSystem, const TSharedPtr<FJsonObject>& UserParamsJson, TArray<FString>& WarningsOut)
    {
        if (!NiagaraSystem || !UserParamsJson.IsValid())
        {
            return false;
        }

        bool bAppliedAnyChanges = false;

        FNiagaraParameterStore& ParameterStore = NiagaraSystem->GetExposedParameters();
        for (const auto& ParamPair : UserParamsJson->Values)
        {
            const FString& ParamName = ParamPair.Key;
            const TSharedPtr<FJsonValue> Value = ParamPair.Value;
            if (!Value.IsValid() || Value->Type != EJson::Object)
            {
                WarningsOut.Add(FString::Printf(TEXT("User parameter '%s' must be an object with 'type' and 'value'"), *ParamName));
                continue;
            }

            TSharedPtr<FJsonObject> ParamObject = Value->AsObject();
            FString TypeString;
            if (!ParamObject->TryGetStringField(TEXT("type"), TypeString))
            {
                WarningsOut.Add(FString::Printf(TEXT("User parameter '%s' is missing a 'type' field"), *ParamName));
                continue;
            }

            FString NormalizedType = TypeString;
            NormalizedType.TrimStartAndEndInline();
            NormalizedType = NormalizedType.ToLower();

            auto RequireNumberValue = [&ParamObject, &ParamName, &WarningsOut](double& OutValue) -> bool
            {
                if (!ParamObject->TryGetNumberField(TEXT("value"), OutValue))
                {
                    WarningsOut.Add(FString::Printf(TEXT("User parameter '%s' expected numeric 'value'"), *ParamName));
                    return false;
                }
                return true;
            };

            auto RequireBoolValue = [&ParamObject, &ParamName, &WarningsOut](bool& OutValue) -> bool
            {
                if (!ParamObject->TryGetBoolField(TEXT("value"), OutValue))
                {
                    WarningsOut.Add(FString::Printf(TEXT("User parameter '%s' expected boolean 'value'"), *ParamName));
                    return false;
                }
                return true;
            };

            auto RequireArrayValue = [&ParamObject, &ParamName, &WarningsOut](int32 ExpectedComponents, const TArray<TSharedPtr<FJsonValue>>*& OutArray) -> bool
            {
                if (!ParamObject->TryGetArrayField(TEXT("value"), OutArray) || !OutArray)
                {
                    WarningsOut.Add(FString::Printf(TEXT("User parameter '%s' expected an array 'value'"), *ParamName));
                    return false;
                }
                if (OutArray->Num() < ExpectedComponents)
                {
                    WarningsOut.Add(FString::Printf(TEXT("User parameter '%s' expected %d components but received %d"), *ParamName, ExpectedComponents, OutArray->Num()));
                    return false;
                }
                return true;
            };

            const TCHAR* ParamTCharName = *ParamName;

            if (NormalizedType == TEXT("float"))
            {
                double RawValue = 0.0;
                if (!RequireNumberValue(RawValue))
                {
                    continue;
                }

                FNiagaraVariable Variable(FNiagaraTypeDefinition::GetFloatDef(), ParamTCharName);
                if (EnsureParameterExists(ParameterStore, Variable))
                {
                    ParameterStore.SetParameterValue(static_cast<float>(RawValue), Variable);
                    bAppliedAnyChanges = true;
                }
            }
            else if (NormalizedType == TEXT("int") || NormalizedType == TEXT("integer"))
            {
                double RawValue = 0.0;
                if (!RequireNumberValue(RawValue))
                {
                    continue;
                }

                FNiagaraVariable Variable(FNiagaraTypeDefinition::GetIntDef(), ParamTCharName);
                if (EnsureParameterExists(ParameterStore, Variable))
                {
                    const int32 IntValue = FMath::RoundToInt(static_cast<float>(RawValue));
                    ParameterStore.SetParameterValue(IntValue, Variable);
                    bAppliedAnyChanges = true;
                }
            }
            else if (NormalizedType == TEXT("bool") || NormalizedType == TEXT("boolean"))
            {
                bool bValue = false;
                if (!RequireBoolValue(bValue))
                {
                    continue;
                }

                FNiagaraVariable Variable(FNiagaraTypeDefinition::GetBoolDef(), ParamTCharName);
                if (EnsureParameterExists(ParameterStore, Variable))
                {
                    ParameterStore.SetParameterValue(bValue, Variable);
                    bAppliedAnyChanges = true;
                }
            }
            else if (NormalizedType == TEXT("vector2") || NormalizedType == TEXT("vec2"))
            {
                const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
                if (!RequireArrayValue(2, ArrayValue))
                {
                    continue;
                }

                FVector2f VecValue(
                    static_cast<float>((*ArrayValue)[0]->AsNumber()),
                    static_cast<float>((*ArrayValue)[1]->AsNumber()));

                FNiagaraVariable Variable(FNiagaraTypeDefinition::GetVec2Def(), ParamTCharName);
                if (EnsureParameterExists(ParameterStore, Variable))
                {
                    ParameterStore.SetParameterValue(VecValue, Variable);
                    bAppliedAnyChanges = true;
                }
            }
            else if (NormalizedType == TEXT("vector3") || NormalizedType == TEXT("vec3"))
            {
                const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
                if (!RequireArrayValue(3, ArrayValue))
                {
                    continue;
                }

                FVector3f VecValue(
                    static_cast<float>((*ArrayValue)[0]->AsNumber()),
                    static_cast<float>((*ArrayValue)[1]->AsNumber()),
                    static_cast<float>((*ArrayValue)[2]->AsNumber()));

                FNiagaraVariable Variable(FNiagaraTypeDefinition::GetVec3Def(), ParamTCharName);
                if (EnsureParameterExists(ParameterStore, Variable))
                {
                    ParameterStore.SetParameterValue(VecValue, Variable);
                    bAppliedAnyChanges = true;
                }
            }
            else if (NormalizedType == TEXT("vector4") || NormalizedType == TEXT("vec4"))
            {
                const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
                if (!RequireArrayValue(4, ArrayValue))
                {
                    continue;
                }

                FVector4f VecValue(
                    static_cast<float>((*ArrayValue)[0]->AsNumber()),
                    static_cast<float>((*ArrayValue)[1]->AsNumber()),
                    static_cast<float>((*ArrayValue)[2]->AsNumber()),
                    static_cast<float>((*ArrayValue)[3]->AsNumber()));

                FNiagaraVariable Variable(FNiagaraTypeDefinition::GetVec4Def(), ParamTCharName);
                if (EnsureParameterExists(ParameterStore, Variable))
                {
                    ParameterStore.SetParameterValue(VecValue, Variable);
                    bAppliedAnyChanges = true;
                }
            }
            else if (NormalizedType == TEXT("color") || NormalizedType == TEXT("linearcolor"))
            {
                const TArray<TSharedPtr<FJsonValue>>* ArrayValue = nullptr;
                if (!RequireArrayValue(4, ArrayValue))
                {
                    continue;
                }

                FLinearColor ColorValue(
                    static_cast<float>((*ArrayValue)[0]->AsNumber()),
                    static_cast<float>((*ArrayValue)[1]->AsNumber()),
                    static_cast<float>((*ArrayValue)[2]->AsNumber()),
                    static_cast<float>((*ArrayValue)[3]->AsNumber()));

                FNiagaraVariable Variable(FNiagaraTypeDefinition::GetColorDef(), ParamTCharName);
                if (EnsureParameterExists(ParameterStore, Variable))
                {
                    ParameterStore.SetParameterValue(ColorValue, Variable);
                    bAppliedAnyChanges = true;
                }
            }
            else
            {
                WarningsOut.Add(FString::Printf(TEXT("User parameter '%s' has unsupported type '%s'"), *ParamName, *TypeString));
            }
        }

        if (bAppliedAnyChanges)
        {
            NiagaraSystem->RequestCompile(false);
            NiagaraSystem->Modify();
        }

        return bAppliedAnyChanges;
    }

    bool ModifyEmitters(UNiagaraSystem* NiagaraSystem, const TSharedPtr<FJsonObject>& EmittersJson, TArray<FString>& WarningsOut)
    {
        if (!NiagaraSystem || !EmittersJson.IsValid())
        {
            return false;
        }

        bool bAppliedAnyChanges = false;
        NiagaraSystem->Modify();

        TArray<FNiagaraEmitterHandle>& EmitterHandles = NiagaraSystem->GetEmitterHandles();

        const TArray<TSharedPtr<FJsonValue>>* AddArray = nullptr;
        if (EmittersJson->TryGetArrayField(TEXT("add"), AddArray) && AddArray)
        {
            for (const TSharedPtr<FJsonValue>& Value : *AddArray)
            {
                if (!Value.IsValid() || Value->Type != EJson::Object)
                {
                    WarningsOut.Add(TEXT("Each entry in emitters.add must be an object"));
                    continue;
                }

                TSharedPtr<FJsonObject> AddObject = Value->AsObject();
                FString TemplatePath;
                if (!AddObject->TryGetStringField(TEXT("template_path"), TemplatePath) || TemplatePath.IsEmpty())
                {
                    WarningsOut.Add(TEXT("Emitter add entry requires a 'template_path'"));
                    continue;
                }

                UNiagaraEmitter* TemplateEmitter = LoadObject<UNiagaraEmitter>(nullptr, *TemplatePath);
                if (!TemplateEmitter)
                {
                    WarningsOut.Add(FString::Printf(TEXT("Failed to load emitter template '%s'"), *TemplatePath));
                    continue;
                }

                FString DesiredName;
                AddObject->TryGetStringField(TEXT("name"), DesiredName);
                bool bEnabled = true;
                AddObject->TryGetBoolField(TEXT("enabled"), bEnabled);

                FName EmitterName = TemplateEmitter->GetFName();
                if (!DesiredName.IsEmpty())
                {
                    EmitterName = FName(*DesiredName);
                }

                auto DoesEmitterNameExist = [&EmitterHandles](const FName& Name)
                {
                    for (const FNiagaraEmitterHandle& Handle : EmitterHandles)
                    {
                        if (Handle.GetName() == Name)
                        {
                            return true;
                        }
                    }
                    return false;
                };

                if (DoesEmitterNameExist(EmitterName))
                {
                    // Generate a unique name by appending a numeric suffix
                    int32 Suffix = 1;
                    FString BaseName = EmitterName.ToString();
                    do
                    {
                        EmitterName = FName(*FString::Printf(TEXT("%s_%d"), *BaseName, Suffix++));
                    }
                    while (DoesEmitterNameExist(EmitterName));
                }

                FNiagaraEmitterHandle& NewHandle = NiagaraSystem->AddEmitterHandle(TemplateEmitter, EmitterName);
                NewHandle.SetIsEnabled(bEnabled);

                bAppliedAnyChanges = true;
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* RemoveArray = nullptr;
        if (EmittersJson->TryGetArrayField(TEXT("remove"), RemoveArray) && RemoveArray)
        {
            for (const TSharedPtr<FJsonValue>& Value : *RemoveArray)
            {
                if (!Value.IsValid() || Value->Type != EJson::String)
                {
                    WarningsOut.Add(TEXT("Each entry in emitters.remove must be a string (emitter name)"));
                    continue;
                }

                FName TargetName(*Value->AsString());
                bool bRemoved = false;

                for (int32 Index = EmitterHandles.Num() - 1; Index >= 0; --Index)
                {
                    if (EmitterHandles[Index].GetName() == TargetName)
                    {
                        NiagaraSystem->RemoveEmitterHandle(EmitterHandles[Index]);
                        bRemoved = true;
                        bAppliedAnyChanges = true;
                        break;
                    }
                }

                if (!bRemoved)
                {
                    WarningsOut.Add(FString::Printf(TEXT("Emitter '%s' was not found for removal"), *TargetName.ToString()));
                }
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* ToggleArray = nullptr;
        if (EmittersJson->TryGetArrayField(TEXT("toggle"), ToggleArray) && ToggleArray)
        {
            for (const TSharedPtr<FJsonValue>& Value : *ToggleArray)
            {
                if (!Value.IsValid() || Value->Type != EJson::Object)
                {
                    WarningsOut.Add(TEXT("Each entry in emitters.toggle must be an object"));
                    continue;
                }

                TSharedPtr<FJsonObject> ToggleObject = Value->AsObject();
                FString TargetNameString;
                bool bEnableValue = true;

                if (!ToggleObject->TryGetStringField(TEXT("name"), TargetNameString) || TargetNameString.IsEmpty())
                {
                    WarningsOut.Add(TEXT("Emitter toggle entry requires a 'name'"));
                    continue;
                }

                if (!ToggleObject->TryGetBoolField(TEXT("enabled"), bEnableValue))
                {
                    WarningsOut.Add(FString::Printf(TEXT("Emitter toggle entry for '%s' requires an 'enabled' boolean"), *TargetNameString));
                    continue;
                }

                FName TargetName(*TargetNameString);
                bool bFound = false;
                for (FNiagaraEmitterHandle& Handle : EmitterHandles)
                {
                    if (Handle.GetName() == TargetName)
                    {
                        Handle.SetIsEnabled(bEnableValue);
                        bAppliedAnyChanges = true;
                        bFound = true;
                        break;
                    }
                }

                if (!bFound)
                {
                    WarningsOut.Add(FString::Printf(TEXT("Emitter '%s' was not found for toggle"), *TargetNameString));
                }
            }
        }

        if (bAppliedAnyChanges)
        {
            NiagaraSystem->RequestCompile(false);
        }

        return bAppliedAnyChanges;
    }
}


TSharedPtr<FJsonObject> FMCPCreateNiagaraSystemHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO("Handling create_niagara_system command");

    FString PackagePath;
    if (!Params->TryGetStringField(TEXT("package_path"), PackagePath) || PackagePath.IsEmpty())
    {
        MCP_LOG_WARNING("Missing 'package_path' field in create_niagara_system command");
        return CreateErrorResponse(TEXT("Missing 'package_path' field"));
    }

    FString SystemName;
    if (!Params->TryGetStringField(TEXT("name"), SystemName) || SystemName.IsEmpty())
    {
        MCP_LOG_WARNING("Missing 'name' field in create_niagara_system command");
        return CreateErrorResponse(TEXT("Missing 'name' field"));
    }

    const TSharedPtr<FJsonObject>* OptionsPtr = nullptr;
    if (!Params->TryGetObjectField(TEXT("options"), OptionsPtr))
    {
        OptionsPtr = nullptr;
    }

    TArray<FString> Warnings;
    TPair<UNiagaraSystem*, bool> Result = CreateNiagaraSystem(PackagePath, SystemName, OptionsPtr ? *OptionsPtr : nullptr, Warnings);
    if (!Result.Value || Result.Key == nullptr)
    {
        return CreateErrorResponse(TEXT("Failed to create Niagara system"));
    }

    UNiagaraSystem* CreatedSystem = Result.Key;

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), CreatedSystem->GetName());
    ResultObj->SetStringField(TEXT("path"), CreatedSystem->GetPathName());

    if (Warnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarningArray;
        for (const FString& Warning : Warnings)
        {
            WarningArray.Add(MakeShared<FJsonValueString>(Warning));
        }
        ResultObj->SetArrayField(TEXT("warnings"), WarningArray);
    }

    return CreateSuccessResponse(ResultObj);
}

TPair<UNiagaraSystem*, bool> FMCPCreateNiagaraSystemHandler::CreateNiagaraSystem(const FString& PackagePath, const FString& SystemName, const TSharedPtr<FJsonObject>& Options, TArray<FString>& OutWarnings)
{
    FString SanitizedPackagePath = PackagePath;
    SanitizedPackagePath.TrimStartAndEndInline();

    if (!SanitizedPackagePath.StartsWith(TEXT("/")))
    {
        SanitizedPackagePath = TEXT("/") + SanitizedPackagePath;
    }

    FString TargetObjectPath = BuildObjectPath(SanitizedPackagePath, SystemName);
    if (FindObject<UNiagaraSystem>(nullptr, *TargetObjectPath) != nullptr)
    {
        MCP_LOG_WARNING("Niagara system already exists at path %s", *TargetObjectPath);
        return TPair<UNiagaraSystem*, bool>(nullptr, false);
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    FAssetData ExistingAsset = AssetRegistryModule.Get().GetAssetByObjectPath(*TargetObjectPath);
    if (ExistingAsset.IsValid())
    {
        MCP_LOG_WARNING("Asset already exists at path %s", *TargetObjectPath);
        return TPair<UNiagaraSystem*, bool>(nullptr, false);
    }

    UNiagaraSystem* TemplateSystem = nullptr;
    if (Options.IsValid())
    {
        FString TemplatePath;
        if (Options->TryGetStringField(TEXT("template_path"), TemplatePath) && !TemplatePath.IsEmpty())
        {
            TemplateSystem = LoadObject<UNiagaraSystem>(nullptr, *TemplatePath);
            if (!TemplateSystem)
            {
                MCP_LOG_WARNING("Failed to load Niagara template system at %s", *TemplatePath);
            }
        }
    }

    UNiagaraSystemFactoryNew* Factory = NewObject<UNiagaraSystemFactoryNew>();
    if (TemplateSystem)
    {
        Factory->SystemToDuplicate = TemplateSystem;
    }

    FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));
    UObject* NewAsset = AssetToolsModule.Get().CreateAsset(SystemName, SanitizedPackagePath, UNiagaraSystem::StaticClass(), Factory);

    UNiagaraSystem* NiagaraSystem = Cast<UNiagaraSystem>(NewAsset);
    if (!NiagaraSystem)
    {
        MCP_LOG_ERROR("CreateAsset did not return a UNiagaraSystem");
        return TPair<UNiagaraSystem*, bool>(nullptr, false);
    }

    NiagaraSystem->Modify();

    ApplySystemCustomizations(NiagaraSystem, Options, OutWarnings);

    if (!SaveNiagaraSystem(NiagaraSystem))
    {
        MCP_LOG_ERROR("Failed to save Niagara system %s", *NiagaraSystem->GetPathName());
        return TPair<UNiagaraSystem*, bool>(nullptr, false);
    }

    // Notify the asset registry of the new asset
    AssetRegistryModule.AssetCreated(NiagaraSystem);

    MCP_LOG_INFO("Created Niagara system %s", *NiagaraSystem->GetPathName());
    return TPair<UNiagaraSystem*, bool>(NiagaraSystem, true);
}

bool FMCPCreateNiagaraSystemHandler::ApplySystemCustomizations(UNiagaraSystem* NiagaraSystem, const TSharedPtr<FJsonObject>& Options, TArray<FString>& WarningsOut)
{
    if (!NiagaraSystem || !Options.IsValid())
    {
        return false;
    }

    bool bModified = false;

    const TSharedPtr<FJsonObject>* UserParamsPtr = nullptr;
    if (Options->TryGetObjectField(TEXT("user_parameters"), UserParamsPtr) && UserParamsPtr)
    {
        bModified |= ApplyUserParameters(NiagaraSystem, *UserParamsPtr, WarningsOut);
    }

    const TSharedPtr<FJsonObject>* EmittersPtr = nullptr;
    if (Options->TryGetObjectField(TEXT("emitters"), EmittersPtr) && EmittersPtr)
    {
        bModified |= ModifyEmitters(NiagaraSystem, *EmittersPtr, WarningsOut);
    }

    if (bModified)
    {
        NiagaraSystem->MarkPackageDirty();
    }

    return bModified;
}

bool FMCPCreateNiagaraSystemHandler::SaveNiagaraSystem(UNiagaraSystem* NiagaraSystem)
{
    if (!NiagaraSystem)
    {
        return false;
    }

    UPackage* Package = NiagaraSystem->GetOutermost();
    if (!Package)
    {
        return false;
    }

    FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    return UPackage::SavePackage(Package, NiagaraSystem, *PackageFilename, SaveArgs);
}


TSharedPtr<FJsonObject> FMCPModifyNiagaraSystemHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO("Handling modify_niagara_system command");

    FString SystemPath;
    if (!Params->TryGetStringField(TEXT("path"), SystemPath) || SystemPath.IsEmpty())
    {
        MCP_LOG_WARNING("Missing 'path' field in modify_niagara_system command");
        return CreateErrorResponse(TEXT("Missing 'path' field"));
    }

    UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!NiagaraSystem)
    {
        MCP_LOG_ERROR("Failed to load Niagara system at path %s", *SystemPath);
        return CreateErrorResponse(FString::Printf(TEXT("Failed to load Niagara system at path: %s"), *SystemPath));
    }

    const TSharedPtr<FJsonObject>* OptionsPtr = nullptr;
    if (!Params->TryGetObjectField(TEXT("options"), OptionsPtr) || OptionsPtr == nullptr)
    {
        MCP_LOG_WARNING("Missing 'options' field in modify_niagara_system command");
        return CreateErrorResponse(TEXT("Missing 'options' field"));
    }

    TArray<FString> Warnings;
    ApplySystemCustomizations(NiagaraSystem, *OptionsPtr, Warnings);

    if (!SaveNiagaraSystem(NiagaraSystem))
    {
        MCP_LOG_ERROR("Failed to save Niagara system %s", *SystemPath);
        return CreateErrorResponse(TEXT("Failed to save Niagara system"));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetStringField(TEXT("name"), NiagaraSystem->GetName());
    ResultObj->SetStringField(TEXT("path"), NiagaraSystem->GetPathName());

    if (Warnings.Num() > 0)
    {
        TArray<TSharedPtr<FJsonValue>> WarningArray;
        for (const FString& Warning : Warnings)
        {
            WarningArray.Add(MakeShared<FJsonValueString>(Warning));
        }
        ResultObj->SetArrayField(TEXT("warnings"), WarningArray);
    }

    return CreateSuccessResponse(ResultObj);
}

bool FMCPModifyNiagaraSystemHandler::ApplySystemCustomizations(UNiagaraSystem* NiagaraSystem, const TSharedPtr<FJsonObject>& Options, TArray<FString>& WarningsOut)
{
    if (!NiagaraSystem || !Options.IsValid())
    {
        return false;
    }

    bool bModified = false;

    const TSharedPtr<FJsonObject>* UserParamsPtr = nullptr;
    if (Options->TryGetObjectField(TEXT("user_parameters"), UserParamsPtr) && UserParamsPtr)
    {
        bModified |= ApplyUserParameters(NiagaraSystem, *UserParamsPtr, WarningsOut);
    }

    const TSharedPtr<FJsonObject>* EmittersPtr = nullptr;
    if (Options->TryGetObjectField(TEXT("emitters"), EmittersPtr) && EmittersPtr)
    {
        bModified |= ModifyEmitters(NiagaraSystem, *EmittersPtr, WarningsOut);
    }

    if (bModified)
    {
        NiagaraSystem->MarkPackageDirty();
    }

    return bModified;
}

bool FMCPModifyNiagaraSystemHandler::SaveNiagaraSystem(UNiagaraSystem* NiagaraSystem)
{
    if (!NiagaraSystem)
    {
        return false;
    }

    UPackage* Package = NiagaraSystem->GetOutermost();
    if (!Package)
    {
        return false;
    }

    FString PackageFilename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    return UPackage::SavePackage(Package, NiagaraSystem, *PackageFilename, SaveArgs);
}


TSharedPtr<FJsonObject> FMCPGetNiagaraSystemInfoHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO("Handling get_niagara_system_info command");

    FString SystemPath;
    if (!Params->TryGetStringField(TEXT("path"), SystemPath) || SystemPath.IsEmpty())
    {
        MCP_LOG_WARNING("Missing 'path' field in get_niagara_system_info command");
        return CreateErrorResponse(TEXT("Missing 'path' field"));
    }

    UNiagaraSystem* NiagaraSystem = LoadObject<UNiagaraSystem>(nullptr, *SystemPath);
    if (!NiagaraSystem)
    {
        MCP_LOG_ERROR("Failed to load Niagara system at path %s", *SystemPath);
        return CreateErrorResponse(FString::Printf(TEXT("Failed to load Niagara system at path: %s"), *SystemPath));
    }

    return CreateSuccessResponse(BuildSystemInfoJson(NiagaraSystem));
}

TSharedPtr<FJsonObject> FMCPGetNiagaraSystemInfoHandler::BuildSystemInfoJson(UNiagaraSystem* NiagaraSystem)
{
    TSharedPtr<FJsonObject> Info = MakeShared<FJsonObject>();
    Info->SetStringField(TEXT("name"), NiagaraSystem->GetName());
    Info->SetStringField(TEXT("path"), NiagaraSystem->GetPathName());

    // Emitters
    TArray<TSharedPtr<FJsonValue>> EmittersArray;
    for (const FNiagaraEmitterHandle& Handle : NiagaraSystem->GetEmitterHandles())
    {
        TSharedPtr<FJsonObject> EmitterJson = MakeShared<FJsonObject>();
        EmitterJson->SetStringField(TEXT("name"), Handle.GetName().ToString());
        EmitterJson->SetBoolField(TEXT("enabled"), Handle.GetIsEnabled());

        if (UNiagaraEmitter* SourceEmitter = Handle.GetSource())
        {
            EmitterJson->SetStringField(TEXT("source_path"), SourceEmitter->GetPathName());
        }
        if (UNiagaraEmitter* InstanceEmitter = Handle.GetInstance())
        {
            EmitterJson->SetStringField(TEXT("instance_path"), InstanceEmitter->GetPathName());
        }

        EmittersArray.Add(MakeShared<FJsonValueObject>(EmitterJson));
    }
    Info->SetArrayField(TEXT("emitters"), EmittersArray);

    // User parameters
    TArray<TSharedPtr<FJsonValue>> ParameterArray;
    FNiagaraParameterStore& ParameterStore = NiagaraSystem->GetExposedParameters();
    int32 ParameterCount = ParameterStore.GetNumParameters();
    for (int32 Index = 0; Index < ParameterCount; ++Index)
    {
        FNiagaraVariable Variable = ParameterStore.GetParameterVariable(Index);
        TSharedPtr<FJsonObject> ParamJson = MakeShared<FJsonObject>();
        ParamJson->SetStringField(TEXT("name"), Variable.GetName().ToString());
        ParamJson->SetStringField(TEXT("type"), Variable.GetType().GetName().ToString());

        if (Variable.GetType() == FNiagaraTypeDefinition::GetFloatDef())
        {
            float Value = 0.f;
            ParameterStore.GetParameterValue(Value, Variable);
            ParamJson->SetNumberField(TEXT("value"), Value);
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetIntDef())
        {
            int32 Value = 0;
            ParameterStore.GetParameterValue(Value, Variable);
            ParamJson->SetNumberField(TEXT("value"), Value);
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetBoolDef())
        {
            bool bValue = false;
            ParameterStore.GetParameterValue(bValue, Variable);
            ParamJson->SetBoolField(TEXT("value"), bValue);
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetVec2Def())
        {
            FVector2f Value = FVector2f::ZeroVector;
            ParameterStore.GetParameterValue(Value, Variable);

            TArray<TSharedPtr<FJsonValue>> ValueArray;
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.X));
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.Y));
            ParamJson->SetArrayField(TEXT("value"), ValueArray);
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetVec3Def())
        {
            FVector3f Value = FVector3f::ZeroVector;
            ParameterStore.GetParameterValue(Value, Variable);

            TArray<TSharedPtr<FJsonValue>> ValueArray;
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.X));
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.Y));
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.Z));
            ParamJson->SetArrayField(TEXT("value"), ValueArray);
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetVec4Def())
        {
            FVector4f Value = FVector4f(0.f, 0.f, 0.f, 0.f);
            ParameterStore.GetParameterValue(Value, Variable);

            TArray<TSharedPtr<FJsonValue>> ValueArray;
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.X));
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.Y));
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.Z));
            ValueArray.Add(MakeShared<FJsonValueNumber>(Value.W));
            ParamJson->SetArrayField(TEXT("value"), ValueArray);
        }
        else if (Variable.GetType() == FNiagaraTypeDefinition::GetColorDef())
        {
            FLinearColor ColorValue = FLinearColor::White;
            ParameterStore.GetParameterValue(ColorValue, Variable);

            TArray<TSharedPtr<FJsonValue>> ValueArray;
            ValueArray.Add(MakeShared<FJsonValueNumber>(ColorValue.R));
            ValueArray.Add(MakeShared<FJsonValueNumber>(ColorValue.G));
            ValueArray.Add(MakeShared<FJsonValueNumber>(ColorValue.B));
            ValueArray.Add(MakeShared<FJsonValueNumber>(ColorValue.A));
            ParamJson->SetArrayField(TEXT("value"), ValueArray);
        }

        ParameterArray.Add(MakeShared<FJsonValueObject>(ParamJson));
    }

    Info->SetArrayField(TEXT("user_parameters"), ParameterArray);

    return Info;
}

