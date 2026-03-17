/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

/**
 * @file archipelago_manager.cpp
 * Game-logic integration for the Archipelago randomizer.
 *
 * Engine unlock strategy:
 *   Vanilla OpenTTD releases vehicles to companies via NewVehicleAvailable()
 *   which is called from CalendarEnginesMonthlyLoop() when the current date
 *   passes an engine's intro_date.  In AP mode engine.cpp skips that call
 *   (AP_IsActive() returns true), so no engine ever becomes available through
 *   the normal date path.  Instead, when the AP server sends us an item we
 *   call EnableEngineForCompany() directly — the same internal function that
 *   NewVehicleAvailable() calls — which sets company_avail, updates railtypes,
 *   and refreshes all affected GUI windows.
 */

#include "stdafx.h"
#include <charconv>
#include "archipelago.h"
#include "archipelago_gui.h"
#include "base_media_graphics.h"
#include "base_media_sounds.h"
#include "base_media_music.h"
#include "currency.h"
#include "engine_base.h"
#include "engine_func.h"
#include "engine_type.h"
#include "company_func.h"
#include "company_base.h"
#include "company_cmd.h"
#include "vehicle_base.h"
#include "industry.h"
#include "town.h"
#include "town_type.h"
#include "cargotype.h"
#include "core/random_func.hpp"
#include "rail.h"
#include "road_func.h"
#include "window_type.h"
#include "language.h"
#include "genworld.h"
#include "settings_type.h"
#include "newgrf_airport.h"
#include "debug.h"
#include "strings_func.h"
#include "news_type.h"
#include "news_func.h"
#include "string_func.h"
#include "table/strings.h"
#include "timer/timer_game_tick.h"
#include "timer/timer_game_calendar.h"
#include "timer/timer_game_realtime.h"
#include "timer/timer.h"

#include <map>
#include <string>
#include <atomic>
#include <set>

#include "core/format.hpp"
#include "console_func.h"
#include "console_internal.h"
#include "window_func.h"
#include "station_base.h"
#include "cargomonitor.h"
#include "newgrf_config.h"
#include "fileio_func.h"
#include "highscore.h"
#include "network/network.h"
#include "network/network_func.h"
#include "network/network_internal.h"
#include "network/network_gui.h"
#include "safeguards.h"

#include <array>

/* Console log helpers */
#define AP_LOG(msg)  IConsolePrint(CC_INFO,    "[AP] " + std::string(msg))
#define AP_OK(msg)   IConsolePrint(CC_WHITE,   "[AP] " + std::string(msg))
#define AP_WARN(msg) IConsolePrint(CC_WARNING, "[AP] WARNING: " + std::string(msg))
#define AP_ERR(msg)  IConsolePrint(CC_ERROR,   "[AP] ERROR: " + std::string(msg))

/* -------------------------------------------------------------------------
 * Cargo type lookup — maps AP cargo names (English) → CargoType index
 * Used by mission evaluation to check delivered_cargo[] by cargo type.
 * ---------------------------------------------------------------------- */

static std::map<std::string, CargoType> _ap_cargo_map;
static bool _ap_cargo_map_built = false;
static std::array<bool, NUM_CARGO> _ap_unlocked_cargo_types{};
static std::set<Colours> _ap_unlocked_company_colours;
static std::set<std::string> _ap_purchased_shop_locations;
static std::set<std::string> _ap_completed_mission_locations;

static void BuildCargoMap()
{
	_ap_cargo_map.clear();

	/* Standard temperate cargo label constants from cargo_type.h */
	static const std::pair<const char *, CargoLabel> kNameLabel[] = {
		{ "passengers",  CT_PASSENGERS },
		{ "coal",        CT_COAL       },
		{ "mail",        CT_MAIL       },
		{ "oil",         CT_OIL        },
		{ "livestock",   CT_LIVESTOCK  },
		{ "goods",       CT_GOODS      },
		{ "grain",       CT_GRAIN      },
		{ "wood",        CT_WOOD       },
		{ "iron ore",    CT_IRON_ORE   },
		{ "steel",       CT_STEEL      },
		{ "valuables",   CT_VALUABLES  },
	};

	for (auto &[name, label] : kNameLabel) {
		CargoType ct = GetCargoTypeByLabel(label);
		if (IsValidCargoType(ct)) {
			_ap_cargo_map[name] = ct;
		}
	}
	_ap_cargo_map_built = true;
}

static CargoType AP_FindCargoType(const std::string &name)
{
	if (name.empty()) return INVALID_CARGO;
	if (!_ap_cargo_map_built) BuildCargoMap();
	std::string lower = name;
	for (char &c : lower) c = (char)tolower((unsigned char)c);
	auto it = _ap_cargo_map.find(lower);
	return (it != _ap_cargo_map.end()) ? it->second : INVALID_CARGO;
}

/* -------------------------------------------------------------------------
 * Session statistics — cumulative cargo delivery and profit accumulators.
 *
 * OpenTTD stores per-period stats in old_economy[0] (last completed period)
 * and cur_economy (current ongoing period).  We detect when old_economy[0]
 * refreshes (= a new financial period ended) and add the values to our
 * running totals so we can track "total cargo delivered this session".
 * ---------------------------------------------------------------------- */

static uint64_t _ap_cumul_cargo[NUM_CARGO]      = {};  ///< Cargo delivered in completed periods
static Money    _ap_cumul_profit                = 0;   ///< Profit earned in completed periods
static bool     _ap_stats_initialized          = false;
static std::map<std::string, int> _ap_received_item_counts; ///< Count of each received item (all types, for inventory display and win-condition checks)

static void AP_RefreshMoneyDisplays()
{
	SetWindowDirty(WC_STATUS_BAR, 0);
	SetWindowDirty(WC_MAIN_TOOLBAR, 0);
	SetWindowClassesDirty(WC_ARCHIPELAGO);
	_ap_status_dirty.store(true);
}

/** Snapshot of last-seen old_economy[0] values (for change detection) */
static uint32_t _ap_snap_cargo[NUM_CARGO]       = {};
static Money    _ap_snap_profit                 = 0;

static void AP_InitSessionStats()
{
	for (CargoType i = 0; i < NUM_CARGO; i++) { _ap_cumul_cargo[i] = 0; _ap_snap_cargo[i] = 0; }
	_ap_cumul_profit      = 0;
	_ap_snap_profit       = 0;
	_ap_stats_initialized = false;
	if (!_ap_cargo_map_built) BuildCargoMap();
}

/**
 * Called every ~10 s from the realtime timer.
 * If old_economy[0] has changed since last call, accumulate the values.
 */
static void AP_UpdateSessionStats()
{
	CompanyID cid = _local_company;
	const Company *c = Company::GetIfValid(cid);
	if (c == nullptr) return;

	if (!_ap_stats_initialized) {
		/* Take initial snapshot so the first "change" doesn't double-count */
		for (CargoType i = 0; i < NUM_CARGO; i++)
			_ap_snap_cargo[i] = c->old_economy[0].delivered_cargo[i];
		_ap_snap_profit       = c->old_economy[0].income - c->old_economy[0].expenses;
		_ap_stats_initialized = true;
		return;
	}

	/* Detect if a new financial period has ended (old_economy[0] changed) */
	bool refreshed = false;
	for (CargoType i = 0; i < NUM_CARGO; i++) {
		if (c->old_economy[0].delivered_cargo[i] != _ap_snap_cargo[i]) {
			refreshed = true;
			break;
		}
	}
	if (!refreshed) {
		Money new_p = c->old_economy[0].income - c->old_economy[0].expenses;
		if (new_p != _ap_snap_profit) refreshed = true;
	}

	if (refreshed) {
		for (CargoType i = 0; i < NUM_CARGO; i++) {
			_ap_cumul_cargo[i] += c->old_economy[0].delivered_cargo[i];
			_ap_snap_cargo[i]   = c->old_economy[0].delivered_cargo[i];
		}
		Money period_profit = c->old_economy[0].income - c->old_economy[0].expenses;
		_ap_cumul_profit += period_profit;
		_ap_snap_profit   = period_profit;
		Debug(misc, 1, "[AP] Economy period snapped. Cumulative profit: ${}", (int64_t)_ap_cumul_profit);
	}
}

static std::string AP_FormatLocalCurrency(int64_t amount)
{
	const CurrencySpec &cs = GetCurrency();
	int64_t scaled = amount * cs.rate;
	std::string number = fmt::format("{}", scaled);
	if (cs.symbol_pos == 0) return cs.prefix + number + cs.suffix;
	return number + cs.suffix;
}

/**
 * Get total cargo delivered of a specific type: completed periods + current period.
 */
static uint64_t AP_GetTotalCargo(CargoType ct)
{
	if (ct == INVALID_CARGO || ct >= NUM_CARGO) return 0;
	const Company *c = Company::GetIfValid(_local_company);
	uint64_t cur = (c != nullptr) ? (uint64_t)c->cur_economy.delivered_cargo[ct] : 0;
	return _ap_cumul_cargo[ct] + cur;
}

/* -------------------------------------------------------------------------
 * Mission evaluation — called every ~1 s from the realtime timer.
 * For each incomplete mission, compute the current progress value and
 * check if the mission goal is met.  When met, send the location check
 * to the AP server and mark it completed.
 * ---------------------------------------------------------------------- */

/**
 * Count real stations (not waypoints) owned by local company.
 */
static int AP_CountStations()
{
	CompanyID cid = _local_company;
	int count = 0;
	for (const Station *st : Station::Iterate()) {
		if (st->owner == cid) count++;
	}
	return count;
}

/**
 * Return the highest number of transport modes served by any single station
 * owned by the local company.
 * Modes are: rail, road (bus+truck combined), air, ship.
 */
static int AP_MaxStationVehicleModes()
{
	CompanyID cid = _local_company;
	int best = 0;
	for (const Station *st : Station::Iterate()) {
		if (st->owner != cid) continue;

		int modes = 0;
		if (st->facilities.Test(StationFacility::Train)) modes++;
		if (st->facilities.Any({StationFacility::BusStop, StationFacility::TruckStop})) modes++;
		if (st->facilities.Test(StationFacility::Airport)) modes++;
		if (st->facilities.Test(StationFacility::Dock)) modes++;

		best = std::max(best, modes);
	}
	return best;
}

/**
 * Count primary vehicles of a given type owned by local company.
 * type == VEH_INVALID means all types.
 */
static int AP_CountVehicles(VehicleType type)
{
	CompanyID cid = _local_company;
	int count = 0;
	for (const Vehicle *v : Vehicle::Iterate()) {
		if (v->owner == cid && v->IsPrimaryVehicle()) {
			if (type == VEH_INVALID || v->type == type) count++;
		}
	}
	return count;
}

/**
 * Determine vehicle type to count based on mission unit field.
 * "trains"        → VEH_TRAIN
 * "aircraft"      → VEH_AIRCRAFT
 * "road vehicles" → VEH_ROAD
 * "ships"         → VEH_SHIP
 * anything else   → VEH_INVALID (= all types)
 */
static VehicleType AP_VehicleTypeFromUnit(const std::string &unit)
{
	if (unit == "trains")        return VEH_TRAIN;
	if (unit == "aircraft")      return VEH_AIRCRAFT;
	if (unit == "road vehicles") return VEH_ROAD;
	if (unit == "ships")         return VEH_SHIP;
	return VEH_INVALID;
}

/**
 * Evaluate a single mission and return the current progress value.
 * Returns true if the mission goal is now met.
 */
