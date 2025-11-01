#include "MCPCommandHandlers_DataTables.h"

#include "MCPFileLogger.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "JsonObjectConverter.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StructOnScope.h"

namespace
{
    /** Ensure package paths start with /Game when callers provide relative paths. */
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
}

bool FMCPDataTableUtils::NormaliseAssetPaths(
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
        OutErrorMessage = TEXT("Data table name cannot be empty.");
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

bool FMCPDataTableUtils::ConvertJsonToStruct(
    const TSharedPtr<FJsonObject>& JsonObject,
    UScriptStruct* StructType,
    uint8* StructData,
    FString& OutErrorMessage)
{
    if (!JsonObject.IsValid())
    {
        OutErrorMessage = TEXT("Row data must be a JSON object.");
        return false;
    }

    if (!StructType)
    {
        OutErrorMessage = TEXT("Row struct type is invalid.");
        return false;
    }

    if (!StructData)
    {
        OutErrorMessage = TEXT("Internal error: Struct data pointer is null.");
        return false;
    }

    if (!FJsonObjectConverter::JsonObjectToUStruct(JsonObject.ToSharedRef(), StructType, StructData, 0, 0))
    {
        OutErrorMessage = FString::Printf(TEXT("Failed to convert JSON to struct '%s'."), *StructType->GetName());
        return false;
    }

    return true;
}

bool FMCPDataTableUtils::ApplyRowsToDataTable(
    UDataTable* DataTable,
    const TSharedPtr<FJsonObject>& RowsObject,
    int32& OutRowsApplied,
    FString& OutErrorMessage)
{
    OutRowsApplied = 0;

    if (!DataTable)
    {
        OutErrorMessage = TEXT("Data table is null.");
        return false;
    }

    if (!RowsObject.IsValid())
    {
        // Nothing to apply; treat as success.
        return true;
    }

    if (!DataTable->RowStruct)
    {
        OutErrorMessage = TEXT("Data table has no row struct assigned.");
        return false;
    }

    for (const auto& RowPair : RowsObject->Values)
    {
        const FString RowNameString = RowPair.Key;
        TSharedPtr<FJsonValue> RowValue = RowPair.Value;
        if (!RowValue.IsValid())
        {
            OutErrorMessage = FString::Printf(TEXT("Row '%s' has invalid data."), *RowNameString);
            return false;
        }

        TSharedPtr<FJsonObject> RowJson = RowValue->AsObject();
        if (!RowJson.IsValid())
        {
            OutErrorMessage = FString::Printf(TEXT("Row '%s' must be a JSON object."), *RowNameString);
            return false;
        }

        FStructOnScope RowStruct(DataTable->RowStruct);
        FString ConversionError;
        if (!ConvertJsonToStruct(RowJson, DataTable->RowStruct, RowStruct.GetStructMemory(), ConversionError))
        {
            OutErrorMessage = ConversionError;
            return false;
        }

        const FName RowName(*RowNameString);
        DataTable->AddRow(RowName, RowStruct.GetStructMemory());
        ++OutRowsApplied;
    }

    return true;
}

bool FMCPDataTableUtils::RemoveRowsFromDataTable(
    UDataTable* DataTable,
    const TArray<TSharedPtr<FJsonValue>>& RowNames,
    int32& OutRowsRemoved,
    FString& OutErrorMessage)
{
    OutRowsRemoved = 0;

    if (!DataTable)
    {
        OutErrorMessage = TEXT("Data table is null.");
        return false;
    }

    for (const TSharedPtr<FJsonValue>& RowNameValue : RowNames)
    {
        if (!RowNameValue.IsValid())
        {
            continue;
        }

        FString RowNameString;
        if (!RowNameValue->TryGetString(RowNameString))
        {
            OutErrorMessage = TEXT("Row names must be strings.");
            return false;
        }

        const FName RowName(*RowNameString);
        if (DataTable->RemoveRow(RowName))
        {
            ++OutRowsRemoved;
        }
    }

    return true;
}

bool FMCPDataTableUtils::SaveAssetPackage(
    UPackage* Package,
    UObject* Asset,
    const FString& PackageName,
    FString& OutErrorMessage)
{
    if (!Package || !Asset)
    {
        OutErrorMessage = TEXT("Invalid package or asset reference for saving.");
        return false;
    }

    const FString PackageFilename = FPackageName::LongPackageNameToFilename(
        PackageName,
        FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    if (!UPackage::SavePackage(Package, Asset, *PackageFilename, SaveArgs))
    {
        OutErrorMessage = FString::Printf(TEXT("Failed to save package '%s'."), *PackageFilename);
        return false;
    }

    return true;
}

TSharedPtr<FJsonObject> FMCPCreateDataTableHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO("Handling create_data_table command");

    FString PackagePath;
    if (!Params->TryGetStringField(FStringView(TEXT("package_path")), PackagePath))
    {
        MCP_LOG_WARNING("Missing 'package_path' field in create_data_table command");
        return CreateErrorResponse(TEXT("Missing 'package_path' field"));
    }

    FString DataTableName;
    if (!Params->TryGetStringField(FStringView(TEXT("name")), DataTableName))
    {
        MCP_LOG_WARNING("Missing 'name' field in create_data_table command");
        return CreateErrorResponse(TEXT("Missing 'name' field"));
    }

    FString RowStructPath;
    if (!Params->TryGetStringField(FStringView(TEXT("row_struct")), RowStructPath))
    {
        MCP_LOG_WARNING("Missing 'row_struct' field in create_data_table command");
        return CreateErrorResponse(TEXT("Missing 'row_struct' field"));
    }

    bool bOverwriteExisting = false;
    Params->TryGetBoolField(FStringView(TEXT("overwrite")), bOverwriteExisting);

    FString PackageName;
    FString ObjectPath;
    FString PathError;
    if (!FMCPDataTableUtils::NormaliseAssetPaths(PackagePath, DataTableName, PackageName, ObjectPath, PathError))
    {
        MCP_LOG_WARNING(TEXT("%s"), *PathError);
        return CreateErrorResponse(PathError);
    }

    UPackage* Package = CreatePackage(*PackageName);
    if (!Package)
    {
        FString Message = FString::Printf(TEXT("Failed to create package '%s'."), *PackageName);
        MCP_LOG_ERROR(TEXT("%s"), *Message);
        return CreateErrorResponse(Message);
    }

    Package->FullyLoad();

    UScriptStruct* RowStruct = LoadObject<UScriptStruct>(nullptr, *RowStructPath);
    if (!RowStruct)
    {
        FString Message = FString::Printf(TEXT("Failed to load row struct '%s'."), *RowStructPath);
        MCP_LOG_ERROR(TEXT("%s"), *Message);
        return CreateErrorResponse(Message);
    }

    UDataTable* DataTable = FindObject<UDataTable>(Package, *DataTableName);
    bool bCreatedNewAsset = false;

    if (DataTable)
    {
        if (!bOverwriteExisting)
        {
            FString Message = FString::Printf(TEXT("Data table '%s' already exists."), *ObjectPath);
            MCP_LOG_WARNING(TEXT("%s"), *Message);
            return CreateErrorResponse(Message);
        }

        DataTable->Modify();
        DataTable->EmptyTable();
    }
    else
    {
        DataTable = NewObject<UDataTable>(Package, *DataTableName, RF_Public | RF_Standalone);
        if (!DataTable)
        {
            FString Message = FString::Printf(TEXT("Failed to create data table '%s'."), *DataTableName);
            MCP_LOG_ERROR(TEXT("%s"), *Message);
            return CreateErrorResponse(Message);
        }

        bCreatedNewAsset = true;
    }

    DataTable->Modify();
    DataTable->RowStruct = RowStruct;

    const TSharedPtr<FJsonObject>* RowsObject = nullptr;
    Params->TryGetObjectField(FStringView(TEXT("rows")), RowsObject);

    int32 RowsApplied = 0;
    FString RowsError;
    if (RowsObject && !FMCPDataTableUtils::ApplyRowsToDataTable(DataTable, *RowsObject, RowsApplied, RowsError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *RowsError);
        return CreateErrorResponse(RowsError);
    }

    DataTable->MarkPackageDirty();
    DataTable->PostEditChange();

    FString SaveError;
    if (!FMCPDataTableUtils::SaveAssetPackage(Package, DataTable, PackageName, SaveError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *SaveError);
        return CreateErrorResponse(SaveError);
    }

    if (bCreatedNewAsset)
    {
        FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        AssetRegistry.AssetCreated(DataTable);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), DataTable->GetName());
    Result->SetStringField(TEXT("path"), DataTable->GetPathName());
    Result->SetStringField(TEXT("row_struct"), DataTable->RowStruct ? DataTable->RowStruct->GetPathName() : TEXT(""));
    Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
    Result->SetBoolField(TEXT("overwrote_existing"), !bCreatedNewAsset);
    Result->SetNumberField(TEXT("rows_applied"), RowsApplied);

    MCP_LOG_INFO(TEXT("Created data table '%s' with %d rows."), *DataTable->GetPathName(), RowsApplied);
    return CreateSuccessResponse(Result);
}

