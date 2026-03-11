from __future__ import annotations
from typing import Dict, List, TYPE_CHECKING
from BaseClasses import Location, Region
if TYPE_CHECKING:
    from .world import OpenTTDWorld

CARGO_TYPES = [  # Temperate
        "Passengers", "Mail", "Coal", "Oil",
        "Livestock", "Goods", "Grain", "Wood",
        "Iron Ore", "Steel", "Valuables",
    ]

STATIC_MISSIONS = {
    # EARLY MISSIONS
    "Buy a vehicle from the shop": 1,
    "Build your first station": 2,
    "Earn ${amount} total profit": 3,

    # VEHICLE MISSIONS
    "Own {amount} trains": 4,
    "Own {amount} road vehicles": 5,
    "Own {amount} ships": 6,
    "Own {amount} aircrafts": 7,

    # CARGO MISSIONS
    "Transport {amount} of Passengers": 8,
    "Transport {amount} of Mail": 9,
    "Transport {amount} of Coal": 10,
    "Transport {amount} of Oil": 11,
    "Transport {amount} of Livestock": 12,
    "Transport {amount} of Goods": 13,
    "Transport {amount} of Grain": 14,
    "Transport {amount} of Wood": 15,
    "Transport {amount} of Iron Ore": 16,
    "Transport {amount} of Steel": 17,
    "Transport {amount} of Valuables": 18,
    "Buy a vehicle from the shop 2": 19,
    "Buy a vehicle from the shop 3": 20,
    "Buy a vehicle from the shop 4": 21,
    "Buy a vehicle from the shop 5": 22,
    "Buy a vehicle from the shop 6": 23,
    "Buy a vehicle from the shop 7": 24,
    "Buy a vehicle from the shop 8": 25,
    "Buy a vehicle from the shop 9": 26,
    "Buy a vehicle from the shop 10": 27,
    "Buy a vehicle from the shop 11": 28,
    "Buy a vehicle from the shop 12": 29,
    "Buy a vehicle from the shop 13": 30,
    "Buy a vehicle from the shop 14": 31,
    "Buy a vehicle from the shop 15": 32,
    "Buy a vehicle from the shop 16": 33,
    "Buy a vehicle from the shop 17": 34,
    "Buy a vehicle from the shop 18": 35,
}
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
LOCATION_NAME_TO_ID: Dict[str, int] = STATIC_MISSIONS.copy()

class OpenTTDLocation(Location):
    game = "OpenTTD"


def create_all_locations(world: OpenTTDWorld) -> List[OpenTTDLocation]:
    overworld = Region("Overworld", world.player, world.multiworld)
    world.multiworld.regions += [overworld]
    for name in STATIC_MISSIONS.keys():
        overworld.locations.append(OpenTTDLocation(world.player, name, STATIC_MISSIONS[name], overworld))



