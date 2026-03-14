from typing import Any
from BaseClasses import ItemClassification, Location, Region
from worlds.AutoWorld import World, WebWorld
from . import items, locations, options, rules

class OpenTTDWeb(WebWorld):
    """Web world for display and documentation."""
    theme = "ocean"
    option_groups = options.openttd_option_groups

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

    topology_present = True

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

    def fill_slot_data(self) -> dict[str, Any]:
        # Provide an explicit id->name map so clients can always resolve
        # ReceivedItems (including precollected items) by item ID.
        item_id_to_name = {
            str(item_id): item_name
            for item_name, item_id in self.item_name_to_id.items()
        }

        location_name_to_id = {
            name: loc_id
            for name, loc_id in self.location_name_to_id.items()
        }

        return {
            "item_id_to_name": item_id_to_name,
            "location_name_to_id": location_name_to_id,
            "mission_count": len(locations.MISSION_DEFINITIONS),
            "missions": locations.get_slot_missions(),

            # World generation settings (match OpenTTD map generation menu)
            "start_year": self.options.start_year.value,
            "map_x": self.options.map_size_x.map_bits,
            "map_y": self.options.map_size_y.map_bits,
            "landscape": self.options.landscape.value,
            "land_generator": self.options.land_generator.value,
            "terrain_type": self.options.terrain_type.value,
            "variety": self.options.variety_distribution.value,
            "tgen_smoothness": self.options.smoothness.value,
            "amount_of_rivers": self.options.rivers.value,
            "water_border_presets": self.options.map_edges.value,
            "town_name": self.options.town_names.value,
            "number_towns": self.options.number_of_towns.value,
            "quantity_sea_lakes": self.options.sea_level.value,
            "industry_density": self.options.industry_density.value,
        }