static bool EvaluateMission(APMission &m)
{
	/* Safety: a mission with amount <= 0 would auto-complete immediately.
	 * This should never happen after the Python fix, but guard here too. */
	if (m.amount <= 0) {
		Debug(misc, 1, "[AP] Mission '{}' has amount<=0, skipping evaluation", m.location);
		return false;
	}

	int64_t current = 0;

	/* ── "transport" type ──────────────────────────────────────────────
	 * "Transport {amount} units of {cargo}"
	 * "Transport {amount} passengers"
	 * Uses cumulative cargo delivered this session. */
	if (m.type == "transport") {
		/* If unit is "passengers" or cargo field is empty, use CT_PASSENGERS */
		CargoType ct = INVALID_CARGO;
		if (m.unit == "passengers" || m.cargo.empty()) {
			ct = AP_FindCargoType("passengers");
		} else {
			ct = AP_FindCargoType(m.cargo);
		}
		if (IsValidCargoType(ct)) {
			current = (int64_t)AP_GetTotalCargo(ct);
		} else {
			/* Fallback: sum all cargo types */
			uint64_t total = 0;
			for (CargoType i = 0; i < NUM_CARGO; i++) total += AP_GetTotalCargo(i);
			current = (int64_t)total;
		}
	}

	/* ── "have" type ───────────────────────────────────────────────────
	 * "Have {amount} vehicles/trains/aircraft/road vehicles running simultaneously" */
	else if (m.type == "have") {
		VehicleType vtype = AP_VehicleTypeFromUnit(m.unit);
		current = (int64_t)AP_CountVehicles(vtype);
	}

	/* ── "build" type ──────────────────────────────────────────────────
	 * "Build {amount} stations" */
	else if (m.type == "build") {
		current = (int64_t)AP_CountStations();
	}

	/* ── "station_vehicle_types" type ────────────────────────────────
	 * "Have a station handle {amount} vehicle types"
	 * Counts rail, road(bus+truck), air, ship at one single station. */
	else if (m.type == "station_vehicle_types") {
		current = (int64_t)AP_MaxStationVehicleModes();
	}

	/* ── "collect_cargo" type ──────────────────────────────────────
	 * "Unlock a cargo type" — mission is satisfied once the cargo type
	 * has been granted via an AP item.  Used by both hardcoded fallback
	 * missions and any APWorld-provided missions of this type. */
	else if (m.type == "collect_cargo") {
		/* cargo field contains cargo name (e.g. "Passengers") */
		CargoType ct = AP_FindCargoType(m.cargo);
		current = (IsValidCargoType(ct) && _ap_unlocked_cargo_types[ct]) ? 1 : 0;
	}

	/* Update live progress on the mission (visible in missions window) */
	m.current_value = current;

	return current >= m.amount;
}

/* -------------------------------------------------------------------------
 * Stored connection credentials (for reconnect)
 * ---------------------------------------------------------------------- */

std::string _ap_last_host;
uint16_t    _ap_last_port     = 38281;
std::string _ap_last_slot;
std::string _ap_last_pass;
bool        _ap_last_ssl = false;
static std::string _ap_progress_slot_identity;

static std::string AP_MakeSlotIdentity(const std::string &host, uint16_t port, const std::string &slot)
{
	if (host.empty() || slot.empty()) return {};
	return fmt::format("{}:{}|{}", host, port, slot);
}

static std::string AP_GetCurrentConnectionSlotIdentity()
{
	return AP_MakeSlotIdentity(_ap_last_host, _ap_last_port, _ap_last_slot);
}

/* -------------------------------------------------------------------------
 * Engine name <-> EngineID map
 * Built once when we enter GM_NORMAL for the first time in a session.
 * Used only for name-based lookups when AP sends us an item.
 * ---------------------------------------------------------------------- */

static std::map<std::string, EngineID> _ap_engine_map;
/* Extra engines that share a name with an already-mapped engine (e.g. the three
 * "Oil Tanker" wagon variants: Rail, Monorail, Maglev).  The primary map stores
 * the first match; extras stores all subsequent ones so they all get unlocked. */
static std::map<std::string, std::vector<EngineID>> _ap_engine_extras;
static bool _ap_engine_map_built = false;

/** EngineIDs that AP has explicitly unlocked for the local company this session.
 *  The periodic re-lock sweep uses this to distinguish "AP-unlocked" from
 *  "re-introduced by StartupEngines()" — only engines in this set survive. */
static std::set<EngineID> _ap_unlocked_engine_ids;

static void BuildEngineMap()
{
	_ap_engine_map.clear();
	_ap_engine_extras.clear();
	for (const Engine *e : Engine::Iterate()) {
		/* Primary: context-aware name — returns the NewGRF/callback name when
		 * available, and the language-file name for vanilla engines.
		 * However, EngineNameContext::PurchaseList only returns a non-empty name
		 * for engines that are currently in the purchase list (i.e. introduced and
		 * not yet expired).  With never_expire_vehicles=true already set above,
		 * expiry is no longer an issue — but intro_date still applies in early
		 * game years, so some future engines may still return empty here. */
		std::string name = GetString(STR_ENGINE_NAME,
		    PackEngineNameDParam(e->index, EngineNameContext::PurchaseList));

		/* Fallback: if the PurchaseList context returned empty (engine not yet
		 * available for purchase), get the name directly from the engine's
		 * string_id.  This is always populated for both vanilla and NewGRF
		 * engines, so it gives us the name regardless of availability. */
		if (name.empty() && e->info.string_id != STR_NEWGRF_INVALID_ENGINE) {
			name = GetString(e->info.string_id);
		}

		if (name.empty()) continue;

		if (_ap_engine_map.count(name) == 0) {
			_ap_engine_map[name] = e->index;
		} else {
			/* Same name used by multiple engine instances (e.g. the three
			 * "Oil Tanker" wagon variants — Rail, Monorail, Maglev).
			 * Store extras so AP_UnlockEngineByName can unlock them all. */
			_ap_engine_extras[name].push_back(e->index);
		}
	}
	_ap_engine_map_built = true;
	Debug(misc, 1, "[AP] Engine map built: {} engines", _ap_engine_map.size());
	AP_LOG(fmt::format("Engine map built: {} engines indexed ({} with shared names)",
	       _ap_engine_map.size(), _ap_engine_extras.size()));
}

/**
 * Force the game language to English (en_GB).
 * AP item names are always English, so the engine name lookup map must be
 * built with English strings.  The simplest guarantee is to switch the
 * running game to English at AP session start and keep it there.
 */
static void ForceEnglishLanguage()
{
	/* Already English? */
	if (_current_language != nullptr) {
		std::string_view iso = _current_language->isocode;
		if (iso.starts_with("en")) return;
	}

	/* Find en_GB in the language list */
	const LanguageMetadata *english = nullptr;
	for (const LanguageMetadata &lng : _languages) {
		std::string_view iso = lng.isocode;
		if (iso == "en_GB") { english = &lng; break; }
		if (iso.starts_with("en") && english == nullptr) english = &lng;
	}

	if (english == nullptr) {
		AP_WARN("Could not find English language pack — engine names may not match AP items!");
		return;
	}

	if (!ReadLanguagePack(english)) {
		AP_WARN("Failed to load English language pack!");
		return;
	}

	AP_OK("Language forced to English (en_GB) for AP item name compatibility.");
}

/* -------------------------------------------------------------------------
 * AP_IsActive — called from engine.cpp to block vanilla date-introduction
 * ---------------------------------------------------------------------- */

bool AP_IsActive()
{
	return _ap_client != nullptr &&
	       _ap_client->GetState() == APState::AUTHENTICATED;
}

bool AP_IsCargoTypeUnlocked(uint8_t cargo_type)
{
	if (cargo_type >= NUM_CARGO) return false;
	if (!AP_IsActive()) return true;
	return _ap_unlocked_cargo_types[cargo_type];
}

static bool AP_IsCargoWagonEngine(const Engine *e)
{
	return e != nullptr &&
		e->type == VEH_TRAIN &&
		e->VehInfo<RailVehicleInfo>().railveh_type == RAILVEH_WAGON;
}

static bool AP_ShouldWagonBeUnlockedForCargo(const Engine *e)
{
	if (!AP_IsCargoWagonEngine(e)) return false;
	CargoType ct = e->GetDefaultCargoType();
	if (!IsValidCargoType(ct) || ct >= NUM_CARGO) return false;
	return _ap_unlocked_cargo_types[ct];
}

static void AP_ApplyCargoWagonUnlocks(CompanyID cid)
{
	if (cid >= MAX_COMPANIES) return;

	bool changed = false;
	for (Engine *e : Engine::Iterate()) {
		if (!AP_IsCargoWagonEngine(e)) continue;

		const bool should_have = AP_ShouldWagonBeUnlockedForCargo(e);
		const bool has = e->company_avail.Test(cid);

		if (should_have) {
			if (!has) {
				e->flags.Set(EngineFlag::Available);
				EnableEngineForCompany(e->index, cid);
				changed = true;
			}
		} else if (has) {
			e->company_avail.Reset(cid);
			changed = true;
		}
	}

	if (changed) InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
}

/* -------------------------------------------------------------------------
 * AP_SendSay — forward a chat/command string to the AP server.
 * Used by the "ap" console command so players can type AP server commands
 * (e.g. !hint, !remaining) directly from the OpenTTD console.
 * ---------------------------------------------------------------------- */

void AP_SendSay(const std::string &text)
{
	if (_ap_client == nullptr) {
		IConsolePrint(CC_ERROR, "[AP] Not connected to Archipelago server.");
		return;
	}
	_ap_client->SendSay(text);
	IConsolePrint(CC_INFO, fmt::format("[AP] Sent: {}", text).c_str());
}

/** Console command: ap <message>
 *  Forwards the message to the Archipelago server as a Say packet.
 *  Examples:
 *    ap !hint "Forest of Magic"
 *    ap !remaining
 *    ap !getitem "Cash Injection £200,000"
 */
static bool ConCmdAP(std::span<std::string_view> argv)
{
	if (argv.size() < 2) {
		IConsolePrint(CC_HELP, "Usage: ap <message>");
		IConsolePrint(CC_HELP, "  Sends a message/command to the Archipelago server.");
		IConsolePrint(CC_HELP, "  Examples:  ap !hint Wills 2-8-0");
		IConsolePrint(CC_HELP, "             ap !remaining");
		IConsolePrint(CC_HELP, "             ap !status");
		return true;
	}
	/* Join all arguments after "ap" into one string */
	std::string msg;
	for (size_t i = 1; i < argv.size(); i++) {
		if (i > 1) msg += ' ';
		msg += std::string(argv[i]);
	}
	AP_SendSay(msg);
	return true;
}

/** Registers the "ap" console command. Called once at game init. */
void AP_RegisterConsoleCommands()
{
	IConsole::CmdRegister("ap", ConCmdAP);
}

/* -------------------------------------------------------------------------
 * AP_UnlockEngineByName
 * Calls OpenTTD's own EnableEngineForCompany() which handles:
 *   - company_avail.Set(company)
 *   - avail_railtypes / avail_roadtypes update
 *   - AddRemoveEngineFromAutoreplaceAndBuildWindows()
 *   - Toolbar invalidation
 * ---------------------------------------------------------------------- */

// Unlock all engines in a progressive tier for a given progressive item name

/* Progressive vehicle tier table — vehicle names per tier per item.
 * Kept at file scope so AP_GetProgressiveTiers() can expose it to the GUI. */
