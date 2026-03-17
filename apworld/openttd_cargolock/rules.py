
from __future__ import annotations
from typing import TYPE_CHECKING
from BaseClasses import CollectionState
from .locations import get_shop_definitions, get_shop_slot_count, get_shop_tier_count
if TYPE_CHECKING:
    from .world import OpenTTDWorld

# All cargo types in Temperate landscape
CARGO_TYPES = {"Passengers", "Mail", "Coal", "Oil", "Livestock", "Grain", "Wood", "Iron Ore", "Steel", "Goods", "Valuables"}

# Cargo dependencies in Temperate landscape based on industry chain:
CARGO_DEPENDENCIES = {
    "Passengers": set(),
    "Mail": set(),
    "Coal": set(),
    "Oil": set(),
    "Livestock": set(),
    "Grain": set(),
    "Wood": set(),
    "Iron Ore": set(),
    "Valuables": set(),            
    "Steel": {"Iron Ore"},
    "Goods": {"Wood", "Grain", "Livestock", "Steel", "Oil"},
}

VEHICULE_CARGO_COMPATIBILITY = {
    "train": CARGO_TYPES,
    "road_vehicle": CARGO_TYPES,
    "ship": CARGO_TYPES,
    "aircraft": {"Passengers", "Mail", "Goods", "Valuables"}
}


def has_vehicle_type(state: CollectionState, player: int, vehicle_type: str) -> bool:
    """Check if player has unlocked a specific vehicle type tier.
    
    Args:
        state: The current game state
        player: The player ID
        vehicle_type: One of "train", "road_vehicle", "ship", or "aircraft"
    """
    vehicle_items = {
        "train": "Progressive Trains",
        "road_vehicle": "Progressive Road Vehicles",
        "ship": "Progressive Ships",
        "aircraft": "Progressive Aircrafts",
    }
    
    item_name = vehicle_items.get(vehicle_type)
    return item_name and state.has(item_name, player)


def has_cargo(state: CollectionState, player: int, cargo: str) -> bool:
    """Check if player has unlocked a cargo type.
    
    For dependent cargos (Goods, Steel, Valuables), this also checks that 
    at least one required source cargo has been unlocked.
    
    Args:
        state: The current game state
        player: The player ID
        cargo: The cargo type name
    """
    if not state.has(cargo, player):
        return False
    
    # For dependent cargos, check that at least one source cargo is available
    dependencies = CARGO_DEPENDENCIES.get(cargo, set())
    if dependencies:
        # At least one dependency must be met
        if not any(state.has(dep, player) for dep in dependencies):
            return False
    
    # Must have a vehicle that can transport this cargo type
    vehicle_types = ["train", "road_vehicle", "ship", "aircraft"]
    for vehicle_type in vehicle_types:
        if has_vehicle_type(state, player, vehicle_type):
            if cargo in VEHICULE_CARGO_COMPATIBILITY[vehicle_type]:
                return True
    return False


def unlocked_vehicle_type_count(state: CollectionState, player: int) -> int:
    """Count currently unlocked transport modes from progressive items."""
    return int(has_vehicle_type(state, player, "train")) + \
        int(has_vehicle_type(state, player, "road_vehicle")) + \
        int(has_vehicle_type(state, player, "ship")) + \
        int(has_vehicle_type(state, player, "aircraft"))


def visible_shop_location_count(state: CollectionState, player: int, shop_slots: int, shop_tiers: int) -> int:
    if shop_slots <= 0 or shop_tiers <= 0:
        return 0
    slots_per_tier = (shop_slots + shop_tiers - 1) // shop_tiers
    unlocked_tiers = 1 + state.count("Progressive Shop Upgrade", player)
    return min(shop_slots, slots_per_tier * unlocked_tiers)


def set_all_rules(world: "OpenTTDWorld") -> None:
    """Set access rules for locations.
    
    Vehicle missions require the corresponding progressive vehicle item.
    Cargo missions require the cargo item and any necessary dependencies.
    """
    multiworld = world.multiworld
    player = world.player
    shop_slots = get_shop_slot_count(world)
    shop_tiers = get_shop_tier_count(world)
    shop_definitions = get_shop_definitions(world)
    
    for location in world.multiworld.get_locations(player):
        name = location.name

        # EARLY MISSIONS
        if name == "Build your first station" or name == "Buy your first vehicle":
            location.access_rule = lambda state: state.has_any(["Progressive Trains", "Progressive Road Vehicles", "Progressive Aircrafts", "Progressive Ships"], player)

        # VEHICLE MISSIONS
        if name.startswith("Own"):
            if "train" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "train")
            elif "road vehicle" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "road_vehicle")
            elif "ship" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "ship")
            elif "aircraft" in name:
                location.access_rule = lambda state: has_vehicle_type(state, player, "aircraft")

        # STATION MULTI-MODE MISSIONS
        elif name.startswith("Have a station handle "):
            required_types = 0
            if "2 vehicle types" in name:
                required_types = 2
            elif "3 vehicle types" in name:
                required_types = 3
            elif "4 vehicle types" in name:
                required_types = 4
            if required_types > 0:
                location.access_rule = lambda state, required_types=required_types: unlocked_vehicle_type_count(state, player) >= required_types

        # CARGO MISSIONS
        elif name.startswith("Transport"):
            for cargo in CARGO_TYPES:
                if cargo in name:
                    location.access_rule = lambda state, cargo=cargo: has_cargo(state, player, cargo)
                    break

        elif name.startswith("Purchase Shop "):
            shop_index = next((index for index, shop in enumerate(shop_definitions, start=1) if shop["location"] == name), 0)
            if shop_index > 0:
                location.access_rule = lambda state, shop_index=shop_index: visible_shop_location_count(state, player, shop_slots, shop_tiers) >= shop_index

    world.multiworld.completion_condition[player] = lambda state: state.has_all(("Passengers", "Mail", "Coal", "Oil", "Livestock", "Goods", "Grain", "Wood", "Iron Ore", "Steel", "Valuables"), world.player)
