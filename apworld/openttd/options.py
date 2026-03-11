from dataclasses import dataclass
from Options import (
    Choice, Range, Toggle, DeathLink, PerGameCommonOptions,
    OptionGroup
)


# ═══════════════════════════════════════════════════════════════
#  RANDOMIZER OPTIONS
# ═══════════════════════════════════════════════════════════════

class StartingVehicleType(Choice):
    """Which vehicle type you start with.
    'random' picks randomly from all transport types.
    For all options, Starting Vehicle Count controls how many vehicles you receive."""
    display_name = "Starting Vehicle Type"
    option_any           = 0
    option_train         = 1
    option_road_vehicle  = 2
    option_aircraft      = 3
    option_ship          = 4
    default = 0


class StartingCashBonus(Choice):
    """Extra cash given to your company at the start of a session,
    on top of whatever loan you take. Helps new players build their
    first routes without going bankrupt.
    None:        £0          (default — no bonus)
    Small:       £50,000
    Medium:      £200,000
    Large:       £500,000
    Very Large:  £2,000,000"""
    display_name       = "Starting Cash Bonus"
    option_none        = 0
    option_small       = 1
    option_medium      = 2
    option_large       = 3
    option_very_large  = 4
    default = 0

# ═══════════════════════════════════════════════════════════════
#  WORLD GENERATION
# ═══════════════════════════════════════════════════════════════

class StartYear(Range):
    """The starting year for the game world. Default is 1950."""
    display_name = "Start Year"
    range_start = 1
    range_end   = 5_000_000
    default     = 1950


class MapSizeX(Choice):
    """Width of the generated map. Minimum 512 — smaller maps don't have enough towns and industries for named missions."""
    display_name = "Map Width"
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    default = 3  # 512

    @property
    def map_bits(self) -> int:
        return self.value + 6


class MapSizeY(Choice):
    """Height of the generated map. Minimum 512 — smaller maps don't have enough towns and industries for named missions."""
    display_name = "Map Height"
    option_512  = 3
    option_1024 = 4
    option_2048 = 5
    default = 3  # 512

    @property
    def map_bits(self) -> int:
        return self.value + 6


class Landscape(Choice):
    """The climate / landscape type for the game world."""
    display_name  = "Landscape"
    option_temperate = 0
    option_arctic    = 1
    option_tropical  = 2
    option_toyland   = 3
    default = 0


class LandGenerator(Choice):
    """Which terrain generator to use."""
    display_name        = "Land Generator"
    option_original     = 0
    option_terragenesis = 1
    default = 1


class IndustryDensity(Choice):
    """Number of industries generated at game start."""
    display_name     = "Industry Density"
    option_fund_only = 0
    option_minimal   = 1
    option_very_low  = 2
    option_low       = 3
    option_normal    = 4
    option_high      = 5
    default = 4


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — ECONOMY & FINANCE
# ═══════════════════════════════════════════════════════════════

class InfiniteMoney(Toggle):
    """Allow spending despite negative balance (cheat mode)."""
    display_name = "Infinite Money"
    default = 0


class Inflation(Toggle):
    """Enable inflation over time."""
    display_name = "Inflation"
    default = 0


class MaxLoan(Range):
    """Maximum initial loan available to the player, in pounds."""
    display_name = "Maximum Initial Loan (£)"
    range_start = 100_000
    range_end   = 500_000_000
    default     = 300_000


class InfrastructureMaintenance(Toggle):
    """Monthly maintenance fee for owned infrastructure."""
    display_name = "Infrastructure Maintenance"
    default = 0


class VehicleCosts(Choice):
    """Running cost multiplier for vehicles."""
    display_name  = "Vehicle Running Costs"
    option_low    = 0
    option_medium = 1
    option_high   = 2
    default = 1


class ConstructionCost(Choice):
    """Construction cost multiplier."""
    display_name  = "Construction Costs"
    option_low    = 0
    option_medium = 1
    option_high   = 2
    default = 1


class EconomyType(Choice):
    """Economy volatility type."""
    display_name    = "Economy Type"
    option_original = 0
    option_smooth   = 1
    option_frozen   = 2
    default = 1


