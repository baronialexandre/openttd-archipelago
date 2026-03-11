from typing import Any
from BaseClasses import ItemClassification, Location, Region
from worlds.AutoWorld import World, WebWorld
from . import items, locations, options, rules

class OpenTTDWeb(WebWorld):
    """Web world for display and documentation."""
    theme = "ocean"

class OpenTTDWorld(World):
    """
    OpenTTD is an open-source transport simulation game.
    Build transport networks using trains, road vehicles, aircraft, and ships.
    All vehicles and cargo types must be unlocked through Archipelago checks!
    """
    game = "OpenTTD"
    web = OpenTTDWeb()
    
    options_dataclass = options.OpenTTDOptions
    options: options.OpenTTDOptions
    
    item_name_to_id = items.ITEM_NAME_TO_ID
    location_name_to_id = locations.LOCATION_NAME_TO_ID

    origin_region_name = "Overworld"

    def create_regions(self) -> None:
        locations.create_all_locations(self)
        
    def set_rules(self) -> None:
        rules.set_all_rules(self)

    def create_items(self) -> None:
        items.create_all_items(self)

    def create_item(self, name: str) -> items.OpenTTDItem:
        return items.create_item_with_correct_classification(self, name)

    def get_filler_item_name(self) -> str:
        return items.get_random_filler_item_name(self)
