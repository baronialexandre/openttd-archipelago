from __future__ import annotations
from typing import Dict, List, TYPE_CHECKING
from BaseClasses import Item, ItemClassification
if TYPE_CHECKING:
    from .world import OpenTTDWorld

PROGRESSIVE_TRAINS: List[List[str]] = [[
    "Kirby Paul Tank (Steam)",
    "Chaney 'Jubilee' (Steam)",
    "Ginzu 'A4' (Steam)",
    "SH '8P' (Steam)",
],[
    "Manley-Morel DMU (Diesel)",
    "'Dash' (Diesel)",
    "SH/Hendry '25' (Diesel)",
    "UU '37' (Diesel)",
    "Floss '47' (Diesel)",
    "SH '125' (Diesel)",
],[
    "SH '30' (Electric)",
    "SH '40' (Electric)",
    "'AsiaStar' (Electric)",
    "'T.I.M.' (Electric)",
],[
    "'X2001' (Electric)",
    "'Millennium Z1' (Electric)",
],[
    "Lev1 'Leviathan' (Electric)",
    "Lev2 'Cyclops' (Electric)",
    "Lev3 'Pegasus' (Electric)",
    "Lev4 'Chimaera' (Electric)",
]]

PROGRESSIVE_ROAD_VEHICLES: List[List[str]] = [[
    "MPS Regal Bus",
    "MPS Mail Truck",
    "Balogh Coal Truck",
    "Hereford Grain Truck",
    "Balogh Goods Truck",
    "Witcombe Oil Tanker",
    "Witcombe Wood Truck",
    "MPS Iron Ore Truck",
    "Balogh Steel Truck",
    "Balogh Armoured Truck",
    "Talbott Livestock Van",
],[
    "Hereford Leopard Bus",
    "Perry Mail Truck",
    "Uhl Coal Truck",
    "Thomas Grain Truck",
    "Craighead Goods Truck",
    "Foster Oil Tanker",
    "Foster Wood Truck",
    "Uhl Iron Ore Truck",
    "Uhl Steel Truck",
    "Uhl Armoured Truck",
    "Uhl Livestock Van",
],[
    "Foster Bus",
    "Reynard Mail Truck",
    "DW Coal Truck",
    "Goss Grain Truck",
    "Goss Goods Truck",
    "Perry Oil Tanker",
    "Moreland Wood Truck",
    "Chippy Iron Ore Truck",
    "Kelling Steel Truck",
    "Foster Armoured Truck",
    "Foster Livestock Van",
],[
    "Foster MkII Superbus",
]]

PROGRESSIVE_AIRCRAFT: List[List[str]] = [[
    "Sampson U52",
    "Bakewell Cotswald LB-3",
    "Coleman Count",
],[
    "FFP Dart",
    "Bakewell Luckett LB-8",
    "Darwin 100",
    "Yate Aerospace YAC 1-11",
    "Bakewell Luckett LB-9",
    "Tricario Helicopter",
    "Guru X2 Helicopter",
],[
    "Darwin 200",
    "Darwin 300",
    "Yate Haugan",
    "Guru Galaxy",
    "Bakewell Luckett LB-10",
    "Airtaxi A21",
    "Bakewell Luckett LB80",
    "Yate Aerospace YAe46",
    "Darwin 400",
    "Darwin 500",
    "Airtaxi A31",
    "Dinger 100",
    "Airtaxi A32",
    "Bakewell Luckett LB-11",
    "Darwin 600",
    "Airtaxi A33",
],[    
    "AirTaxi A34-1000",
    "Dinger 1000",
    "Dinger 200",
    "Yate Z-Shuttle",
    "Kelling K1",
    "Kelling K6",
    "FFP Hyperdart 2",
    "Kelling K7",
    "Darwin 700",
]]

PROGRESSIVE_SHIPS: List[List[str]] = [[
    "MPS Passenger Ferry",
    "MPS Oil Tanker",
    "Yate Cargo Ship",
],[
    "FFP Passenger Ferry",
    "CS-Inc. Oil Tanker",
    "Bakewell Cargo Ship",
],[
    "Bakewell 300 Hovercraft",
]]

PROGRESSIVE_VEHICLE_TIERS: Dict[str, int] = {
    "Progressive Trains": len(PROGRESSIVE_TRAINS),
    "Progressive Road Vehicles": len(PROGRESSIVE_ROAD_VEHICLES),
    "Progressive Aircrafts": len(PROGRESSIVE_AIRCRAFT),
    "Progressive Ships": len(PROGRESSIVE_SHIPS),
}

CARGO_TYPES = [  # Temperate
        "Passengers", 
        "Mail", 
        "Coal", 
        "Oil",
        "Livestock", 
        "Goods", 
        "Grain", 
        "Wood",
        "Iron Ore", 
        "Steel", 
        "Valuables",
]

FILLER_ITEMS: List[str] = [
    "50,000 $",
    "Choo chooo!"
]

