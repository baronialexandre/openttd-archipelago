/*
 * This file is part of OpenTTD.
 * OpenTTD is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, version 2.
 */

#include "stdafx.h"
#include "core/format.hpp"
#include "core/string_consumer.hpp"
#include "archipelago.h"
#include "archipelago_gui.h"
#include "gui.h"
#include "window_gui.h"
#include "window_func.h"
#include "gfx_func.h"
#include "strings_func.h"
#include "viewport_func.h"
#include "querystring_gui.h"
#include "textbuf_gui.h"
#include "fontcache.h"
#include "currency.h"
#include "fios.h"
#include "network/network.h"
#include "table/strings.h"
#include "table/sprites.h"
#include "safeguards.h"

/* =========================================================================
 * CONNECT WINDOW
 * ========================================================================= */

enum APWidgets : WidgetID {
	WAPGUI_LABEL_SERVER,
	WAPGUI_EDIT_SERVER,
	WAPGUI_LABEL_SLOT,
	WAPGUI_EDIT_SLOT,
	WAPGUI_LABEL_PASS,
	WAPGUI_EDIT_PASS,
	WAPGUI_STATUS,
	WAPGUI_SLOT_INFO,
	WAPGUI_SEL_ACTIONS,
	WAPGUI_BTN_CONNECT_MENU,
	WAPGUI_BTN_CONNECT_INGAME,
	WAPGUI_BTN_DISCONNECT_INGAME,
};

enum APPlayModeWidgets : WidgetID {
	WAPPM_TEXT,
	WAPPM_BTN_SINGLE,
	WAPPM_BTN_MULTI,
	WAPPM_BTN_CANCEL,
};

enum APStartFlowWidgets : WidgetID {
	WAPSF_TEXT,
	WAPSF_BTN_GENERATE,
	WAPSF_BTN_LOAD,
	WAPSF_BTN_CANCEL,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_GREY),
		NWidget(WWT_CAPTION, COLOUR_GREY), SetStringTip(STR_ARCHIPELAGO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_GREY),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_GREY), SetResize(1, 1),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4), SetPadding(6),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_SERVER), SetStringTip(STR_ARCHIPELAGO_LABEL_SERVER), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_SERVER), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_SLOT), SetStringTip(STR_ARCHIPELAGO_LABEL_SLOT), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_SLOT), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_LABEL_PASS), SetStringTip(STR_ARCHIPELAGO_LABEL_PASS), SetMinimalSize(80, 14),
				NWidget(WWT_EDITBOX, COLOUR_GREY, WAPGUI_EDIT_PASS), SetStringTip(STR_EMPTY), SetMinimalSize(200, 14), SetFill(1, 0),
			EndContainer(),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_STATUS),    SetMinimalSize(284, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPGUI_SLOT_INFO), SetMinimalSize(284, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_SELECTION, INVALID_COLOUR, WAPGUI_SEL_ACTIONS),
				NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPGUI_BTN_CONNECT_MENU), SetStringTip(STR_ARCHIPELAGO_BTN_CONNECT), SetMinimalSize(220, 14), SetFill(1, 0),
				EndContainer(),
				NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPGUI_BTN_CONNECT_INGAME), SetStringTip(STR_ARCHIPELAGO_BTN_CONNECT), SetMinimalSize(108, 14), SetFill(1, 0),
					NWidget(WWT_PUSHTXTBTN, COLOUR_RED,   WAPGUI_BTN_DISCONNECT_INGAME), SetStringTip(STR_ARCHIPELAGO_BTN_DISCONNECT), SetMinimalSize(108, 14), SetFill(1, 0),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
	NWidget(WWT_RESIZEBOX, COLOUR_GREY),
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_play_mode_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 1),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4), SetPadding(6),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPPM_TEXT), SetMinimalSize(320, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPPM_BTN_SINGLE), SetStringTip(STR_EMPTY, STR_INTRO_TOOLTIP_NEW_GAME), SetMinimalSize(102, 14), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPPM_BTN_MULTI),  SetStringTip(STR_EMPTY, STR_INTRO_TOOLTIP_MULTIPLAYER), SetMinimalSize(102, 14), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPPM_BTN_CANCEL), SetStringTip(STR_EMPTY, STR_NULL), SetMinimalSize(102, 14), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_start_flow_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 1),
		NWidget(NWID_VERTICAL), SetPIP(4, 4, 4), SetPadding(6),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPSF_TEXT), SetMinimalSize(320, 14), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 4, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN,  WAPSF_BTN_GENERATE), SetStringTip(STR_EMPTY, STR_NULL), SetMinimalSize(102, 14), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_ORANGE, WAPSF_BTN_LOAD),     SetStringTip(STR_EMPTY, STR_INTRO_TOOLTIP_LOAD_GAME), SetMinimalSize(102, 14), SetFill(1, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_GREY,   WAPSF_BTN_CANCEL),   SetStringTip(STR_EMPTY, STR_NULL), SetMinimalSize(102, 14), SetFill(1, 0),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoConnectWindow;
static void ShowAPPlayModeChoiceWindow();
static void ShowAPStartFlowChoiceWindow();

struct ArchipelagoConnectWindow : public Window {
	QueryString server_buf;
	QueryString slot_buf;
	QueryString pass_buf;
	std::string server_str, slot_str, pass_str;
	APState  last_state  = APState::DISCONNECTED;
	bool     last_has_sd = false;
	bool     menu_multiplayer_mode = false;

