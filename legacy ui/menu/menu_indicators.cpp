#include "../../globals.hpp"
#include "../config_system.h"
#include "../config_vars.h"

#include "menu.h"
#include "../../entlistener.hpp"

#include <ShlObj.h>
#include <algorithm>
#include <map>
#include <format>

// Username retrieval method
#define X1 RegOpenKeyExA
#define X2 RegQueryValueExA
#define X3 RegCloseKey
#define X4 HeapAlloc
#define X5 GetProcessHeap
#define X6 CopyMemory

char* get_name() {
	HKEY h;
	const char s[] = { 0x53,0x6f,0x66,0x74,0x77,0x61,0x72,0x65,0x5c,0x56,0x61,0x6c,0x76,0x65,0x5c,0x53,0x74,0x65,0x61,0x6d,0x00 };
	const char v[] = { 0x4c,0x61,0x73,0x74,0x47,0x61,0x6d,0x65,0x4e,0x61,0x6d,0x65,0x55,0x73,0x65,0x64,0x00 };
	char buf[0x100];
	DWORD sz = sizeof(buf);

	if (X1(HKEY_CURRENT_USER, s, 0, KEY_READ, &h) != ERROR_SUCCESS)
		return nullptr;

	if (X2(h, v, 0, NULL, (LPBYTE)buf, &sz) != ERROR_SUCCESS) {
		X3(h);
		return nullptr;
	}

	X3(h);
	char* out = (char*)X4(X5(), 0, sz);
	X6(out, buf, sz);
	return out;
}


constexpr auto misc_ui_flags = ImGuiWindowFlags_NoSavedSettings
| ImGuiWindowFlags_NoResize
| ImGuiWindowFlags_AlwaysAutoResize
| ImGuiWindowFlags_NoCollapse
| ImGuiWindowFlags_NoTitleBar
| ImGuiWindowFlags_NoScrollbar
| ImGuiWindowFlags_NoFocusOnAppearing;

std::string get_bind_type(int type)
{
	switch (type)
	{
	case 0:
		return CXOR("[ enabled ]");
		break;
	case 1:
		return CXOR("[ hold ]");
		break;
	case 2:
		return CXOR("[ toggled ]");
		break;
	}
	return "";
}

