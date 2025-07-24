#include "globals.hpp"
#include "event_logs.hpp"

void c_event_logs::on_item_purchase(c_game_event* event)
{
	if (std::strcmp(event->get_name(), CXOR("item_purchase")) || !HACKS->local)
		return;

	if (!(g_cfg.visuals.eventlog.logs & 8))
		return;

	auto userid = event->get_int(CXOR("userid"));
	if (!userid)
		return;

	auto user_id = HACKS->engine->get_player_for_user_id(userid);
	auto player = (c_cs_player*)HACKS->entity_list->get_client_entity(user_id);
	if (!player)
		return;

	if (player->is_teammate())
		return;

	push_message(tfm::format(CXOR("%s bought %s"), player->get_name(), event->get_string(CXOR("weapon"))));
}

void c_event_logs::on_bomb_plant(c_game_event* event)
{
	const char* event_name = event->get_name();
	if (!(g_cfg.visuals.eventlog.logs & 16))
		return;

	auto userid = event->get_int(CXOR("userid"));
	if (!userid)
		return;

	auto user_id = HACKS->engine->get_player_for_user_id(userid);
	auto player = (c_cs_player*)HACKS->entity_list->get_client_entity(user_id);
	if (!player)
		return;

	if (!std::strcmp(event_name, CXOR("bomb_planted")))
		push_message(tfm::format(CXOR("%s is planted the bomb"), player->get_name()));

	if (!std::strcmp(event_name, CXOR("bomb_begindefuse")))
		push_message(tfm::format(CXOR("%s is defusing the bomb"), player->get_name()));
}

void c_event_logs::on_player_hurt(c_game_event* event)
{
	if (!(g_cfg.visuals.eventlog.logs & 1))
		return;

	if (std::strcmp(event->get_name(), CXOR("player_hurt")) || !HACKS->local)
		return;

	auto attacker = HACKS->engine->get_player_for_user_id(event->get_int(CXOR("attacker")));
	if (HACKS->local->index() != attacker)
		return;

	auto user_id = HACKS->engine->get_player_for_user_id(event->get_int(CXOR("userid")));
	auto player = (c_cs_player*)HACKS->entity_list->get_client_entity(user_id);

	if (!player)
		return;

	if (player->is_teammate())
		return;

	auto group = event->get_int(CXOR("hitgroup"));
	auto dmg_health = event->get_int(CXOR("dmg_health"));
	auto health = event->get_int(CXOR("health"));

	auto string_group = main_utils::hitgroup_to_string(group);

	if (group == HITGROUP_GENERIC || group == HITGROUP_GEAR)
		push_message(tfm::format(CXOR("Hit %s for %d (%d remaining)"), 
			player->get_name().c_str(),
			dmg_health, 
			health));
	else
		push_message(tfm::format(CXOR("Hit %s in the %s for %d (%d remaining)"), 
			player->get_name().c_str(),
			string_group.c_str(), 
			dmg_health, 
			health));
}

void c_event_logs::on_game_events(c_game_event* event)
{
	on_player_hurt(event);
	on_bomb_plant(event);
	on_item_purchase(event);
}

void c_event_logs::filter_console()
{
	HACKS->convars.con_filter_text->fn_change_callbacks.remove_count();
	HACKS->convars.con_filter_enable->fn_change_callbacks.remove_count();

	if (set_console)
	{
		set_console = false;

		HACKS->cvar->find_convar(CXOR("developer"))->set_value(0);
		HACKS->convars.con_filter_enable->set_value(1);
		HACKS->convars.con_filter_text->set_value(CXOR(""));
	}

	auto filter = g_cfg.visuals.eventlog.enable && g_cfg.visuals.eventlog.filter_console;
	if (log_value != filter)
	{
		log_value = filter;

		if (!log_value)
			HACKS->convars.con_filter_text->set_value(CXOR(""));
		else
			HACKS->convars.con_filter_text->set_value(CXOR("IrWL5106TZZKNFPz4P4Gl3pSN?J370f5hi373ZjPg%VOVh6lN"));
	}
}