static const std::map<std::string, std::vector<std::vector<std::string>>> _ap_progressive_tiers = {
    {"Progressive Trains", {
        {"Kirby Paul Tank (Steam)", "Chaney 'Jubilee' (Steam)", "Ginzu 'A4' (Steam)", "SH '8P' (Steam)"},
        {"Manley-Morel DMU (Diesel)", "'Dash' (Diesel)", "SH/Hendry '25' (Diesel)", "UU '37' (Diesel)", "Floss '47' (Diesel)", "SH '125' (Diesel)"},
        {"SH '30' (Electric)", "SH '40' (Electric)", "'AsiaStar' (Electric)", "'T.I.M.' (Electric)"},
        {"'X2001' (Electric)", "'Millennium Z1' (Electric)"},
        {"Lev1 'Leviathan' (Electric)", "Lev2 'Cyclops' (Electric)", "Lev3 'Pegasus' (Electric)", "Lev4 'Chimaera' (Electric)"}
    }},
    {"Progressive Road Vehicles", {
        {"MPS Regal Bus", "MPS Mail Truck", "Balogh Coal Truck", "Hereford Grain Truck", "Balogh Goods Truck", "Witcombe Oil Tanker", "Witcombe Wood Truck", "MPS Iron Ore Truck", "Balogh Steel Truck", "Balogh Armoured Truck", "Talbott Livestock Van"},
        {"Hereford Leopard Bus", "Perry Mail Truck", "Uhl Coal Truck", "Thomas Grain Truck", "Craighead Goods Truck", "Foster Oil Tanker", "Foster Wood Truck", "Uhl Iron Ore Truck", "Uhl Steel Truck", "Uhl Armoured Truck", "Uhl Livestock Van"},
        {"Foster Bus", "Reynard Mail Truck", "DW Coal Truck", "Goss Grain Truck", "Goss Goods Truck", "Perry Oil Tanker", "Moreland Wood Truck", "Chippy Iron Ore Truck", "Kelling Steel Truck", "Foster Armoured Truck", "Foster Livestock Van"},
        {"Foster MkII Superbus"}
    }},
    {"Progressive Aircrafts", {
		{"Sampson U52", "Bakewell Cotswald LB-3", "Tricario Helicopter"},
		{"Coleman Count", "FFP Dart", "Bakewell Luckett LB-8", "Darwin 100", "Yate Aerospace YAC 1-11", "Bakewell Luckett LB-9", "Guru X2 Helicopter"},
        {"Darwin 200", "Darwin 300", "Yate Haugan", "Guru Galaxy", "Bakewell Luckett LB-10", "Airtaxi A21", "Bakewell Luckett LB80", "Yate Aerospace YAe46", "Darwin 400", "Darwin 500", "Airtaxi A31", "Dinger 100", "Airtaxi A32", "Bakewell Luckett LB-11", "Darwin 600", "Airtaxi A33"},
        {"AirTaxi A34-1000", "Dinger 1000", "Dinger 200", "Yate Z-Shuttle", "Kelling K1", "Kelling K6", "FFP Hyperdart 2", "Kelling K7", "Darwin 700"}
    }},
    {"Progressive Airports", {
		{"Heliport", "Helistation", "Helidepot", "Small Airport"},
		{"City Airport", "Commuter Airport"},
		{"Metropolitan Airport", "International Airport"},
		{"Intercontinental Airport"}
	}},
    {"Progressive Ships", {
        {"MPS Passenger Ferry", "MPS Oil Tanker", "Yate Cargo Ship"},
        {"FFP Passenger Ferry", "CS-Inc. Oil Tanker", "Bakewell Cargo Ship"},
        {"Bakewell 300 Hovercraft"}
    }}
};

struct APAirportTierUnlock {
	const char *name;
	uint8_t airport_type;
};

static constexpr TimerGameCalendar::Year AP_LOCKED_AIRPORT_YEAR = TimerGameCalendar::Year{5000001};

static const std::vector<std::vector<APAirportTierUnlock>> _ap_progressive_airport_tiers = {
	{{"Heliport", AT_HELIPORT}, {"Helistation", AT_HELISTATION}, {"Helidepot", AT_HELIDEPOT}, {"Small Airport", AT_SMALL}},
	{{"City Airport", AT_LARGE}, {"Commuter Airport", AT_COMMUTER}},
	{{"Metropolitan Airport", AT_METROPOLITAN}, {"International Airport", AT_INTERNATIONAL}},
	{{"Intercontinental Airport", AT_INTERCON}},
};

/* Tracks how many tiers have been unlocked per progressive item this session. */
static std::map<std::string, int> _ap_unlocked_tier_counts;

const std::map<std::string, std::vector<std::vector<std::string>>> &AP_GetProgressiveTiers() { return _ap_progressive_tiers; }
const std::map<std::string, int>                                   &AP_GetUnlockedTierCounts() { return _ap_unlocked_tier_counts; }

static bool AP_IsProgressiveUnlocked(const char *name)
{
	if (!AP_IsActive()) return true;
	auto it = _ap_unlocked_tier_counts.find(name);
	return it != _ap_unlocked_tier_counts.end() && it->second > 0;
}

bool AP_IsTrainUnlocked() { return AP_IsProgressiveUnlocked("Progressive Trains"); }
bool AP_IsRoadVehicleUnlocked() { return AP_IsProgressiveUnlocked("Progressive Road Vehicles"); }
bool AP_IsAircraftUnlocked() { return AP_IsProgressiveUnlocked("Progressive Aircrafts"); }
bool AP_IsShipUnlocked() { return AP_IsProgressiveUnlocked("Progressive Ships"); }

static void AP_SetAirportUnlocked(uint8_t airport_type, bool unlocked)
{
	if (airport_type >= NUM_AIRPORTS || airport_type == AT_OILRIG) return;
	AirportSpec *as = AirportSpec::GetWithoutOverride(airport_type);
	if (as == nullptr || !as->enabled) return;
	as->min_year = unlocked ? TimerGameCalendar::Year{0} : AP_LOCKED_AIRPORT_YEAR;
}

static void AP_LockAllAirports()
{
	int airport_count = 0;
	for (uint8_t i = 0; i < NUM_AIRPORTS; i++) {
		if (i == AT_OILRIG) continue;
		AirportSpec *as = AirportSpec::GetWithoutOverride(i);
		if (as == nullptr || !as->enabled) continue;
		AP_SetAirportUnlocked(i, false);
		airport_count++;
	}
	InvalidateWindowData(WC_BUILD_STATION, TRANSPORT_AIR);
	InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
	Debug(misc, 0, "[AP] All {} airports locked for progressive unlocks", airport_count);
}

static bool AP_UnlockAirportByName(const std::string &name)
{
	if (name != "Progressive Airports") return false;

	int &tier = _ap_unlocked_tier_counts[name];
	if (tier >= (int)_ap_progressive_airport_tiers.size()) return false;

	for (const APAirportTierUnlock &airport : _ap_progressive_airport_tiers[tier]) {
		AP_SetAirportUnlocked(airport.airport_type, true);
	}

	InvalidateWindowData(WC_BUILD_STATION, TRANSPORT_AIR);
	InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
	Debug(misc, 0, "[AP] Progressive airport tier unlocked: tier {}", tier + 1);
	tier++;
	return true;
}

bool AP_IsCompanyColourUnlocked(Colours colour)
{
	if (!AP_IsActive()) return true;
	if (colour >= COLOUR_END) return true;
	return _ap_unlocked_company_colours.count(colour) > 0;
}

static bool AP_TryUnlockCompanyColour(const std::string &item_name)
{
	static const std::pair<const char *, Colours> kCompanyColourItems[] = {
		{ "Company Colour: Dark Blue",  COLOUR_DARK_BLUE  },
		{ "Company Colour: Pale Green", COLOUR_PALE_GREEN },
		{ "Company Colour: Pink",       COLOUR_PINK       },
		{ "Company Colour: Yellow",     COLOUR_YELLOW     },
		{ "Company Colour: Red",        COLOUR_RED        },
		{ "Company Colour: Light Blue", COLOUR_LIGHT_BLUE },
		{ "Company Colour: Green",      COLOUR_GREEN      },
		{ "Company Colour: Dark Green", COLOUR_DARK_GREEN },
		{ "Company Colour: Blue",       COLOUR_BLUE       },
		{ "Company Colour: Cream",      COLOUR_CREAM      },
		{ "Company Colour: Mauve",      COLOUR_MAUVE      },
		{ "Company Colour: Purple",     COLOUR_PURPLE     },
		{ "Company Colour: Orange",     COLOUR_ORANGE     },
		{ "Company Colour: Brown",      COLOUR_BROWN      },
		{ "Company Colour: Grey",       COLOUR_GREY       },
		{ "Company Colour: White",      COLOUR_WHITE      },
	};

	for (const auto &[name, colour] : kCompanyColourItems) {
		if (item_name != name) continue;

		const bool was_empty = _ap_unlocked_company_colours.empty();
		_ap_unlocked_company_colours.insert(colour);

		/* Ensure the active player's default company colour is always one they own.
		 * This makes the precollected starting colour immediately take effect. */
		Company *c = Company::GetIfValid(_local_company);
		if (c != nullptr && (was_empty || !AP_IsCompanyColourUnlocked(c->colour))) {
			Command<CMD_SET_COMPANY_COLOUR>::Post(LS_DEFAULT, true, colour);
		}

		return true;
	}

	return false;
}

static bool AP_UnlockEngineByName(const std::string &name)
{
    CompanyID cid = _local_company;
    if (cid >= MAX_COMPANIES) return false;
    if (!_ap_engine_map_built) BuildEngineMap();

    // Define progressive tiers based on Python source of truth
    const auto &progressive_tiers = _ap_progressive_tiers;

    // Track which tier is unlocked for each progressive item
    auto &unlocked_tiers = _ap_unlocked_tier_counts;

    auto prog = progressive_tiers.find(name);
    if (prog != progressive_tiers.end()) {
        int &tier = unlocked_tiers[name];
        if (tier >= (int)prog->second.size()) return false; // All tiers unlocked
        const auto &vehicles = prog->second[tier];
        for (const std::string &veh : vehicles) {
            auto it = _ap_engine_map.find(veh);
            if (it != _ap_engine_map.end()) {
                Engine *e = Engine::GetIfValid(it->second);
                if (e) {
                    _ap_unlocked_engine_ids.insert(it->second);
                    e->flags.Set(EngineFlag::Available);
                    EnableEngineForCompany(it->second, cid);
                }
            }
        }
        InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
        Debug(misc, 0, "[AP] Progressive tier unlocked: {} tier {}", name, tier+1);
        tier++;
        return true;
    }

    // Item name not recognized as a progressive tier
    Debug(misc, 1, "[AP] UnlockEngine: '{}' not a recognized progressive item", name);
    return false;
}

/* -------------------------------------------------------------------------
 * In-game news/chat notification
 * ---------------------------------------------------------------------- */

static TextColour AP_ToConsoleColour(APPrintColour colour)
{
	switch (colour) {
		case APPrintColour::RED:
		case APPrintColour::SALMON:
			return CC_ERROR;
		case APPrintColour::YELLOW:
		case APPrintColour::ORANGE:
			return CC_WARNING;
		case APPrintColour::GREEN:
			return CC_INFO;
		case APPrintColour::BLUE:
		case APPrintColour::MAGENTA:
		case APPrintColour::CYAN:
		case APPrintColour::PLUM:
		case APPrintColour::SLATEBLUE:
			return CC_INFO;
		case APPrintColour::WHITE:
			return CC_WHITE;
		default:
			return CC_INFO;
	}
}

static std::string AP_TrimAPPrefix(std::string text)
{
	while (text.rfind("[AP]", 0) == 0) {
		text.erase(0, 4);
		while (!text.empty() && text[0] == ' ') text.erase(0, 1);
	}
	if (text.rfind("AP:", 0) == 0) {
		text.erase(0, 3);
		while (!text.empty() && text[0] == ' ') text.erase(0, 1);
	}
	return text;
}