	static std::string StatusStr(APState s, bool has_sd, const std::string &err) {
		switch (s) {
			case APState::DISCONNECTED:   return "Not connected.";
			case APState::CONNECTING:     return "Connecting...";
			case APState::CONNECTED:      return "Authenticating...";
			case APState::AUTHENTICATING: return "Authenticating...";
			case APState::AUTHENTICATED:  return has_sd ? "Connected! Slot data loaded." : "Connected. Waiting for data...";
			case APState::AP_ERROR:       return "Error: " + err;
			default:                      return "...";
		}
	}

	static std::string SlotInfoStr(bool has_sd) {
		if (!has_sd || _ap_client == nullptr) return "";
		APSlotData sd = _ap_client->GetSlotData();
		return "Missions: " + fmt::format("{}", sd.missions.size());
	}

	void StartConnection(bool start_new_mode)
	{
		if (_ap_client == nullptr) return;

		/* In multiplayer, only the host/server may own the AP connection. */
		if (_networking && !_network_server) {
			AP_ShowConsole("[AP] Multiplayer client mode: only the host can connect to Archipelago.");
			return;
		}

		AP_SetMenuConnectMultiplayer(this->menu_multiplayer_mode);
		AP_SetMenuConnectStartNew(start_new_mode);

		/* Strip any scheme prefix the user may have typed — auto-detect handles it */
		std::string raw = server_str;
		if (raw.rfind("wss://", 0) == 0)    raw = raw.substr(6);
		else if (raw.rfind("ws://", 0) == 0) raw = raw.substr(5);
		std::string host = raw;
		uint16_t port = 38281;
		auto colon = raw.rfind(':');
		if (colon != std::string::npos) {
			host = raw.substr(0, colon);
			int p = ParseInteger<int>(raw.substr(colon + 1)).value_or(0);
			if (p > 0 && p < 65536) port = (uint16_t)p;
		}

		_ap_last_host = host; _ap_last_port = port;
		_ap_last_slot = slot_str; _ap_last_pass = pass_str;
		AP_SaveConnectionConfig(); /* persist for next session */
		_ap_last_ssl = false; /* unused — auto-detect in WorkerThread */
		_ap_client->Connect(host, port, slot_str, pass_str, "OpenTTD", false);

		if (!start_new_mode && _game_mode == GM_MENU) {
			ShowSaveLoadDialog(FT_SAVEGAME, SLO_LOAD);
		}

		this->SetDirty();
	}

	void OnMenuPlayModeChosen(bool multiplayer_mode)
	{
		this->menu_multiplayer_mode = multiplayer_mode;
		ShowAPStartFlowChoiceWindow();
	}

	ArchipelagoConnectWindow(WindowDesc &desc, WindowNumber wnum)
		: Window(desc), server_buf(256), slot_buf(64), pass_buf(64)
	{
		this->CreateNestedTree();
		this->querystrings[WAPGUI_EDIT_SERVER] = &server_buf;
		this->querystrings[WAPGUI_EDIT_SLOT]   = &slot_buf;
		this->querystrings[WAPGUI_EDIT_PASS]   = &pass_buf;
		this->FinishInitNested(wnum);

		/* Restore last connection settings from ap_connection.cfg */
		AP_LoadConnectionConfig();

		std::string full = _ap_last_host.empty() ? "archipelago.gg:38281"
		                 : _ap_last_host + ":" + fmt::format("{}", _ap_last_port);
		server_buf.text.Assign(full.c_str());
		server_str = full;
		if (!_ap_last_slot.empty()) { slot_buf.text.Assign(_ap_last_slot.c_str()); slot_str = _ap_last_slot; }
		if (!_ap_last_pass.empty()) { pass_buf.text.Assign(_ap_last_pass.c_str()); pass_str = _ap_last_pass; }
	}

	void OnEditboxChanged(WidgetID wid) override {
		switch (wid) {
			case WAPGUI_EDIT_SERVER: server_str = server_buf.text.GetText().data(); break;
			case WAPGUI_EDIT_SLOT:   slot_str   = slot_buf.text.GetText().data();   break;
			case WAPGUI_EDIT_PASS:   pass_str   = pass_buf.text.GetText().data();   break;
		}
	}

	void OnGameTick() override {
		if (_ap_client == nullptr) return;
		APState s = _ap_client->GetState();
		bool    h = _ap_client->HasSlotData();
		if (s != last_state || h != last_has_sd) { last_state = s; last_has_sd = h; this->SetDirty(); }
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (_ap_client == nullptr) return;
		const int text_y = r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2);
		switch (widget) {
			case WAPGUI_STATUS:
				DrawString(r.left, r.right, r.top,
				    StatusStr(_ap_client->GetState(), _ap_client->HasSlotData(), _ap_client->GetLastError()),
				    TC_BLACK);
				break;
			case WAPGUI_SLOT_INFO:
				DrawString(r.left, r.right, r.top, SlotInfoStr(_ap_client->HasSlotData()), TC_DARK_GREEN);
				break;
			case WAPGUI_BTN_CONNECT_MENU:
			case WAPGUI_BTN_CONNECT_INGAME:
				DrawString(r.left, r.right, text_y, "Connect", TC_BLACK, SA_HOR_CENTER);
				break;
			case WAPGUI_BTN_DISCONNECT_INGAME:
				DrawString(r.left, r.right, text_y, "Disconnect", TC_BLACK, SA_HOR_CENTER);
				break;
		}
	}

	void OnPaint() override
	{
		this->GetWidget<NWidgetStacked>(WAPGUI_SEL_ACTIONS)->SetDisplayedPlane(_game_mode == GM_MENU ? 0 : 1);
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPGUI_BTN_CONNECT_MENU:
			case WAPGUI_BTN_CONNECT_INGAME: {
				if (_ap_client == nullptr) break;
				if (_game_mode == GM_MENU) {
					ShowAPPlayModeChoiceWindow();
				} else {
					/* In an active game, reconnect should never trigger world generation. */
					this->menu_multiplayer_mode = _networking;
					this->StartConnection(false);
				}
				break;
			}
			case WAPGUI_BTN_DISCONNECT_INGAME:
				if (_ap_client) _ap_client->Disconnect();
				this->SetDirty();
				break;
		}
	}
};

