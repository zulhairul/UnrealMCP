#include "MCPCommandHandlers_GameplayAbilities.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "AttributeSet.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/DataTable.h"
#include "GameplayEffect.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "GameplayTagRequirements.h"
#include "GameplayTagsManager.h"
#include "MCPCommandHandlers_DataTables.h"
#include "MCPFileLogger.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

namespace
{
    FString EnsureGameRoot(const FString& InPath)
    {
        FString Sanitised = InPath;
        Sanitised.TrimStartAndEndInline();

        if (Sanitised.IsEmpty())
        {
            return TEXT("/Game");
        }

        if (Sanitised.StartsWith(TEXT("/")))
        {
            return Sanitised;
        }

        if (Sanitised.StartsWith(TEXT("Game/")))
        {
            return FString::Printf(TEXT("/%s"), *Sanitised);
        }

        return FString::Printf(TEXT("/Game/%s"), *Sanitised);
    }

    bool NormaliseAssetPaths(
        const FString& InPackagePath,
        const FString& AssetName,
        FString& OutPackageName,
        FString& OutObjectPath,
        FString& OutErrorMessage)
    {
        FString TrimmedName = AssetName;
        TrimmedName.TrimStartAndEndInline();

        if (TrimmedName.IsEmpty())
        {
            OutErrorMessage = TEXT("Asset name cannot be empty.");
            return false;
        }

        FString PackagePath = EnsureGameRoot(InPackagePath);
        PackagePath.TrimEndInline();

        while (PackagePath.EndsWith(TEXT("/")))
        {
            PackagePath.RemoveFromEnd(TEXT("/"));
        }

        const FString PackageName = FString::Printf(TEXT("%s/%s"), *PackagePath, *TrimmedName);

        if (!FPackageName::IsValidLongPackageName(PackageName))
        {
            OutErrorMessage = FString::Printf(TEXT("Invalid package name '%s'."), *PackageName);
            return false;
        }

        OutPackageName = PackageName;
        OutObjectPath = FString::Printf(TEXT("%s.%s"), *PackageName, *TrimmedName);
        return true;
    }

    bool ParseGameplayTag(const FString& TagString, FGameplayTag& OutTag, FString& OutErrorMessage)
    {
        if (TagString.IsEmpty())
        {
            OutErrorMessage = TEXT("Gameplay tag strings cannot be empty.");
            return false;
        }

        UGameplayTagsManager& TagsManager = UGameplayTagsManager::Get();
        FGameplayTag Tag = TagsManager.RequestGameplayTag(FName(*TagString), false);

        if (!Tag.IsValid())
        {
            OutErrorMessage = FString::Printf(TEXT("Gameplay tag '%s' is not defined in the project."), *TagString);
            return false;
        }

        OutTag = Tag;
        return true;
    }

    bool ApplyTagArrayToContainer(const TArray<TSharedPtr<FJsonValue>>& TagsArray, FGameplayTagContainer& Container, FString& OutErrorMessage)
    {
        Container.Reset();

        for (const TSharedPtr<FJsonValue>& TagValue : TagsArray)
        {
            if (!TagValue.IsValid())
            {
                continue;
            }

            FString TagString;
            if (!TagValue->TryGetString(TagString))
            {
                OutErrorMessage = TEXT("Gameplay tags must be provided as strings.");
                return false;
            }

            FGameplayTag Tag;
            if (!ParseGameplayTag(TagString, Tag, OutErrorMessage))
            {
                return false;
            }

            Container.AddTag(Tag);
        }

        return true;
    }

    bool ApplyTagArrayToRequirements(const TArray<TSharedPtr<FJsonValue>>& TagsArray, FGameplayTagRequirements& Requirements, FString& OutErrorMessage)
    {
        Requirements.RequireTags.Reset();

        for (const TSharedPtr<FJsonValue>& TagValue : TagsArray)
        {
            if (!TagValue.IsValid())
            {
                continue;
            }

            FString TagString;
            if (!TagValue->TryGetString(TagString))
            {
                OutErrorMessage = TEXT("Gameplay tags must be provided as strings.");
                return false;
            }

            FGameplayTag Tag;
            if (!ParseGameplayTag(TagString, Tag, OutErrorMessage))
            {
                return false;
            }

            Requirements.RequireTags.AddTag(Tag);
        }

        return true;
    }

