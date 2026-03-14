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

MISSIONS = [mission["location"] for mission in MISSION_DEFINITIONS]
    # station rating is pretty stupid
    #("Maintain 75%+ station rating for {amount} months", "months"),
    #("Maintain 90%+ station rating for {amount} months", "months"),
    #
    # ("Connect {amount} cities with rail", "cities"),
    # ("Service {amount} different towns", "towns"),
    
    # location specific mission ?
    #("Deliver {amount} passengers to [Town]", "passengers_to_town"),
    #("Deliver {amount} mail to [Town]", "mail_to_town"),
    #("Supply {amount} tons to [Industry near Town]", "cargo_to_industry"),
    #("Transport {amount} tons from [Industry near Town]", "cargo_from_industry"),&
    #("Deliver {amount} tons of {cargo} to one station", "tons"),
    
    # in x months who cares
    #("Earn £{amount} in one month", "£/month"),
    #("Earn £{amount} in a single month", "£/month"),
    
# Export location name to ID mapping for Archipelago
LOCATION_NAME_TO_ID: Dict[str, int] = {
    name: i for i, name in enumerate(MISSIONS, start=1)
}


def get_slot_missions() -> List[Dict[str, object]]:
    return [dict(mission) for mission in MISSION_DEFINITIONS]

class OpenTTDLocation(Location):
    game = "OpenTTD"


def create_all_locations(world: OpenTTDWorld) -> List[OpenTTDLocation]:
    overworld = Region("Overworld", world.player, world.multiworld)
    world.multiworld.regions += [overworld]
    for name in MISSIONS:
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))

