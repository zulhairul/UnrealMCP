"""Data table commands for the UnrealMCP bridge.

This module exposes tools for creating and modifying Unreal Engine data tables
through the MCP bridge, enabling AI-driven workflows for structured game data.
"""

import os
import sys
from typing import Dict, List, Optional

from mcp.server.fastmcp import Context

# Import send_command from the parent module
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from unreal_mcp_bridge import send_command


def register_all(mcp):
    """Register all data table-related commands with the MCP server."""

    @mcp.tool()
    def create_data_table(
        ctx: Context,
        package_path: str,
        name: str,
        row_struct: str,
        rows: Optional[Dict[str, Dict]] = None,
        overwrite: bool = False,
    ) -> str:
        """Create a new data table asset.

        Args:
            package_path: Long package path where the asset should be created (e.g. "/Game/Data").
            name: Name of the data table asset.
            row_struct: Long object path to the struct that defines the table rows (e.g. "/Script/YourModule.YourRowStruct").
            rows: Optional dictionary of initial rows to add. Keys are row names, values are dictionaries matching the struct fields.
            overwrite: When true and a table already exists at the same location, its contents will be replaced.
        """

        try:
            params = {
                "package_path": package_path,
                "name": name,
                "row_struct": row_struct,
                "overwrite": overwrite,
            }
            if rows:
                params["rows"] = rows

            response = send_command("create_data_table", params)

            if response["status"] == "success":
                result = response["result"]
                action = "Created" if not result.get("overwrote_existing") else "Rebuilt"
                return (
                    f"{action} data table '{result['name']}' at {result['path']} "
                    f"with {result.get('row_count', 0)} rows."
                )

            return f"Error: {response['message']}"
        except Exception as exc:  # pragma: no cover - defensive logging
            return f"Error creating data table: {exc}"

    @mcp.tool()
    def modify_data_table(
        ctx: Context,
        path: str,
        add_or_update_rows: Optional[Dict[str, Dict]] = None,
        remove_rows: Optional[List[str]] = None,
        clear_existing: bool = False,
    ) -> str:
        """Modify an existing data table asset.

        Args:
            path: Long object path to the data table (e.g. "/Game/Data/MyTable.MyTable").
            add_or_update_rows: Optional dictionary mapping row names to new data. Rows will be added or replaced.
            remove_rows: Optional list of row names to remove from the table.
            clear_existing: When true, existing rows are cleared before applying updates.
        """

        if not (add_or_update_rows or remove_rows or clear_existing):
            return "No modifications requested. Provide rows to add/update or remove, or set clear_existing=True."

        try:
            params = {"path": path}
            if add_or_update_rows:
                params["add_or_update_rows"] = add_or_update_rows
            if remove_rows:
                params["remove_rows"] = remove_rows
            if clear_existing:
                params["clear_existing"] = True

            response = send_command("modify_data_table", params)

            if response["status"] == "success":
                result = response["result"]
                applied = result.get("rows_applied", 0)
                removed = result.get("rows_removed", 0)
                row_count = result.get("row_count", 0)
                return (
                    f"Updated data table '{result['name']}' at {result['path']}: "
                    f"+{applied} / -{removed} (total {row_count})."
                )

            return f"Error: {response['message']}"
        except Exception as exc:  # pragma: no cover - defensive logging
            return f"Error modifying data table: {exc}"

