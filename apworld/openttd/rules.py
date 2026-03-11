
from __future__ import annotations
from typing import TYPE_CHECKING
from BaseClasses import CollectionState
if TYPE_CHECKING:
    from .world import OpenTTDWorld

# Root cargos (no dependencies, can be starting cargo)
ROOT_CARGOS = {"Passengers", "Mail", "Coal", "Oil", "Livestock", "Grain", "Wood", "Iron Ore", "Valuables"}

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


def has_vehicle_type(state: CollectionState, player: int, vehicle_type: str) -> bool:
    """Check if player has unlocked a specific vehicle type tier.
    
    Args:
        state: The current game state
        player: The player ID
        vehicle_type: One of "trains", "road_vehicles", "aircraft", or "ships"
    """
    vehicle_items = {
        "trains": "Progressive Trains",
        "road_vehicles": "Progressive Road Vehicules",
        "aircraft": "Progressive Aircrafts",
        "ships": "Progressive Ships",
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
        return any(state.has(dep, player) for dep in dependencies)
    
    return True


def set_all_rules(world: "OpenTTDWorld") -> None:
    """Set access rules for locations.
    
    Vehicle missions require the corresponding progressive vehicle item.
    Cargo missions require the cargo item and any necessary dependencies.
    """
    multiworld = world.multiworld
    player = world.player
    
    # Define which locations require which items
    location_rules = {
        "Own {amount} trains": lambda state: has_vehicle_type(state, player, "trains"),
        "Own {amount} road vehicles": lambda state: has_vehicle_type(state, player, "road_vehicles"),
        "Own {amount} ships": lambda state: has_vehicle_type(state, player, "ships"),
        "Own {amount} aircrafts": lambda state: has_vehicle_type(state, player, "aircraft"),
        "Transport {amount} of Passengers": lambda state: has_cargo(state, player, "Passengers"),
        "Transport {amount} of Mail": lambda state: has_cargo(state, player, "Mail"),
        "Transport {amount} of Coal": lambda state: has_cargo(state, player, "Coal"),
        "Transport {amount} of Oil": lambda state: has_cargo(state, player, "Oil"),
        "Transport {amount} of Livestock": lambda state: has_cargo(state, player, "Livestock"),
        "Transport {amount} of Goods": lambda state: has_cargo(state, player, "Goods"),
        "Transport {amount} of Grain": lambda state: has_cargo(state, player, "Grain"),
        "Transport {amount} of Wood": lambda state: has_cargo(state, player, "Wood"),
        "Transport {amount} of Iron Ore": lambda state: has_cargo(state, player, "Iron Ore"),
        "Transport {amount} of Steel": lambda state: has_cargo(state, player, "Steel"),
        "Transport {amount} of Valuables": lambda state: has_cargo(state, player, "Valuables"),
    }
    
    # Apply rules to locations
    for location_name, rule in location_rules.items():
        try:
            location = multiworld.get_location(location_name, player)
            location.access_rule = rule
        except KeyError:
            # Location doesn't exist (e.g., dynamically generated missions)
            pass

