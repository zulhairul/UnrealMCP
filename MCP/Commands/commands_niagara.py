"""Niagara VFX-focused commands for the Unreal MCP bridge."""

import sys
import os
from typing import Any, Dict, Optional

from mcp.server.fastmcp import Context

# Ensure bridge utilities are importable
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from unreal_mcp_bridge import send_command  # pylint: disable=wrong-import-position


def _build_options(
    template_path: Optional[str] = None,
    user_parameters: Optional[Dict[str, Dict[str, Any]]] = None,
    emitters: Optional[Dict[str, Any]] = None,
) -> Dict[str, Any]:
    options: Dict[str, Any] = {}
    if template_path:
        options["template_path"] = template_path
    if user_parameters:
        options["user_parameters"] = user_parameters
    if emitters:
        options["emitters"] = emitters
    return options


def register_all(mcp) -> None:
    """Register Niagara-related MCP tools."""

    @mcp.tool()
    def create_niagara_system(
        ctx: Context,
        package_path: str,
        name: str,
        template_path: Optional[str] = None,
        user_parameters: Optional[Dict[str, Dict[str, Any]]] = None,
        emitters: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Create a Niagara system asset in the project.

        Args:
            package_path: Unreal long package path (e.g. '/Game/VFX').
            name: Asset name for the Niagara system.
            template_path: Optional asset path for a Niagara system template to duplicate.
            user_parameters: Optional mapping of parameter names to {'type', 'value'} definitions.
            emitters: Optional dictionary describing emitter operations, e.g.
                {
                    "add": [
                        {"template_path": "/Niagara/Emitters/NS_SpriteBurst.NS_SpriteBurst", "name": "Burst", "enabled": True}
                    ]
                }

        Returns:
            The raw JSON response from Unreal with created asset metadata.
        """

        try:
            options = _build_options(template_path, user_parameters, emitters)
            params = {
                "package_path": package_path,
                "name": name,
            }
            if options:
                params["options"] = options

            response = send_command("create_niagara_system", params)
            return response
        except Exception as exc:  # pragma: no cover - defensive
            return {"status": "error", "message": str(exc)}

    @mcp.tool()
    def modify_niagara_system(
        ctx: Context,
        path: str,
        user_parameters: Optional[Dict[str, Dict[str, Any]]] = None,
        emitters: Optional[Dict[str, Any]] = None,
    ) -> Dict[str, Any]:
        """Modify an existing Niagara system.

        Args:
            path: Full object path to the Niagara system (e.g. '/Game/VFX/NS_Fire.NS_Fire').
            user_parameters: Optional parameter overrides.
            emitters: Optional emitter operations (add/remove/toggle entries).

        Returns:
            Raw JSON response from Unreal summarizing modifications.
        """

        try:
            options = _build_options(None, user_parameters, emitters)
            if not options:
                return {
                    "status": "error",
                    "message": "No modifications specified. Provide user_parameters and/or emitters.",
                }

            params = {
                "path": path,
                "options": options,
            }

            response = send_command("modify_niagara_system", params)
            return response
        except Exception as exc:  # pragma: no cover - defensive
            return {"status": "error", "message": str(exc)}

    @mcp.tool()
    def get_niagara_system_info(ctx: Context, path: str) -> Dict[str, Any]:
        """Fetch metadata about a Niagara system.

        Args:
            path: Full object path to the Niagara system asset.

        Returns:
            Dictionary containing emitters, exposed parameters, and asset metadata.
        """

        try:
            params = {"path": path}
            response = send_command("get_niagara_system_info", params)
            return response
        except Exception as exc:  # pragma: no cover - defensive
            return {"status": "error", "message": str(exc)}

