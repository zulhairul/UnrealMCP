"""UI-focused commands for generating MVVM assets in Unreal."""

import json
import sys
import os
from typing import List, Optional, Dict, Any

from mcp.server.fastmcp import Context

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from unreal_mcp_bridge import send_command


def _serialize_properties(properties: Optional[List[Dict[str, Any]]]) -> List[Dict[str, Any]]:
    if not properties:
        return []

    serialized: List[Dict[str, Any]] = []
    for prop in properties:
        name = str(prop.get("name", "")).strip()
        if not name:
            continue

        serialized.append(
            {
                "name": name,
                "type": str(prop.get("type", "String")),
                "default": str(prop.get("default", "")),
            }
        )
    return serialized


def register_all(mcp):
    """Register UI commands with the MCP bridge."""

    @mcp.tool()
    def create_mvvm_ui(
        ctx: Context,
        name: str,
        package_path: str = "/Game/UI",
        viewmodel_properties: Optional[List[Dict[str, Any]]] = None,
        widget_name: Optional[str] = None,
        viewmodel_name: Optional[str] = None,
    ) -> str:
        """Create a Common Activatable widget wired to an MVVM ViewModel.

        Args:
            name: Base name for the generated assets.
            package_path: Destination content folder (e.g. /Game/UI/Login).
            viewmodel_properties: Optional list of property descriptors dict(name, type, default).
            widget_name: Optional override for the widget asset name.
            viewmodel_name: Optional override for the viewmodel asset name.
        """

        options: Dict[str, Any] = {}

        properties = _serialize_properties(viewmodel_properties)
        viewmodel_options: Dict[str, Any] = {}
        if properties:
            viewmodel_options["properties"] = properties
        if viewmodel_name:
            viewmodel_options["name"] = viewmodel_name
        if viewmodel_options:
            options["viewmodel"] = viewmodel_options

        widget_options: Dict[str, Any] = {}
        if widget_name:
            widget_options["name"] = widget_name
        if widget_options:
            options["widget"] = widget_options

        params = {
            "name": name,
            "package_path": package_path,
        }
        if options:
            params["options"] = options

        try:
            response = send_command("create_mvvm_ui", params)
        except Exception as exc:  # pragma: no cover - network / runtime errors
            return f"Error creating MVVM UI: {exc}"

        if response.get("status") != "success":
            return f"Error: {response.get('message', 'Unknown error')}"

        result = response.get("result", {})
        return json.dumps(result, indent=2)
