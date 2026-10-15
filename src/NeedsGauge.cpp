#include "NeedsGauge.h"

#include "Band.h"
#include "Draw.h"
#include "Lang.h"
#include "Needs.h"
#include "UI.h"

// The hunger, thirst and fatigue gauges, one per need: a ring around an icon, the icon and a bar, or arcs on a circle.
// Bars and arcs are drawn like the temperature gauge (a band and its marker where the need is). The ring is full while
// fed / quenched / rested and empties stage by stage, its color telling the state (fine, penalties, the worst stages,
// a bonus like Well Fed). The icons are drawn shapes: an apple, a water drop, a crescent moon.
namespace UI
{
	namespace
	{
		constexpr float kFadeTime = 0.35f;
		constexpr float kColorTime = 0.4f;  // seconds a color change takes

		struct NeedState
		{
			float fill{ NAN };
			float shown{ 0.0f };
			float comfortableFor{ 0.0f };
			RGBA  color{};
			bool  colored{ false };
		};
		std::array<NeedState, kNeedCount> states;

		// ---- icons, drawn in a box of +-a_half around a_c ----

		template <std::size_t N>
		void FillShape(ImDrawList* a_list, std::array<ImVec2, N>& a_points, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			MakeClockwise(a_points);
			if (a_border > 0.0f) {
				ImDrawListManager::AddPolyline(a_list, a_points.data(), static_cast<int>(N), a_outline, ImDrawFlags_Closed, a_border * 2.0f);
			}
			ImDrawListManager::AddConcavePolyFilled(a_list, a_points.data(), static_cast<int>(N), a_fill);
		}

		// points of a circle (center, radius in icon units) from angle a0 to a1, appended at a_out; without the ends
		// where the arc joins another one (ImGui's fill doesn't like the same point twice)
		template <std::size_t N>
		void CirclePoints(std::array<ImVec2, N>& a_out, std::size_t& a_at, std::size_t a_count, ImVec2 a_center, float a_radius, float a0, float a1, bool a_ends = true)
		{
			for (std::size_t i = 0; i < a_count; ++i) {
				const float t = a_ends ? static_cast<float>(i) / (a_count - 1) : static_cast<float>(i + 1) / (a_count + 1);
				const float a = std::lerp(a0, a1, t);
				a_out[a_at++] = a_center + ImVec2{ std::cos(a), std::sin(a) } * a_radius;
			}
		}

		template <std::size_t N>
		void Place(std::array<ImVec2, N>& a_points, ImVec2 a_c, float a_half)
		{
			for (auto& p : a_points) {
				p = a_c + p * a_half;
			}
		}

		// hunger: an apple, two overlapping circles with a stem and a leaf
		void Apple(ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			constexpr float kX = 0.34f, kY = 0.16f, kR = 0.66f;
			// where the circles cross, seen from the left circle's center (the right one is its mirror image)
			const float  cross = std::atan2(std::sqrt(kR * kR - kX * kX), kX);
			std::array<ImVec2, 38> body;
			std::size_t            n = 0;
			CirclePoints(body, n, 20, { -kX, kY }, kR, -cross, -2.0f * kPi + cross);     // left half, over the top round to the bottom
			CirclePoints(body, n, 18, { kX, kY }, kR, kPi - cross, -kPi + cross, false);  // right half, bottom round to the top
			Place(body, a_c, a_half);

			const ImVec2 stem0 = a_c + ImVec2{ 0.0f, -0.42f } * a_half;
			const ImVec2 stem1 = a_c + ImVec2{ 0.12f, -0.95f } * a_half;
			const ImVec2 leaf = a_c + ImVec2{ 0.40f, -0.80f } * a_half;
			const ImVec2 leafSize{ 0.28f * a_half, 0.13f * a_half };
			const float  stemWidth = std::max(1.0f, 0.14f * a_half);
			if (a_border > 0.0f) {
				ImDrawListManager::AddLine(a_list, stem0, stem1, a_outline, stemWidth + a_border * 2.0f);
				ImDrawListManager::AddEllipseFilled(a_list, leaf, leafSize + ImVec2{ a_border, a_border }, a_outline, -0.45f, 16);
			}
			FillShape(a_list, body, a_fill, a_outline, a_border);
			ImDrawListManager::AddLine(a_list, stem0, stem1, a_fill, stemWidth);
			ImDrawListManager::AddEllipseFilled(a_list, leaf, leafSize, a_fill, -0.45f, 16);
		}