TSharedPtr<FJsonObject> FMCPModifyDataTableHandler::Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket)
{
    MCP_LOG_INFO("Handling modify_data_table command");

    FString DataTablePath;
    if (!Params->TryGetStringField(FStringView(TEXT("path")), DataTablePath))
    {
        MCP_LOG_WARNING("Missing 'path' field in modify_data_table command");
        return CreateErrorResponse(TEXT("Missing 'path' field"));
    }

    FString NormalisedPath = DataTablePath;
    NormalisedPath.TrimStartAndEndInline();

    FString PackagePathPart;
    FString ObjectNamePart;

    if (NormalisedPath.Split(TEXT("."), &PackagePathPart, &ObjectNamePart, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        PackagePathPart = EnsureGameRoot(PackagePathPart);
        if (ObjectNamePart.IsEmpty())
        {
            ObjectNamePart = FPackageName::GetLongPackageAssetName(PackagePathPart);
        }
    }
    else
    {
        PackagePathPart = EnsureGameRoot(NormalisedPath);
        ObjectNamePart = FPackageName::GetLongPackageAssetName(PackagePathPart);
    }

    NormalisedPath = FString::Printf(TEXT("%s.%s"), *PackagePathPart, *ObjectNamePart);

    UDataTable* DataTable = LoadObject<UDataTable>(nullptr, *NormalisedPath);
    if (!DataTable)
    {
        FString Message = FString::Printf(TEXT("Failed to load data table '%s'."), *NormalisedPath);
        MCP_LOG_ERROR(TEXT("%s"), *Message);
        return CreateErrorResponse(Message);
    }

    bool bClearExisting = false;
    Params->TryGetBoolField(FStringView(TEXT("clear_existing")), bClearExisting);

    const TSharedPtr<FJsonObject>* RowsObject = nullptr;
    Params->TryGetObjectField(FStringView(TEXT("add_or_update_rows")), RowsObject);

    const TArray<TSharedPtr<FJsonValue>>* RemoveRowsArray = nullptr;
    Params->TryGetArrayField(FStringView(TEXT("remove_rows")), RemoveRowsArray);

    DataTable->Modify();

    if (bClearExisting)
    {
        DataTable->EmptyTable();
    }

    int32 RowsApplied = 0;
    FString OperationError;
    if (RowsObject && !FMCPDataTableUtils::ApplyRowsToDataTable(DataTable, *RowsObject, RowsApplied, OperationError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *OperationError);
        return CreateErrorResponse(OperationError);
    }

    int32 RowsRemoved = 0;
    if (RemoveRowsArray && !FMCPDataTableUtils::RemoveRowsFromDataTable(DataTable, *RemoveRowsArray, RowsRemoved, OperationError))
    {
        MCP_LOG_ERROR(TEXT("%s"), *OperationError);
        return CreateErrorResponse(OperationError);
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
    Result->SetStringField(TEXT("name"), DataTable->GetName());
    Result->SetStringField(TEXT("path"), DataTable->GetPathName());
    Result->SetNumberField(TEXT("row_count"), DataTable->GetRowMap().Num());
    Result->SetNumberField(TEXT("rows_applied"), RowsApplied);
    Result->SetNumberField(TEXT("rows_removed"), RowsRemoved);
    Result->SetBoolField(TEXT("cleared_existing"), bClearExisting);

    MCP_LOG_INFO(TEXT("Modified data table '%s' (applied: %d, removed: %d)."), *DataTable->GetPathName(), RowsApplied, RowsRemoved);
    return CreateSuccessResponse(Result);
}