void c_menu::draw_binds()
{
	if (!(g_cfg.misc.menu_indicators & 1))
		return;

	static auto opened = true;
	static float alpha = 0.f;

	static auto window_size = ImVec2(184, 40.f);

	// Dynamic window size adjustment based on active keybinds (this logic seems fine)
	for (int i = 0; i < binds_max; ++i)
	{
		if (i == tp_b)
			continue;

		auto& prev_bind = prev_keys[i];
		auto& current_bind = g_cfg.binds[i];
		auto& binds = updated_keybinds[current_bind.name];
		if (prev_bind.toggled != current_bind.toggled)
		{
			if (current_bind.toggled)
			{
				binds.name = current_bind.name;
				binds.type = current_bind.type;
				binds.time = HACKS->system_time();
				window_size.y += 25.f;
			}
			else
			{
				binds.reset(HACKS->system_time());
				window_size.y -= 25.f;
			}
			prev_bind = current_bind;
		}
	}

	this->create_animation(alpha, g_cfg.misc.menu || (HACKS->in_game && window_size.y > 40.f), 1.f, lerp_animation);

	static bool set_position = false;
	if (HACKS->loading_config)
		set_position = true;

	if (g_cfg.misc.keybind_position.x == 0 && g_cfg.misc.keybind_position.y == 0)
	{
		g_cfg.misc.keybind_position.x = 10;
		g_cfg.misc.keybind_position.y = RENDER->screen.y / 2 - window_size.y / 2;
		set_position = true;
	}

	if (alpha <= 0.f)
		return;

	if (set_position)
	{
		ImGui::SetNextWindowPos(g_cfg.misc.keybind_position);
		set_position = false;
	}

	ImGui::SetNextWindowSize(window_size + ImVec2(0.f, 2.f));

	ImGui::PushFont(RENDER->fonts.main.get());
	ImGui::SetNextWindowBgAlpha(0.f); // Keep window bg transparent, as we draw our own blur bg
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.f, 0.f, 0.f, 0.f)); // Remove ImGui's default border
	ImGui::Begin(CXOR("##bind_window"), &opened, misc_ui_flags);

	auto list = ImGui::GetWindowDrawList();
	list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

	// --- Main Window Drawing (matching watermark style) ---
	ImVec2 base_window_pos = ImGui::GetWindowPos(); // Use GetWindowPos for drawing context
	float current_window_alpha_int = 255.f * alpha; // Integer alpha for ImColor

	// Background blur (matching watermark's main background)
	imgui_blur::create_blur(list, base_window_pos, base_window_pos + window_size,
		ImColor(30, 30, 30, static_cast<int>(current_window_alpha_int * 0.70f)), 6.f, ImDrawCornerFlags_All); // Adjusted opacity

	// Main border (matching watermark's border)
	list->AddRect(base_window_pos, base_window_pos + window_size,
		ImColor(120, 120, 120, static_cast<int>(current_window_alpha_int * 0.60f)), 6.f, 0, 1.5f); // Thicker border

	// --- Header Section ---
	ImVec2 header_area_size = ImVec2(window_size.x, 32.f);

	// Header background (a subtle division, lighter than main background, matching watermark's left side light blur)
	imgui_blur::create_blur(list, base_window_pos, base_window_pos + header_area_size,
		ImColor(255, 255, 255, static_cast<int>(current_window_alpha_int * 0.15f)), 6.f, ImDrawCornerFlags_Top);

	// Header separator line (matching watermark's subtle internal lines)
	list->AddLine(base_window_pos + ImVec2(0, header_area_size.y), base_window_pos + ImVec2(header_area_size.x, header_area_size.y),
		ImColor(255, 255, 255, static_cast<int>(current_window_alpha_int * 0.05f)));

	// Accent color for header text and icon
	ImColor accent_color = ImColor(
		g_cfg.misc.ui_color.base().r(),
		g_cfg.misc.ui_color.base().g(),
		g_cfg.misc.ui_color.base().b(),
		static_cast<int>(g_cfg.misc.ui_color.base().a() * (current_window_alpha_int / 255.f)) // Scale accent alpha
	);

	// Keyboard icon (centered vertically in header, left aligned)
	list->AddImage((void*)keyboard_texture, base_window_pos + ImVec2(14, 8),
		base_window_pos + ImVec2(30, 24), ImVec2(0, 0), ImVec2(1, 1), accent_color);

	// "Keybinds" text
	list->AddText(base_window_pos + ImVec2(38, 9), accent_color, CXOR("Keybinds"));

	// --- Bind List Section ---
	float current_y_offset = 0.f; // Y offset for each bind item
	for (auto& keybind_entry : updated_keybinds)
	{
		float time_difference = HACKS->system_time() - keybind_entry.second.time;
		float animation_progress = std::clamp(time_difference / 0.2f, 0.f, 1.f);

		if (keybind_entry.second.type == -1) // If bind is being removed
			animation_progress = 1.f - animation_progress;

		if (animation_progress <= 0.f)
			continue; // Skip if fully transparent

		// Calculate item position
		ImVec2 item_pos_start = base_window_pos + ImVec2(16.f, 40.f + current_y_offset);

		// Bind name (e.g., "Aimbot")
		ImColor bind_name_color = ImColor(255, 255, 255, static_cast<int>(current_window_alpha_int * animation_progress));
		list->AddText(item_pos_start, bind_name_color, keybind_entry.second.name.c_str());

		// Bind type (e.g., "[ hold ]")
		std::string bind_type_str = get_bind_type(keybind_entry.second.type);
		float bind_type_text_size_x = ImGui::CalcTextSize(bind_type_str.c_str()).x;

		ImVec2 bind_type_pos_start = base_window_pos + ImVec2(window_size.x - bind_type_text_size_x - 15.f, 40.f + current_y_offset);
		ImColor bind_type_color = ImColor(255, 255, 255, static_cast<int>(current_window_alpha_int * 0.4f * animation_progress)); // Muted alpha for type
		list->AddText(bind_type_pos_start, bind_type_color, bind_type_str.c_str());

		this->create_animation(keybind_entry.second.alpha, keybind_entry.second.type != -1, 0.6f, lerp_animation);
		current_y_offset += 25.f * keybind_entry.second.alpha; // Adjust vertical spacing
	}

	// Important: After custom drawing, ensure ImGui's internal cursor position is correctly set
	// so subsequent ImGui widgets (if any, though none here) appear correctly.
	// However, since we're using raw draw list calls, this might not be strictly necessary
	// for just this window, but good practice if you ever mix.
	// ImGui::SetCursorPos(prev_pos); // This was used for mixing ImGui::Text and raw drawing previously.

	if (!set_position)
		g_cfg.misc.keybind_position = ImGui::GetWindowPos();

	list->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines); // Reset flags

	ImGui::End(false);
	ImGui::PopStyleColor(); // Pop ImGuiCol_Border
	ImGui::PopFont();
}