static void AP_ShowNews(const std::string &text, APPrintColour colour = APPrintColour::DEFAULT)
{
	/* Normalize server/client messages so console/news never show duplicated
	 * tags like "[AP] [AP] ...". */
	std::string clean = AP_TrimAPPrefix(text);
	const TextColour console_colour = AP_ToConsoleColour(colour);

	IConsolePrint(console_colour, "AP: " + clean);
	Debug(misc, 0, "[AP] {}", clean);

	/* Show AP messages in local chat only.
	 * In multiplayer, broadcasting AP lines from host to all clients causes
	 * duplicates when each client also has their own AP connection. */
	if (_networking) {
		std::string ext_user = "Server";
		std::string ext_text = clean;
		auto sep = clean.find(": ");
		if (sep != std::string::npos && sep > 0) {
			ext_user = clean.substr(0, sep);
			ext_text = clean.substr(sep + 2);
		}
		NetworkTextMessage(NETWORK_ACTION_EXTERNAL_CHAT, console_colour, false, ext_user, ext_text, std::string("AP"));
	}

	if (_game_mode == GM_NORMAL) {
		AddNewsItem(
			GetEncodedString(STR_ARCHIPELAGO_NEWS, clean),
			NewsType::General,
			NewsStyle::Small,
			{}
		);
	}
}

/* -------------------------------------------------------------------------
 * Pending / deferred state
 * ---------------------------------------------------------------------- */

static APSlotData  _ap_pending_sd;
static bool        _ap_pending_world_start         = false;
static bool        _ap_goal_sent                   = false;
static bool        _ap_session_started             = false; ///< True once we've done first-tick setup in GM_NORMAL
static bool        _ap_menu_connect_start_new      = true;  ///< Menu connect mode: true=start new world, false=load save
static bool        _ap_menu_connect_multiplayer    = false; ///< Main-menu AP connect target: false=singleplayer, true=multiplayer.
static bool        _ap_open_network_window_pending = false; ///< Open MP window after AP world enters GM_NORMAL.


/* _ap_world_started_this_session is reset per-connection so that a
 * reconnect to the same save can re-trigger AP world-start handshake. */
static bool        _ap_world_started_this_session  = false;
static bool        _ap_named_entity_refresh_needed = false; ///< Set after Load() — defer GetString calls to first game tick
static bool        _ap_host_autoconnect_attempted  = false; ///< One-shot guard for MP host auto-connect in GM_NORMAL
static std::string _ap_starting_grants_applied_slot;        ///< Slot name for which starters/cash were already applied in this save/session.

/* Items received before we've entered GM_NORMAL are queued here */
static std::vector<APItem> _ap_pending_items;

static void AP_ResetProgressStateForNewSlot()
{
	_ap_completed_mission_locations.clear();
	_ap_purchased_shop_locations.clear();
	_ap_goal_sent = false;
	_ap_pending_items.clear();
	_ap_received_item_counts.clear();
	_ap_unlocked_tier_counts.clear();
	_ap_starting_grants_applied_slot.clear();
	AP_InitSessionStats();
}

/* Exposed for GUI polling */
std::atomic<bool> _ap_status_dirty{ false };

/* Public accessors */
const APSlotData                   &AP_GetSlotData()           { return _ap_pending_sd; }
const std::map<std::string, int>   &AP_GetReceivedItemCounts() { return _ap_received_item_counts; }
bool              AP_IsConnected()  { return _ap_client != nullptr &&
                                     _ap_client->GetState() == APState::AUTHENTICATED; }

static int AP_GetShopTierSize()
{
	const APSlotData &sd = AP_GetSlotData();
	const int total_slots = sd.enable_shop ? (int)sd.shop_locations.size() : 0;
	if (total_slots <= 0) return 0;
	const int shop_tiers = std::max(1, std::min(sd.shop_tiers, total_slots));
	return (total_slots + shop_tiers - 1) / shop_tiers;
}

int AP_GetVisibleShopLocationCount()
{
	const APSlotData &sd = AP_GetSlotData();
	if (!sd.enable_shop) return 0;
	const int total_slots = (int)sd.shop_locations.size();
	if (total_slots <= 0) return 0;
	const int upgrades = _ap_received_item_counts.count("Progressive Shop Upgrade")
		? _ap_received_item_counts.at("Progressive Shop Upgrade")
		: 0;
	return std::min(total_slots, AP_GetShopTierSize() * (1 + upgrades));
}

bool AP_IsShopLocationPurchased(const std::string &location_name)
{
	return _ap_purchased_shop_locations.count(location_name) != 0;
}

bool AP_PurchaseShopLocation(const std::string &location_name)
{
	if (!AP_GetSlotData().enable_shop) return false;
	const int visible_count = AP_GetVisibleShopLocationCount();
	const auto it = std::find_if(_ap_pending_sd.shop_locations.begin(), _ap_pending_sd.shop_locations.end(),
	    [&](const APShopLocation &shop) { return shop.location == location_name; });
	if (it == _ap_pending_sd.shop_locations.end()) return false;
	if ((int)std::distance(_ap_pending_sd.shop_locations.begin(), it) >= visible_count) return false;
	if (AP_IsShopLocationPurchased(location_name)) return false;

	Money available = GetAvailableMoney(_local_company);
	if (available < (Money)it->cost) {
		AP_ShowNews("Not enough money for " + it->name + ".", APPrintColour::RED);
		return false;
	}

	SubtractMoneyFromCompany(CommandCost(EXPENSES_OTHER, (Money)it->cost));
	_ap_purchased_shop_locations.insert(location_name);
	AP_SendCheckByName(location_name);
	AP_ShowNews(fmt::format("Purchased {} for {}.", it->name, it->cost), APPrintColour::GREEN);
	SetWindowClassesDirty(WC_ARCHIPELAGO);
	_ap_status_dirty.store(true);
	return true;
}

void AP_SetMenuConnectStartNew(bool start_new)
{
	_ap_menu_connect_start_new = start_new;
}

void AP_SetMenuConnectMultiplayer(bool multiplayer_mode)
{
	_ap_menu_connect_multiplayer = multiplayer_mode;
	_ap_open_network_window_pending = multiplayer_mode;
}

/* -------------------------------------------------------------------------
 * Callbacks
 * ---------------------------------------------------------------------- */

/* Forward declaration — defined later in this file */
static void AP_AssignNamedEntities();

static std::set<std::string> AP_ParseCompletedMissionLocations(const std::string &s)
{
	std::set<std::string> done;
	if (s.empty()) return done;

	std::string token;
	for (char c : s) {
		if (c == ',') {
			if (!token.empty()) done.insert(token);
			token.clear();
		} else {
			token += c;
		}
	}
	if (!token.empty()) done.insert(token);
	return done;
}

static void AP_ApplyCompletedMissionRestore()
{
	for (APMission &m : _ap_pending_sd.missions) {
		bool completed = (_ap_completed_mission_locations.count(m.location) != 0);
		if (!completed) {
			auto id_it = _ap_pending_sd.location_name_to_id.find(m.location);
			if (id_it != _ap_pending_sd.location_name_to_id.end()) {
				completed = (_ap_pending_sd.checked_locations.count(id_it->second) != 0);
			}
		}
		if (completed) {
			m.completed = true;
			_ap_completed_mission_locations.insert(m.location);
		}
	}
}

static void AP_ApplyPurchasedShopRestore()
{
	for (const APShopLocation &shop : _ap_pending_sd.shop_locations) {
		bool purchased = (_ap_purchased_shop_locations.count(shop.location) != 0);
		if (!purchased) {
			auto id_it = _ap_pending_sd.location_name_to_id.find(shop.location);
			if (id_it != _ap_pending_sd.location_name_to_id.end()) {
				purchased = (_ap_pending_sd.checked_locations.count(id_it->second) != 0);
			}
		}
		if (purchased) _ap_purchased_shop_locations.insert(shop.location);
	}
}

static void AP_OnSlotData(const APSlotData &sd)
{
	AP_OK("[CALLBACK] AP_OnSlotData called on main thread!");
	Debug(misc, 1, "[AP] Slot data received. {} missions, start_year={}",
	      sd.missions.size(), sd.start_year);

	const std::string incoming_slot_identity = AP_GetCurrentConnectionSlotIdentity();
	if (!_ap_progress_slot_identity.empty() &&
	    !incoming_slot_identity.empty() &&
	    _ap_progress_slot_identity != incoming_slot_identity) {
		AP_WARN(fmt::format("Slot changed ({} -> {}); clearing cached AP progress state.",
		    _ap_progress_slot_identity, incoming_slot_identity));
		AP_ResetProgressStateForNewSlot();
	}
	if (!incoming_slot_identity.empty()) {
		_ap_progress_slot_identity = incoming_slot_identity;
	}

	_ap_pending_sd      = sd;
	_ap_session_started = false; /* reset so first-tick setup runs again */
	_ap_goal_sent       = false;
	_ap_engine_map_built = false; /* rebuild map for new session */
	_ap_cargo_map_built  = false; /* rebuild cargo map for new session */
	_ap_unlocked_cargo_types.fill(false);
	AP_ApplyCompletedMissionRestore();
	AP_ApplyPurchasedShopRestore();
	_ap_status_dirty.store(true);

	/* Only auto-start world if we're on the main menu, start-new mode is selected,
	 * and we haven't already started a world for this connection session. */
	if (_game_mode == GM_MENU && _ap_menu_connect_start_new && !_ap_world_started_this_session) {
		_ap_client->ResetReceivedItemsIndex(); /* fresh game — treat all incoming items as new */
		_ap_world_started_this_session = true;
		_ap_pending_world_start        = true;
		AP_OK(fmt::format("Slot data ready — scheduling world generation (year={}, map={}x{})",
		      sd.start_year, (1 << sd.map_x), (1 << sd.map_y)));
	} else {
		AP_ERR(fmt::format("[OnSlotData] World start SKIPPED: game_mode={} start_new_mode={} world_started={}",
		      (int)_game_mode, _ap_menu_connect_start_new, _ap_world_started_this_session));

		ShowArchipelagoStatusWindow();
	}
}