struct APPlayModeChoiceWindow : public Window {
	APPlayModeChoiceWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(wnum);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const int text_y = r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2);
		if (widget == WAPPM_TEXT) {
			DrawString(r.left, r.right, r.top, "Play AP as Singleplayer or Multiplayer?", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPPM_BTN_SINGLE) {
			DrawString(r.left, r.right, text_y, "Singleplayer", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPPM_BTN_MULTI) {
			DrawString(r.left, r.right, text_y, "Multiplayer", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPPM_BTN_CANCEL) {
			DrawString(r.left, r.right, text_y, "Cancel", TC_BLACK, SA_HOR_CENTER);
			return;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		auto *connect = static_cast<ArchipelagoConnectWindow *>(FindWindowById(WC_ARCHIPELAGO, 0));
		if (connect == nullptr) {
			this->Close();
			return;
		}

		switch (widget) {
			case WAPPM_BTN_SINGLE:
				this->Close();
				connect->OnMenuPlayModeChosen(false);
				break;
			case WAPPM_BTN_MULTI:
				this->Close();
				connect->OnMenuPlayModeChosen(true);
				break;
			case WAPPM_BTN_CANCEL:
				this->Close();
				break;
		}
	}
};

struct APStartFlowChoiceWindow : public Window {
	APStartFlowChoiceWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->CreateNestedTree();
		this->FinishInitNested(wnum);
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		const int text_y = r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2);
		if (widget == WAPSF_TEXT) {
			DrawString(r.left, r.right, r.top, "Generate a new AP map or load an AP save?", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPSF_BTN_GENERATE) {
			DrawString(r.left, r.right, text_y, "Generate", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPSF_BTN_LOAD) {
			DrawString(r.left, r.right, text_y, "Load", TC_BLACK, SA_HOR_CENTER);
			return;
		}
		if (widget == WAPSF_BTN_CANCEL) {
			DrawString(r.left, r.right, text_y, "Cancel", TC_BLACK, SA_HOR_CENTER);
			return;
		}
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int click_count) override
	{
		auto *connect = static_cast<ArchipelagoConnectWindow *>(FindWindowById(WC_ARCHIPELAGO, 0));
		if (connect == nullptr) {
			this->Close();
			return;
		}

		switch (widget) {
			case WAPSF_BTN_GENERATE:
				this->Close();
				connect->StartConnection(true);
				break;
			case WAPSF_BTN_LOAD:
				this->Close();
				connect->StartConnection(false);
				break;
			case WAPSF_BTN_CANCEL:
				this->Close();
				break;
		}
	}
};

static WindowDesc _ap_play_mode_desc(
	WDP_CENTER, {"ap_play_mode"}, 360, 96,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_play_mode_widgets
);

static WindowDesc _ap_start_flow_desc(
	WDP_CENTER, {"ap_start_flow"}, 360, 96,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_start_flow_widgets
);

static void ShowAPPlayModeChoiceWindow()
{
	CloseWindowById(WC_ARCHIPELAGO, 4);
	AllocateWindowDescFront<APPlayModeChoiceWindow>(_ap_play_mode_desc, 4);
}

static void ShowAPStartFlowChoiceWindow()
{
	CloseWindowById(WC_ARCHIPELAGO, 5);
	AllocateWindowDescFront<APStartFlowChoiceWindow>(_ap_start_flow_desc, 5);
}

static WindowDesc _ap_connect_desc(
	WDP_CENTER, {}, 380, 196,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_widgets
);

void ShowArchipelagoConnectWindow()
{
	if (_ap_client == nullptr) InitArchipelago();
	AllocateWindowDescFront<ArchipelagoConnectWindow>(_ap_connect_desc, 0);
}

/* =========================================================================
 * STATUS WINDOW — persistent top-right overlay
 * ========================================================================= */

enum APStatusWidgets : WidgetID {
	WAPST_STATUS_LINE,
	WAPST_GOAL_LINE,
	WAPST_BTN_MISSIONS,
	WAPST_BTN_INVENTORY,
	WAPST_SEL_SETTINGS,
	WAPST_BTN_SETTINGS_CONNECTED,
	WAPST_BTN_SETTINGS_DISCONNECTED,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_status_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN), SetStringTip(STR_ARCHIPELAGO_STATUS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetResize(1, 1),
		NWidget(NWID_VERTICAL), SetPIP(2, 2, 2), SetPadding(4),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_STATUS_LINE), SetMinimalSize(220, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(WWT_TEXT, INVALID_COLOUR, WAPST_GOAL_LINE),   SetMinimalSize(220, 12), SetFill(1, 0), SetStringTip(STR_EMPTY),
			NWidget(NWID_HORIZONTAL), SetPIP(0, 3, 0),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPST_BTN_INVENTORY),  SetStringTip(STR_ARCHIPELAGO_BTN_INVENTORY),  SetMinimalSize(70, 14),
				NWidget(WWT_PUSHTXTBTN, COLOUR_YELLOW, WAPST_BTN_MISSIONS),  SetStringTip(STR_ARCHIPELAGO_BTN_MISSIONS),  SetMinimalSize(70, 14),
				NWidget(NWID_SELECTION, INVALID_COLOUR, WAPST_SEL_SETTINGS),
					NWidget(WWT_PUSHTXTBTN, COLOUR_GREEN, WAPST_BTN_SETTINGS_CONNECTED), SetStringTip(STR_EMPTY), SetMinimalSize(70, 14),
					NWidget(WWT_PUSHTXTBTN, COLOUR_RED,   WAPST_BTN_SETTINGS_DISCONNECTED), SetStringTip(STR_EMPTY), SetMinimalSize(70, 14),
				EndContainer(),
			EndContainer(),
		EndContainer(),
	EndContainer(),
};

struct ArchipelagoStatusWindow : public Window {
	APState last_state  = APState::DISCONNECTED;
	bool    last_has_sd = false;

	ArchipelagoStatusWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc) {
		this->CreateNestedTree();
		this->FinishInitNested(wnum);
	}

	static std::string StatusLine() {
		if (_ap_client == nullptr) return "AP: No client";
		const std::string slot = _ap_last_slot.empty() ? std::string("?") : _ap_last_slot;
		switch (_ap_client->GetState()) {
			case APState::AUTHENTICATED:  return "AP: Connected (slot: " + slot + ")";
			case APState::CONNECTING:     return "AP: Connecting...";
			case APState::AUTHENTICATING: return "AP: Authenticating...";
			case APState::AP_ERROR:       return "AP: Error - " + _ap_client->GetLastError();
			case APState::DISCONNECTED:   return "AP: Disconnected";
			default:                      return "AP: ...";
		}
	}

	static std::string GoalLine() {
		if (_ap_client == nullptr || !_ap_client->HasSlotData()) return "No slot data";

		/* New AP world goal: collect all 11 cargo unlock items. */
		static constexpr const char *CARGO_WIN_ITEMS[] = {
			"Passengers", "Mail", "Coal", "Oil", "Livestock", "Goods",
			"Grain", "Wood", "Iron Ore", "Steel", "Valuables"
		};
		const std::map<std::string, int> &counts = AP_GetReceivedItemCounts();
		int collected = 0;
		for (const char *cargo : CARGO_WIN_ITEMS) {
			if (counts.count(cargo) > 0) collected++;
		}
		return fmt::format("Goal: Cargo unlocks {}/11", collected);
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		if (_ap_client == nullptr) return;
		APState s = _ap_client->GetState();
		bool    h = _ap_client->HasSlotData();
		bool    d = _ap_status_dirty.exchange(false);
		if (s != last_state || h != last_has_sd || d) {
			last_state = s; last_has_sd = h;
			this->SetDirty();
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		switch (widget) {
			case WAPST_STATUS_LINE: {
				bool ok = (_ap_client && _ap_client->GetState() == APState::AUTHENTICATED);
				DrawString(r.left, r.right, r.top, StatusLine(), ok ? TC_GREEN : TC_RED);
				break;
			}
			case WAPST_GOAL_LINE:
				DrawString(r.left, r.right, r.top, GoalLine(), TC_GOLD);
				break;
			case WAPST_BTN_SETTINGS_CONNECTED:
			case WAPST_BTN_SETTINGS_DISCONNECTED:
				DrawString(r.left, r.right,
				    r.top + std::max(0, ((int)r.Height() - GetCharacterHeight(FS_NORMAL)) / 2),
				    "AP Settings", TC_BLACK, SA_HOR_CENTER);
				break;
		}
	}

	void OnPaint() override {
		bool connected = AP_IsConnected();
		this->GetWidget<NWidgetStacked>(WAPST_SEL_SETTINGS)->SetDisplayedPlane(connected ? 0 : 1);
		this->SetWidgetDisabledState(WAPST_BTN_MISSIONS, !AP_IsConnected());
		this->SetWidgetDisabledState(WAPST_BTN_INVENTORY, !AP_IsConnected());
		this->DrawWidgets();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPST_BTN_MISSIONS:
				ShowArchipelagoMissionsWindow();
				break;
			case WAPST_BTN_INVENTORY:
				ShowArchipelagoInventoryWindow();
				break;
			case WAPST_BTN_SETTINGS_CONNECTED:
			case WAPST_BTN_SETTINGS_DISCONNECTED:
				ShowArchipelagoConnectWindow();
				break;
		}
	}
};