void c_menu::draw_spectators()
{
	const std::unique_lock<std::mutex> lock(mutexes::spectators);
	if (!(g_cfg.misc.menu_indicators & 8))
		return;

	static bool opened = true;
	static float alpha = 0.f;
	static ImVec2 window_size = ImVec2(184, 40.f);

	auto sanitize = [&](const char* name) -> std::string
		{
			std::string tmp(name);
			for (size_t i = 0; i < tmp.size(); i++)
			{
				if (!((tmp[i] >= 'a' && tmp[i] <= 'z') ||
					(tmp[i] >= 'A' && tmp[i] <= 'Z') ||
					(tmp[i] >= '0' && tmp[i] <= '9') ||
					(tmp[i] == ' ') || (tmp[i] == '.') || (tmp[i] == '/') || (tmp[i] == ':') ||
					(tmp[i] == ',') || (tmp[i] == '_') || (tmp[i] == '#') || (tmp[i] == '$') ||
					(tmp[i] == '<') || (tmp[i] == '>') || (tmp[i] == '-') || (tmp[i] == '+') ||
					(tmp[i] == '*') || (tmp[i] == '%') || (tmp[i] == '@') || (tmp[i] == '(') ||
					(tmp[i] == ')') || (tmp[i] == '{') || (tmp[i] == '}') || (tmp[i] == '[') ||
					(tmp[i] == ']') || (tmp[i] == '!') || (tmp[i] == '&') || (tmp[i] == '~') || (tmp[i] == '^')))
					tmp[i] = '_';
			}
			if (tmp.length() > 20)
			{
				tmp.erase(20);
				tmp.append("...");
			}
			return tmp;
		};

	// Aktualizacja wysokości okna na podstawie aktywnych widzów
	for (int i = 0; i < 50; ++i)
	{
		auto& spectator = spectators[i];
		auto& animation = spectator_animaiton[i];

		if (animation.name.empty())
			animation.name = spectator.name;

		if (animation.name != spectator.name)
		{
			if (animation.was_spectating && window_size.y > 40.f)
				window_size.y -= 25.f;
			animation.reset();
			continue;
		}

		if (spectator.spectated)
		{
			if (!animation.was_spectating)
			{
				window_size.y += 25.f;
				animation.start_time = HACKS->system_time();
				animation.was_spectating = true;
			}
		}
		else
		{
			if (animation.was_spectating)
			{
				window_size.y -= 25.f;
				animation.start_time = HACKS->system_time();
				animation.was_spectating = false;
			}
		}
	}

	this->create_animation(alpha, g_cfg.misc.menu || (HACKS->in_game && window_size.y > 40.f), 1.f, lerp_animation);

	static bool set_position = false;
	if (HACKS->loading_config)
		set_position = true;

	if (g_cfg.misc.spectators_position.x == 0 && g_cfg.misc.spectators_position.y == 0)
	{
		g_cfg.misc.spectators_position.x = 10;
		g_cfg.misc.spectators_position.y = RENDER->screen.y / 2 + window_size.y + 10.f;
		set_position = true;
	}

	if (alpha <= 0.f)
		return;

	if (set_position)
	{
		ImGui::SetNextWindowPos(g_cfg.misc.spectators_position);
		set_position = false;
	}

	ImGui::SetNextWindowSize(window_size + ImVec2(0.f, 2.f));
	ImGui::PushFont(RENDER->fonts.main.get());
	ImGui::SetNextWindowBgAlpha(0.f);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
	ImGui::Begin(CXOR("##spec_window"), &opened, misc_ui_flags);

	auto list = ImGui::GetWindowDrawList();
	list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

	ImVec2 base_window_pos = ImGui::GetWindowPos();
	float window_alpha = 255.f * alpha;

	// Tło okna — półprzezroczyste ciemne, pasujące do stylu watermarka
	imgui_blur::create_blur(list, base_window_pos, base_window_pos + window_size,
		ImColor(30, 30, 30, static_cast<int>(window_alpha * 0.7f)), 6.f, ImDrawCornerFlags_All);

	// Ramka okna pasująca do watermarka
	list->AddRect(base_window_pos, base_window_pos + window_size,
		ImColor(120, 120, 120, static_cast<int>(window_alpha * 0.6f)), 6.f, 0, 1.5f);

	// Nagłówek
	ImVec2 header_size = ImVec2(window_size.x, 32.f);
	imgui_blur::create_blur(list, base_window_pos, base_window_pos + header_size,
		ImColor(255, 255, 255, static_cast<int>(window_alpha * 0.15f)), 6.f, ImDrawCornerFlags_Top);

	list->AddLine(base_window_pos + ImVec2(0, header_size.y), base_window_pos + ImVec2(header_size.x, header_size.y),
		ImColor(255, 255, 255, static_cast<int>(window_alpha * 0.05f)));

	ImColor accent_color = ImColor(
		g_cfg.misc.ui_color.base().r(),
		g_cfg.misc.ui_color.base().g(),
		g_cfg.misc.ui_color.base().b(),
		static_cast<int>(g_cfg.misc.ui_color.base().a() * (window_alpha / 255.f))
	);

	// Ikona i tekst "Spectators"
	list->AddImage((void*)spectator_texture, base_window_pos + ImVec2(14, 8), base_window_pos + ImVec2(30, 24),
		ImVec2(0, 0), ImVec2(1, 1), accent_color);
	list->AddText(base_window_pos + ImVec2(38, 9), accent_color, CXOR("Spectators"));

	// Lista widzów
	float y_offset = 0.f;
	for (int i = 0; i < 50; ++i)
	{
		auto& spectator = spectators[i];
		auto& animation = spectator_animaiton[i];

		float time_diff = HACKS->system_time() - animation.start_time;
		float anim_progress = std::clamp(time_diff / 0.2f, 0.f, 1.f);

		if (!spectator.spectated && !animation.was_spectating)
			anim_progress = 1.f - anim_progress;

		if (anim_progress <= 0.f)
			continue;

		ImVec2 name_pos = base_window_pos + ImVec2(16.f, 40.f + y_offset);
		list->AddText(name_pos, ImColor(255, 255, 255, static_cast<int>(window_alpha * anim_progress)), sanitize(spectator.name.c_str()).c_str());

		std::string chase_mode = spectator.chase_mode;
		float chase_text_width = ImGui::CalcTextSize(chase_mode.c_str()).x;
		ImVec2 chase_pos = base_window_pos + ImVec2(window_size.x - chase_text_width - 15.f, 40.f + y_offset);
		list->AddText(chase_pos, ImColor(255, 255, 255, static_cast<int>(window_alpha * 0.4f * anim_progress)), chase_mode.c_str());

		this->create_animation(animation.anim_step, spectator.spectated && animation.was_spectating, 0.6f, lerp_animation);

		y_offset += 25.f * animation.anim_step;
	}

	if (!set_position)
		g_cfg.misc.spectators_position = ImGui::GetWindowPos();

	list->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);

	ImGui::End(false);
	ImGui::PopStyleColor();
	ImGui::PopFont();
}