static void AP_OnItemReceived(const APItem &item)
{
	Debug(misc, 1, "[AP] Item received: '{}' (id={}) replay={}", item.item_name, item.item_id, item.is_replay);

	/* Queue items that arrive before we've entered GM_NORMAL */
	if (!_ap_session_started) {
		_ap_pending_items.push_back(item);
		return;
	}

	if (item.item_name.empty()) {
		Debug(misc, 1, "[AP] WARNING: empty item name for id {}", item.item_id);
		return;
	}

	const bool is_replay = item.is_replay;

	/* Track every received item for the inventory window. */
	_ap_received_item_counts[item.item_name]++;
	_ap_status_dirty.store(true);

	/* Progressive vehicle unlock — delegate entirely to EnableEngineForCompany */
	if (AP_UnlockEngineByName(item.item_name)) {
		if (!is_replay) AP_ShowNews("Unlocked: " + item.item_name);
		return;
	}

	if (AP_UnlockAirportByName(item.item_name)) {
		if (!is_replay) AP_ShowNews("Airport tier unlocked: " + item.item_name);
		SetWindowClassesDirty(WC_ARCHIPELAGO);
		_ap_status_dirty.store(true);
		return;
	}

	/* -- Cargo type items ----------------------------------------- */
	static const std::set<std::string> CARGO_ITEM_NAMES = {
		"Passengers", "Mail", "Coal", "Oil", "Livestock", "Goods",
		"Grain", "Wood", "Iron Ore", "Steel", "Valuables"
	};
	if (CARGO_ITEM_NAMES.count(item.item_name)) {
		CargoType ct = AP_FindCargoType(item.item_name);
		if (IsValidCargoType(ct) && ct < NUM_CARGO) {
			_ap_unlocked_cargo_types[ct] = true;
			AP_ApplyCargoWagonUnlocks(_local_company);
		}
		if (!is_replay) AP_ShowNews("Cargo type unlocked: " + item.item_name);
		SetWindowClassesDirty(WC_ARCHIPELAGO);
		_ap_status_dirty.store(true);
		return;
	}

	if (item.item_name == "Progressive Shop Upgrade") {
		if (!is_replay && AP_GetSlotData().enable_shop) {
			AP_ShowNews(fmt::format("Shop expanded: {}/{} locations visible.", AP_GetVisibleShopLocationCount(), _ap_pending_sd.shop_locations.size()));
		}
		SetWindowClassesDirty(WC_ARCHIPELAGO);
		_ap_status_dirty.store(true);
		return;
	}

	/* -- Company colour items ------------------------------------- */
	if (AP_TryUnlockCompanyColour(item.item_name)) {
		if (!is_replay) AP_ShowNews("Company colour unlocked: " + item.item_name.substr(std::string("Company Colour: ").size()));
		return;
	}

	/* -- Filler items --------------------------------------------- */
	if (item.item_name == "Cash Injection") {
		if (!is_replay) {
			CompanyID fid = _local_company;
			Company *fc = Company::GetIfValid(fid);
			if (fc != nullptr) {
				fc->money += (Money)50000LL;
				AP_RefreshMoneyDisplays();
			}
			AP_ShowNews("Bonus: +" + AP_FormatLocalCurrency(50000) + "!");
		}
		return;
	}
	if (item.item_name == "Choo chooo!") {
		if (!is_replay) AP_ShowNews("Choo chooo!");
		return;
	}

	/* Unknown item */
	AP_WARN("Unknown item: '" + item.item_name + "' — not handled");
}

static void AP_OnConnected()
{
	Debug(misc, 1, "[AP] Connected to Archipelago server.");
	AP_OK("Connected to Archipelago server");
	/* Force English immediately on connect so any subsequent string lookups
	 * (including engine name map building) use English names. */
	ForceEnglishLanguage();
	_ap_status_dirty.store(true);
}

static void AP_OnDisconnected(const std::string &reason)
{
	Debug(misc, 1, "[AP] Disconnected: {}", reason);
	AP_WARN("Disconnected: " + reason);
	_ap_world_started_this_session = false;
	_ap_pending_world_start = false;
	_ap_session_started = false;
	_ap_unlocked_cargo_types.fill(false);
	_ap_unlocked_company_colours.clear();
	if (_game_mode == GM_MENU) _ap_completed_mission_locations.clear();
	AP_LOG("Session flags reset — next connect can start world");
	_ap_status_dirty.store(true);
}

static void AP_OnPrint(const std::string &msg, APPrintColour colour)
{
	Debug(misc, 0, "[AP] Server: {}", msg);

	AP_ShowNews(msg, colour);
}

/* -------------------------------------------------------------------------
 * Locations Updated — fired when RoomUpdate brings new checked locations
 * from another client for the same slot (e.g. a slow-release client).
 * ---------------------------------------------------------------------- */

static void AP_OnLocationsUpdated()
{
	if (_ap_client != nullptr) {
		APSlotData current_sd = _ap_client->GetSlotData();
		_ap_pending_sd.checked_locations = current_sd.checked_locations;
		AP_ApplyCompletedMissionRestore();
		AP_ApplyPurchasedShopRestore();
	}
	SetWindowClassesDirty(WC_ARCHIPELAGO);
	_ap_status_dirty.store(true);
}


/* -------------------------------------------------------------------------
 * Handler registration
 * ---------------------------------------------------------------------- */

static bool _handlers_registered = false;

void EnsureHandlersRegistered()
{
	if (_handlers_registered || _ap_client == nullptr) return;
	_handlers_registered = true;

	_ap_client->callbacks.on_connected     = AP_OnConnected;
	_ap_client->callbacks.on_disconnected  = AP_OnDisconnected;
	_ap_client->callbacks.on_print         = AP_OnPrint;
	_ap_client->callbacks.on_slot_data     = AP_OnSlotData;
	_ap_client->callbacks.on_item_received = AP_OnItemReceived;
	_ap_client->callbacks.on_locations_updated = AP_OnLocationsUpdated;
}

/* -------------------------------------------------------------------------
 * Win-condition check
 * ---------------------------------------------------------------------- */

static bool CheckWinCondition()
{
	/* Current apworld win condition: collect all 11 cargo type items. */
	static constexpr const char *CARGO_WIN_ITEMS[] = {
		"Passengers", "Mail", "Coal", "Oil", "Livestock", "Goods",
		"Grain", "Wood", "Iron Ore", "Steel", "Valuables"
	};
	for (const char *cargo : CARGO_WIN_ITEMS) {
		if (!_ap_received_item_counts.count(cargo)) return false;
	}
	return true;
}

/* -------------------------------------------------------------------------
 * Public API — called by intro_gui.cpp to safely start the world
 * ---------------------------------------------------------------------- */

static uint32_t _ap_world_seed_to_use = 0;

bool AP_ShouldStartWorld()
{
	return _ap_pending_world_start && _game_mode == GM_MENU;
}

void AP_ConsumeWorldStart()
{
	if (!_ap_pending_world_start) return;
	_ap_pending_world_start = false;

	const APSlotData &sd = _ap_pending_sd;

	/* ── World generation ─────────────────────────────────────────── */
	_settings_newgame.game_creation.starting_year =
	    TimerGameCalendar::Year(sd.start_year);
	if (sd.map_x >= 6 && sd.map_x <= 12)
		_settings_newgame.game_creation.map_x = sd.map_x;
	if (sd.map_y >= 6 && sd.map_y <= 12)
		_settings_newgame.game_creation.map_y = sd.map_y;
	if (sd.landscape <= 3)
		_settings_newgame.game_creation.landscape = (LandscapeType)sd.landscape;
	if (sd.land_generator <= 1)
		_settings_newgame.game_creation.land_generator = sd.land_generator;
	if (sd.terrain_type <= 5)
		_settings_newgame.difficulty.terrain_type = sd.terrain_type;
	if (sd.quantity_sea_lakes <= 4)
		_settings_newgame.difficulty.quantity_sea_lakes = sd.quantity_sea_lakes;
	if (sd.variety <= 5)
		_settings_newgame.game_creation.variety = sd.variety;
	if (sd.tgen_smoothness <= 3)
		_settings_newgame.game_creation.tgen_smoothness = sd.tgen_smoothness;
	if (sd.amount_of_rivers <= 3)
		_settings_newgame.game_creation.amount_of_rivers = sd.amount_of_rivers;
	if (sd.town_name <= 255)
		_settings_newgame.game_creation.town_name = sd.town_name;
	if (sd.number_towns <= 4)
		_settings_newgame.difficulty.number_towns = sd.number_towns;

	if (sd.water_border_presets <= 2) {
		switch (sd.water_border_presets) {
			case 0: // Random
				_settings_newgame.game_creation.water_borders = BorderFlag::Random;
				_settings_newgame.construction.freeform_edges = true;
				break;
			case 1: // Manual
				_settings_newgame.game_creation.water_borders = {};
				_settings_newgame.construction.freeform_edges = true;
				break;
			case 2: // Infinite water
				_settings_newgame.game_creation.water_borders = BORDERFLAGS_ALL;
				_settings_newgame.construction.freeform_edges = false;
				break;
		}
		_settings_newgame.game_creation.water_border_presets = static_cast<BorderFlagPresets>(sd.water_border_presets);
	}

	_ap_world_seed_to_use = (sd.world_seed != 0) ? sd.world_seed : GENERATE_NEW_SEED;

	_settings_newgame.difficulty.industry_density      = sd.industry_density;

	Debug(misc, 0, "[AP] World start ready: seed={}, year={}, map={}x{}, landscape={} terrain={} sea={} towns={}",
	      _ap_world_seed_to_use, sd.start_year,
	      (1 << sd.map_x), (1 << sd.map_y), (int)sd.landscape,
	      (int)sd.terrain_type, (int)sd.quantity_sea_lakes, (int)sd.number_towns);
	Debug(misc, 0, "[AP] World settings: industry_density={}", (int)sd.industry_density);
	AP_OK(fmt::format("Generating world: seed={} year={} size={}x{} landscape={}",
	      _ap_world_seed_to_use, sd.start_year,
	      (1 << sd.map_x), (1 << sd.map_y), (int)sd.landscape));
}

uint32_t AP_GetWorldSeed()
{
	return _ap_world_seed_to_use;
}

/* -------------------------------------------------------------------------
 * Item and location check API
 * ---------------------------------------------------------------------- */

void AP_ShowConsole(const std::string &msg)
{
	IConsolePrint(CC_INFO, msg);
}

void AP_SendCheckByName(const std::string &location_name)
{
	if (_ap_client == nullptr) return;
	_ap_client->SendCheckByName(location_name);
}



/* -------------------------------------------------------------------------
 * REAL-TIME TIMER — 250ms
 *
 * Responsibilities:
 *   1. Pump the AP network client (Tick)
 *   2. First-tick session setup when we enter GM_NORMAL:
 *        - Build engine name map (English, with never_expire set)
 *        - Lock all engines and wagons; unlock via AP progression
 *        - Apply starting cash bonus and starting cargo type
 *        - Populate hardcoded missions if APWorld sent none
 *        - Flush any items that arrived before GM_NORMAL
 *        - Open the AP status window
 *   3. Win-condition polling every ~10 s
 *
 * NOTE: Engine locking on session start is done inside the first-tick
 * setup block (step 2 above), not via the vanilla date-based introduction
 * loop.  engine.cpp skips NewVehicleAvailable() when AP_IsActive() is true,
 * so no engine becomes available through the normal date path.  Instead the
 * session setup locks engines/wagons directly and AP items unlock them on
 * demand via EnableEngineForCompany() (wagons are gated by cargo unlocks).
 * ---------------------------------------------------------------------- */

static uint32_t _ap_realtime_ticks = 0;
static constexpr uint32_t AP_MISSION_CHECK_TICKS   = 4;   /* 1 second  (4 × 250 ms) */
static constexpr uint32_t AP_STATS_AND_LOCK_TICKS  = 40;  /* 10 seconds (40 × 250 ms) */
static constexpr uint32_t AP_WIN_CHECK_TICKS       = 40;  /* 10 seconds (40 × 250 ms) */

bool AP_GetGoalSent()        { return _ap_goal_sent; }
void AP_SetGoalSent(bool v)  { _ap_goal_sent = v; }

std::string AP_GetStartingGrantsAppliedSlot() { return _ap_starting_grants_applied_slot; }
void AP_SetStartingGrantsAppliedSlot(const std::string &slot) { _ap_starting_grants_applied_slot = slot; }

std::string AP_GetProgressSlotIdentity() { return _ap_progress_slot_identity; }
void AP_SetProgressSlotIdentity(const std::string &slot_identity) { _ap_progress_slot_identity = slot_identity; }



std::string AP_GetCompletedMissionsStr()
{
    std::string out;
    for (const APMission &m : _ap_pending_sd.missions) {
        if (!m.completed) continue;
        if (!out.empty()) out += ',';
        out += m.location;
    }
    return out;
}

std::string AP_GetPurchasedShopLocationsStr()
{
	std::string out;
	for (const std::string &location : _ap_purchased_shop_locations) {
		if (!out.empty()) out += ',';
		out += location;
	}
	return out;
}

void AP_SetCompletedMissionsStr(const std::string &s)
{
	_ap_completed_mission_locations = AP_ParseCompletedMissionLocations(s);
	AP_ApplyCompletedMissionRestore();
}

