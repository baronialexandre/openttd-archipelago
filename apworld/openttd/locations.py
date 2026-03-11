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

MISSIONS = [
    # EARLY MISSIONS
    "Buy your first vehicle",
    "Build your first station",

    # GOAL
    # "Earn ${amount} total profit",

    # VEHICLE MISSIONS
    "Own 1 train",
    "Own 1 road vehicle",
    "Own 1 ship",
    "Own 1 aircraft",
    "Own 2 trains",
    "Own 2 road vehicles",
    "Own 2 ships",
    "Own 2 aircrafts",

    # CARGO MISSIONS
    "Transport 1 unit of Passengers",
    "Transport 1 unit of Mail",
    "Transport 1 unit of Coal",
    "Transport 1 unit of Oil",
    "Transport 1 unit of Livestock",
    "Transport 1 unit of Goods",
    "Transport 1 unit of Grain",
    "Transport 1 unit of Wood",
    "Transport 1 unit of Iron Ore",
    "Transport 1 unit of Steel",
    "Transport 1 unit of Valuables",
    "Transport 1000 units of Passengers",
    "Transport 1000 units of Mail",
    "Transport 1000 units of Coal",
    "Transport 1000 units of Oil",
    "Transport 1000 units of Livestock",
    "Transport 1000 units of Goods",
    "Transport 1000 units of Grain",
    "Transport 1000 units of Wood",
    "Transport 1000 units of Iron Ore",
    "Transport 1000 units of Steel",
    "Transport 1000 units of Valuables",
]
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

class OpenTTDLocation(Location):
    game = "OpenTTD"


def create_all_locations(world: OpenTTDWorld) -> List[OpenTTDLocation]:
    overworld = Region("Overworld", world.player, world.multiworld)
    world.multiworld.regions += [overworld]
    for name in MISSIONS:
        overworld.locations.append(OpenTTDLocation(world.player, name, LOCATION_NAME_TO_ID[name], overworld))