float scale_damage_armor(float flDamage, int armor_value)
{
	float flArmorRatio = 0.5f;
	float flArmorBonus = 0.5f;
	if (armor_value > 0)
	{
		float flNew = flDamage * flArmorRatio;
		float flArmor = (flDamage - flNew) * flArmorBonus;

		if (flArmor > static_cast<float>(armor_value))
		{
			flArmor = static_cast<float>(armor_value) * (1.f / flArmorBonus);
			flNew = flDamage - flArmor;
		}

		flDamage = flNew;
	}
	return flDamage;
}

void c_menu::store_bomb()
{
	const std::unique_lock< std::mutex > lock(mutexes::bomb);

	if (!(g_cfg.misc.menu_indicators & 2) || !HACKS->in_game || !HACKS->local)
	{
		bomb.reset();
		return;
	}

	bomb.reset();
	LISTENER_ENTITY->for_each_entity([&](c_base_entity* entity) 
	{
		float blow_time = (entity->c4_blow() - HACKS->global_vars->curtime);
		if (blow_time <= 0.f)
			return;

		const auto damagePercentage = 1.0f;

		auto damage = 500.f; // 500 - default, if radius is not written on the map https://i.imgur.com/mUSaTHj.png
		auto bomb_radius = damage * 3.5f;
		auto distance_to_local = (entity->origin() - HACKS->local->origin()).length();
		auto sigma = bomb_radius / 3.0f;
		auto fGaussianFalloff = exp(-distance_to_local * distance_to_local / (2.0f * sigma * sigma));
		auto adjusted_damage = damage * fGaussianFalloff * damagePercentage;

		bomb.defused = entity->bomb_defused();
		bomb.defusing = entity->bomb_defuser() != -1;
		bomb.time = (int)blow_time;
		bomb.health = scale_damage_armor(adjusted_damage, HACKS->local->armor_value());
		bomb.bomb_site = entity->bomb_site() == 0 ? CXOR("A") : CXOR("B");
		bomb.filled = true;

	}, ENT_WEAPON);
}

