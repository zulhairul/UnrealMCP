"""Template management commands for Unreal Engine."""

import json
import os
import sys
from typing import Any, Dict, Optional

from mcp.server.fastmcp import Context

# Ensure the bridge module can be imported when running as a script
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from unreal_mcp_bridge import send_command  # noqa: E402  pylint: disable=wrong-import-position


def register_all(mcp):
    """Register template-related commands with the MCP server."""

    @mcp.tool()
    def import_template_variant(
        ctx: Context,
        variant: str,
        destination_folder: Optional[str] = None,
        category: Optional[str] = None,
        overwrite_existing: bool = False,
    ) -> str:
        """Import a project template variant (third person, first person, top down).

        Args:
            variant: Human-friendly template name (e.g. "third person").
            destination_folder: Optional folder to place the imported assets under Content/.
            category: Optional high-level category folder (defaults to MCPTemplates).
            overwrite_existing: Replace the destination folder if it already exists.
        """

        params: Dict[str, Any] = {"variant": variant}
        if destination_folder:
            params["destination_folder"] = destination_folder
        if category:
            params["category"] = category
        if overwrite_existing:
            params["overwrite_existing"] = True

        try:
            response = send_command("import_template_variant", params)
        except Exception as exc:  # pragma: no cover - transport errors
            return f"Error importing template: {exc}"

        status = response.get("status")
        if status == "success":
            return json.dumps(response.get("result", {}), indent=2)

        return f"Error: {response.get('message', 'Unknown error') }"