ITEM_NAME_TO_ID = {
    "Progressive Trains": 1,
    "Progressive Road Vehicles": 2,
    "Progressive Aircrafts": 3,
    "Progressive Ships": 4,
    "Passengers": 5,
    "Mail": 6,
    "Coal": 7,
    "Oil": 8,
    "Livestock": 9,
    "Goods": 10,
    "Grain": 11,
    "Wood": 12,
    "Iron Ore": 13,
    "Steel": 14,
    "Valuables": 15,
    "50,000 $": 16,
    "Choo chooo!": 17
}

DEFAULT_ITEM_CLASSIFICATION = {
    "Progressive Trains": ItemClassification.progression,
    "Progressive Road Vehicles": ItemClassification.progression,
    "Progressive Aircrafts": ItemClassification.progression,
    "Progressive Ships": ItemClassification.progression,
    "Passengers": ItemClassification.progression,
    "Mail": ItemClassification.progression,
    "Coal": ItemClassification.progression,
    "Oil": ItemClassification.progression,
    "Livestock": ItemClassification.progression,
    "Goods": ItemClassification.progression,
    "Grain": ItemClassification.progression,
    "Wood": ItemClassification.progression,
    "Iron Ore": ItemClassification.progression,
    "Steel": ItemClassification.progression,
    "Valuables": ItemClassification.progression,
    "50,000 $": ItemClassification.filler,
    "Choo chooo!": ItemClassification.filler
}

class OpenTTDItem(Item):
    game = "OpenTTD"

def get_random_filler_item_name(world: OpenTTDWorld) -> str:
    return world.random.choice(FILLER_ITEMS)

def create_item_with_correct_classification(world: OpenTTDWorld, name: str) -> OpenTTDItem:
    classification = DEFAULT_ITEM_CLASSIFICATION[name]
    return OpenTTDItem(name, classification, ITEM_NAME_TO_ID[name], world.player)

def create_all_items(world: OpenTTDWorld) -> None:
    # world.options.starting_vehicle_type
    if world.options.starting_vehicle_type == 0:
        starting_vehicle_type = world.random.choice(list(PROGRESSIVE_VEHICLE_TIERS.keys()))
    elif world.options.starting_vehicle_type == 1:
        starting_vehicle_type = "Progressive Trains"
    elif world.options.starting_vehicle_type == 2:
        starting_vehicle_type = "Progressive Road Vehicles"
    elif world.options.starting_vehicle_type == 3:
        starting_vehicle_type = "Progressive Aircrafts"
    elif world.options.starting_vehicle_type == 4:
        starting_vehicle_type = "Progressive Ships"

    world.multiworld.push_precollected(world.create_item(starting_vehicle_type))
    PROGRESSIVE_VEHICLE_TIERS[starting_vehicle_type] -= 1  # Remove one tier of the chosen vehicle type from the item pool, since it's precollected.    

    # world.options.starting_cargo_type
    if world.options.starting_cargo_type == 0:
        starting_cargo_type = world.random.choice([c for c in CARGO_TYPES if c not in {"Goods", "Steel"}])
    elif world.options.starting_cargo_type == 1:
        starting_cargo_type = "Passengers"
    elif world.options.starting_cargo_type == 2:    
        starting_cargo_type = "Mail"
    elif world.options.starting_cargo_type == 3:
        starting_cargo_type = "Coal"
    elif world.options.starting_cargo_type == 4:
        starting_cargo_type = "Oil"
    elif world.options.starting_cargo_type == 5:
        starting_cargo_type = "Livestock"
    elif world.options.starting_cargo_type == 6:
        starting_cargo_type = "Grain"
    elif world.options.starting_cargo_type == 7:
        starting_cargo_type = "Wood"
    elif world.options.starting_cargo_type == 8:
        starting_cargo_type = "Iron Ore"
    elif world.options.starting_cargo_type == 9:
        starting_cargo_type = "Valuables"
    world.multiworld.push_precollected(world.create_item(starting_cargo_type))
    CARGO_TYPES.remove(starting_cargo_type)  # Remove the chosen cargo type from the item pool, since it's precollected.
    
    itempool: list[Item] = []
    for name , count in PROGRESSIVE_VEHICLE_TIERS.items():
        for _ in range(1, count + 1):
            itempool.append(create_item_with_correct_classification(world, name))
    for name in CARGO_TYPES:
        itempool.append(create_item_with_correct_classification(world, name))
    
    number_of_unfilled_locations = len(world.multiworld.get_unfilled_locations(world.player))
    needed_number_of_filler_items = number_of_unfilled_locations - len(itempool)
    itempool += [world.create_filler() for _ in range(needed_number_of_filler_items)]
    world.multiworld.itempool += itempool

    
# Flatten all vehicles into a single list
#ALL_VEHICLES: List[str] = []
#for tier in PROGRESSIVE_TRAINS:
#    ALL_VEHICLES.extend(tier)
#for tier in PROGRESSIVE_ROAD_VEHICLES:
#    ALL_VEHICLES.extend(tier)
#for tier in PROGRESSIVE_AIRCRAFT:
#    ALL_VEHICLES.extend(tier)
#for tier in PROGRESSIVE_SHIPS:
#    ALL_VEHICLES.extend(tier)