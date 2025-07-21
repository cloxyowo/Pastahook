#include "globals.hpp"
#include "resolver.hpp"
#include "animations.hpp"
#include "server_bones.hpp"
#include "ragebot.hpp"
#include "penetration.hpp"

//caca resolver from osiris paste

namespace resolver
{
	inline void prepare_jitter(c_cs_player* player, resolver_info_t& resolver_info, anim_record_t* current)
	{
		auto& jitter = resolver_info.jitter;
		jitter.yaw_cache[jitter.yaw_cache_offset % YAW_CACHE_SIZE] = current->eye_angles.y;

		if (jitter.yaw_cache_offset >= YAW_CACHE_SIZE + 1)
			jitter.yaw_cache_offset = 0;
		else
			jitter.yaw_cache_offset++;

		for (int i = 0; i < YAW_CACHE_SIZE - 1; ++i)
		{
			float diff = std::fabsf(jitter.yaw_cache[i] - jitter.yaw_cache[i + 1]);
			if (diff <= 0.f)
			{
				if (jitter.static_ticks < 3)
					jitter.static_ticks++;
				else
					jitter.jitter_ticks = 0;
			}
			else if (diff >= 10.f)
			{
				if (jitter.jitter_ticks < 3)
					jitter.jitter_ticks++;
				else
					jitter.static_ticks = 0;
			}
		}

		jitter.is_jitter = jitter.jitter_ticks > jitter.static_ticks;
	}

	inline vec3_t get_point_direction(c_cs_player* player)
	{
		vec3_t fw{};

		float at_target_yaw = math::calc_angle(HACKS->local->origin(), player->origin()).y;
		math::calc_angle(vec3_t(0, at_target_yaw - 90.f, 0), fw);

		return fw;
	}

	inline void prepare_freestanding(c_cs_player* player)
	{
		auto& info = resolver_info[player->index()];
		auto& jitter = info.jitter;
		auto& freestanding = info.freestanding;

		auto layers = player->animlayers();

		if (!layers || !HACKS->weapon_info || !HACKS->local || !HACKS->local->is_alive() || player->is_bot() || !g_cfg.rage.resolver)
		{
			if (freestanding.updated)
				freestanding.reset();

			return;
		}

		auto weight = layers[6].weight;
		if (jitter.is_jitter || weight > 0.75f)
		{
			if (freestanding.updated)
				freestanding.reset();

			return;
		}

		auto& cache = player->bone_cache();
		if (!cache.count() || !cache.base())
			return;

		freestanding.updated = true;

		bool inverse_side{};
		{
			float at_target = math::normalize_yaw(math::calc_angle(HACKS->local->origin(), player->get_abs_origin()).y);
			float angle = math::normalize_yaw(player->eye_angles().y);

			const bool sideways_left = std::abs(math::normalize_yaw(angle - math::normalize_yaw(at_target - 90.f))) < 45.f;
			const bool sideways_right = std::abs(math::normalize_yaw(angle - math::normalize_yaw(at_target + 90.f))) < 45.f;

			bool forward = std::abs(math::normalize_yaw(angle - math::normalize_yaw(at_target + 180.f))) < 45.f;
			inverse_side = forward && !(sideways_left || sideways_right);
		}

		auto direction = get_point_direction(player);

		static matrix3x4_t predicted_matrix[128]{};
		std::memcpy(predicted_matrix, cache.base(), sizeof(predicted_matrix));

		auto store_changed_matrix_data = [&](const vec3_t& new_position, bullet_t& out)
			{
				auto old_abs_origin = player->get_abs_origin();

				math::change_bones_position(predicted_matrix, 128, player->origin(), new_position);
				{
					static matrix3x4_t old_cache[128]{};
					player->store_bone_cache(old_cache);
					{
						player->set_abs_origin(new_position);
						player->set_bone_cache(predicted_matrix);

						auto head_pos = cache.base()[8].get_origin();
						out = penetration::simulate(HACKS->local, player, ANIMFIX->get_local_anims()->eye_pos, head_pos);
					}
					player->set_bone_cache(old_cache);
				}
				math::change_bones_position(predicted_matrix, 128, new_position, player->origin());
			};

		{

			bullet_t left{}, right{};

			auto left_dir = inverse_side ? (player->origin() + direction * 40.f) : (player->origin() - direction * 40.f);
			store_changed_matrix_data(left_dir, left);

			auto right_dir = inverse_side ? (player->origin() - direction * 40.f) : (player->origin() + direction * 40.f);
			store_changed_matrix_data(right_dir, right);

			if (left.damage > right.damage)
				freestanding.side = 1;
			else if (left.damage < right.damage)
				freestanding.side = -1;

			if (freestanding.side)
				freestanding.updated = true;
		}
	}

