"""Gameplay Ability System commands for Unreal Engine.

This module exposes MCP tools that streamline working with the Gameplay Ability System (GAS)
from conversational interfaces. It allows creating gameplay effect assets, generating modifier
payloads and registering effects into data tables for runtime discovery.
"""

from __future__ import annotations

import os
import sys
from typing import Dict, List, Optional

from mcp.server.fastmcp import Context

# Import send_command from the parent module
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from unreal_mcp_bridge import send_command


def register_all(mcp):
    """Register all Gameplay Ability System related MCP tools."""

    @mcp.tool()
    def make_ge_modifier(
        ctx: Context,
        attribute_set: str,
        attribute_property: str,
        magnitude: float,
        operation: str = "Additive",
        source_require_tags: Optional[List[str]] = None,
        target_require_tags: Optional[List[str]] = None,
    ) -> Dict:
        """Build a Gameplay Effect modifier entry that can be reused in config payloads.

        Args:
            attribute_set: Full class path to the attribute set (e.g. '/Script/MyGame.MyAttributeSet').
            attribute_property: The attribute property name within the set (e.g. 'Health').
            magnitude: Constant magnitude applied by the modifier.
            operation: The modifier operation ('Additive', 'Multiplicative', 'Division', 'Override').
            source_require_tags: Optional array of gameplay tags required on the source.
            target_require_tags: Optional array of gameplay tags required on the target.

        Returns:
            A dictionary suitable for inclusion under the ``modifiers`` array in ``create_gameplay_effect``.
        """

        modifier: Dict[str, object] = {
            "attribute": {
                "set": attribute_set,
                "property": attribute_property,
            },
            "magnitude": magnitude,
            "operation": operation,
        }

        if source_require_tags:
            modifier["source_requirements"] = {"require": source_require_tags}

        if target_require_tags:
            modifier["target_requirements"] = {"require": target_require_tags}

        return modifier

    @mcp.tool()
    def make_application_requirements(
        ctx: Context,
        require_tags: Optional[List[str]] = None,
        ignore_tags: Optional[List[str]] = None,
    ) -> Dict:
        """Create an application requirement payload for Gameplay Effects.

        Args:
            require_tags: Gameplay tags that must be present for the effect to apply.
            ignore_tags: Gameplay tags that block the effect when present.

        Returns:
            A dictionary usable as the ``application_requirements`` field in effect configs.
        """

        payload: Dict[str, List[str]] = {}
        if require_tags:
            payload["require"] = require_tags
        if ignore_tags:
            payload["ignore"] = ignore_tags
        return payload

    @mcp.tool()
    def create_gameplay_effect(
        ctx: Context,
        package_path: str,
        name: str,
        config: Optional[Dict] = None,
        parent_class: Optional[str] = None,
        overwrite: bool = False,
    ) -> Dict:
        """Create or update a Gameplay Effect asset in the project.

        Args:
            package_path: Target content path (e.g. '/Game/Abilities/Effects').
            name: Asset name to create within the package.
            config: Dictionary describing Gameplay Effect properties, including keys like:
                - duration_policy: 'Instant', 'Infinite', or 'Has_Duration' (case-insensitive).
                - duration_seconds: Required when duration_policy is 'Has_Duration'.
                - period_seconds: Optional tick period for periodic effects.
                - execute_period_on_application: bool flag to trigger the first tick immediately.
                - stack_limit, stacking_type, stack_duration_policy, stack_period_policy, stack_expiration_policy.
                - modifiers: List of modifier dicts (see make_ge_modifier).
                - granted_tags: List of gameplay tags granted while the effect is active.
                - application_requirements: Dict describing required and ignored tags.
            parent_class: Optional subclass of UGameplayEffect to instantiate.
            overwrite: When True, allows replacing an existing Gameplay Effect asset.

        Returns:
            On success, a dictionary containing the created Gameplay Effect metadata.
            On failure, a dictionary containing an ``error`` field with the reason.
        """

        try:
            params: Dict[str, object] = {
                "package_path": package_path,
                "name": name,
                "overwrite": overwrite,
            }
            if config:
                params["config"] = config
            if parent_class:
                params["parent_class"] = parent_class

            response = send_command("create_gameplay_effect", params)
            if response.get("status") == "success":
                return response.get("result", {})
            return {"error": response.get("message", "Unknown error")}
        except Exception as error:  # noqa: BLE001
            return {"error": str(error)}

    @mcp.tool()
    def create_attribute_set(
        ctx: Context,
        module_name: str,
        class_name: str,
        attributes: Optional[List[Dict[str, object]]] = None,
        public_subfolder: str = "Attributes",
        private_subfolder: str = "Attributes",
        overwrite: bool = False,
        module_api: Optional[str] = None,
    ) -> Dict[str, object]:
        """Generate a `UAttributeSet` subclass with optional replicated attributes.

        Args:
            module_name: Target module the class should live in (e.g. 'UnrealMCP' or the project module name).
            class_name: Name of the C++ class to create (without prefix 'U').
            attributes: Optional list describing attributes to scaffold. Each entry supports:
                - name (str): Attribute property name (PascalCase recommended).
                - initial_value (float, default 0.0): Initial base/current value.
                - category (str, default 'Attributes'): UPROPERTY category metadata.
                - replicated (bool, default True): Whether to generate replication plumbing.
                - tooltip (str, optional): Tooltip metadata for the property.
            public_subfolder: Subfolder relative to the module's Public directory.
            private_subfolder: Subfolder relative to the module's Private directory.
            overwrite: When True, replaces existing files if they already exist.
            module_api: Optional custom API export macro for the class (defaults to MODULE_API derived from module_name).

        Returns:
            Details about the generated header and source files, or an error payload.
        """

        try:
            params: Dict[str, object] = {
                "module_name": module_name,
                "class_name": class_name,
                "public_subfolder": public_subfolder,
                "private_subfolder": private_subfolder,
                "overwrite": overwrite,
            }

            if attributes:
                params["attributes"] = attributes

            if module_api:
                params["module_api"] = module_api

            response = send_command("create_attribute_set", params)
            if response.get("status") == "success":
                return response.get("result", {})

            return {"error": response.get("message", "Unknown error")}
        except Exception as error:  # noqa: BLE001
            return {"error": str(error)}

    @mcp.tool()
    def register_gameplay_effect(
        ctx: Context,
        data_table_path: str,
        row_name: str,
        gameplay_effect_path: str,
        effect_field: str = "GameplayEffect",
        additional_data: Optional[Dict] = None,
        overwrite: bool = True,
    ) -> Dict:
        """Register a Gameplay Effect reference into a data table row.

        Args:
            data_table_path: Asset path to the data table (package or object reference).
            row_name: Name of the row to create or update.
            gameplay_effect_path: Object path to the Gameplay Effect asset.
            effect_field: Field name within the row struct that should reference the effect.
            additional_data: Optional dictionary merged into the row payload (e.g. level requirements).
            overwrite: When False, the command fails if the row already exists.

        Returns:
            On success, a dictionary describing the applied row update.
            On failure, a dictionary containing an ``error`` field.
        """

        try:
            params: Dict[str, object] = {
                "data_table_path": data_table_path,
                "row_name": row_name,
                "gameplay_effect_path": gameplay_effect_path,
                "effect_field": effect_field,
                "overwrite": overwrite,
            }
            if additional_data:
                params["additional_data"] = additional_data

            response = send_command("register_gameplay_effect", params)
            if response.get("status") == "success":
                return response.get("result", {})
            return {"error": response.get("message", "Unknown error")}
        except Exception as error:  # noqa: BLE001
            return {"error": str(error)}