static WindowDesc _ap_status_desc(
	WDP_AUTO, {"ap_status"}, 228, 82,
	WC_ARCHIPELAGO_TICKER, WC_NONE, {},
	_nested_ap_status_widgets
);

void ShowArchipelagoStatusWindow()
{
	AllocateWindowDescFront<ArchipelagoStatusWindow>(_ap_status_desc, 0);
}

/* =========================================================================
 * MISSIONS WINDOW (Bug F fix)
 *
 * Scrollable list of all missions from slot_data.
 * Shows: description and completed/pending status.
 * ========================================================================= */

enum APMissionsWidgets : WidgetID {
	WAPM_CAPTION,
	WAPM_SCROLLBAR,
	WAPM_HSCROLLBAR,
	WAPM_LIST,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_missions_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_BROWN),
		NWidget(WWT_CAPTION, COLOUR_BROWN), SetStringTip(STR_ARCHIPELAGO_MISSIONS_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_BROWN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_BROWN), SetResize(1, 1),
		/* Mission list + vertical scrollbar */
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_GREY, WAPM_LIST), SetMinimalSize(220, 120), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPM_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_BROWN, WAPM_SCROLLBAR),
		EndContainer(),
		/* Horizontal scrollbar + resize box */
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_HSCROLLBAR, COLOUR_BROWN, WAPM_HSCROLLBAR),
			NWidget(WWT_RESIZEBOX, COLOUR_BROWN),
		EndContainer(),
	EndContainer(),
};

