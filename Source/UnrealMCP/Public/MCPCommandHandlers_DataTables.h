#pragma once

#include "CoreMinimal.h"
#include "MCPCommandHandlers.h"
#include "Engine/DataTable.h"

#include "Dom/JsonObject.h"

/**
 * Utility helpers for working with data tables via MCP commands.
 */
class FMCPDataTableUtils
{
public:
    /**
     * Normalise package and object paths for a data table asset.
     */
    static bool NormaliseAssetPaths(
        const FString& InPackagePath,
        const FString& AssetName,
        FString& OutPackageName,
        FString& OutObjectPath,
        FString& OutErrorMessage);

    /**
     * Convert a JSON object into row data for the supplied struct.
     */
    static bool ConvertJsonToStruct(
        const TSharedPtr<FJsonObject>& JsonObject,
        UScriptStruct* StructType,
        uint8* StructData,
        FString& OutErrorMessage);

    /**
     * Apply the supplied rows to the target data table (add or replace).
     */
    static bool ApplyRowsToDataTable(
        UDataTable* DataTable,
        const TSharedPtr<FJsonObject>& RowsObject,
        int32& OutRowsApplied,
        FString& OutErrorMessage);

    /**
     * Remove rows from the data table.
     */
    static bool RemoveRowsFromDataTable(
        UDataTable* DataTable,
        const TArray<TSharedPtr<FJsonValue>>& RowNames,
        int32& OutRowsRemoved,
        FString& OutErrorMessage);

    /**
     * Save the package that owns the supplied asset.
     */
    static bool SaveAssetPackage(
        UPackage* Package,
        UObject* Asset,
        const FString& PackageName,
        FString& OutErrorMessage);
};

/**
 * Handler for creating data tables.
 */
class FMCPCreateDataTableHandler : public FMCPCommandHandlerBase
{
public:
    FMCPCreateDataTableHandler()
        : FMCPCommandHandlerBase(TEXT("create_data_table"))
    {
    }

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;
};

/**
 * Handler for modifying existing data tables.
 */
class FMCPModifyDataTableHandler : public FMCPCommandHandlerBase
{
public:
    FMCPModifyDataTableHandler()
        : FMCPCommandHandlerBase(TEXT("modify_data_table"))
    {
    }

    virtual TSharedPtr<FJsonObject> Execute(const TSharedPtr<FJsonObject>& Params, FSocket* ClientSocket) override;
};