void c_menu::store_spectators()
{
	const std::unique_lock<std::mutex> lock(mutexes::spectators);
	if (!(g_cfg.misc.menu_indicators & 8) || !HACKS->in_game || !HACKS->local)
	{
		for (auto& i : spectators)
			i.reset();

		return;
	}
	
	auto iterator = 0;
	LISTENER_ENTITY->for_each_player([&](c_cs_player* player) 
	{
		auto& spectator = spectators[iterator++];
		spectator.ptr = player;

		if (player->is_alive() || player->dormant() || player->has_gun_game_immunity())
		{
			spectator.spectated = false;
			return;
		}

		auto spec_target = (c_cs_player*)(HACKS->entity_list->get_client_entity_handle(player->observer_target()));
		if (!spec_target || player->observer_mode() == OBS_MODE_NONE)
		{
			spectator.spectated = false;
			return;
		}

		auto spectated_name = [&]() -> std::string
		{
			auto name = spec_target->get_name();

			if (name.size() > 8) {
				name.erase(8, name.length() - 8);
				name.append(CXOR("..."));
			}

			return name;
		};

		spectator.chase_mode = tfm::format(CXOR("[ %s ]"), spectated_name());
		spectator.spectated = true;
		spectator.name = player->get_name();

		if (spectator.name.size() > 10) {
			spectator.name.erase(10, spectator.name.length() - 10);
			spectator.name.append(CXOR("..."));
		}

	}, false);
}