	inline void prepare_side(c_cs_player* player, anim_record_t* current, anim_record_t* last)
	{
		auto& info = resolver_info[player->index()];
		if (!HACKS->weapon_info || !HACKS->local || !HACKS->local->is_alive() || player->is_bot() || !g_cfg.rage.resolver)
		{
			if (info.resolved)
				info.reset();

			return;
		}

		auto state = player->animstate();
		if (!state)
		{
			if (info.resolved)
				info.reset();

			return;
		}

		auto hdr = player->get_studio_hdr();
		if (!hdr)
			return;

		if (current->shooting) {

			if (player->pose_parameter()[11] > 0.85f)
				info.side = 1;

			else if (player->pose_parameter()[11] < 0.15f)
				info.side = -1;
			else
				info.side = 0;

			info.mode = XOR("Onshot");
			info.resolved = true;

			return;
		}

		if (current->choke < 2)
			info.add_legit_ticks();
		else
			info.add_fake_ticks();

		if (info.is_legit())
		{
			info.resolved = false;
			info.mode = XOR("no fake");
			return;
		}

		prepare_jitter(player, info, current);
		prepare_freestanding(player);
		auto& jitter = info.jitter;
		auto& freestanding = info.freestanding;
		float vel_sqr = player->abs_velocity().length_sqr();
		if (jitter.is_jitter && (vel_sqr < 256.f || current->layers[ANIMATION_LAYER_MOVEMENT_MOVE].weight <= 0.f))
		{
			const auto eye_angles = player->eye_angles();
			auto v73 = fmodf(eye_angles.y, 360.0f);
			auto srcAngle = v73;
			if (v73 > 180.0f)
			{
				v73 = v73 - 360.0f;
				srcAngle = v73;
			}
			if (v73 < -180.0f)
				srcAngle = v73 + 360.0f;
			const auto lby = player->lower_body_yaw();
			auto destAngle = fmodf(lby, 360.0f);
			auto destAngle1 = destAngle;
			if (destAngle > 180.0f)
			{
				destAngle = destAngle - 360.0f;
				destAngle1 = destAngle;
			}
			if (destAngle < -180.0f)
			{
				destAngle = destAngle + 360.0f;
				destAngle1 = destAngle;
			}
			const auto delta = fmodf(destAngle - srcAngle, 360.0f);
			auto angle_1 = delta;
			if (destAngle1 <= srcAngle)
			{
				if (delta <= -180.0f)
					angle_1 = delta + 360.0f;
			}
			else if (delta >= 180.0f)
			{
				angle_1 = delta - 360.0f;
			}
			auto angle = fmodf(angle_1, 360.0f);
			if (angle > 180.0f)
				angle = angle - 360.0f;
			if (angle < -180.0f)
				angle = angle + 360.0f;

			auto final_side = (2 * (angle <= 0.0f) + 1);
			std::clamp(final_side, -1, 1);


			info.side = final_side;
			info.resolved = true;
			info.mode = XOR("jitter");
		}
		else
		{
			if (freestanding.updated)
			{
				info.resolved = true;
				info.mode = XOR("freestanding");
				info.side = freestanding.side;
			}
			else {
				auto& misses = RAGEBOT->missed_shots[player->index()];
				if (misses > 0)
				{
					switch (misses % 3)
					{
					case 1:
						info.side = -1;
						break;
					case 2:
						info.side = 1;
						break;
					case 0:
						info.side = 0;
						break;
					}

					info.resolved = true;
					info.mode = XOR("brute");
				}
				else
				{
					info.side = 0;
					info.mode = XOR("static");

					info.resolved = true;
				}
			}
		}
	}

	inline void apply_side(c_cs_player* player, anim_record_t* current, int choke)
	{
		auto& info = resolver_info[player->index()];
		if (!HACKS->weapon_info || !HACKS->local || !HACKS->local->is_alive() || !info.resolved || info.side == 1337 || player->is_teammate(false))
			return;

		auto state = player->animstate();
		if (!state)
			return;

		float desync_angle = (choke < 2) ? state->get_max_rotation() : 120.f;
		state->abs_yaw = math::normalize_yaw(player->eye_angles().y + desync_angle * info.side);

	}
}