/* ---------------------------------------------------------------------------
 * Shared currency formatting helpers (used by mission UI text).
 * Uses GetCurrency() so the symbol and rate are always in sync with game settings.
 * --------------------------------------------------------------------------- */

/** Compact money string: "$50M", "£200k", "1,5M kr" etc. */
static std::string AP_FormatMoneyCompact(int64_t amount)
{
	const CurrencySpec &cs = GetCurrency();
	int64_t scaled = amount * cs.rate;

	std::string num;
	if (scaled >= 1000000) num = fmt::format("{:.1f}M", scaled / 1000000.0);
	else if (scaled >= 1000) num = fmt::format("{}k", scaled / 1000);
	else num = fmt::format("{}", scaled);

	if (cs.symbol_pos == 0) return cs.prefix + num + cs.suffix;
	return num + cs.suffix;
}

struct ArchipelagoMissionsWindow : public Window {
	int           row_height    = 0;      /* computed in constructor from font height */
	int           max_line_px   = 0;      /* width in pixels of the longest line */
	std::vector<const APMission *> visible_missions;
	Scrollbar *scrollbar  = nullptr;
	Scrollbar *hscrollbar = nullptr;

	void RebuildVisibleList() {
		visible_missions.clear();
		const APSlotData &sd = AP_GetSlotData();
		for (const APMission &m : sd.missions) {
			visible_missions.push_back(&m);
		}
		if (this->scrollbar) {
			this->scrollbar->SetCount((int)visible_missions.size());
		}
		/* Compute max line pixel width so the horizontal scrollbar range is correct */
		max_line_px = 0;
		for (const APMission *m : visible_missions) {
			std::string prefix = m->completed ? "[X] " : "[ ] ";
			std::string mission_num;
			{
				const std::string &loc = m->location;
				auto last_us = loc.rfind('_');
				if (last_us != std::string::npos && last_us + 1 < loc.size()) {
					mission_num = " #" + loc.substr(last_us + 1);
				}
			}
			std::string line = prefix + mission_num + " - " + m->description;
			int w = GetStringBoundingBox(line).width;
			if (w > max_line_px) max_line_px = w;
		}
		UpdateHScrollbar();
		this->SetDirty();
	}

	void UpdateHScrollbar() {
		if (!this->hscrollbar) return;
		NWidgetBase *nw = this->GetWidget<NWidgetBase>(WAPM_LIST);
		int visible_w = (nw != nullptr) ? (int)nw->current_x - 8 : 460;
		int total     = std::max(max_line_px + 16, visible_w);
		this->hscrollbar->SetCount(total);
		this->hscrollbar->SetCapacity(visible_w);
	}

	ArchipelagoMissionsWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc) {
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3; /* +3px vertical padding */
		this->CreateNestedTree();
		this->scrollbar  = this->GetScrollbar(WAPM_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->hscrollbar = this->GetScrollbar(WAPM_HSCROLLBAR);
		this->hscrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		this->resize.step_width  = 1;
		RebuildVisibleList();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override {
		if (_ap_status_dirty.load()) RebuildVisibleList();
	}

	void OnClick([[maybe_unused]] Point pt, WidgetID widget, [[maybe_unused]] int cc) override {
		switch (widget) {
			case WAPM_LIST: {
				/* Click on a mission row — if it has a named entity (town/industry),
				 * scroll the main viewport to that location on the map. */
				int rh    = GetCharacterHeight(FS_NORMAL) + 3;
				const Rect &r = this->GetWidget<NWidgetBase>(WAPM_LIST)->GetCurrentRect();
				int row   = this->scrollbar->GetPosition() + (pt.y - r.top - 2) / rh;
				if (row < 0 || row >= (int)visible_missions.size()) break;
				const APMission *m = visible_missions[row];
				if (m->named_entity.tile != UINT32_MAX) {
					ScrollMainWindowToTile(TileIndex{m->named_entity.tile});
				}
				break;
			}
		}
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override {
		if (widget != WAPM_LIST) return;

		/* Recompute row height at draw time — GetCharacterHeight returns the
		 * correct scaled value for the current UI zoom level.  Using the cached
		 * constructor value causes rows to overlap at UI scale >=2. */
		int rh = GetCharacterHeight(FS_NORMAL) + 3;

		int y = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / rh) + 1;

		for (int i = first; i < last && i < (int)visible_missions.size(); i++) {
			const APMission *m = visible_missions[i];

			TextColour tc = m->completed ? TC_DARK_GREEN : TC_WHITE;

			/* Format: [X] #042 - Description  (current/target) */
			std::string prefix = m->completed ? "[X] " : "[ ] ";
			/* Extract mission number from location string (e.g. "Mission_Easy_042" -> "#042") */
			std::string mission_num;
			{
				const std::string &loc = m->location;
				auto last_us = loc.rfind('_');
				if (last_us != std::string::npos && last_us + 1 < loc.size()) {
					mission_num = " #" + loc.substr(last_us + 1);
				}
			}

			/* Build progress string for incomplete missions.
			 * Named missions and maintain missions always show progress (even 0)
			 * so the player knows the tracker is live. */
			bool is_named    = (m->type == "passengers_to_town" || m->type == "mail_to_town" ||
			                    m->type == "cargo_from_industry" || m->type == "cargo_to_industry");
			bool is_maintain = (m->type == "maintain_75" ||
			                    m->type.find("maintain") != std::string::npos);
			std::string progress_str;
			if (!m->completed && m->amount > 0 && (m->current_value > 0 || is_named || is_maintain)) {
				if (is_maintain) {
					/* Show as "X/Y months" — never as money */
					progress_str = fmt::format("  ({}/{} months)", m->current_value, m->amount);
				} else {
					/* Detect money missions by unit — only £ and £/month, NOT plain "months" */
					bool is_money = (m->unit == "\xC2\xA3" || m->unit == "\xC2\xA3/month" ||
					                 m->unit == "£" || m->unit == "£/month" ||
					                 m->unit.find("/month") != std::string::npos);
					if (is_money) {
						progress_str = fmt::format("  ({}/{})",
						    AP_FormatMoneyCompact(m->current_value),
						    AP_FormatMoneyCompact(m->amount));
					} else {
						auto fmt_num = [](int64_t v) -> std::string {
							if (v >= 1000000) return fmt::format("{:.1f}M", v / 1000000.0);
							if (v >= 1000)    return fmt::format("{}k", v / 1000);
							return fmt::format("{}", v);
						};
						progress_str = fmt::format("  ({}/{})", fmt_num(m->current_value), fmt_num(m->amount));
					}
				}
			}

			/* Bug 2 fix: replace hardcoded £ in description with the current
			 * game currency prefix so the mission text matches the player's
			 * chosen currency (e.g. USD shows $ instead of £). */
			std::string desc = m->description;
			{
				const CurrencySpec &cs = GetCurrency();
				std::string pound_utf8 = "\xC2\xA3"; /* UTF-8 encoding of £ */
				std::string replacement = cs.prefix.empty() ? std::string(cs.suffix) : std::string(cs.prefix);
				/* Only replace if the game isn't using GBP (prefix "£") */
				if (replacement != pound_utf8 && replacement != "\xC2\xA3") {
					size_t pos = 0;
					while ((pos = desc.find(pound_utf8, pos)) != std::string::npos) {
						desc.replace(pos, pound_utf8.size(), replacement);
						pos += replacement.size();
					}
				}
			}

			/* For named missions (town/industry assigned), append a map-pin symbol
			 * to hint that clicking this row will scroll the viewport there. */
			std::string nav_hint;
			if (m->named_entity.tile != UINT32_MAX) {
				nav_hint = " \xe2\x86\x91"; /* ↑ unicode arrow — visual cue to scroll map */
			}
			std::string line = prefix + mission_num + " - " + desc + progress_str + nav_hint;

			int x_off = this->hscrollbar ? -this->hscrollbar->GetPosition() : 0;
			DrawString(r.left + 4 + x_off, r.right + max_line_px, y, line, tc, SA_LEFT | SA_FORCE);
			y += rh;
			if (y > r.bottom) break;
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override {
		UpdateHScrollbar();
		this->SetDirty();
	}

	void OnResize() override {
		if (this->scrollbar) {
			int rh = GetCharacterHeight(FS_NORMAL) + 3;
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPM_LIST)->current_y / rh);
		}
		UpdateHScrollbar();
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPM_LIST) {
			resize.height = row_height;
			resize.width = 1;
			size.height = std::max(size.height, (uint)(row_height * 15));
		}
	}
};

static WindowDesc _ap_missions_desc(
	WDP_AUTO, {"ap_missions"}, 480, 340,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_missions_widgets
);

void ShowArchipelagoMissionsWindow()
{
	AllocateWindowDescFront<ArchipelagoMissionsWindow>(_ap_missions_desc, 2);
}

/* =========================================================================
 * INVENTORY WINDOW — all AP items received this session
 * ========================================================================= */

enum APInventoryWidgets : WidgetID {
	WAPINV_CAPTION,
	WAPINV_LIST,
	WAPINV_SCROLLBAR,
};