class Bribe(Toggle):
    """Allow bribing the local authority."""
    display_name = "Allow Bribing"
    default = 1


class ExclusiveRights(Toggle):
    """Allow buying exclusive transport rights."""
    display_name = "Exclusive Rights"
    default = 1


class FundBuildings(Toggle):
    """Allow funding new buildings in towns."""
    display_name = "Fund Buildings"
    default = 1


class FundRoads(Toggle):
    """Allow funding local road reconstruction."""
    display_name = "Fund Roads"
    default = 1


class GiveMoney(Toggle):
    """Allow giving money to other companies."""
    display_name = "Give Money to Competitors"
    default = 1


class TownCargoScale(Range):
    """Scale cargo production of towns (percent)."""
    display_name = "Town Cargo Scale (%)"
    range_start = 15
    range_end   = 500
    default     = 100


class IndustryCargoScale(Range):
    """Scale cargo production of industries (percent)."""
    display_name = "Industry Cargo Scale (%)"
    range_start = 15
    range_end   = 500
    default     = 100


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — VEHICLES & INFRASTRUCTURE
# ═══════════════════════════════════════════════════════════════

class MaxTrains(Range):
    """Maximum number of trains per company."""
    display_name = "Max Trains"
    range_start = 0
    range_end   = 65535
    default     = 500


class MaxRoadVehicles(Range):
    """Maximum number of road vehicles per company."""
    display_name = "Max Road Vehicles"
    range_start = 0
    range_end   = 65535
    default     = 500


class MaxAircraft(Range):
    """Maximum number of aircraft per company."""
    display_name = "Max Aircraft"
    range_start = 0
    range_end   = 65535
    default     = 200


class MaxShips(Range):
    """Maximum number of ships per company."""
    display_name = "Max Ships"
    range_start = 0
    range_end   = 65535
    default     = 300


class MaxTrainLength(Range):
    """Maximum length for trains in tiles. Extended beyond vanilla limit of 64."""
    display_name = "Max Train Length (tiles)"
    range_start = 1
    range_end   = 1000
    default     = 7


class StationSpread(Range):
    """
    How many tiles apart station parts may be and still join.
    Set to 1024 for virtually unlimited spread.
    """
    display_name = "Station Spread (tiles)"
    range_start = 4
    range_end   = 1024
    default     = 12


class RoadStopOnTownRoad(Toggle):
    """Allow drive-through road stops on roads owned by towns."""
    display_name = "Road Stops on Town Roads"
    default = 1


class RoadStopOnCompetitorRoad(Toggle):
    """Allow drive-through road stops on roads owned by competitors."""
    display_name = "Road Stops on Competitor Roads"
    default = 1


class CrossingWithCompetitor(Toggle):
    """Allow level crossings with roads or rails owned by competitors."""
    display_name = "Level Crossings with Competitors"
    default = 1


class RoadSide(Choice):
    """Which side of the road vehicles drive on."""
    display_name = "Drive Side"
    option_left  = 0
    option_right = 1
    default = 1


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — TOWNS & ENVIRONMENT
# ═══════════════════════════════════════════════════════════════

class TownGrowthRate(Choice):
    """How fast towns grow."""
    display_name     = "Town Growth Rate"
    option_none      = 0
    option_slow      = 1
    option_normal    = 2
    option_fast      = 3
    option_very_fast = 4
    default = 2


class FoundTown(Choice):
    """Whether players can found new towns."""
    display_name          = "Town Founding"
    option_forbidden      = 0
    option_allowed        = 1
    option_custom_layout  = 2
    default = 0


class AllowTownRoads(Toggle):
    """Allow towns to build their own roads."""
    display_name = "Towns Build Roads"
    default = 1


# ═══════════════════════════════════════════════════════════════
#  GAME SETTINGS — DISASTERS & ACCIDENTS
# ═══════════════════════════════════════════════════════════════

class Disasters(Toggle):
    """Enable random disasters (floods, UFOs, etc.)."""
    display_name = "Disasters"
    default = 0


class PlaneCrashes(Choice):
    """Frequency of plane crashes."""
    display_name   = "Plane Crashes"
    option_none    = 0
    option_reduced = 1
    option_normal  = 2
    default = 2