void AP_SetPurchasedShopLocationsStr(const std::string &s)
{
	_ap_purchased_shop_locations.clear();
	if (s.empty()) {
		AP_ApplyPurchasedShopRestore();
		_ap_status_dirty.store(true);
		return;
	}

	std::string token;
	for (char c : s) {
		if (c == ',') {
			if (!token.empty()) _ap_purchased_shop_locations.insert(token);
			token.clear();
		} else {
			token += c;
		}
	}
	if (!token.empty()) _ap_purchased_shop_locations.insert(token);
	AP_ApplyPurchasedShopRestore();
	_ap_status_dirty.store(true);
}

void AP_GetCumulStats(uint64_t *cargo_out, int num_cargo, int64_t *profit_out)
{
    for (int i = 0; i < num_cargo && i < (int)NUM_CARGO; i++)
        cargo_out[i] = _ap_cumul_cargo[i];
    *profit_out = (int64_t)_ap_cumul_profit;
}

void AP_SetCumulStats(const uint64_t *cargo_in, int num_cargo, int64_t profit_in)
{
    for (int i = 0; i < num_cargo && i < (int)NUM_CARGO; i++)
        _ap_cumul_cargo[i] = cargo_in[i];
    _ap_cumul_profit = (Money)profit_in;
    _ap_stats_initialized = true;
}

/* Returns "location=N:P,..." for all maintain missions.
 * N = consecutive months OK, P = 1 if first-month guard is pending, else 0. */
std::string AP_GetMaintainCountersStr()
{
    std::string out;
    for (const APMission &m : _ap_pending_sd.missions) {
        if (m.type.find("maintain") == std::string::npos) continue;
        if (m.maintain_months_ok == 0 && !m.maintain_first_month_pending) continue;
        if (!out.empty()) out += ',';
        out += m.location + '=' + fmt::format("{}:{}", m.maintain_months_ok,
               m.maintain_first_month_pending ? 1 : 0);
    }
    return out;
}

void AP_SetMaintainCountersStr(const std::string &s)
{
    if (s.empty()) return;
    /* Parse "loc=N:P,..." — P is optional for backwards compat with old saves */
    std::string token;
    auto apply = [&](const std::string &t) {
        auto eq = t.find('=');
        if (eq == std::string::npos) return;
        std::string loc = t.substr(0, eq);
        std::string val = t.substr(eq + 1);
        int n = 0;
        bool pending = false;
        auto colon = val.find(':');
        auto parse_int = [](const std::string &str) {
            int v = 0;
            for (char c : str) if (c >= '0' && c <= '9') v = v * 10 + (c - '0');
            return v;
        };
        if (colon != std::string::npos) {
            n       = parse_int(val.substr(0, colon));
            pending = (parse_int(val.substr(colon + 1)) != 0);
        } else {
            n = parse_int(val);
        }
        for (APMission &m : _ap_pending_sd.missions) {
            if (m.location == loc) {
                m.maintain_months_ok           = n;
                m.maintain_first_month_pending = pending;
                break;
            }
        }
    };
    for (char c : s) {
        if (c == ',') { apply(token); token.clear(); }
        else token += c;
    }
    apply(token);
}
std::string AP_GetNamedEntityStr()
{
	std::string out;
	for (const APMission &m : _ap_pending_sd.missions) {
		if (m.named_entity.id < 0) continue;
		if (!out.empty()) out += ';';
		out += m.location + ':' +
		       fmt::format("{}", m.named_entity.id) + ':' +
		       fmt::format("{}", m.named_entity.cumulative);
	}
	return out;
}

/** Restore named entity assignments from save/load string.
 *  Format: "location:entity_id:cumulative;..." (semicolon-separated)
 *  Uses std::from_chars — no sscanf/strchr (both forbidden by safeguards.h). */
/* Forward declarations for helpers defined later in this file */
static std::string AP_TownName(const Town *t);
static std::string AP_IndustryLabel(const Industry *ind);
static void AP_StrReplace(std::string &s, const std::string &from, const std::string &to);

void AP_SetNamedEntityStr(const std::string &s)
{
	if (s.empty()) return;

	std::string_view sv(s);
	while (!sv.empty()) {
		/* Find the ';' that ends this entry (or end-of-string) */
		auto semi = sv.find(';');
		std::string_view entry = sv.substr(0, semi);
		if (semi == std::string_view::npos) sv = {}; else sv = sv.substr(semi + 1);

		/* Split entry into "loc : eid : cum" */
		auto c1 = entry.find(':');
		if (c1 == std::string_view::npos) continue;
		auto c2 = entry.find(':', c1 + 1);
		if (c2 == std::string_view::npos) continue;

		std::string_view loc_sv  = entry.substr(0, c1);
		std::string_view eid_sv  = entry.substr(c1 + 1, c2 - c1 - 1);
		std::string_view cum_sv  = entry.substr(c2 + 1);

		int32_t  eid = -1;
		uint64_t cum = 0;
		std::from_chars(eid_sv.data(), eid_sv.data() + eid_sv.size(), eid);
		std::from_chars(cum_sv.data(), cum_sv.data() + cum_sv.size(), cum);

		std::string loc_str(loc_sv);
		for (APMission &m : _ap_pending_sd.missions) {
			if (m.location != loc_str) continue;
			m.named_entity.id         = eid;
			m.named_entity.cumulative = cum;

			/* Name/tile/cargo resolution requires live map pointers (ind->town etc.)
			 * which are NOT valid during chunk Load — AfterLoadGame() resolves them later.
			 * We set a flag here and defer the GetString calls to the first game tick. */
			_ap_named_entity_refresh_needed = true;
			break;
		}
	}
}
/* -------------------------------------------------------------------------
 * Named-destination missions: assign map entities and accumulate progress.
 * Placed after all AP state variables so _ap_pending_sd is accessible.
 * ---------------------------------------------------------------------- */

/** Get town name using OTTDv15 variadic GetString API. */
static std::string AP_TownName(const Town *t)
{
	if (t == nullptr) return "Unknown";
	return GetString(STR_TOWN_NAME, t->index);
}

/** Get "IndustryType near TownName" label. */
static std::string AP_IndustryLabel(const Industry *ind)
{
	if (ind == nullptr) return "Unknown Industry";
	std::string ind_name  = GetString(STR_INDUSTRY_NAME, ind->index);
	std::string town_name = GetString(STR_TOWN_NAME,     ind->town->index);
	return ind_name + " near " + town_name;
}

/**
 * Re-resolve name/tile/cargo for all named-entity missions that have a valid
 * eid but an empty name.  Called on first game tick after a savegame load,
 * when AfterLoadGame() has fully resolved all pool pointers (ind->town etc.).
 */
static void AP_RefreshNamedEntityNames()
{
	for (APMission &m : _ap_pending_sd.missions) {
		int32_t eid = m.named_entity.id;
		if (eid < 0) continue; /* not yet assigned */
		if (!m.named_entity.name.empty()) continue; /* already resolved */

		if (m.type == "passengers_to_town" || m.type == "mail_to_town") {
			const Town *t = Town::GetIfValid((TownID)eid);
			if (t != nullptr) {
				m.named_entity.name       = AP_TownName(t);
				m.named_entity.tile       = t->xy.base();
				m.named_entity.tae        = (m.type == "passengers_to_town") ? TAE_PASSENGERS : TAE_MAIL;
				m.named_entity.cargo_type = (uint8_t)((m.type == "passengers_to_town")
				    ? AP_FindCargoType("passengers")
				    : AP_FindCargoType("mail"));
				AP_StrReplace(m.description, "[Town]", m.named_entity.name);
			}
		} else if (m.type == "cargo_from_industry") {
			const Industry *ind = Industry::GetIfValid((IndustryID)eid);
			if (ind != nullptr && ind->town != nullptr) {
				m.named_entity.name       = AP_IndustryLabel(ind);
				m.named_entity.tile       = ind->location.tile.base();
				uint8_t first_ct = 0xFF;
				for (const auto &slot : ind->produced) {
					if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; }
				}
				m.named_entity.cargo_type = first_ct;
				AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);
			}
		} else if (m.type == "cargo_to_industry") {
			const Industry *ind = Industry::GetIfValid((IndustryID)eid);
			if (ind != nullptr && ind->town != nullptr) {
				m.named_entity.name       = AP_IndustryLabel(ind);
				m.named_entity.tile       = ind->location.tile.base();
				uint8_t first_ct = 0xFF;
				for (const auto &slot : ind->accepted) {
					if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; }
				}
				m.named_entity.cargo_type = first_ct;
				AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);
			}
		}
	}
	_ap_named_entity_refresh_needed = false;
}

/** Replace first occurrence of 'from' in 's' with 'to'. */
static void AP_StrReplace(std::string &s, const std::string &from, const std::string &to)
{
	size_t pos = s.find(from);
	if (pos != std::string::npos) s.replace(pos, from.size(), to);
}

/**
 * At session start: assign real map towns/industries to named-destination
 * missions (type "passengers_to_town", "mail_to_town", "cargo_to_industry",
 * "cargo_from_industry").  Assignments are seed-deterministic.
 */
static void AP_AssignNamedEntities()
{
	/* Collect candidates */
	std::vector<const Town     *> towns;
	std::vector<const Industry *> prod_inds;
	std::vector<const Industry *> acc_inds;
	for (const Town     *t   : Town::Iterate())     towns.push_back(t);
	for (const Industry *ind : Industry::Iterate()) {
		if (!ind->produced.empty()) prod_inds.push_back(ind);
		if (!ind->accepted.empty()) acc_inds.push_back(ind);
	}
	if (towns.empty()) return;

	/* XOR-shift RNG seeded from world seed */
	/* Use the actual map generation seed for deterministic town/industry
	 * assignment.  _ap_pending_sd.world_seed is 0 (Python sends 0 and lets
	 * OpenTTD pick its own seed), so we fall back to the real game seed. */
	const uint32_t map_seed = (_ap_pending_sd.world_seed != 0)
		? _ap_pending_sd.world_seed
		: _settings_game.game_creation.generation_seed;
	uint32_t rng = map_seed ^ 0xDEADBEEFu;
	auto next_rng = [&]() -> uint32_t {
		rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; return rng;
	};
	auto shuffle = [&](auto &v) {
		for (size_t i = v.size(); i > 1; --i) std::swap(v[i-1], v[next_rng() % i]);
	};
	shuffle(towns); shuffle(prod_inds); shuffle(acc_inds);

	std::set<int32_t> used_towns, used_inds;
	size_t ti = 0, pi = 0, ai = 0;

	for (APMission &m : _ap_pending_sd.missions) {
		if (m.named_entity.id >= 0 && !m.named_entity.name.empty()) continue; /* already fully resolved */

		if (m.type == "passengers_to_town" || m.type == "mail_to_town") {
			while (ti < towns.size() && used_towns.count((int32_t)towns[ti]->index.base())) ti++;
			if (ti >= towns.size()) ti = 0;
			const Town *t           = towns[ti++];
			m.named_entity.id       = (int32_t)t->index.base();
			m.named_entity.name     = AP_TownName(t);
			m.named_entity.tile     = t->xy.base();
			m.named_entity.tae      = (m.type == "passengers_to_town") ? TAE_PASSENGERS : TAE_MAIL;
			m.named_entity.cargo_type = (uint8_t)((m.type == "passengers_to_town")
			    ? AP_FindCargoType("passengers")
			    : AP_FindCargoType("mail"));
			used_towns.insert(m.named_entity.id);
			AP_StrReplace(m.description, "[Town]", m.named_entity.name);

		} else if (m.type == "cargo_from_industry") {
			while (pi < prod_inds.size() && used_inds.count((int32_t)prod_inds[pi]->index.base())) pi++;
			if (pi >= prod_inds.size()) pi = 0;
			const Industry *ind        = prod_inds[pi++];
			m.named_entity.id          = (int32_t)ind->index.base();
			m.named_entity.name        = AP_IndustryLabel(ind);
			m.named_entity.tile        = ind->location.tile.base();
			m.named_entity.cargo_slot  = 0;
			/* First VALID produced slot — slot[0] may be INVALID_CARGO */
			{ uint8_t first_ct = 0xFF;
			  for (const auto &slot : ind->produced) { if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; } }
			  m.named_entity.cargo_type = first_ct; }
			used_inds.insert(m.named_entity.id);
			AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);

		} else if (m.type == "cargo_to_industry") {
			while (ai < acc_inds.size() && used_inds.count((int32_t)acc_inds[ai]->index.base())) ai++;
			if (ai >= acc_inds.size()) ai = 0;
			const Industry *ind        = acc_inds[ai++];
			m.named_entity.id          = (int32_t)ind->index.base();
			m.named_entity.name        = AP_IndustryLabel(ind);
			m.named_entity.tile        = ind->location.tile.base();
			m.named_entity.cargo_slot  = 0;
			/* First VALID accepted slot — slot[0] may be INVALID_CARGO */
			{ uint8_t first_ct = 0xFF;
			  for (const auto &slot : ind->accepted) { if (IsValidCargoType(slot.cargo)) { first_ct = (uint8_t)slot.cargo; break; } }
			  m.named_entity.cargo_type = first_ct; }
			used_inds.insert(m.named_entity.id);
			AP_StrReplace(m.description, "[Industry near Town]", m.named_entity.name);
		}
	}
}

