#pragma once

#include "Framework.h"
#include "Settings.h"

// Small drawing helpers shared by the temperature gauge and the needs gauges (render thread)
namespace UI
{
	using namespace ImGuiMCP;

	inline constexpr float kPi = std::numbers::pi_v<float>;

	inline ImVec2 operator+(ImVec2 a, ImVec2 b) { return { a.x + b.x, a.y + b.y }; }
	inline ImVec2 operator-(ImVec2 a, ImVec2 b) { return { a.x - b.x, a.y - b.y }; }
	inline ImVec2 operator*(ImVec2 a, float f) { return { a.x * f, a.y * f }; }

	inline ImVec2 Normalized(ImVec2 a)
	{
		const float len = std::sqrt(a.x * a.x + a.y * a.y);
		return len > 1e-6f ? ImVec2{ a.x / len, a.y / len } : ImVec2{ 0.0f, -1.0f };
	}

	inline float SmoothStep(float x)
	{
		x = std::clamp(x, 0.0f, 1.0f);
		return x * x * (3.0f - 2.0f * x);
	}

	inline float Approach(float a_value, float a_goal, float a_step)
	{
		return a_value < a_goal ? std::min(a_value + a_step, a_goal) : std::max(a_value - a_step, a_goal);
	}

	inline ImU32 ToU32(const RGBA& a_color, float a_alpha)
	{
		const auto channel = [](float v) { return static_cast<ImU32>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
		return channel(a_color[0]) | (channel(a_color[1]) << 8) | (channel(a_color[2]) << 16) |
		       (channel(a_color[3] * a_alpha) << 24);
	}

	inline RGBA Mix(const RGBA& a, const RGBA& b, float t)
	{
		return { std::lerp(a[0], b[0], t), std::lerp(a[1], b[1], t), std::lerp(a[2], b[2], t), std::lerp(a[3], b[3], t) };
	}

	// ImGui fills (and anti-aliases) polygons whose points go clockwise on screen; the other way round its concave
	// fill closes notches (the dart's became a blunt triangle when the arrow sat on the other side of the gauge)
	inline void MakeClockwise(std::span<ImVec2> a_points)
	{
		float area = 0.0f;
		for (std::size_t i = 0; i < a_points.size(); ++i) {
			const auto& a = a_points[i];
			const auto& b = a_points[(i + 1) % a_points.size()];
			area += a.x * b.y - b.x * a.y;
		}
		if (area < 0.0f) {
			std::ranges::reverse(a_points);
		}
	}

	// text with a thin dark outline
	inline void Text(ImDrawList* a_list, float a_size, ImVec2 a_pos, const std::string& a_text, ImU32 a_col, ImU32 a_shadow)
	{
		const auto  font = GetFont();
		const float o = std::max(1.0f, a_size / 14.0f);
		for (const auto& d : { ImVec2{ -o, 0 }, ImVec2{ o, 0 }, ImVec2{ 0, -o }, ImVec2{ 0, o } }) {
			ImDrawListManager::AddText(a_list, font, a_size, a_pos + d, a_shadow, a_text.c_str());
		}
		ImDrawListManager::AddText(a_list, font, a_size, a_pos, a_col, a_text.c_str());
	}

	inline ImVec2 TextSize(float a_size, const std::string& a_text)
	{
		const auto  base = CalcTextSize(a_text.c_str());
		const float scale = a_size / std::max(1.0f, GetFontSize());
		return { base.x * scale, base.y * scale };
	}
}
