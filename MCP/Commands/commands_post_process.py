"""Post-processing and color grading tools for Unreal MCP."""

import json
import os
import sys

from mcp.server.fastmcp import Context

# Import send_command from the parent module
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from unreal_mcp_bridge import send_command


def register_all(mcp):
    """Register post-processing tools with the MCP server."""

    @mcp.tool()
    def apply_color_grading(
        ctx: Context,
        settings: dict,
        volume_name: str | None = None,
        create_if_missing: bool = True,
    ) -> str:
        """Apply color grading overrides to a post process volume.

        Args:
            settings: Dictionary of color grading options (e.g., color_saturation, temperature).
            volume_name: Optional actor name/label of the target post process volume.
            create_if_missing: Create an unbound global volume if none is found.
        """

        if not isinstance(settings, dict) or not settings:
            return "Error: settings must be a non-empty dictionary"

        params: dict[str, object] = {"settings": settings}
        if volume_name:
            params["volume_name"] = volume_name
        params["create_if_missing"] = bool(create_if_missing)

        try:
            response = send_command("apply_color_grading", params)
        except Exception as exc:  # pragma: no cover - network errors
            return f"Error applying color grading: {exc}"

        if response.get("status") == "success":
            return json.dumps(response.get("result", {}), indent=2)
        return f"Error: {response.get('message', 'Unknown error')}"