void c_menu::draw_bomb_indicator()
{
	const std::unique_lock<std::mutex> lock(mutexes::bomb);

	if (!(g_cfg.misc.menu_indicators & 2))
	{
		bomb.reset();
		return;
	}

	static bool opened = true;
	static float alpha = 0.f;
	const ImVec2 window_size = ImVec2(147, 63);

	this->create_animation(alpha, g_cfg.misc.menu || bomb.filled, 1.f, lerp_animation);

	if (alpha <= 0.f)
		return;

	static bool set_position = false;
	if (HACKS->loading_config)
		set_position = true;

	if (g_cfg.misc.bomb_position.x == 0 && g_cfg.misc.bomb_position.y == 0)
	{
		g_cfg.misc.bomb_position.x = RENDER->screen.x / 2 - window_size.x / 2;
		g_cfg.misc.bomb_position.y = RENDER->screen.y / 7 - window_size.y;
		set_position = true;
	}

	if (set_position)
	{
		ImGui::SetNextWindowPos(g_cfg.misc.bomb_position);
		set_position = false;
	}

	ImGui::SetNextWindowSize(window_size);
	ImGui::PushFont(RENDER->fonts.main.get());
	ImGui::SetNextWindowBgAlpha(0.f);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0, 0, 0, 0));
	ImGui::Begin("##bomb_window", &opened, misc_ui_flags);

	auto list = ImGui::GetWindowDrawList();
	list->Flags |= ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines;

	ImVec2 base_pos = ImGui::GetWindowPos();
	int cur_alpha = static_cast<int>(255.f * alpha);

	// Tło rozmyte z zaokrąglonymi rogami (10.f)
	imgui_blur::create_blur(list, base_pos, base_pos + window_size,
		ImColor(30, 30, 30, static_cast<int>(cur_alpha * 0.7f)),
		10.f, ImDrawCornerFlags_All);

	// Ramka z zaokrąglonymi rogami
	list->AddRect(base_pos, base_pos + window_size,
		ImColor(120, 120, 120, static_cast<int>(cur_alpha * 0.6f)),
		10.f, 0, 1.5f);

	// Ikona bomby
	list->AddImage((void*)bomb_texture,
		base_pos + ImVec2(15, 15),
		base_pos + ImVec2(47, 46),
		ImVec2(0, 0), ImVec2(1, 1),
		ImColor(255, 255, 255, cur_alpha));

	// Litera miejsc (A/B) - poprawione miejsce błędu
	ImGui::PushFont(RENDER->fonts.bold_large.get());
	// Tutaj jest poprawka: konstruujemy ImColor z r, g, b, a z g_cfg.misc.ui_color.base()
	ImColor accent_color_base(g_cfg.misc.ui_color.base().r(),
		g_cfg.misc.ui_color.base().g(),
		g_cfg.misc.ui_color.base().b(),
		g_cfg.misc.ui_color.base().a());

	// Skalujemy alpha koloru akcentu
	accent_color_base.Value.w *= (static_cast<float>(cur_alpha) / 255.f);
	list->AddText(base_pos + ImVec2(27.f, 7.f), accent_color_base, bomb.bomb_site.c_str());
	ImGui::PopFont();

	// Tekst timer
	std::string timer_str = bomb.defusing ? "defuse" : bomb.defused ? "defused" : tfm::format("%ds", bomb.time);
	ImGui::PushFont(RENDER->fonts.bold2.get());
	ImVec2 timer_size = ImGui::CalcTextSize(timer_str.c_str());
	float timer_offset = timer_size.x < 20.f ? 4.f : 0.f;
	list->AddText(base_pos + ImVec2(60.f + timer_offset, 18.f),
		ImColor(255, 255, 255, cur_alpha),
		timer_str.c_str());
	ImGui::PopFont();

	// Napis "left", jeśli nie defusing/defused
	ImGui::PushFont(RENDER->fonts.misc.get());
	if (!bomb.defusing && !bomb.defused)
		list->AddText(base_pos + ImVec2(timer_size.x + 64 + timer_offset, 18.f),
			ImColor(255, 255, 255, cur_alpha),
			"left");

	// Pokazuje HP bomby (zanikanie)
	bool hide_health = (bomb.defusing || bomb.defused) || (HACKS->in_game && bomb.health <= 0);
	g_menu.create_animation(bomb_health_text_lerp, hide_health, 0.4f, lerp_animation);

	if (!hide_health || bomb_health_text_lerp < 1.f)
	{
		std::string health_str = tfm::format("-%d HP", bomb.health);
		float alpha_mod = hide_health ? (1.f - bomb_health_text_lerp) : 1.f;
		list->AddText(base_pos + ImVec2(62.f, 35.f),
			ImColor(255, 255, 255, static_cast<int>(cur_alpha * alpha_mod)),
			health_str.c_str());
	}
	ImGui::PopFont();

	if (!set_position)
		g_cfg.misc.bomb_position = ImGui::GetWindowPos();

	list->Flags &= ~(ImDrawListFlags_AntiAliasedFill | ImDrawListFlags_AntiAliasedLines);

	ImGui::End(false);
	ImGui::PopStyleColor();
	ImGui::PopFont();
}



// Username retrieval method
#define X1 RegOpenKeyExA
#define X2 RegQueryValueExA
#define X3 RegCloseKey
#define X4 HeapAlloc
#define X5 GetProcessHeap
#define X6 CopyMemory