static constexpr std::initializer_list<NWidgetPart> _nested_ap_inventory_widgets = {
	NWidget(NWID_HORIZONTAL),
		NWidget(WWT_CLOSEBOX, COLOUR_DARK_GREEN),
		NWidget(WWT_CAPTION, COLOUR_DARK_GREEN, WAPINV_CAPTION), SetStringTip(STR_ARCHIPELAGO_INVENTORY_CAPTION, STR_TOOLTIP_WINDOW_TITLE_DRAG_THIS), SetFill(1, 0), SetResize(1, 0),
		NWidget(WWT_STICKYBOX, COLOUR_DARK_GREEN),
	EndContainer(),
	NWidget(WWT_PANEL, COLOUR_DARK_GREEN), SetResize(1, 1),
		NWidget(NWID_HORIZONTAL),
			NWidget(WWT_PANEL, COLOUR_DARK_GREEN, WAPINV_LIST), SetMinimalSize(220, 200), SetFill(1, 1), SetResize(1, 1), SetScrollbar(WAPINV_SCROLLBAR), EndContainer(),
			NWidget(NWID_VSCROLLBAR, COLOUR_DARK_GREEN, WAPINV_SCROLLBAR),
		EndContainer(),
		NWidget(NWID_HORIZONTAL),
			NWidget(NWID_SPACER), SetFill(1, 0), SetResize(1, 0),
			NWidget(WWT_RESIZEBOX, COLOUR_DARK_GREEN),
		EndContainer(),
	EndContainer(),
};

/** All cargo types in display order (matches apworld CARGO_TYPES). */
static const char * const AP_CARGO_ORDER[] = {
	"Passengers", "Mail", "Coal", "Oil", "Livestock",
	"Goods", "Grain", "Wood", "Iron Ore", "Steel", "Valuables",
};
static const char * const AP_VEHICLE_ORDER[] = {
	"Progressive Trains", "Progressive Road Vehicles",
	"Progressive Aircrafts", "Progressive Ships",
};

struct ArchipelagoInventoryWindow : public Window {
	struct InventoryRow {
		enum Type { HEADER, VEHICLE, VEHICLE_TIER, VEHICLE_NAME, CARGO, FILLER } type;
		std::string text;
		bool owned = false;
		int  count = 0;
	};

	std::vector<InventoryRow> rows;
	Scrollbar *scrollbar = nullptr;
	int row_height = 0;
	std::map<std::string, bool> expanded; /* vehicle item name → fold state */

	void RebuildRows()
	{
		rows.clear();
		const std::map<std::string, int> &counts = AP_GetReceivedItemCounts();

		/* ── Vehicles ──────────────────────────────────────────── */
		bool any_vehicle = false;
		for (const char *name : AP_VEHICLE_ORDER) {
			auto it = counts.find(name);
			if (it != counts.end() && it->second > 0) { any_vehicle = true; break; }
		}
		if (any_vehicle) {
			rows.push_back({ InventoryRow::HEADER, "— Vehicles —" });
			const auto &prog_tiers  = AP_GetProgressiveTiers();
			const auto &tier_counts = AP_GetUnlockedTierCounts();
			for (const char *name : AP_VEHICLE_ORDER) {
				auto it = counts.find(name);
				if (it == counts.end() || it->second == 0) continue;
				rows.push_back({ InventoryRow::VEHICLE, name, true, it->second });
				/* Emit tier sub-rows only when the item is expanded */
				auto exp_it = expanded.find(name);
				bool is_expanded = (exp_it != expanded.end()) && exp_it->second;
				if (is_expanded) {
					auto tier_def = prog_tiers.find(name);
					auto tier_cnt = tier_counts.find(name);
					if (tier_def != prog_tiers.end() && tier_cnt != tier_counts.end()) {
						int unlocked = tier_cnt->second;
						for (int t = 0; t < unlocked && t < (int)tier_def->second.size(); t++) {
							std::string tier_label = fmt::format("Tier {}", t + 1);
							rows.push_back({ InventoryRow::VEHICLE_TIER, tier_label, true, t + 1 });
							for (const std::string &veh : tier_def->second[t]) {
								rows.push_back({ InventoryRow::VEHICLE_NAME, veh, true, 0 });
							}
						}
					}
				}
			}
		}

		/* ── Cargo ─────────────────────────────────────────────── */
		int unlocked_cargo = 0;
		for (const char *name : AP_CARGO_ORDER) {
			bool owned = counts.count(name) > 0;
			if (owned) unlocked_cargo++;
		}
		rows.push_back({ InventoryRow::HEADER, fmt::format("— Cargo ({}/{}) —", unlocked_cargo, (int)std::size(AP_CARGO_ORDER)) });
		for (const char *name : AP_CARGO_ORDER) {
			bool owned = counts.count(name) > 0;
			rows.push_back({ InventoryRow::CARGO, name, owned, owned ? 1 : 0 });
		}

		/* ── Filler ────────────────────────────────────────────── */
		static const char * const FILLER_ITEMS[] = { "50,000 $", "Choo chooo!" };
		bool any_filler = false;
		for (const char *name : FILLER_ITEMS) {
			auto it = counts.find(name);
			if (it != counts.end() && it->second > 0) { any_filler = true; break; }
		}
		if (any_filler) {
			rows.push_back({ InventoryRow::HEADER, "— Filler —" });
			for (const char *name : FILLER_ITEMS) {
				auto it = counts.find(name);
				if (it != counts.end() && it->second > 0) {
					rows.push_back({ InventoryRow::FILLER, name, true, it->second });
				}
			}
		}

		if (this->scrollbar) this->scrollbar->SetCount((int)rows.size());
		this->SetDirty();
	}