    EGameplayEffectDurationType ParseDurationPolicy(const FString& PolicyString)
    {
        FString Normalised = PolicyString.ToLower();

        if (Normalised == TEXT("instant"))
        {
            return EGameplayEffectDurationType::Instant;
        }

        if (Normalised == TEXT("infinite"))
        {
            return EGameplayEffectDurationType::Infinite;
        }

        if (Normalised == TEXT("has_duration") || Normalised == TEXT("duration"))
        {
            return EGameplayEffectDurationType::HasDuration;
        }

        return EGameplayEffectDurationType::Instant;
    }

    bool ParseModifierOperation(const FString& OperationString, EGameplayModOp::Type& OutOperation, FString& OutErrorMessage)
    {
        FString Normalised = OperationString.ToLower();

        if (Normalised == TEXT("additive") || Normalised == TEXT("add"))
        {
            OutOperation = EGameplayModOp::Additive;
            return true;
        }

        if (Normalised == TEXT("multiplicative") || Normalised == TEXT("multiply"))
        {
            OutOperation = EGameplayModOp::Multiplicative;
            return true;
        }

        if (Normalised == TEXT("division") || Normalised == TEXT("divide"))
        {
            OutOperation = EGameplayModOp::Division;
            return true;
        }

        if (Normalised == TEXT("override") || Normalised == TEXT("set"))
        {
            OutOperation = EGameplayModOp::Override;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Unknown modifier operation '%s'."), *OperationString);
        return false;
    }

    bool ParseStackingType(const FString& Input, EGameplayEffectStackingType& OutType, FString& OutErrorMessage)
    {
        FString Normalised = Input.ToLower();

        if (Normalised == TEXT("none"))
        {
            OutType = EGameplayEffectStackingType::None;
            return true;
        }

        if (Normalised == TEXT("aggregate_by_source") || Normalised == TEXT("source"))
        {
            OutType = EGameplayEffectStackingType::AggregateBySource;
            return true;
        }

        if (Normalised == TEXT("aggregate_by_target") || Normalised == TEXT("target"))
        {
            OutType = EGameplayEffectStackingType::AggregateByTarget;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Unknown stacking type '%s'."), *Input);
        return false;
    }

    bool ParseStackingDurationPolicy(const FString& Input, EGameplayEffectStackingDurationPolicy& OutPolicy, FString& OutErrorMessage)
    {
        FString Normalised = Input.ToLower();

        if (Normalised == TEXT("refresh_on_add"))
        {
            OutPolicy = EGameplayEffectStackingDurationPolicy::RefreshOnSuccessfulApplication;
            return true;
        }

        if (Normalised == TEXT("never_refresh"))
        {
            OutPolicy = EGameplayEffectStackingDurationPolicy::NeverRefresh;
            return true;
        }

        if (Normalised == TEXT("additive"))
        {
            OutPolicy = EGameplayEffectStackingDurationPolicy::StackDuration;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Unknown stacking duration policy '%s'."), *Input);
        return false;
    }

    bool ParseStackingPeriodPolicy(const FString& Input, EGameplayEffectStackingPeriodPolicy& OutPolicy, FString& OutErrorMessage)
    {
        FString Normalised = Input.ToLower();

        if (Normalised == TEXT("reset_on_add"))
        {
            OutPolicy = EGameplayEffectStackingPeriodPolicy::ResetOnSuccessfulApplication;
            return true;
        }

        if (Normalised == TEXT("never_reset"))
        {
            OutPolicy = EGameplayEffectStackingPeriodPolicy::NeverReset;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Unknown stacking period policy '%s'."), *Input);
        return false;
    }

    bool ParseStackingExpirationPolicy(const FString& Input, EGameplayEffectStackingExpirationPolicy& OutPolicy, FString& OutErrorMessage)
    {
        FString Normalised = Input.ToLower();

        if (Normalised == TEXT("remove_oldest"))
        {
            OutPolicy = EGameplayEffectStackingExpirationPolicy::RemoveOldest;
            return true;
        }

        if (Normalised == TEXT("clear_stack"))
        {
            OutPolicy = EGameplayEffectStackingExpirationPolicy::ClearStack;
            return true;
        }

        if (Normalised == TEXT("refresh_duration"))
        {
            OutPolicy = EGameplayEffectStackingExpirationPolicy::RefreshDuration;
            return true;
        }

        OutErrorMessage = FString::Printf(TEXT("Unknown stacking expiration policy '%s'."), *Input);
        return false;
    }

    bool ResolveGameplayAttribute(const TSharedPtr<FJsonObject>& AttributeJson, FGameplayAttribute& OutAttribute, FString& OutErrorMessage)
    {
        if (!AttributeJson.IsValid())
        {
            OutErrorMessage = TEXT("Modifier attribute must be an object containing 'set' and 'property'.");
            return false;
        }

        FString AttributeSetPath;
        if (!AttributeJson->TryGetStringField(TEXT("set"), AttributeSetPath))
        {
            OutErrorMessage = TEXT("Modifier attribute is missing 'set' field with the attribute set class path.");
            return false;
        }

        FString AttributeName;
        if (!AttributeJson->TryGetStringField(TEXT("property"), AttributeName))
        {
            OutErrorMessage = TEXT("Modifier attribute is missing 'property' field with the attribute name.");
            return false;
        }

        UClass* AttributeSetClass = LoadObject<UClass>(nullptr, *AttributeSetPath);
        if (!AttributeSetClass || !AttributeSetClass->IsChildOf(UAttributeSet::StaticClass()))
        {
            OutErrorMessage = FString::Printf(TEXT("Failed to load attribute set class '%s'."), *AttributeSetPath);
            return false;
        }

        FProperty* AttributeProperty = FindFProperty<FProperty>(AttributeSetClass, *AttributeName);
        if (!AttributeProperty)
        {
            OutErrorMessage = FString::Printf(TEXT("Attribute '%s' was not found on set '%s'."), *AttributeName, *AttributeSetPath);
            return false;
        }

        OutAttribute = FGameplayAttribute(AttributeProperty);
        return true;
    }
}

FMCPCreateGameplayEffectHandler::FMCPCreateGameplayEffectHandler()
    : FMCPCommandHandlerBase(TEXT("create_gameplay_effect"))
{
}

TSharedPtr<FJsonObject> FMCPCreateGameplayEffectHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO(TEXT("Handling create_gameplay_effect command"));

    FString PackagePath;
    if (!Params->TryGetStringField(TEXT("package_path"), PackagePath))
    {
        MCP_LOG_WARNING(TEXT("Missing 'package_path' parameter for create_gameplay_effect"));
        return CreateErrorResponse(TEXT("Missing 'package_path' field"));
    }

    FString EffectName;
    if (!Params->TryGetStringField(TEXT("name"), EffectName))
    {
        MCP_LOG_WARNING(TEXT("Missing 'name' parameter for create_gameplay_effect"));
        return CreateErrorResponse(TEXT("Missing 'name' field"));
    }

    FString PackageName;
    FString ObjectPath;
    FString PathError;
    if (!NormaliseAssetPaths(PackagePath, EffectName, PackageName, ObjectPath, PathError))
    {
        MCP_LOG_WARNING(TEXT("%s"), *PathError);
        return CreateErrorResponse(PathError);
    }

    FString ParentClassPath;
    Params->TryGetStringField(TEXT("parent_class"), ParentClassPath);

    bool bOverwriteExisting = false;
    Params->TryGetBoolField(TEXT("overwrite"), bOverwriteExisting);

    UClass* ParentClass = UGameplayEffect::StaticClass();
    if (!ParentClassPath.IsEmpty())
    {
        UClass* LoadedClass = LoadObject<UClass>(nullptr, *ParentClassPath);
        if (!LoadedClass)
        {
            FString Message = FString::Printf(TEXT("Failed to load parent class '%s'."), *ParentClassPath);
            MCP_LOG_ERROR(TEXT("%s"), *Message);
            return CreateErrorResponse(Message);
        }

        if (!LoadedClass->IsChildOf(UGameplayEffect::StaticClass()))
        {
            FString Message = FString::Printf(TEXT("Parent class '%s' is not a Gameplay Effect."), *ParentClassPath);
            MCP_LOG_WARNING(TEXT("%s"), *Message);
            return CreateErrorResponse(Message);
        }

        ParentClass = LoadedClass;
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        FString Message = FString::Printf(TEXT("Failed to create package '%s'."), *PackageName);
        MCP_LOG_ERROR(TEXT("%s"), *Message);
        return CreateErrorResponse(Message);
    }

    Package->FullyLoad();

    UGameplayEffect* GameplayEffect = FindObject<UGameplayEffect>(Package, *EffectName);
    const bool bExistingAsset = (GameplayEffect != nullptr);

    if (GameplayEffect)
    {
        if (!bOverwriteExisting)
        {
            FString Message = FString::Printf(TEXT("Gameplay Effect '%s' already exists."), *ObjectPath);
            MCP_LOG_WARNING(TEXT("%s"), *Message);
            return CreateErrorResponse(Message);
        }

        GameplayEffect->Modify();
        GameplayEffect->Modifiers.Reset();
        GameplayEffect->GrantedTags.Reset();
    }
    else
    {
        GameplayEffect = NewObject<UGameplayEffect>(Package, ParentClass, *EffectName, RF_Public | RF_Standalone);
        if (!GameplayEffect)
        {
            FString Message = FString::Printf(TEXT("Failed to create Gameplay Effect '%s'."), *EffectName);
            MCP_LOG_ERROR(TEXT("%s"), *Message);
            return CreateErrorResponse(Message);
        }
    }

    const TSharedPtr<FJsonObject>* ConfigJson = nullptr;
    if (Params->TryGetObjectField(TEXT("config"), ConfigJson))
    {
        FString ConfigError;
        if (!ConfigureGameplayEffect(GameplayEffect, *ConfigJson, ConfigError))
        {
            MCP_LOG_ERROR(TEXT("Failed to configure Gameplay Effect: %s"), *ConfigError);
            return CreateErrorResponse(ConfigError);
        }
    }

    GameplayEffect->MarkPackageDirty();
    GameplayEffect->PostEditChange();

    FString SaveError;
    if (!SaveGameplayEffect(GameplayEffect, PackageName, !bExistingAsset, SaveError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *SaveError);
        return CreateErrorResponse(SaveError);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), GameplayEffect->GetName());
    Result->SetStringField(TEXT("path"), GameplayEffect->GetPathName());
    Result->SetStringField(TEXT("class"), GameplayEffect->GetClass()->GetPathName());
    Result->SetBoolField(TEXT("overwrote_existing"), bExistingAsset);

    MCP_LOG_INFO(TEXT("Gameplay Effect '%s' saved successfully."), *GameplayEffect->GetPathName());
    return CreateSuccessResponse(Result);
}

bool FMCPCreateGameplayEffectHandler::ConfigureGameplayEffect(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const
{
    if (!GameplayEffect)
    {
        OutErrorMessage = TEXT("Gameplay Effect reference is invalid.");
        return false;
    }

    if (!ConfigureDuration(GameplayEffect, Config, OutErrorMessage))
    {
        return false;
    }

    if (!ConfigurePeriodAndStacking(GameplayEffect, Config, OutErrorMessage))
    {
        return false;
    }

    if (!ConfigureTags(GameplayEffect, Config, OutErrorMessage))
    {
        return false;
    }

    const TArray<TSharedPtr<FJsonValue>>* ModifiersArray = nullptr;
    if (Config->TryGetArrayField(TEXT("modifiers"), ModifiersArray))
    {
        if (!ConfigureModifiers(GameplayEffect, *ModifiersArray, OutErrorMessage))
        {
            return false;
        }
    }

    return true;
}

bool FMCPCreateGameplayEffectHandler::ConfigureDuration(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const
{
    FString DurationPolicyString;
    if (Config->TryGetStringField(TEXT("duration_policy"), DurationPolicyString))
    {
        GameplayEffect->DurationPolicy = ParseDurationPolicy(DurationPolicyString);
    }

    if (GameplayEffect->DurationPolicy == EGameplayEffectDurationType::HasDuration)
    {
        double DurationSeconds = 0.0;
        if (!Config->TryGetNumberField(TEXT("duration_seconds"), DurationSeconds))
        {
            OutErrorMessage = TEXT("duration_seconds must be provided when duration_policy is 'HasDuration'.");
            return false;
        }

        GameplayEffect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(static_cast<float>(DurationSeconds)));
    }
    else
    {
        GameplayEffect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(0.0f));
    }

    return true;
}

bool FMCPCreateGameplayEffectHandler::ConfigurePeriodAndStacking(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const
{
    double PeriodSeconds = 0.0;
    if (Config->TryGetNumberField(TEXT("period_seconds"), PeriodSeconds))
    {
        GameplayEffect->Period = FScalableFloat(static_cast<float>(PeriodSeconds));
    }
    else
    {
        GameplayEffect->Period = FScalableFloat(0.0f);
    }

    bool bExecuteOnApplication = false;
    if (Config->TryGetBoolField(TEXT("execute_period_on_application"), bExecuteOnApplication))
    {
        GameplayEffect->bExecutePeriodicEffectOnApplication = bExecuteOnApplication;
    }

    int32 StackLimit = 0;
    if (Config->TryGetNumberField(TEXT("stack_limit"), StackLimit))
    {
        GameplayEffect->StackLimitCount = StackLimit;
    }

    FString StackingTypeString;
    if (Config->TryGetStringField(TEXT("stacking_type"), StackingTypeString))
    {
        EGameplayEffectStackingType StackingType;
        if (!ParseStackingType(StackingTypeString, StackingType, OutErrorMessage))
        {
            return false;
        }
        GameplayEffect->StackingType = StackingType;
    }

    FString DurationPolicyString;
    if (Config->TryGetStringField(TEXT("stack_duration_policy"), DurationPolicyString))
    {
        EGameplayEffectStackingDurationPolicy Policy;
        if (!ParseStackingDurationPolicy(DurationPolicyString, Policy, OutErrorMessage))
        {
            return false;
        }
        GameplayEffect->StackDurationRefreshPolicy = Policy;
    }

    FString PeriodPolicyString;
    if (Config->TryGetStringField(TEXT("stack_period_policy"), PeriodPolicyString))
    {
        EGameplayEffectStackingPeriodPolicy Policy;
        if (!ParseStackingPeriodPolicy(PeriodPolicyString, Policy, OutErrorMessage))
        {
            return false;
        }
        GameplayEffect->StackPeriodResetPolicy = Policy;
    }

    FString ExpirationPolicyString;
    if (Config->TryGetStringField(TEXT("stack_expiration_policy"), ExpirationPolicyString))
    {
        EGameplayEffectStackingExpirationPolicy Policy;
        if (!ParseStackingExpirationPolicy(ExpirationPolicyString, Policy, OutErrorMessage))
        {
            return false;
        }
        GameplayEffect->StackExpirationPolicy = Policy;
    }

    return true;
}

bool FMCPCreateGameplayEffectHandler::ConfigureModifiers(UGameplayEffect* GameplayEffect, const TArray<TSharedPtr<FJsonValue>>& ModifiersArray, FString& OutErrorMessage) const
{
    GameplayEffect->Modifiers.Reset();

    for (const TSharedPtr<FJsonValue>& ModifierValue : ModifiersArray)
    {
        if (!ModifierValue.IsValid())
        {
            continue;
        }

        TSharedPtr<FJsonObject> ModifierJson = ModifierValue->AsObject();
        if (!ModifierJson.IsValid())
        {
            OutErrorMessage = TEXT("Each modifier entry must be a JSON object.");
            return false;
        }

        const TSharedPtr<FJsonObject>* AttributeJson = nullptr;
        if (!ModifierJson->TryGetObjectField(TEXT("attribute"), AttributeJson))
        {
            OutErrorMessage = TEXT("Modifier entry is missing 'attribute' object.");
            return false;
        }

        FGameplayAttribute Attribute;
        if (!ResolveGameplayAttribute(*AttributeJson, Attribute, OutErrorMessage))
        {
            return false;
        }

        EGameplayModOp::Type Operation = EGameplayModOp::Additive;
        FString OperationString;
        if (ModifierJson->TryGetStringField(TEXT("operation"), OperationString))
        {
            if (!ParseModifierOperation(OperationString, Operation, OutErrorMessage))
            {
                return false;
            }
        }

        double MagnitudeValue = 0.0;
        if (!ModifierJson->TryGetNumberField(TEXT("magnitude"), MagnitudeValue))
        {
            OutErrorMessage = TEXT("Modifier entry must provide a numeric 'magnitude'.");
            return false;
        }

        FGameplayModifierInfo ModifierInfo;
        ModifierInfo.Attribute = Attribute;
        ModifierInfo.ModifierOp = Operation;
        ModifierInfo.ModifierMagnitude = FScalableFloat(static_cast<float>(MagnitudeValue));

        const TSharedPtr<FJsonObject>* SourceRequirementsJson = nullptr;
        if (ModifierJson->TryGetObjectField(TEXT("source_requirements"), SourceRequirementsJson))
        {
            const TArray<TSharedPtr<FJsonValue>>* RequiredTags = nullptr;
            if ((*SourceRequirementsJson)->TryGetArrayField(TEXT("require"), RequiredTags))
            {
                if (!ApplyTagArrayToRequirements(*RequiredTags, ModifierInfo.SourceTagRequirements, OutErrorMessage))
                {
                    return false;
                }
            }
        }

        const TSharedPtr<FJsonObject>* TargetRequirementsJson = nullptr;
        if (ModifierJson->TryGetObjectField(TEXT("target_requirements"), TargetRequirementsJson))
        {
            const TArray<TSharedPtr<FJsonValue>>* RequiredTags = nullptr;
            if ((*TargetRequirementsJson)->TryGetArrayField(TEXT("require"), RequiredTags))
            {
                if (!ApplyTagArrayToRequirements(*RequiredTags, ModifierInfo.TargetTagRequirements, OutErrorMessage))
                {
                    return false;
                }
            }
        }

        GameplayEffect->Modifiers.Add(ModifierInfo);
    }

    return true;
}

bool FMCPCreateGameplayEffectHandler::ConfigureTags(UGameplayEffect* GameplayEffect, const TSharedPtr<FJsonObject>& Config, FString& OutErrorMessage) const
{
    const TArray<TSharedPtr<FJsonValue>>* GrantedTagsArray = nullptr;
    if (Config->TryGetArrayField(TEXT("granted_tags"), GrantedTagsArray))
    {
        if (!ApplyTagArrayToContainer(*GrantedTagsArray, GameplayEffect->GrantedTags, OutErrorMessage))
        {
            return false;
        }
    }

    const TSharedPtr<FJsonObject>* ApplicationRequirements = nullptr;
    if (Config->TryGetObjectField(TEXT("application_requirements"), ApplicationRequirements))
    {
        const TArray<TSharedPtr<FJsonValue>>* RequireTags = nullptr;
        if ((*ApplicationRequirements)->TryGetArrayField(TEXT("require"), RequireTags))
        {
            if (!ApplyTagArrayToRequirements(*RequireTags, GameplayEffect->ApplicationTagRequirements, OutErrorMessage))
            {
                return false;
            }
        }

        const TArray<TSharedPtr<FJsonValue>>* IgnoreTags = nullptr;
        if ((*ApplicationRequirements)->TryGetArrayField(TEXT("ignore"), IgnoreTags))
        {
            GameplayEffect->ApplicationTagRequirements.IgnoreTags.Reset();
            for (const TSharedPtr<FJsonValue>& TagValue : *IgnoreTags)
            {
                if (!TagValue.IsValid())
                {
                    continue;
                }

                FString TagString;
                if (!TagValue->TryGetString(TagString))
                {
                    OutErrorMessage = TEXT("Gameplay tags must be provided as strings.");
                    return false;
                }

                FGameplayTag Tag;
                if (!ParseGameplayTag(TagString, Tag, OutErrorMessage))
                {
                    return false;
                }

                GameplayEffect->ApplicationTagRequirements.IgnoreTags.AddTag(Tag);
            }
        }
    }

    return true;
}

bool FMCPCreateGameplayEffectHandler::SaveGameplayEffect(UGameplayEffect* GameplayEffect, const FString& PackageName, bool bCreatedNewAsset, FString& OutErrorMessage) const
{
    if (!GameplayEffect)
    {
        OutErrorMessage = TEXT("Gameplay Effect reference is invalid.");
        return false;
    }

    UPackage* Package = GameplayEffect->GetPackage();
    if (!Package)
    {
        OutErrorMessage = TEXT("Gameplay Effect package reference is invalid.");
        return false;
    }

    const FString PackageFilename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    if (!UPackage::SavePackage(Package, GameplayEffect, *PackageFilename, SaveArgs))
    {
        OutErrorMessage = FString::Printf(TEXT("Failed to save Gameplay Effect package '%s'."), *PackageFilename);
        return false;
    }

    if (bCreatedNewAsset)
    {
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        AssetRegistry.AssetCreated(GameplayEffect);
    }

    return true;
}

FMCPRegisterGameplayEffectHandler::FMCPRegisterGameplayEffectHandler()
    : FMCPCommandHandlerBase(TEXT("register_gameplay_effect"))
{
}

TSharedPtr<FJsonObject> FMCPRegisterGameplayEffectHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO(TEXT("Handling register_gameplay_effect command"));

    FString DataTablePath;
    if (!Params->TryGetStringField(TEXT("data_table_path"), DataTablePath))
    {
        MCP_LOG_WARNING(TEXT("Missing 'data_table_path' parameter for register_gameplay_effect"));
        return CreateErrorResponse(TEXT("Missing 'data_table_path' field"));
    }

    FString RowNameString;
    if (!Params->TryGetStringField(TEXT("row_name"), RowNameString))
    {
        MCP_LOG_WARNING(TEXT("Missing 'row_name' parameter for register_gameplay_effect"));
        return CreateErrorResponse(TEXT("Missing 'row_name' field"));
    }

    FString GameplayEffectPath;
    if (!Params->TryGetStringField(TEXT("gameplay_effect_path"), GameplayEffectPath))
    {
        MCP_LOG_WARNING(TEXT("Missing 'gameplay_effect_path' parameter for register_gameplay_effect"));
        return CreateErrorResponse(TEXT("Missing 'gameplay_effect_path' field"));
    }

    FString EffectFieldName = TEXT("GameplayEffect");
    Params->TryGetStringField(TEXT("effect_field"), EffectFieldName);

    FString NormalisedTablePath = DataTablePath;
    NormalisedTablePath.TrimStartAndEndInline();

    if (!NormalisedTablePath.Contains(TEXT(".")))
    {
        FString PackagePath = EnsureGameRoot(NormalisedTablePath);
        FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
        NormalisedTablePath = FString::Printf(TEXT("%s.%s"), *PackagePath, *AssetName);
    }

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *NormalisedTablePath);
    if (!DataTable)
    {
        FString Message = FString::Printf(TEXT("Failed to load data table '%s'."), *NormalisedTablePath);
        MCP_LOG_ERROR(TEXT("%s"), *Message);
        return CreateErrorResponse(Message);
    }

    bool bOverwriteRow = true;
    Params->TryGetBoolField(TEXT("overwrite"), bOverwriteRow);

    const FName RowName(*RowNameString);
    if (DataTable->GetRowMap().Contains(RowName) && !bOverwriteRow)
    {
        FString Message = FString::Printf(TEXT("Row '%s' already exists in data table '%s'."), *RowNameString, *NormalisedTablePath);
        MCP_LOG_WARNING(TEXT("%s"), *Message);
        return CreateErrorResponse(Message);
    }

    UGameplayEffect* GameplayEffect = LoadObject<UGameplayEffect>(nullptr, *GameplayEffectPath);
    if (!GameplayEffect)
    {
        FString Message = FString::Printf(TEXT("Failed to load Gameplay Effect '%s'."), *GameplayEffectPath);
        MCP_LOG_ERROR(TEXT("%s"), *Message);
        return CreateErrorResponse(Message);
    }

    const TSharedPtr<FJsonObject>* AdditionalDataJson = nullptr;
    if (Params->TryGetObjectField(TEXT("additional_data"), AdditionalDataJson))
    {
        // no-op, pointer stored below
    }

    TSharedPtr<FJsonObject> RowPayload;
    FString PayloadError;
    if (!BuildRowPayload(RowNameString, GameplayEffectPath, EffectFieldName, AdditionalDataJson ? *AdditionalDataJson : nullptr, RowPayload, PayloadError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *PayloadError);
        return CreateErrorResponse(PayloadError);
    }

    TSharedPtr<FJsonObject> RowsObject = MakeShared<FJsonObject>();
    RowsObject->SetObjectField(RowNameString, RowPayload);

    DataTable->Modify();

    int32 RowsApplied = 0;
    FString ApplyError;
    if (!FMCPDataTableUtils::ApplyRowsToDataTable(DataTable, RowsObject, RowsApplied, ApplyError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *ApplyError);
        return CreateErrorResponse(ApplyError);
    }

    DataTable->MarkPackageDirty();
    DataTable->PostEditChange();

    FString SaveError;
    if (!FMCPDataTableUtils::SaveAssetPackage(DataTable->GetPackage(), DataTable, DataTable->GetPackage()->GetName(), SaveError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *SaveError);
        return CreateErrorResponse(SaveError);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("data_table_path"), DataTable->GetPathName());
    Result->SetStringField(TEXT("row_name"), RowNameString);
    Result->SetStringField(TEXT("gameplay_effect_path"), GameplayEffect->GetPathName());
    Result->SetNumberField(TEXT("rows_applied"), RowsApplied);

    MCP_LOG_INFO(TEXT("Registered Gameplay Effect '%s' to data table '%s' as row '%s'."), *GameplayEffect->GetPathName(), *DataTable->GetPathName(), *RowNameString);
    return CreateSuccessResponse(Result);
}

bool FMCPRegisterGameplayEffectHandler::BuildRowPayload(const FString& RowName, const FString& EffectPath, const FString& EffectField, const TSharedPtr<FJsonObject>& AdditionalData, TSharedPtr<FJsonObject>& OutRowPayload, FString& OutErrorMessage) const
{
    OutRowPayload = MakeShared<FJsonObject>();

    if (AdditionalData.IsValid())
    {
        for (const auto& FieldPair : AdditionalData->Values)
        {
            OutRowPayload->SetField(FieldPair.Key, FieldPair.Value);
        }
    }

    OutRowPayload->SetStringField(EffectField, EffectPath);

    return true;
}