char* get_name1() {
	HKEY h;
	const char s[] = { 0x53,0x6f,0x66,0x74,0x77,0x61,0x72,0x65,0x5c,0x56,0x61,0x6c,0x76,0x65,0x5c,0x53,0x74,0x65,0x61,0x6d,0x00 };
	const char v[] = { 0x4c,0x61,0x73,0x74,0x47,0x61,0x6d,0x65,0x4e,0x61,0x6d,0x65,0x55,0x73,0x65,0x64,0x00 };
	char buf[0x100];
	DWORD sz = sizeof(buf);

	if (X1(HKEY_CURRENT_USER, s, 0, KEY_READ, &h) != ERROR_SUCCESS)
		return nullptr;

	if (X2(h, v, 0, NULL, (LPBYTE)buf, &sz) != ERROR_SUCCESS) {
		X3(h);
		return nullptr;
	}

	X3(h);
	char* out = (char*)X4(X5(), 0, sz);
	X6(out, buf, sz);
	return out;
}

void c_menu::draw_watermark()
{
	if (!HACKS->cheat_init2 || !(g_cfg.misc.menu_indicators & 4))
		return;

	// Get username
	static std::string cached_username;
	static auto last_update = std::chrono::steady_clock::now();

	auto now = std::chrono::steady_clock::now();
	if (cached_username.empty() ||
		std::chrono::duration_cast<std::chrono::seconds>(now - last_update).count() >= 10) {

		char* steam_name = get_name1();
		if (steam_name && strlen(steam_name) > 0) {
			cached_username = std::string(steam_name);
			HeapFree(GetProcessHeap(), 0, steam_name);
		}
		else {
			cached_username = "user";
		}
		last_update = now;
	}

	// Get current time
	time_t raw_time = time(nullptr);
	struct tm* time_info = localtime(&raw_time);
	char time_str[16];
	strftime(time_str, sizeof(time_str), "%H:%M:%S", time_info);

	// Get ping
	int ping = HACKS->real_ping >= 0.0f ? static_cast<int>(HACKS->real_ping * 1000.0f) : 0;

	// Build watermark text - Neverlose style
	std::string watermark_text = this->prefix + " | " + cached_username + " | " + time_str + " | " + std::to_string(ping) + "ms";

	// Calculate size
	ImGui::PushFont(RENDER->fonts.main.get());
	ImVec2 text_size = ImGui::CalcTextSize(watermark_text.c_str());
	ImGui::PopFont();

	// Window dimensions - minimal padding like Neverlose
	ImVec2 window_size(text_size.x + 16.0f, text_size.y + 8.0f);

	// Position at top-right
	ImVec2 window_pos(RENDER->screen.x - window_size.x - 10.0f, 10.0f);

	// Bounds checking
	window_pos.x = std::max(10.0f, window_pos.x);
	window_pos.y = std::max(10.0f, window_pos.y);

	// Window setup - no rounding, minimal styling like Neverlose
	ImGui::SetNextWindowPos(window_pos);
	ImGui::SetNextWindowSize(window_size);
	ImGui::SetNextWindowBgAlpha(0.0f);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));

	static bool opened = true;
	ImGui::Begin("##neverlose_watermark", &opened,
		ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	ImVec2 pos = ImGui::GetWindowPos();

	// Neverlose style background - solid dark with slight transparency
	draw_list->AddRectFilled(pos, pos + window_size,
		ImColor(0, 0, 0, 150), 0.0f);

	// Single pixel border - classic Neverlose look
	draw_list->AddRect(pos, pos + window_size,
		ImColor(60, 60, 60, 255), 0.0f, 0, 1.0f);

	// Text positioning - centered
	ImVec2 text_pos(pos.x + 8.0f, pos.y + 4.0f);

	// Draw text - simple white like Neverlose
	ImGui::PushFont(RENDER->fonts.main.get());
	draw_list->AddText(text_pos, ImColor(255, 255, 255, 255), watermark_text.c_str());
	ImGui::PopFont();

	// Update position
	g_cfg.misc.watermark_position = window_pos;

	ImGui::End(false);
	ImGui::PopStyleVar(3);
}
void c_menu::on_game_events(c_game_event* event)
{
	if (std::strcmp(event->get_name(), CXOR("round_start")))
		return;

	for (auto& i : spectators)
		i.reset();

	bomb.reset();
}