	ArchipelagoInventoryWindow(WindowDesc &desc, WindowNumber wnum) : Window(desc)
	{
		this->row_height = GetCharacterHeight(FS_NORMAL) + 3;
		this->CreateNestedTree();
		this->scrollbar = this->GetScrollbar(WAPINV_SCROLLBAR);
		this->scrollbar->SetStepSize(1);
		this->FinishInitNested(wnum);
		this->resize.step_height = row_height;
		RebuildRows();
	}

	void OnRealtimeTick([[maybe_unused]] uint delta_ms) override
	{
		if (_ap_status_dirty.load()) RebuildRows();
	}

	void DrawWidget(const Rect &r, WidgetID widget) const override
	{
		if (widget != WAPINV_LIST) return;

		int rh    = GetCharacterHeight(FS_NORMAL) + 3;
		int y     = r.top + 2;
		int first = this->scrollbar->GetPosition();
		int last  = first + (r.Height() / rh) + 1;

		for (int i = first; i < last && i < (int)rows.size(); i++) {
			const InventoryRow &row = rows[i];
			switch (row.type) {
				case InventoryRow::HEADER:
					DrawString(r.left + 4, r.right - 4, y, row.text, TC_GOLD, SA_CENTER | SA_FORCE);
					break;

				case InventoryRow::VEHICLE: {
					auto exp_it = expanded.find(row.text);
					bool is_exp = (exp_it != expanded.end()) && exp_it->second;
					const int total_tiers = AP_GetProgressiveTiers().count(row.text)
					    ? (int)AP_GetProgressiveTiers().at(row.text).size() : row.count;
					std::string arrow = is_exp ? "[-] " : "[+] ";
					std::string line  = fmt::format("{}{} ({} / {} tiers)", arrow, row.text, row.count, total_tiers);
					DrawString(r.left + 8, r.right - 4, y, line, TC_YELLOW, SA_LEFT | SA_FORCE);
					break;
				}

				case InventoryRow::VEHICLE_TIER: {
					/* Alternate colours by tier number so they're visually distinct */
					static constexpr TextColour TIER_COLOURS[] = {
					    TC_LIGHT_BLUE, TC_GREEN, TC_YELLOW, TC_ORANGE, TC_RED
					};
					int idx = std::max(0, row.count - 1) % 5;
					DrawString(r.left + 20, r.right - 4, y, row.text, TIER_COLOURS[idx], SA_LEFT | SA_FORCE);
					break;
				}

				case InventoryRow::VEHICLE_NAME:
					DrawString(r.left + 36, r.right - 4, y, row.text, TC_WHITE, SA_LEFT | SA_FORCE);
					break;

				case InventoryRow::CARGO: {
					TextColour tc = row.owned ? TC_GREEN : TC_GREY;
					std::string marker = row.owned ? "[X]" : "[ ]";
					std::string line = marker + " " + row.text;
					DrawString(r.left + 8, r.right - 4, y, line, tc, SA_LEFT | SA_FORCE);
					break;
				}

				case InventoryRow::FILLER: {
					std::string line = row.text;
					if (row.count > 1) line += fmt::format("  x{}", row.count);
					DrawString(r.left + 8, r.right - 4, y, line, TC_SILVER, SA_LEFT | SA_FORCE);
					break;
				}
			}
			y += rh;
			if (y > r.bottom) break;
		}
	}

	void OnClick(Point pt, WidgetID widget, [[maybe_unused]] int cc) override
	{
		if (widget != WAPINV_LIST) return;

		int rh = GetCharacterHeight(FS_NORMAL) + 3;
		const NWidgetBase *wid = this->GetWidget<NWidgetBase>(WAPINV_LIST);
		int rel_y = pt.y - wid->pos_y - 2;
		if (rel_y < 0) return;

		int clicked_row = this->scrollbar->GetPosition() + rel_y / rh;
		if (clicked_row < 0 || clicked_row >= (int)rows.size()) return;

		const InventoryRow &row = rows[clicked_row];
		if (row.type == InventoryRow::VEHICLE) {
			bool is_expanded = expanded.count(row.text) != 0 && expanded.at(row.text);
			expanded[row.text] = !is_expanded;
			RebuildRows();
		}
	}

	void OnScrollbarScroll([[maybe_unused]] WidgetID widget) override { this->SetDirty(); }

	void OnResize() override
	{
		if (this->scrollbar) {
			int rh = GetCharacterHeight(FS_NORMAL) + 3;
			this->scrollbar->SetCapacity(
			    this->GetWidget<NWidgetBase>(WAPINV_LIST)->current_y / rh);
		}
	}

	void UpdateWidgetSize(WidgetID widget, Dimension &size, [[maybe_unused]] const Dimension &padding,
	                      [[maybe_unused]] Dimension &fill, Dimension &resize) override
	{
		if (widget == WAPINV_LIST) {
			resize.height = row_height;
			resize.width  = 1;
			size.height   = std::max(size.height, (uint)(row_height * 18));
		}
	}
};

static WindowDesc _ap_inventory_desc(
	WDP_AUTO, {"ap_inventory"}, 300, 360,
	WC_ARCHIPELAGO, WC_NONE, {},
	_nested_ap_inventory_widgets
);

void ShowArchipelagoInventoryWindow()
{
	AllocateWindowDescFront<ArchipelagoInventoryWindow>(_ap_inventory_desc, 3);
}