// rebuilt in-game CConPanel::DrawNotify
void c_event_logs::render_logs()
{
	if (!g_cfg.visuals.eventlog.enable)
		return;

	constexpr auto font_size = 15.f;
	auto render_font = RENDER->fonts.eventlog;
	auto time = HACKS->system_time();
	float anim_speed = 0.18f; // seconds for slide


	float screen_w = RENDER->screen.x;
	float screen_h = RENDER->screen.y;
	float y_base = screen_h - 140.f;
	float x_center = screen_w * 0.5f;


	auto& bold_font = RENDER->fonts.bold;


	while (this->animated_logs.size() > this->event_logs.size())
		this->animated_logs.erase(this->animated_logs.begin());
	for (size_t i = 0; i < this->event_logs.size(); ++i) {
		if (i >= this->animated_logs.size())
			this->animated_logs.push_back({ this->event_logs[i], 0.f });
		else
			this->animated_logs[i].log = this->event_logs[i];
	}

	if (this->animated_logs.size() > 4)
		this->animated_logs.erase(this->animated_logs.begin(), this->animated_logs.end() - 4);


	float offset = 0.f;
	int logs_shown = 0;
	for (int i = 0; i < (int)this->animated_logs.size(); ++i)
	{
		auto& anim_log = this->animated_logs[i];
		auto* event_log = &anim_log.log;

		anim_log.anim_time = 1.f;

		auto time_left = 1.f - std::clamp((time - event_log->life_time) / 5.f, 0.f, 1.f);
		if (time_left <= 0.5f)
		{
			float f = std::clamp(time_left, 0.0f, .5f) / .5f;
			event_log->clr.a() = ((std::uint8_t)(f * 255.0f));
			if (time_left <= 0.f)
			{
				this->event_logs.erase(this->event_logs.begin() + i);
				this->animated_logs.erase(this->animated_logs.begin() + i);
				--i;
				continue;
			}
		}

		std::string prefix = CXOR("Pastahook | ");

		std::string msg = event_log->message;
		ImVec2 prefix_size = RENDER->get_text_size(&bold_font, prefix);
		ImVec2 msg_size = RENDER->get_text_size(&bold_font, msg);
		float full_w = prefix_size.x + msg_size.x;
		float text_h = std::max(prefix_size.y, msg_size.y);
		float x = x_center - full_w * 0.5f;
		float y = y_base + offset;
		float padding_x = 16.f;
		float padding_y = 6.f;
		float bg_x = x - padding_x;
		float bg_y = y - padding_y;
		float bg_w = full_w + padding_x * 2.f;
		float bg_h = text_h + padding_y * 2.f;
		RENDER->filled_rect(bg_x, bg_y, bg_w, bg_h, c_color(24, 24, 28, 200), 8.f, ImDrawCornerFlags_All);

		float rainbow_speed = 0.6f;
		float hue = std::fmod((time * rainbow_speed + logs_shown * 0.18f), 1.0f);
		float r, g, b;
		float ihue = std::floor(hue * 6.0f);
		float f = hue * 6.0f - ihue;
		float q = 1 - f;
		switch (static_cast<int>(ihue) % 6) {
		case 0: r = 1, g = f, b = 0; break;
		case 1: r = q, g = 1, b = 0; break;
		case 2: r = 0, g = 1, b = f; break;
		case 3: r = 0, g = q, b = 1; break;
		case 4: r = f, g = 0, b = 1; break;
		case 5: r = 1, g = 0, b = q; break;
		}
		c_color rainbow_col(int(r * 255), int(g * 255), int(b * 255), 255);

		RENDER->text(x, y, rainbow_col, memory::bits_t(FONT_DROPSHADOW), &bold_font, prefix);

		RENDER->text(x + prefix_size.x, y, c_color(255, 255, 255, 255), memory::bits_t(FONT_DROPSHADOW), &bold_font, msg);

		offset += text_h + 6.f + 12.f;
		++logs_shown;
	}
}