/**
 * Called monthly: accumulate named-entity progress and protect industries
 * from random closure while their mission is active.
 */
static void AP_UpdateNamedMissions()
{
	CompanyID cid = _local_company;
	if (cid >= MAX_COMPANIES) return;

	for (APMission &m : _ap_pending_sd.missions) {
		if (m.completed)           continue;
		if (m.named_entity.id < 0) continue;
		if (m.named_entity.cargo_type == 0xFF) continue;

		CargoType ct = (CargoType)m.named_entity.cargo_type;

		if (m.type == "passengers_to_town" || m.type == "mail_to_town") {
			/* Use cargomonitor: company-specific deliveries to this town only */
			TownID tid = (TownID)m.named_entity.id;
			if (!Town::IsValidID(tid)) { m.named_entity.cumulative = (uint64_t)m.amount; continue; }
			CargoMonitorID monitor = EncodeCargoTownMonitor(cid, ct, tid);
			int32_t delivered = GetDeliveryAmount(monitor, true); /* true = keep monitoring */
			if (delivered > 0) m.named_entity.cumulative += (uint64_t)delivered;

		} else if (m.type == "cargo_from_industry") {
			/* Use cargomonitor: company-specific pickups from this industry.
			 * Sum ALL produced cargo slots — some industries produce multiple
			 * cargo types (e.g. Oil Refinery produces both Goods and Oil). */
			IndustryID iid = (IndustryID)m.named_entity.id;
			Industry *ind = Industry::GetIfValid(iid);
			if (ind == nullptr) { m.named_entity.cumulative = (uint64_t)m.amount; continue; }
			if (ind->prod_level < PRODLEVEL_DEFAULT) ind->prod_level = PRODLEVEL_DEFAULT;
			for (const auto &slot : ind->produced) {
				if (!IsValidCargoType(slot.cargo)) continue;
				CargoMonitorID monitor = EncodeCargoIndustryMonitor(cid, slot.cargo, iid);
				int32_t picked_up = GetPickupAmount(monitor, true);
				if (picked_up > 0) m.named_entity.cumulative += (uint64_t)picked_up;
			}

		} else if (m.type == "cargo_to_industry") {
			/* Use cargomonitor: company-specific deliveries to this industry.
			 * Sum ALL accepted cargo slots — industries like Cadton Factory
			 * accept Livestock + Grain + Steel; only counting slot 0 (Livestock)
			 * meant Steel and Grain deliveries never registered. */
			IndustryID iid = (IndustryID)m.named_entity.id;
			Industry *ind = Industry::GetIfValid(iid);
			if (ind == nullptr) { m.named_entity.cumulative = (uint64_t)m.amount; continue; }
			if (ind->prod_level < PRODLEVEL_DEFAULT) ind->prod_level = PRODLEVEL_DEFAULT;
			for (const auto &slot : ind->accepted) {
				if (!IsValidCargoType(slot.cargo)) continue;
				CargoMonitorID monitor = EncodeCargoIndustryMonitor(cid, slot.cargo, iid);
				int32_t delivered = GetDeliveryAmount(monitor, true);
				if (delivered > 0) m.named_entity.cumulative += (uint64_t)delivered;
			}
		}
	}
}


/* -------------------------------------------------------------------------
 * Monthly timer: advance "maintain rating" mission counters.
 * For each incomplete maintain-mission, check if ALL rated stations owned by
 * the player currently meet the threshold. If yes, increment the counter;
 * if any station falls below, reset it to zero.
 * ---------------------------------------------------------------------- */
static const IntervalTimer<TimerGameCalendar> _ap_calendar_maintain_check(
	{ TimerGameCalendar::MONTH, TimerGameCalendar::Priority::NONE },
	[](auto) {
		if (!_ap_session_started) return;
	}
);

/* Legacy item rotation removed.
 * No periodic item-list refresh timer is needed anymore. */

/* -------------------------------------------------------------------------
 * Mission evaluation — calls EvaluateMission() for all incomplete missions
 * and fires AP_SendCheckByName when a mission is satisfied.
 * Placed here (after AP_ShowNews and all state variables) to avoid
 * forward-declaration issues.
 * ---------------------------------------------------------------------- */

/**
 * Iterate all incomplete missions and send checks for any that are now met.
 * Called from the realtime timer every ~1 s.
 */
static void CheckMissions()
{
	if (_ap_client == nullptr) return;
	if (!_ap_session_started) return;

	int completed_this_pass = 0;
	for (APMission &m : _ap_pending_sd.missions) {
		if (m.completed) continue;
		if (EvaluateMission(m)) {
			m.completed = true;
			completed_this_pass++;
			AP_SendCheckByName(m.location);
			AP_ShowNews("Mission complete: " + m.description);
			Debug(misc, 0, "[AP] Mission completed: {} ({})", m.location, m.description);
		}
	}

	if (completed_this_pass > 0) {
		SetWindowClassesDirty(WC_ARCHIPELAGO);
		_ap_status_dirty.store(true);
	}
}



void AP_OnLeaveGame()
{
	if (_ap_client == nullptr) return;
	if (_ap_client->GetState() == APState::DISCONNECTED) return;
	AP_OK("Left game; disconnecting from Archipelago.");
	_ap_client->Disconnect();
	_ap_session_started = false;
	_ap_world_started_this_session = false;
	_ap_starting_grants_applied_slot.clear();
	_ap_pending_world_start = false;
	_ap_pending_items.clear();
	_ap_host_autoconnect_attempted = false;
	_ap_open_network_window_pending = false;
	_ap_status_dirty.store(true);
}

