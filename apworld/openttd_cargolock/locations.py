from __future__ import annotations
from typing import Dict, List, TYPE_CHECKING
from BaseClasses import Location, Region
from . import items
if TYPE_CHECKING:
    from .world import OpenTTDWorld

CARGO_TYPES = [  # Temperate
        "Passengers", "Mail", "Coal", "Oil",
        "Livestock", "Goods", "Grain", "Wood",
        "Iron Ore", "Steel", "Valuables",
    ]

MISSION_DEFINITIONS = [
    # EARLY MISSIONS
    {"location": "Buy your first vehicle", "description": "Buy your first vehicle", "type": "have", "cargo": "", "unit": "", "amount": 1, "difficulty": "normal"},
    {"location": "Build your first station", "description": "Build your first station", "type": "build", "cargo": "", "unit": "", "amount": 1, "difficulty": "normal"},

    # VEHICLE MISSIONS
    {"location": "Own 1 train", "description": "Own 1 train", "type": "have", "cargo": "", "unit": "trains", "amount": 1, "difficulty": "normal"},
    {"location": "Own 1 road vehicle", "description": "Own 1 road vehicle", "type": "have", "cargo": "", "unit": "road vehicles", "amount": 1, "difficulty": "normal"},
    {"location": "Own 1 ship", "description": "Own 1 ship", "type": "have", "cargo": "", "unit": "ships", "amount": 1, "difficulty": "normal"},
    {"location": "Own 1 aircraft", "description": "Own 1 aircraft", "type": "have", "cargo": "", "unit": "aircraft", "amount": 1, "difficulty": "normal"},
    {"location": "Own 2 trains", "description": "Own 2 trains", "type": "have", "cargo": "", "unit": "trains", "amount": 2, "difficulty": "normal"},
    {"location": "Own 2 road vehicles", "description": "Own 2 road vehicles", "type": "have", "cargo": "", "unit": "road vehicles", "amount": 2, "difficulty": "normal"},
    {"location": "Own 2 ships", "description": "Own 2 ships", "type": "have", "cargo": "", "unit": "ships", "amount": 2, "difficulty": "normal"},
    {"location": "Own 2 aircrafts", "description": "Own 2 aircrafts", "type": "have", "cargo": "", "unit": "aircraft", "amount": 2, "difficulty": "normal"},

    # STATION MULTI-MODE MISSIONS
    {"location": "Have a station handle 2 vehicle types", "description": "Have a station handle 2 vehicle types", "type": "station_vehicle_types", "cargo": "", "unit": "vehicle types", "amount": 2, "difficulty": "normal"},
    {"location": "Have a station handle 3 vehicle types", "description": "Have a station handle 3 vehicle types", "type": "station_vehicle_types", "cargo": "", "unit": "vehicle types", "amount": 3, "difficulty": "normal"},
    {"location": "Have a station handle 4 vehicle types", "description": "Have a station handle 4 vehicle types", "type": "station_vehicle_types", "cargo": "", "unit": "vehicle types", "amount": 4, "difficulty": "normal"},

    # CARGO MISSIONS (1 unit)
    {"location": "Transport 1 unit of Passengers", "description": "Transport 1 unit of Passengers", "type": "transport", "cargo": "Passengers", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Mail", "description": "Transport 1 unit of Mail", "type": "transport", "cargo": "Mail", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Coal", "description": "Transport 1 unit of Coal", "type": "transport", "cargo": "Coal", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Oil", "description": "Transport 1 unit of Oil", "type": "transport", "cargo": "Oil", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Livestock", "description": "Transport 1 unit of Livestock", "type": "transport", "cargo": "Livestock", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Goods", "description": "Transport 1 unit of Goods", "type": "transport", "cargo": "Goods", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Grain", "description": "Transport 1 unit of Grain", "type": "transport", "cargo": "Grain", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Wood", "description": "Transport 1 unit of Wood", "type": "transport", "cargo": "Wood", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Iron Ore", "description": "Transport 1 unit of Iron Ore", "type": "transport", "cargo": "Iron Ore", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Steel", "description": "Transport 1 unit of Steel", "type": "transport", "cargo": "Steel", "unit": "units", "amount": 1, "difficulty": "normal"},
    {"location": "Transport 1 unit of Valuables", "description": "Transport 1 unit of Valuables", "type": "transport", "cargo": "Valuables", "unit": "units", "amount": 1, "difficulty": "normal"},

    # CARGO MISSIONS (1000 units)
    {"location": "Transport 1000 units of Passengers", "description": "Transport 1000 units of Passengers", "type": "transport", "cargo": "Passengers", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Mail", "description": "Transport 1000 units of Mail", "type": "transport", "cargo": "Mail", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Coal", "description": "Transport 1000 units of Coal", "type": "transport", "cargo": "Coal", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Oil", "description": "Transport 1000 units of Oil", "type": "transport", "cargo": "Oil", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Livestock", "description": "Transport 1000 units of Livestock", "type": "transport", "cargo": "Livestock", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Goods", "description": "Transport 1000 units of Goods", "type": "transport", "cargo": "Goods", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Grain", "description": "Transport 1000 units of Grain", "type": "transport", "cargo": "Grain", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Wood", "description": "Transport 1000 units of Wood", "type": "transport", "cargo": "Wood", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Iron Ore", "description": "Transport 1000 units of Iron Ore", "type": "transport", "cargo": "Iron Ore", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Steel", "description": "Transport 1000 units of Steel", "type": "transport", "cargo": "Steel", "unit": "units", "amount": 1000, "difficulty": "normal"},
    {"location": "Transport 1000 units of Valuables", "description": "Transport 1000 units of Valuables", "type": "transport", "cargo": "Valuables", "unit": "units", "amount": 1000, "difficulty": "normal"},
]

# Legacy table removed — costs now randomized between ShopCostMin and ShopCostMax.
BASE_SHOP_COSTS: List[int] = []

MAX_SHOP_SLOTS = 100

MISSIONS = [mission["location"] for mission in MISSION_DEFINITIONS]

# Export location name to ID mapping for Archipelago
LOCATION_NAME_TO_ID: Dict[str, int] = {
    name: i for i, name in enumerate(MISSIONS, start=1)
}

for index in range(1, MAX_SHOP_SLOTS + 1):
    LOCATION_NAME_TO_ID[f"Purchase Shop {index}"] = len(MISSIONS) + index


def get_shop_slot_count(world: OpenTTDWorld) -> int:
    if not world.options.enable_shop.value:
        return 0
    return world.options.shop_slots.value


def get_shop_tier_count(world: OpenTTDWorld) -> int:
    shop_slots = get_shop_slot_count(world)
    if shop_slots <= 0:
        return 0
    return min(shop_slots, world.options.shop_tiers.value)


def get_shop_definitions(world: OpenTTDWorld) -> List[Dict[str, object]]:
    """Generate shop locations with random costs between ShopCostMin and ShopCostMax.

    The generated list is cached on the world instance so all callers in one
    generation pass see the exact same shop costs and order.
    """
    cached = getattr(world, "_openttd_cached_shop_definitions", None)
    if cached is not None:
        return cached

    shop_slots = get_shop_slot_count(world)
    if shop_slots <= 0:
        setattr(world, "_openttd_cached_shop_definitions", [])
        return []
    
    min_cost = world.options.shop_cost_min.value
    max_cost = world.options.shop_cost_max.value
    
    # Ensure min <= max
    if min_cost > max_cost:
        min_cost, max_cost = max_cost, min_cost
    
    generated = [
        {
            "location": f"Purchase Shop {index}",
            "name": f"Shop {index}",
            "cost": _generate_random_shop_cost(world.random, min_cost, max_cost),
        }
        for index in range(1, shop_slots + 1)
    ]
    setattr(world, "_openttd_cached_shop_definitions", generated)
    return generated


def _generate_random_shop_cost(rng: "object", min_cost: int, max_cost: int) -> int:
    """Generate a random shop cost rounded to nearest 5000."""
    cost = rng.randint(min_cost, max_cost)
    return int(round(cost / 5000.0) * 5000)


def get_slot_missions() -> List[Dict[str, object]]:
    return [dict(mission) for mission in MISSION_DEFINITIONS]


def get_slot_shop_locations(world: OpenTTDWorld) -> List[Dict[str, object]]:
    return [dict(shop) for shop in get_shop_definitions(world)]


def get_slot_location_name_to_id(world: OpenTTDWorld) -> Dict[str, int]:
    selected_names = list(MISSIONS)
    selected_names.extend(shop["location"] for shop in get_shop_definitions(world))
    return {name: LOCATION_NAME_TO_ID[name] for name in selected_names}

class OpenTTDLocation(Location):
    game = "OpenTTD Cargolock"


def create_all_locations(world: OpenTTDWorld) -> List[OpenTTDLocation]:
    overworld = Region("Overworld", world.player, world.multiworld)
    world.multiworld.regions += [overworld]
    for name in MISSIONS:
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))
    for shop in get_shop_definitions(world):
        name = shop["location"]
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))