		// thirst: a water drop, a circle with a point on top
		void Drop(ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			constexpr ImVec2 kCenter{ 0.0f, 0.30f };
			constexpr float  kR = 0.62f;
			constexpr float  kTip = -0.98f;
			const float      side = std::acos(kR / (kCenter.y - kTip));  // where the sides touch the circle
			std::array<ImVec2, 30> pts;
			std::size_t            n = 0;
			pts[n++] = { 0.0f, kTip };
			CirclePoints(pts, n, 29, kCenter, kR, -kPi * 0.5f + side, kPi * 1.5f - side);
			Place(pts, a_c, a_half);
			FillShape(a_list, pts, a_fill, a_outline, a_border);
		}

		// fatigue: a crescent moon, a circle with a smaller one cut out of its upper right
		void Moon(ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			constexpr ImVec2 kCut{ 0.40f, -0.32f };
			constexpr float  kR = 0.88f;  // the moon
			constexpr float  kCutR = 0.70f;
			const float      d = std::sqrt(kCut.x * kCut.x + kCut.y * kCut.y);
			const float      a = (kR * kR - kCutR * kCutR + d * d) / (2.0f * d);  // the crossing points' chord, from the center
			const float      h = std::sqrt(std::max(kR * kR - a * a, 0.0f));
			const float      toCut = std::atan2(kCut.y, kCut.x);
			const float      outer = std::atan2(h, a);      // the crossings seen from the moon's center
			const float      inner = std::atan2(h, d - a);  // ... and from the cut's center
			std::array<ImVec2, 46> pts;
			std::size_t            n = 0;
			CirclePoints(pts, n, 30, {}, kR, toCut + outer, toCut + 2.0f * kPi - outer);             // the lit edge
			CirclePoints(pts, n, 16, kCut, kCutR, toCut + kPi + inner, toCut + kPi - inner, false);  // the shadow's edge, back again
			Place(pts, a_c, a_half);
			FillShape(a_list, pts, a_fill, a_outline, a_border);
		}