class VehicleBreakdowns(Choice):
    """Likelihood of vehicle breakdowns."""
    display_name   = "Vehicle Breakdowns"
    option_none    = 0
    option_reduced = 1
    option_normal  = 2
    default = 1


# ═══════════════════════════════════════════════════════════════
#  OPTION GROUPS — defines the categories in the Options Creator
# ═══════════════════════════════════════════════════════════════

OPTION_GROUPS = [
    OptionGroup("Randomizer", [
        StartingVehicleType,
        StartingCashBonus,
    ]),
    OptionGroup("World Generation", [
        StartYear,
        MapSizeX,
        MapSizeY,
        Landscape,
        LandGenerator,
        IndustryDensity,
    ]),
    OptionGroup("Economy & Finance", [
        InfiniteMoney,
        Inflation,
        MaxLoan,
        InfrastructureMaintenance,
        VehicleCosts,
        ConstructionCost,
        EconomyType,
        Bribe,
        ExclusiveRights,
        FundBuildings,
        FundRoads,
        GiveMoney,
        TownCargoScale,
        IndustryCargoScale,
    ]),
    OptionGroup("Vehicles & Infrastructure", [
        MaxTrains,
        MaxRoadVehicles,
        MaxAircraft,
        MaxShips,
        MaxTrainLength,
        StationSpread,
        RoadStopOnTownRoad,
        RoadStopOnCompetitorRoad,
        CrossingWithCompetitor,
        RoadSide,
    ]),
    OptionGroup("Towns & Environment", [
        TownGrowthRate,
        FoundTown,
        AllowTownRoads,
    ]),
    OptionGroup("Disasters & Accidents", [
        Disasters,
        PlaneCrashes,
        VehicleBreakdowns,
    ]),
]


# ═══════════════════════════════════════════════════════════════
#  MAIN OPTIONS DATACLASS
# ═══════════════════════════════════════════════════════════════

class OpenTTDDeathLink(DeathLink):
    """When you die, everyone dies. When anyone else dies, you die.
    Death is triggered by vehicle crashes. Off by default."""
    default = 0


@dataclass
class OpenTTDOptions(PerGameCommonOptions):
    # Randomizer
    starting_vehicle_type:           StartingVehicleType
    starting_cash_bonus:             StartingCashBonus
    # World Generation
    start_year:                      StartYear
    map_size_x:                      MapSizeX
    map_size_y:                      MapSizeY
    landscape:                       Landscape
    land_generator:                  LandGenerator
    industry_density:                IndustryDensity
    # Economy & Finance
    infinite_money:                  InfiniteMoney
    inflation:                       Inflation
    max_loan:                        MaxLoan
    infrastructure_maintenance:      InfrastructureMaintenance
    vehicle_costs:                   VehicleCosts
    construction_cost:               ConstructionCost
    economy_type:                    EconomyType
    bribe:                           Bribe
    exclusive_rights:                ExclusiveRights
    fund_buildings:                  FundBuildings
    fund_roads:                      FundRoads
    give_money:                      GiveMoney
    town_cargo_scale:                TownCargoScale
    industry_cargo_scale:            IndustryCargoScale
    # Vehicles & Infrastructure
    max_trains:                      MaxTrains
    max_roadveh:                     MaxRoadVehicles
    max_aircraft:                    MaxAircraft
    max_ships:                       MaxShips
    max_train_length:                MaxTrainLength
    station_spread:                  StationSpread
    road_stop_on_town_road:          RoadStopOnTownRoad
    road_stop_on_competitor_road:    RoadStopOnCompetitorRoad
    crossing_with_competitor:        CrossingWithCompetitor
    road_side:                       RoadSide
    # Towns & Environment
    town_growth_rate:                TownGrowthRate
    found_town:                      FoundTown
    allow_town_roads:                AllowTownRoads
    # Disasters & Accidents
    disasters:                       Disasters
    plane_crashes:                   PlaneCrashes
    vehicle_breakdowns:              VehicleBreakdowns
    # Death Link
    death_link:                      OpenTTDDeathLink