static IntervalTimer<TimerGameRealtime> _ap_realtime_timer(
	{ std::chrono::milliseconds(250), TimerGameRealtime::ALWAYS },
	[](auto) {
		if (_ap_client == nullptr) return;
		EnsureHandlersRegistered();

		/* Dispatch inbound AP events */
		_ap_client->Tick();

		/* Better MP flow: in host/server mode, auto-connect AP at game start when
		 * saved credentials exist. Network clients may still connect manually from
		 * the AP window for their own slot/company. */
		if (_networking && _network_server && _game_mode == GM_NORMAL &&
		    !_ap_session_started && !_ap_host_autoconnect_attempted &&
		    _ap_client->GetState() == APState::DISCONNECTED &&
		    !_ap_last_host.empty() && !_ap_last_slot.empty()) {
			_ap_host_autoconnect_attempted = true;
			AP_OK("Multiplayer host detected; auto-connecting Archipelago for this server session.");
			_ap_client->Connect(_ap_last_host, _ap_last_port, _ap_last_slot, _ap_last_pass, "OpenTTD Cargolock", _ap_last_ssl);
		}

		/* First-tick session setup when we enter GM_NORMAL */
		if (!_ap_session_started &&
		    _game_mode == GM_NORMAL &&
		    _local_company < MAX_COMPANIES &&
		    _ap_client->GetState() == APState::AUTHENTICATED) {

			const bool apply_starting_grants = _ap_starting_grants_applied_slot != _ap_last_slot;

			_ap_session_started = true;
			_ap_received_item_counts.clear();  /* reset for fresh item tracking this session */
			_ap_unlocked_tier_counts.clear();  /* reset tier progress — items will replay and re-unlock */
			_ap_unlocked_cargo_types.fill(false);
			_ap_unlocked_company_colours.clear();

			CompanyID cid = _local_company;
			Company *c = Company::GetIfValid(cid);

			/* Force English so that engine names match AP item names */
			ForceEnglishLanguage();

			/* Disable vehicle/airport expiry NOW — before building the engine
			 * map.  EngineNameContext::PurchaseList only returns a name for
			 * engines that are currently available; without this flag some
			 * engines (especially early steam locos that have expired by 1950)
			 * return an empty string and never make it into _ap_engine_map,
			 * causing "Unknown item" warnings when AP tries to unlock them. */
			_settings_game.vehicle.never_expire_vehicles = true;
			_settings_game.station.never_expire_airports = true;

			/* Build the engine name → ID lookup map (uses current language = English) */
			BuildEngineMap();

			/* Build the cargo name → type map for mission evaluation */
			BuildCargoMap();

			/* Reset session statistics for mission evaluation */
			AP_InitSessionStats();

			if (_ap_pending_sd.missions.empty()) {
				AP_WARN("No missions in slot_data; mission checks will remain disabled.");
			}

			/* AP settings: vehicle/airport expiry already disabled above (before
			 * BuildEngineMap).  No additional setting needed here. */

			/* Strip the local company from every ENGINE that was auto-unlocked
			 * by StartupOneEngine() at game start.
			 *
			 * Non-wagon engines unlock via AP vehicle items.
			 * Wagon engines unlock via AP cargo items (matching default cargo).
			 */
			_ap_unlocked_engine_ids.clear();

			int locked_count = 0;
			for (Engine *e : Engine::Iterate()) {
				if (!e->company_avail.Test(cid)) { continue; }
				e->company_avail.Reset(cid);
				locked_count++;
			}
			Debug(misc, 1, "[AP] Session lock complete: {} engines locked", locked_count);

			/* Unlock ALL rail and road types unconditionally — ensures any
			 * AP-randomised engine's track/road type is always buildable. */
			if (c != nullptr) {
				c->avail_railtypes = GetRailTypes(true);
				c->avail_roadtypes = GetRoadTypes(true);
			}

			AP_LockAllAirports();

			InvalidateWindowClassesData(WC_BUILD_VEHICLE);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_RAIL);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_ROAD);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_WATER);
			InvalidateWindowData(WC_BUILD_TOOLBAR, TRANSPORT_AIR);
			AP_OK(fmt::format("AP session started: {} engines locked, all railtypes/roadtypes unlocked, airports gated by AP progression.", locked_count));

			if (apply_starting_grants) {
				/* Unlock all starting vehicles.
				 * The APWorld is responsible for ensuring only climate-compatible
				 * vehicles appear in starting_vehicles — no fallback substitution. */
				for (const std::string &sv : _ap_pending_sd.starting_vehicles) {
					if (sv.empty()) continue;
					if (!_ap_engine_map_built) BuildEngineMap();
					if (AP_UnlockEngineByName(sv)) {
						AP_OK("Starting vehicle unlocked: " + sv);
						AP_ShowNews("Starting vehicle: " + sv);
					} else {
						/* Engine not found — try rebuilding the map once (covers edge
						 * cases where the map was built before all NewGRFs finished
						 * loading). */
						AP_WARN("Starting vehicle '" + sv + "' not found — rebuilding engine map and retrying");
						_ap_engine_map_built = false;
						BuildEngineMap();
						if (AP_UnlockEngineByName(sv)) {
							AP_OK("Starting vehicle unlocked after map rebuild: " + sv);
							AP_ShowNews("Starting vehicle: " + sv);
						} else {
							AP_ERR("Starting vehicle '" + sv + "' not found in engine map!");
						}
					}
				}
			}

			/* Apply starting cash bonus if configured */
			if (_ap_pending_sd.starting_cash_bonus && c != nullptr && apply_starting_grants) {
				const Money bonus_amount = c->current_loan;
				if (bonus_amount > 0) {
					c->money += bonus_amount;
					AP_RefreshMoneyDisplays();
					AP_ShowNews("Starting bonus: +" + AP_FormatLocalCurrency(bonus_amount) + " (one loan).");
					AP_OK("Starting cash bonus applied: +" + AP_FormatLocalCurrency(bonus_amount) + " (one loan)");
				}
			}

			/* Apply starting cargo type unlock if configured (0=any, 1-9=specific cargo).
			 * Indices must exactly match the Python StartingCargoType enum in options.py.
			 * Note: Goods and Steel are excluded (secondary cargos) — valuables is index 9. */
			if (_ap_pending_sd.starting_cargo_type > 0 && _ap_pending_sd.starting_cargo_type <= 9 && apply_starting_grants) {
				static constexpr const char *CARGO_NAMES[] = {
					"", "Passengers", "Mail", "Coal", "Oil", "Livestock",
					"Grain", "Wood", "Iron Ore", "Valuables"
				};
				CargoType ct = AP_FindCargoType(CARGO_NAMES[_ap_pending_sd.starting_cargo_type]);
				if (IsValidCargoType(ct)) {
					_ap_unlocked_cargo_types[ct] = true;
					AP_ApplyCargoWagonUnlocks(cid);
					AP_ShowNews("Starting cargo unlocked: " + std::string(CARGO_NAMES[_ap_pending_sd.starting_cargo_type]));
					AP_OK("Starting cargo type unlocked: " + std::string(CARGO_NAMES[_ap_pending_sd.starting_cargo_type]));
					SetWindowClassesDirty(WC_ARCHIPELAGO);
					_ap_status_dirty.store(true);
				}
			}

			/* Populate hardcoded missions if none are provided by APWorld.
			 * Covers all 11 AP cargo item names (matches CARGO_ITEM_NAMES in AP_OnItemReceived). */
			if (_ap_pending_sd.missions.empty()) {
				static constexpr const char *CARGO_NAMES_HC[] = {
					"Passengers", "Mail", "Coal", "Oil", "Livestock", "Goods",
					"Grain", "Wood", "Iron Ore", "Steel", "Valuables"
				};
				for (int i = 0; i < 11; i++) {
					APMission m;
					m.location = std::string("Collect: ") + CARGO_NAMES_HC[i];
					m.description = std::string("Unlock the ") + CARGO_NAMES_HC[i] + " cargo type";
					m.type = "collect_cargo";
					m.cargo = CARGO_NAMES_HC[i];
					m.unit = "";
					m.amount = 1;
					m.difficulty = "normal";
					m.completed = false;
					m.current_value = 0;
					_ap_pending_sd.missions.push_back(m);
				}
				AP_OK(fmt::format("No missions in slot_data; populated {} hardcoded cargo-collection missions", _ap_pending_sd.missions.size()));
			}

			if (apply_starting_grants) _ap_starting_grants_applied_slot = _ap_last_slot;

			/* Flush items that arrived before GM_NORMAL */
			for (const APItem &item : _ap_pending_items) AP_OnItemReceived(item);
			_ap_pending_items.clear();

			/* Open the status overlay */
			ShowArchipelagoStatusWindow();

			if (_ap_open_network_window_pending && !_networking) {
				_ap_open_network_window_pending = false;
				AP_OK("Opening Create Server window for AP game hosting.");
				ShowNetworkStartServerWindow();
			}

			AP_OK(fmt::format("AP session started. {} engines in map. Mission evaluation active.",
			      _ap_engine_map.size()));
		}

		/* Mission checks run every ~1 s for snappier AP completion feedback.
		 * Heavier maintenance (engine re-lock + economy stat accumulation)
		 * and win condition remain every ~10 s. */
		_ap_realtime_ticks++;

		if (_ap_realtime_ticks % AP_MISSION_CHECK_TICKS == 0 &&
		    _ap_session_started &&
		    _game_mode == GM_NORMAL) {
			/* Evaluate all incomplete missions every ~1 s for snappy AP completion feedback. */
			CheckMissions();
		}

		if (_ap_realtime_ticks % AP_STATS_AND_LOCK_TICKS == 0 &&
		    _ap_session_started &&
		    _game_mode == GM_NORMAL) {

			/* Re-apply engine locks every ~10 s as a safety net against
			 * OpenTTD's engine introduction system (StartupEngines/StartupOneEngine)
			 * which runs on NewGRF changes and re-sets EngineFlag::Available and
			 * company_avail for all engines whose intro_date has passed.
			 * We use _ap_unlocked_engine_ids (set by AP_UnlockEngineByName) to
			 * distinguish "AP-unlocked" from "re-introduced by StartupEngines". */
			CompanyID lock_cid = _local_company;
			if (lock_cid < MAX_COMPANIES) {
				const bool has_lock_list = !_ap_pending_sd.locked_vehicles.empty();
				bool need_invalidate = false;
				for (Engine *e : Engine::Iterate()) {
					if (AP_IsCargoWagonEngine(e)) {
						const bool should_have = AP_ShouldWagonBeUnlockedForCargo(e);
						if (should_have) {
							if (!e->company_avail.Test(lock_cid)) {
								e->flags.Set(EngineFlag::Available);
								EnableEngineForCompany(e->index, lock_cid);
								need_invalidate = true;
							}
						} else if (e->company_avail.Test(lock_cid)) {
							e->company_avail.Reset(lock_cid);
							need_invalidate = true;
						}
						continue;
					}

					/* If AP has explicitly unlocked this engine this session, leave it alone */
					if (_ap_unlocked_engine_ids.count(e->index) > 0) continue;
					if (has_lock_list) {
						std::string eng_name = GetString(STR_ENGINE_NAME,
							PackEngineNameDParam(e->index, EngineNameContext::PurchaseList));
						if (eng_name.empty()) eng_name = GetString(e->info.string_id);
						bool in_list = (_ap_pending_sd.locked_vehicles.count(eng_name) > 0);
						if (!in_list) continue;
					}

					/* Otherwise suppress company_avail — do NOT clear EngineFlag::Available,
					 * as that would cause CalendarEnginesMonthlyLoop to re-introduce
					 * the engine for all companies via NewVehicleAvailable(). */
					if (e->company_avail.Test(lock_cid)) {
						e->company_avail.Reset(lock_cid);
						need_invalidate = true;
					}
				}
				if (need_invalidate) InvalidateWindowClassesData(WC_BUILD_VEHICLE, 0);
			}

			/* Accumulate cargo/profit from completed economy periods */
			AP_UpdateSessionStats();

		}

		if (_ap_realtime_ticks >= AP_WIN_CHECK_TICKS &&
		    _ap_session_started && !_ap_goal_sent &&
		    _game_mode == GM_NORMAL) {

			_ap_realtime_ticks = 0;
			if (CheckWinCondition()) {
				_ap_goal_sent = true;
				_ap_client->SendGoal();
				Debug(misc, 0, "[AP] Win condition reached! Goal sent.");
				AP_OK("*** WIN CONDITION REACHED! Goal sent to server! ***");
				AP_ShowNews("WIN CONDITION REACHED! Goal sent to server!");
				/* Show vanilla endgame screen immediately for the local slot that
				 * actually reached its AP goal. In multiplayer this stays instance-
				 * local because CheckWinCondition() uses each client's own AP state. */
				ShowEndGameChart();
			}
		}
	}
);

/* ---------------------------------------------------------------------------
 * AP Connection Config — persist server/slot between game sessions
 * Written to <personal_dir>/ap_connection.cfg (simple key=value format).
 * Called by GUI on successful connect (save) and on window open (load).
 * -------------------------------------------------------------------------- */

void AP_SaveConnectionConfig()
{
	std::string path = _personal_dir + "ap_connection.cfg";
	FILE *f = fopen(path.c_str(), "w");
	if (f == nullptr) return;
	fmt::print(f, "host={}\n", _ap_last_host);
	fmt::print(f, "port={}\n", (unsigned)_ap_last_port);
	fmt::print(f, "slot={}\n", _ap_last_slot);
	fmt::print(f, "pass={}\n", _ap_last_pass);
	fmt::print(f, "ssl={}\n", _ap_last_ssl ? 1 : 0);
	fclose(f);
}

void AP_LoadConnectionConfig()
{
	std::string path = _personal_dir + "ap_connection.cfg";
	FILE *f = fopen(path.c_str(), "r");
	if (f == nullptr) return;
	char line[512];
	while (fgets(line, sizeof(line), f)) {
		/* Strip trailing newline */
		size_t len = strlen(line);
		while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) line[--len] = '\0';
		std::string s(line);
		auto eq = s.find('=');
		if (eq == std::string::npos) continue;
		std::string key = s.substr(0, eq);
		std::string val = s.substr(eq + 1);
		if (key == "host" && !val.empty()) _ap_last_host = val;
		else if (key == "port") { uint16_t p = 0; auto [ptr, ec] = std::from_chars(val.data(), val.data() + val.size(), p); if (ec == std::errc{} && p > 0) _ap_last_port = p; }
		else if (key == "slot") _ap_last_slot = val;
		else if (key == "pass") _ap_last_pass = val;
		else if (key == "ssl") _ap_last_ssl = (val == "1");
	}
	fclose(f);
}

/* ---------------------------------------------------------------------------
 * AP_EnsureBasesets — activate bundled OpenGFX/OpenSFX/OpenMSX if the engine
 * is currently running with fallback (silent / missing-sprite) sets.
 * Called once from intro_gui.cpp OnInit(), after FindSets() has already run.
 * Only switches a set if (a) the current set is marked fallback AND
 * (b) the named set is actually present on disk.
 * The ini_set write makes the choice persist to openttd.cfg on shutdown.
 * -------------------------------------------------------------------------- */
void AP_EnsureBasesets()
{
	/* Graphics — only switch if current is fallback */
	const GraphicsSet *gfx = BaseGraphics::GetUsedSet();
	if (gfx == nullptr || gfx->fallback) {
		if (BaseGraphics::SetSetByName("OpenGFX")) {
			BaseGraphics::ini_data.name = "OpenGFX";
		}
	}
	/* Sound */
	const SoundsSet *sfx = BaseSounds::GetUsedSet();
	if (sfx == nullptr || sfx->fallback) {
		if (BaseSounds::SetSetByName("OpenSFX")) {
			BaseSounds::ini_set = "OpenSFX";
		}
	}
	/* Music */
	const MusicSet *msx = BaseMusic::GetUsedSet();
	if (msx == nullptr || msx->fallback) {
		if (BaseMusic::SetSetByName("OpenMSX")) {
			BaseMusic::ini_set = "OpenMSX";
		}
	}
}
