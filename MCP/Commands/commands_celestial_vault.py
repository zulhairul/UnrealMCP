"""Celestial Vault integration commands for the Unreal MCP bridge."""

from __future__ import annotations

import json
from typing import Any, Dict, List, Optional

from mcp.server.fastmcp import Context

from unreal_mcp_bridge import send_command


def _clean_sequence(values: Optional[List[float]]) -> Optional[List[float]]:
    if values is None:
        return None
    cleaned = [float(v) for v in values if v is not None]
    return cleaned if cleaned else None


def register_all(mcp) -> None:
    """Register Celestial Vault tools."""

    @mcp.tool()
    def setup_celestial_vault(
        ctx: Context,
        blueprint_path: str = "",
        actor_label: str = "",
        actor_name: str = "",
        location: Optional[List[float]] = None,
        rotation: Optional[List[float]] = None,
        scale: Optional[List[float]] = None,
        settings: Optional[Dict[str, Any]] = None,
        components: Optional[List[Dict[str, Any]]] = None,
    ) -> str:
        """Create or configure the Celestial Vault sky actor.

        Args:
            blueprint_path: Optional path to the Celestial Vault blueprint class.
            actor_label: Actor label to reuse or assign when spawning.
            actor_name: Internal actor name to reuse if already present.
            location: Optional XYZ transform override.
            rotation: Optional Pitch/Yaw/Roll transform override.
            scale: Optional XYZ scale override.
            settings: Property overrides applied directly to the sky actor.
            components: List of dictionaries with `property` and nested `settings`
                to apply to component references on the actor.
        """

        params: Dict[str, Any] = {}

        if blueprint_path:
            params["blueprint_path"] = blueprint_path
        if actor_label:
            params["actor_label"] = actor_label
        if actor_name:
            params["actor_name"] = actor_name

        cleaned_location = _clean_sequence(location)
        if cleaned_location:
            params["location"] = cleaned_location

        cleaned_rotation = _clean_sequence(rotation)
        if cleaned_rotation:
            params["rotation"] = cleaned_rotation

        cleaned_scale = _clean_sequence(scale)
        if cleaned_scale:
            params["scale"] = cleaned_scale

        if settings:
            params["settings"] = settings

        if components:
            params["components"] = components

        try:
            response = send_command("setup_celestial_vault", params)
        except Exception as exc:  # noqa: BLE001
            return f"Error calling setup_celestial_vault: {exc}"

        if response.get("status") == "success":
            return json.dumps(response.get("result", {}), indent=2)

        return f"Error: {response.get('message', 'Unknown error')}"