		void Icon(Need a_need, ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			switch (a_need) {
			case Need::kHunger:
				Apple(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case Need::kThirst:
				Drop(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case Need::kFatigue:
				Moon(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			}
		}

		// ---- the gauges ----

		const RGBA& StateColor(const NeedsSettings& n, const NeedReading& a_reading)
		{
			return a_reading.bonus ? n.bonus : a_reading.danger ? n.danger : a_reading.comfortable ? n.fine : n.warning;
		}

		NeedReadings Demo(float a_time, bool a_sweep)
		{
			// a fed, a thirsty and an exhausted one; sweeping: all of them going round
			static constexpr std::array<std::array<const char*, 3>, kNeedCount> kNames{ {
				{ "Satisfied", "Hungry", "Starving" },
				{ "Quenched", "Thirsty", "Dehydrated" },
				{ "Rested", "Tired", "Exhausted" },
			} };
			constexpr std::array kFills{ 0.85f, 0.45f, 0.12f };
			NeedReadings         r;
			for (std::size_t i = 0; i < kNeedCount; ++i) {
				NeedReading n;
				n.segments = 5;
				n.zones = { NeedZone::kFine, NeedZone::kFine, NeedZone::kPenalty, NeedZone::kDanger, NeedZone::kDanger };
				n.fill = a_sweep ? 0.5f + 0.5f * std::sin(a_time * 0.6f + i * 2.1f) : kFills[i];
				n.comfortable = n.fill > 0.6f;
				n.danger = n.fill < 0.4f;
				n.stage = Lang::T(kNames[i][n.comfortable ? 0 : n.danger ? 2 : 1]);
				r[i] = n;
			}
			return r;
		}

		struct Look
		{
			const Settings& s;
			ImDrawList*     list;
			float           px;
			float           alpha;
			float           time;
		};

		// the label: below a ring in a row, otherwise to the right of the gauge
		void Label(const Look& a_look, const std::string& a_text, ImVec2 a_anchor, bool a_below)
		{
			const auto& n = a_look.s.needs;
			const float size = n.labelSize * a_look.px;
			const auto  ts = TextSize(size, a_text);
			const auto  pos = a_below ? ImVec2{ a_anchor.x - ts.x * 0.5f, a_anchor.y } : ImVec2{ a_anchor.x, a_anchor.y - ts.y * 0.5f };
			Text(a_look.list, size, pos, a_text, ToU32(a_look.s.labelColor, a_look.alpha), ToU32(a_look.s.outlineColor, a_look.alpha * 0.9f));
		}

		// 0..1 while the need is at its worst stages and pulsing is on, else 0
		float Glow(const Look& a_look, const NeedReading& a_reading)
		{
			return a_look.s.needs.pulseDanger && a_reading.danger ? 0.5f + 0.5f * std::sin(a_look.time * 6.0f) : 0.0f;
		}

		// the need's icon, swelling and tinted with a_tint while it pulses
		void PulsingIcon(const Look& a_look, Need a_need, const NeedReading& a_reading, ImVec2 a_c, float a_half, const RGBA& a_tint)
		{
			const auto& s = a_look.s;
			const float glow = Glow(a_look, a_reading);
			const RGBA  icon = glow > 0.0f ? Mix(s.needs.icon, a_tint, 0.6f * glow) : s.needs.icon;
			Icon(a_need, a_look.list, a_c, a_half * (1.0f + 0.08f * glow), ToU32(icon, a_look.alpha), ToU32(s.outlineColor, a_look.alpha),
				std::max(0.5f, s.outline * a_look.px * 0.7f));
		}

		void Ring(const Look& a_look, Need a_need, const NeedReading& a_reading, const NeedState& a_state, ImVec2 a_c)
		{
			const auto& s = a_look.s;
			const auto& n = s.needs;
			const auto  list = a_look.list;
			const float px = a_look.px;
			const float t = n.thickness * px;
			const float r = std::max(n.size * px * 0.5f - t * 0.5f, 1.0f);
			const float border = s.outline * px;
			const float glow = Glow(a_look, a_reading);
			constexpr int kSegs = 48;

			if (glow > 0.0f) {
				ImDrawListManager::AddCircle(list, a_c, r, ToU32(n.danger, a_look.alpha * 0.35f * glow), kSegs, t * 2.6f);
			}
			ImDrawListManager::AddCircle(list, a_c, r, ToU32(n.track, a_look.alpha), kSegs, t + border * 2.0f);
			if (a_state.fill >= 0.999f) {
				ImDrawListManager::AddCircle(list, a_c, r, ToU32(a_state.color, a_look.alpha), kSegs, t);
			} else if (a_state.fill > 0.002f) {
				// from the top clockwise; it empties back towards the top
				const float start = -kPi * 0.5f;
				ImDrawListManager::PathClear(list);
				ImDrawListManager::PathArcTo(list, a_c, r, start, start + 2.0f * kPi * a_state.fill, std::max(4, static_cast<int>(kSegs * a_state.fill)));
				ImDrawListManager::PathStroke(list, ToU32(a_state.color, a_look.alpha), 0, t);
			}
			if (n.ticks) {
				const ImU32 tick = ToU32(s.outlineColor, std::min(1.0f, a_look.alpha * 1.5f));
				for (int j = 1; j < a_reading.segments; ++j) {
					const float  a = -kPi * 0.5f + 2.0f * kPi * (1.0f - static_cast<float>(j) / a_reading.segments);
					const ImVec2 dir{ std::cos(a), std::sin(a) };
					ImDrawListManager::AddLine(list, a_c + dir * (r - t * 0.5f - border), a_c + dir * (r + t * 0.5f + border), tick,
						std::max(1.0f, 1.3f * px));
				}
			}
			if (n.icons) {
				PulsingIcon(a_look, a_need, a_reading, a_c, (r - t * 0.5f) * 0.66f, n.danger);
			}
			if (n.showLabel && !a_reading.stage.empty()) {
				const std::string text = Lang::T(a_reading.stage.c_str());
				if (n.column) {
					Label(a_look, text, a_c + ImVec2{ r + t * 0.5f + 5.0f * px, 0.0f }, false);
				} else {
					Label(a_look, text, a_c + ImVec2{ 0.0f, r + t * 0.5f + 3.0f * px }, true);
				}
			}
		}

		// A need drawn like the temperature gauge along a shape (u = 0 the empty end, 1 the full end): the band in the
		// need's color (or by stage: the worst in the temperature gauge's red shades, then amber penalties, its green,
		// gold for a bonus), lines between the stages and the marker where the need is, on the a_side side (+1 the
		// normals' side).
		void NeedBand(const Look& a_look, Need a_need, const NeedReading& a_reading, const NeedState& a_state, const Shape& a_shape, float a_side)
		{
			const auto& s = a_look.s;
			const auto& n = s.needs;
			const auto  list = a_look.list;
			const float px = a_look.px;
			const float alpha = a_look.alpha;
			const float half = n.thickness * px * 0.5f;
			const float border = s.outline * px;
			const ImU32 outline = ToU32(s.outlineColor, alpha);
			const int   segs = std::clamp(a_reading.segments, 1, kMaxNeedSegments);

			// segment at u (0 = the full end), the end of the worst stages along the band
			const auto segment = [&](float u) { return std::clamp(static_cast<int>((1.0f - u) * segs), 0, segs - 1); };
			int        firstDanger = segs;
			for (int j = 0; j < segs; ++j) {
				if (a_reading.zones[j] == NeedZone::kDanger) {
					firstDanger = j;
					break;
				}
			}
			const float dangerEnd = 1.0f - static_cast<float>(firstDanger) / segs;
			const RGBA& own = n.needColors[static_cast<std::size_t>(a_need)];
			const auto  color = [&](float u) -> RGBA {
				if (n.colorByNeed) {
					return own;
				}
				switch (a_reading.zones[segment(u)]) {
				case NeedZone::kBonus:
					return n.bonus;
				case NeedZone::kFine:
					return s.comfortable;
				case NeedZone::kPenalty:
					return n.warning;
				default:
					return Mix(s.hotEnd, s.hotEdge, dangerEnd > 0.0f ? std::clamp(u / dangerEnd, 0.0f, 1.0f) : 1.0f);
				}
			};
			const auto fade = [&](float u) { return n.fadeEnds ? SmoothStep(std::min(u, 1.0f - u) / 0.1f) : 1.0f; };

			if (border > 0.0f) {
				Band(list, a_shape, half + border, s.roundEnds, [&](float u) { return ToU32(s.outlineColor, alpha * fade(u)); });
			}
			Band(list, a_shape, half, s.roundEnds, [&](float u) { return ToU32(color(u), alpha * fade(u)); });
			// dark lines between the stages; where the zone changes (by stage) they're stronger, like the temperature gauge's
			for (int j = 1; j < segs; ++j) {
				const bool zone = !n.colorByNeed && a_reading.zones[j] != a_reading.zones[j - 1];
				if (zone ? !(s.zoneLines || n.ticks) : !n.ticks) {
					continue;
				}
				const float  u = 1.0f - static_cast<float>(j) / segs;
				const ImVec2 p = a_shape.Point(u);
				const ImVec2 nrm = a_shape.Normal(u);
				const float  line = half + border * 0.5f;
				ImDrawListManager::AddLine(list, p - nrm * line, p + nrm * line, ToU32(s.outlineColor, std::min(1.0f, alpha * (zone ? 1.5f : 0.9f))),
					std::max(1.0f, (zone ? 1.3f : 1.0f) * px));
			}

			// the marker, as on the temperature gauge
			const float  u = std::clamp(a_state.fill, 0.0f, 1.0f);
			const ImVec2 c = a_shape.Point(u);
			const ImVec2 m = a_shape.Normal(u) * a_side;
			const ImVec2 t = a_shape.Tangent(u);
			const float  pulse = n.pulseDanger && a_reading.danger ? 1.0f + 0.14f * (0.5f + 0.5f * std::sin(a_look.time * 7.0f)) : 1.0f;
			const ImU32  markerCol = ToU32(s.markerColor, alpha);
			const bool   line = n.marker == NeedMarker::kLine || (n.marker == NeedMarker::kTemperature && s.marker == MarkerStyle::kLine);
			if (line) {
				const float over = half + border + 3.0f * px * s.markerSize * pulse;
				ImDrawListManager::AddLine(list, c - m * over, c + m * over, outline, 4.5f * px * s.markerSize);
				ImDrawListManager::AddLine(list, c - m * over, c + m * over, markerCol, 2.5f * px * s.markerSize);
			} else {
				Dart(list, c + m * (half + border + 1.0f * px), m, t, 12.0f * px * s.markerSize * pulse, markerCol, outline,
					std::max(1.0f, border * 0.8f) * 0.6f);
			}
		}

		// The bar: the temperature gauge lying down, the empty end on the left, the marker above it
		void Bar(const Look& a_look, Need a_need, const NeedReading& a_reading, const NeedState& a_state, ImVec2 a_topLeft)
		{
			const auto& s = a_look.s;
			const auto& n = s.needs;
			const float px = a_look.px;
			const float size = n.size * px;
			const float iconW = n.icons ? size * 1.2f : 0.0f;
			const ImVec2 origin{ a_topLeft.x + iconW + n.barLength * px * 0.5f, a_topLeft.y + size * 0.5f };
			// rotated a quarter turn: the empty end (u = 0) on the left; a negative curve bends the other way
			const auto  shape = BuildShape({ n.barLength, std::abs(n.barCurve), 90.0f, n.barCurve < 0.0f, false, origin }, px);
			const float up = shape.Normal(0.5f).y < 0.0f ? 1.0f : -1.0f;
			NeedBand(a_look, a_need, a_reading, a_state, shape, up);

			const float border = s.outline * px;
			const float half = n.thickness * px * 0.5f;
			if (n.icons) {
				PulsingIcon(a_look, a_need, a_reading, { a_topLeft.x + size * 0.5f, origin.y }, size * 0.42f, s.hotEdge);
			}
			if (n.showLabel && !a_reading.stage.empty()) {
				const std::string text = Lang::T(a_reading.stage.c_str());
				if (n.column) {
					Label(a_look, text, shape.Point(1.0f) + ImVec2{ half + border + 6.0f * px, 0.0f }, false);
				} else {
					Label(a_look, text, shape.Point(0.5f) - shape.Normal(0.5f) * up * (half + border + 2.0f * px), true);
				}
			}
		}

		// The arc: a piece of the circle around n.posX / n.posY from a_from to a_to degrees, the marker on the outside.
		// With no gap the needs make one line, like the temperature gauge's zones.
		void Arc(const Look& a_look, Need a_need, const NeedReading& a_reading, const NeedState& a_state, ImVec2 a_center, float a_from, float a_to)
		{
			const auto& s = a_look.s;
			const auto& n = s.needs;
			const float px = a_look.px;
			const float radius = n.arcRadius * px;
			// u = 1 (full) at the arc's start unless flipped
			const auto  shape = n.arcFlip ? ArcShape(a_center, radius, a_from, a_to) : ArcShape(a_center, radius, a_to, a_from);
			NeedBand(a_look, a_need, a_reading, a_state, shape, 1.0f);

			const float  border = s.outline * px;
			const float  half = n.thickness * px * 0.5f;
			// no icons: the arcs are told apart by their colors
			const ImVec2 out = Clock((a_from + a_to) * 0.5f);
			const float  reach = radius + half + border + 3.0f * px;
			if (n.showLabel && !a_reading.stage.empty()) {
				// like the temperature gauge's label: beside the arc, on its side of the circle
				const std::string text = Lang::T(a_reading.stage.c_str());
				const float       size = n.labelSize * px;
				const auto        ts = TextSize(size, text);
				const ImVec2      p = a_center + out * (reach + 2.0f * px);
				ImVec2            pos;
				if (out.x > 0.35f) {
					pos = { p.x, p.y - ts.y * 0.5f };
				} else if (out.x < -0.35f) {
					pos = { p.x - ts.x, p.y - ts.y * 0.5f };
				} else {
					pos = { p.x - ts.x * 0.5f, out.y > 0.0f ? p.y : p.y - ts.y };
				}
				Text(a_look.list, size, pos, text, ToU32(s.labelColor, a_look.alpha), ToU32(s.outlineColor, a_look.alpha * 0.9f));
			}
		}
	}

	bool PlaceNeedsOnTemperatureCircle(Settings& s)
	{
		const auto screen = GetIO()->DisplaySize;
		if (screen.x <= 0.0f || screen.y <= 0.0f) {
			return false;
		}
		const float px = screen.y / 1080.0f * s.scale;
		const auto  circle = ShapeCircle({ s.length, s.curve, s.rotation, s.mirror, s.flip, { s.posX * screen.x, s.posY * screen.y } }, px);
		if (!circle) {
			return false;
		}
		auto& n = s.needs;
		n.posX = circle->center.x / screen.x;
		n.posY = circle->center.y / screen.y;
		n.arcRadius = circle->radius / px;
		// the other side: mirrored left / right, or top / bottom when the gauge sits above or below the circle
		const float side = std::abs(std::sin(circle->middle * kPi / 180.0f)) > 0.5f ? -circle->middle : circle->middle + 180.0f;
		const auto  live = Needs::Current();
		int         count = 0;
		for (std::size_t i = 0; i < kNeedCount; ++i) {
			count += live[i] && n.show[i] ? 1 : 0;
		}
		count = count > 0 ? count : 2;
		const float total = std::min(s.curve, 300.0f);
		n.arcGap = std::min(n.arcGap, total / count * 0.5f);
		n.arcSpan = (total - n.arcGap * (count - 1)) / count;
		n.arcStart = side - total * 0.5f;
		return true;
	}

	void DrawNeeds(const Settings& s, float a_dt, float a_time, bool a_preview, bool a_gameplay, float a_hud)
	{
		const auto& n = s.needs;
		if (!n.enabled) {
			states = {};
			return;
		}
		auto        readings = Needs::Current();
		const auto& prev = GetPreview();
		if (a_preview && (prev.needs || std::ranges::none_of(readings, [](const auto& r) { return r.has_value(); }))) {
			readings = Demo(a_time, prev.needs && prev.needsSweep);
		}
		if (!a_preview && !a_gameplay) {
			return;  // menus: gone right away, back right away
		}

		const auto screen = GetIO()->DisplaySize;
		if (screen.x <= 0.0f || screen.y <= 0.0f) {
			return;
		}
		const float px = screen.y / 1080.0f * s.scale;

		// the gauges in their places: needs the mod doesn't track (or that are switched off) leave no gap
		std::array<std::size_t, kNeedCount> order{};
		std::size_t                         count = 0;
		for (std::size_t i = 0; i < kNeedCount; ++i) {
			if (readings[i] && n.show[i]) {
				order[count++] = i;
			} else {
				states[i] = {};
			}
		}
		if (count == 0) {
			return;
		}
		const float size = n.size * px;
		const float itemW = n.style == NeedStyle::kRing ? size : (n.icons ? size * 1.2f : 0.0f) + n.barLength * px;
		const float step = (n.column ? size : itemW) + n.spacing * px;
		const float total = step * count - n.spacing * px;
		const ImVec2 center{ n.posX * screen.x, n.posY * screen.y };
		// top left of the first item
		const ImVec2 first = n.column ? ImVec2{ center.x - itemW * 0.5f, center.y - total * 0.5f } : ImVec2{ center.x - total * 0.5f, center.y - size * 0.5f };

		const Look look{ s, GetBackgroundDrawList(), px, 0.0f, a_time };
		for (std::size_t k = 0; k < count; ++k) {
			const auto  i = order[k];
			const auto& reading = *readings[i];
			auto&       st = states[i];

			bool want = true;
			if (!a_preview && n.visibility == Visibility::kNotComfortable) {
				st.comfortableFor = reading.comfortable ? st.comfortableFor + a_dt : 0.0f;
				want = st.comfortableFor < n.hideDelay;
			}
			st.shown = a_preview ? 1.0f : Approach(st.shown, want ? 1.0f : 0.0f, a_dt / kFadeTime);

			// the fill glides like the temperature marker, the color fades over
			if (std::isnan(st.fill) || s.smoothing <= 0.0f || (a_preview && prev.needs && prev.needsSweep)) {
				st.fill = reading.fill;
			} else {
				st.fill += (reading.fill - st.fill) * (1.0f - std::exp(-a_dt * 3.0f / s.smoothing));
			}
			const RGBA& goal = StateColor(n, reading);
			st.color = st.colored ? Mix(st.color, goal, std::min(1.0f, a_dt / kColorTime)) : goal;
			st.colored = true;

			auto item = look;
			item.alpha = s.opacity * SmoothStep(st.shown) * a_hud;
			if (item.alpha <= 0.01f) {
				continue;
			}
			if (n.style == NeedStyle::kArc) {
				// one after another along the circle, a gap between them
				const float slot = static_cast<float>(n.arcReverse ? count - 1 - k : k);
				const float from = n.arcStart + slot * (n.arcSpan + std::copysign(n.arcGap, n.arcSpan));
				Arc(item, static_cast<Need>(i), reading, st, center, from, from + n.arcSpan);
				continue;
			}
			const ImVec2 topLeft = first + (n.column ? ImVec2{ 0.0f, step * k } : ImVec2{ step * k, 0.0f });
			if (n.style == NeedStyle::kRing) {
				Ring(item, static_cast<Need>(i), reading, st, topLeft + ImVec2{ size * 0.5f, size * 0.5f });
			} else {
				Bar(item, static_cast<Need>(i), reading, st, topLeft);
			}
		}
	}
}
