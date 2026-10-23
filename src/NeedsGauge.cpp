#include "NeedsGauge.h"

#include "Band.h"
#include "Draw.h"
#include "Lang.h"
#include "Needs.h"
#include "Stress.h"
#include "UI.h"

// The hunger, thirst and fatigue gauges, one per need: a ring around an icon, the icon and a bar, or arcs on a circle.
// Bars and arcs are drawn like the temperature gauge (a band and its marker where the need is). The ring is full while
// fed / quenched / rested and empties stage by stage, its color telling the state (fine, penalties, the worst stages,
// a bonus like Well Fed). The icons are drawn shapes: an apple, a water drop, a crescent moon. The stress gauge is one
// more of them with its own settings (a lightning bolt), the fear indicator an eye with the phobia's name.
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
		NeedState                         stressState;
		float                             fearShown{ 0.0f };

		enum class MeterIcon
		{
			kApple,
			kDrop,
			kMoon,
			kBolt,
			kBottle,
			kDumbbell,
			kHeart,
		};

		// one gauge of a group: what it shows and how
		struct Meter
		{
			MeterIcon          icon;
			RGBA               color;  // its own color (needColors)
			const NeedReading* reading;
			NeedState*         state;
			bool               idle;   // fine for the "only when not comfortable" setting
			bool               fillUp{ false };  // the ring fills up as it gets worse (stress)
		};

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

		// stress: a lightning bolt
		void Bolt(ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			std::array<ImVec2, 7> pts{ { { 0.30f, -0.98f }, { -0.58f, 0.14f }, { -0.06f, 0.14f }, { -0.30f, 0.98f }, { 0.60f, -0.18f },
				{ 0.07f, -0.18f }, { 0.36f, -0.98f } } };
			Place(pts, a_c, a_half);
			FillShape(a_list, pts, a_fill, a_outline, a_border);
		}

		// inebriation: a mead bottle, a round body with shoulders and a neck
		void Bottle(ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			std::array<ImVec2, 23> pts;
			std::size_t            n = 0;
			pts[n++] = { -0.17f, -0.98f };  // the neck's top left, then down the neck
			pts[n++] = { -0.17f, -0.48f };
			CirclePoints(pts, n, 18, { 0.0f, 0.30f }, 0.62f, -kPi * 0.5f - 0.28f, -kPi * 2.5f + 0.28f);  // the body, round from the left shoulder to the right one
			pts[n++] = { 0.17f, -0.48f };
			pts[n++] = { 0.17f, -0.98f };
			pts[n++] = { 0.0f, -1.02f };
			Place(pts, a_c, a_half);
			FillShape(a_list, pts, a_fill, a_outline, a_border);
		}

		// weight: a dumbbell, a bar with two weights on each end
		void Dumbbell(ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			const auto box = [&](float x0, float y0, float x1, float y1, float a_grow) {
				const ImVec2 a = a_c + ImVec2{ x0, y0 } * a_half - ImVec2{ a_grow, a_grow };
				const ImVec2 b = a_c + ImVec2{ x1, y1 } * a_half + ImVec2{ a_grow, a_grow };
				return std::pair{ a, b };
			};
			constexpr std::array<std::array<float, 4>, 5> kParts{ { { -0.62f, -0.12f, 0.62f, 0.12f },  // the bar
				{ -0.78f, -0.62f, -0.52f, 0.62f }, { 0.52f, -0.62f, 0.78f, 0.62f },                    // the big weights
				{ -0.98f, -0.38f, -0.76f, 0.38f }, { 0.76f, -0.38f, 0.98f, 0.38f } } };                // the small ones
			const float round = 0.08f * a_half;
			if (a_border > 0.0f) {
				for (const auto& p : kParts) {
					const auto [a, b] = box(p[0], p[1], p[2], p[3], a_border);
					ImDrawListManager::AddRectFilled(a_list, a, b, a_outline, round + a_border, 0);
				}
			}
			for (const auto& p : kParts) {
				const auto [a, b] = box(p[0], p[1], p[2], p[3], 0.0f);
				ImDrawListManager::AddRectFilled(a_list, a, b, a_fill, round, 0);
			}
		}

		// vitality: a heart, two lobes meeting in a point at the bottom
		void Heart(ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			constexpr float kR = 0.50f;  // the lobes, meeting at the dip (0, -0.47)
			std::array<ImVec2, 33> pts;
			std::size_t            n = 0;
			pts[n++] = { 0.0f, 0.96f };                                                  // the point
			CirclePoints(pts, n, 16, { -0.47f, -0.30f }, kR, kPi * 0.78f, kPi * 1.89f);  // the left lobe, up to the dip
			CirclePoints(pts, n, 16, { 0.47f, -0.30f }, kR, kPi * 1.11f, kPi * 2.22f);   // the right lobe, down again
			Place(pts, a_c, a_half);
			FillShape(a_list, pts, a_fill, a_outline, a_border);
		}

		void Icon(MeterIcon a_icon, ImDrawList* a_list, ImVec2 a_c, float a_half, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			switch (a_icon) {
			case MeterIcon::kApple:
				Apple(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case MeterIcon::kDrop:
				Drop(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case MeterIcon::kMoon:
				Moon(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case MeterIcon::kBolt:
				Bolt(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case MeterIcon::kBottle:
				Bottle(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case MeterIcon::kDumbbell:
				Dumbbell(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			case MeterIcon::kHeart:
				Heart(a_list, a_c, a_half, a_fill, a_outline, a_border);
				break;
			}
		}

		// fear: a wide open eye, a_half = half its width
		void Eye(ImDrawList* a_list, ImVec2 a_c, float a_half, float a_open, ImU32 a_col, ImU32 a_dark, ImU32 a_outline, float a_border)
		{
			// the lids: two arcs meeting at the corners, opened a_open (0..1) of the way
			const float lid = a_half * (0.30f + 0.22f * a_open);
			std::array<ImVec2, 32> pts;
			std::size_t            k = 0;
			for (int i = 0; i <= 16; ++i) {  // the upper lid, left corner to right corner
				const float x = -1.0f + 2.0f * i / 16.0f;
				pts[k++] = a_c + ImVec2{ x * a_half, -(1.0f - x * x) * lid };
			}
			for (int i = 1; i < 16; ++i) {  // the lower lid back, without the corners
				const float x = 1.0f - 2.0f * i / 16.0f;
				pts[k++] = a_c + ImVec2{ x * a_half, (1.0f - x * x) * lid * 0.85f };
			}
			MakeClockwise(pts);
			if (a_border > 0.0f) {
				ImDrawListManager::AddPolyline(a_list, pts.data(), static_cast<int>(pts.size()), a_outline, ImDrawFlags_Closed, a_border * 2.0f + 1.8f);
			}
			ImDrawListManager::AddConcavePolyFilled(a_list, pts.data(), static_cast<int>(pts.size()), a_dark);
			ImDrawListManager::AddPolyline(a_list, pts.data(), static_cast<int>(pts.size()), a_col, ImDrawFlags_Closed, std::max(1.2f, a_half * 0.11f));
			// the iris and a small pupil (a frightened stare)
			const float iris = std::min(lid * 0.95f, a_half * 0.42f);
			ImDrawListManager::AddCircleFilled(a_list, a_c, iris, a_col, 24);
			ImDrawListManager::AddCircleFilled(a_list, a_c, iris * 0.32f, a_dark, 16);
		}

		// ---- the gauges ----

		const RGBA& StateColor(const NeedsSettings& n, const NeedReading& a_reading)
		{
			return a_reading.bonus ? n.bonus : a_reading.danger ? n.danger : a_reading.comfortable ? n.fine : n.warning;
		}

		NeedReadings Demo(float a_time, bool a_sweep)
		{
			// a fed, a thirsty, an exhausted, a drunk, a muscular and a healthy one; sweeping: all of them going round
			static constexpr std::array<std::array<const char*, 3>, kNeedCount> kNames{ {
				{ "Satisfied", "Hungry", "Starving" },
				{ "Quenched", "Thirsty", "Dehydrated" },
				{ "Rested", "Tired", "Exhausted" },
				{ "Sober", "Drunk", "Wasted" },
				{ "Muscular", "Wiry", "Skinny" },
				{ "Healthy", "Drained", "Dying" },
			} };
			constexpr std::array kFills{ 0.85f, 0.45f, 0.12f, 0.55f, 1.0f, 0.7f };
			NeedReadings         r;
			for (std::size_t i = 0; i < kNeedCount; ++i) {
				NeedReading n;
				n.segments = 5;
				n.zones = { NeedZone::kFine, NeedZone::kFine, NeedZone::kPenalty, NeedZone::kDanger, NeedZone::kDanger };
				n.fill = a_sweep ? 0.5f + 0.5f * std::sin(a_time * 0.6f + i * 2.1f) : kFills[i];
				n.comfortable = n.fill > 0.6f;
				n.danger = n.fill < 0.4f;
				n.stage = Lang::T(kNames[i][n.comfortable ? 0 : n.danger ? 2 : 1]);
				n.fillsUp = i == static_cast<std::size_t>(Need::kInebriation);
				if (i == static_cast<std::size_t>(Need::kWeight)) {
					n.comfortable = true;
					n.danger = false;
					n.zones = { NeedZone::kFine, NeedZone::kFine, NeedZone::kFine, NeedZone::kFine, NeedZone::kFine };
					n.detail = std::format("{:.0f} kg", 50.0f + 50.0f * n.fill);
				}
				r[i] = n;
			}
			return r;
		}

		// made up stress like Stress and Fear's (calm, minor .. critical stress) and a phobia
		StressReading StressDemo(float a_time, bool a_sweep)
		{
			StressReading r;
			NeedReading   n;
			n.segments = 5;
			n.zones = { NeedZone::kFine, NeedZone::kPenalty, NeedZone::kPenalty, NeedZone::kDanger, NeedZone::kDanger };
			n.fill = a_sweep ? 0.5f + 0.5f * std::sin(a_time * 0.5f) : 0.45f;
			const int stage = std::clamp(static_cast<int>((1.0f - n.fill) * 5.0f), 0, 4);
			static constexpr std::array kNames{ "Calm", "Minor Combat Stress", "Moderate Combat Stress", "Severe Combat Stress",
				"Critical Combat Stress" };
			n.comfortable = stage == 0;
			n.danger = stage >= 3;
			n.stage = Lang::T(kNames[stage]);
			n.value = (1.0f - n.fill) * 100.0f;
			n.max = 100.0f;
			r.stress = n;
			r.fears = true;
			r.phobias = { "Fear of Wolves", "Fear of Spiders" };
			if (!a_sweep || std::fmod(a_time, 8.0f) < 5.0f) {
				r.afraid = { "Fear of Wolves" };
			}
			return r;
		}

		struct Look
		{
			const Settings&      s;
			const NeedsSettings& n;
			ImDrawList*          list;
			float                px;
			float                alpha;
			float                time;
		};

		// what the label says: the stage's name, after the detail if there is one ("100 kg Muscular")
		std::string LabelText(const NeedReading& a_reading)
		{
			const std::string stage = a_reading.stage.empty() ? std::string{} : Lang::T(a_reading.stage.c_str());
			return a_reading.detail.empty() ? stage : stage.empty() ? a_reading.detail : a_reading.detail + " " + stage;
		}

		// the label: below a ring in a row, otherwise to the right of the gauge
		void Label(const Look& a_look, const std::string& a_text, ImVec2 a_anchor, bool a_below)
		{
			const float size = a_look.n.labelSize * a_look.px;
			const auto  ts = TextSize(size, a_text);
			const auto  pos = a_below ? ImVec2{ a_anchor.x - ts.x * 0.5f, a_anchor.y } : ImVec2{ a_anchor.x, a_anchor.y - ts.y * 0.5f };
			Text(a_look.list, size, pos, a_text, ToU32(a_look.s.labelColor, a_look.alpha), ToU32(a_look.s.outlineColor, a_look.alpha * 0.9f));
		}

		// 0..1 while the meter is at its worst stages and pulsing is on, else 0
		float Glow(const Look& a_look, const NeedReading& a_reading)
		{
			return a_look.n.pulseDanger && a_reading.danger ? 0.5f + 0.5f * std::sin(a_look.time * 6.0f) : 0.0f;
		}

		// the meter's icon, swelling and tinted with a_tint while it pulses
		void PulsingIcon(const Look& a_look, const Meter& a_meter, ImVec2 a_c, float a_half, const RGBA& a_tint)
		{
			const auto& s = a_look.s;
			const float glow = Glow(a_look, *a_meter.reading);
			const RGBA  icon = glow > 0.0f ? Mix(a_look.n.icon, a_tint, 0.6f * glow) : a_look.n.icon;
			Icon(a_meter.icon, a_look.list, a_c, a_half * (1.0f + 0.08f * glow), ToU32(icon, a_look.alpha), ToU32(s.outlineColor, a_look.alpha),
				std::max(0.5f, s.outline * a_look.px * 0.7f));
		}

		void Ring(const Look& a_look, const Meter& a_meter, ImVec2 a_c, bool a_column)
		{
			const auto& s = a_look.s;
			const auto& n = a_look.n;
			const auto& reading = *a_meter.reading;
			const auto  list = a_look.list;
			const float px = a_look.px;
			const float t = n.thickness * px;
			const float r = std::max(n.size * px * 0.5f - t * 0.5f, 1.0f);
			const float border = s.outline * px;
			const float glow = Glow(a_look, reading);
			const float fill = a_meter.fillUp != a_meter.reading->fillsUp ? 1.0f - a_meter.state->fill : a_meter.state->fill;
			const RGBA& color = a_meter.state->color;
			constexpr int kSegs = 48;

			if (glow > 0.0f) {
				ImDrawListManager::AddCircle(list, a_c, r, ToU32(n.danger, a_look.alpha * 0.35f * glow), kSegs, t * 2.6f);
			}
			ImDrawListManager::AddCircle(list, a_c, r, ToU32(n.track, a_look.alpha), kSegs, t + border * 2.0f);
			if (fill >= 0.999f) {
				ImDrawListManager::AddCircle(list, a_c, r, ToU32(color, a_look.alpha), kSegs, t);
			} else if (fill > 0.002f) {
				// from the top clockwise; it empties back towards the top
				const float start = -kPi * 0.5f;
				ImDrawListManager::PathClear(list);
				ImDrawListManager::PathArcTo(list, a_c, r, start, start + 2.0f * kPi * fill, std::max(4, static_cast<int>(kSegs * fill)));
				ImDrawListManager::PathStroke(list, ToU32(color, a_look.alpha), 0, t);
			}
			if (n.ticks) {
				const ImU32 tick = ToU32(s.outlineColor, std::min(1.0f, a_look.alpha * 1.5f));
				for (int j = 1; j < reading.segments; ++j) {
					const float  a = -kPi * 0.5f + 2.0f * kPi * (1.0f - static_cast<float>(j) / reading.segments);
					const ImVec2 dir{ std::cos(a), std::sin(a) };
					ImDrawListManager::AddLine(list, a_c + dir * (r - t * 0.5f - border), a_c + dir * (r + t * 0.5f + border), tick,
						std::max(1.0f, 1.3f * px));
				}
			}
			if (n.icons) {
				PulsingIcon(a_look, a_meter, a_c, (r - t * 0.5f) * 0.66f, n.danger);
			}
			if (const auto text = LabelText(reading); n.showLabel && !text.empty()) {
				if (a_column) {
					Label(a_look, text, a_c + ImVec2{ r + t * 0.5f + 5.0f * px, 0.0f }, false);
				} else {
					Label(a_look, text, a_c + ImVec2{ 0.0f, r + t * 0.5f + 3.0f * px }, true);
				}
			}
		}

		// A meter drawn like the temperature gauge along a shape (u = 0 the empty end, 1 the full end): the band in the
		// meter's color (or by stage: the worst in the temperature gauge's red shades, then amber penalties, its green,
		// gold for a bonus), lines between the stages and the marker where the meter is, on the a_side side (+1 the
		// normals' side).
		void NeedBand(const Look& a_look, const Meter& a_meter, const Shape& a_shape, float a_side)
		{
			const auto& s = a_look.s;
			const auto& n = a_look.n;
			const auto& reading = *a_meter.reading;
			const auto  list = a_look.list;
			const float px = a_look.px;
			const float alpha = a_look.alpha;
			const float half = n.thickness * px * 0.5f;
			const float border = s.outline * px;
			const ImU32 outline = ToU32(s.outlineColor, alpha);
			const int   segs = std::clamp(reading.segments, 1, kMaxNeedSegments);

			// segment at u (0 = the full end), the end of the worst stages along the band
			const auto segment = [&](float u) { return std::clamp(static_cast<int>((1.0f - u) * segs), 0, segs - 1); };
			int        firstDanger = segs;
			for (int j = 0; j < segs; ++j) {
				if (reading.zones[j] == NeedZone::kDanger) {
					firstDanger = j;
					break;
				}
			}
			const float dangerEnd = 1.0f - static_cast<float>(firstDanger) / segs;
			const auto  color = [&](float u) -> RGBA {
				if (n.colorByNeed) {
					return a_meter.color;
				}
				switch (reading.zones[segment(u)]) {
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
				const bool zone = !n.colorByNeed && reading.zones[j] != reading.zones[j - 1];
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
			const float  u = std::clamp(a_meter.state->fill, 0.0f, 1.0f);
			const ImVec2 c = a_shape.Point(u);
			const ImVec2 m = a_shape.Normal(u) * a_side;
			const ImVec2 t = a_shape.Tangent(u);
			const float  pulse = n.pulseDanger && reading.danger ? 1.0f + 0.14f * (0.5f + 0.5f * std::sin(a_look.time * 7.0f)) : 1.0f;
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

		// The bar: the temperature gauge lying down, the empty end on the left (flipped: the right), the marker above it
		void Bar(const Look& a_look, const Meter& a_meter, ImVec2 a_topLeft, bool a_column)
		{
			const auto& s = a_look.s;
			const auto& n = a_look.n;
			const auto& reading = *a_meter.reading;
			const float px = a_look.px;
			const float size = n.size * px;
			const float iconW = n.icons ? size * 1.2f : 0.0f;
			const ImVec2 origin{ a_topLeft.x + iconW + n.barLength * px * 0.5f, a_topLeft.y + size * 0.5f };
			// rotated a quarter turn: the empty end (u = 0) on the left; a negative curve bends the other way
			const auto  shape = BuildShape({ n.barLength, std::abs(n.barCurve), 90.0f, n.barCurve < 0.0f, n.arcFlip, origin }, px);
			const float up = shape.Normal(0.5f).y < 0.0f ? 1.0f : -1.0f;
			NeedBand(a_look, a_meter, shape, up);

			const float border = s.outline * px;
			const float half = n.thickness * px * 0.5f;
			if (n.icons) {
				PulsingIcon(a_look, a_meter, { a_topLeft.x + size * 0.5f, origin.y }, size * 0.42f, s.hotEdge);
			}
			if (const auto text = LabelText(reading); n.showLabel && !text.empty()) {
				if (a_column) {
					const ImVec2 right = shape.Point(0.0f).x > shape.Point(1.0f).x ? shape.Point(0.0f) : shape.Point(1.0f);
					Label(a_look, text, right + ImVec2{ half + border + 6.0f * px, 0.0f }, false);
				} else {
					Label(a_look, text, shape.Point(0.5f) - shape.Normal(0.5f) * up * (half + border + 2.0f * px), true);
				}
			}
		}

		// The arc: a piece of the circle around n.posX / n.posY from a_from to a_to degrees, the marker on the outside.
		// With no gap the needs make one line, like the temperature gauge's zones.
		void Arc(const Look& a_look, const Meter& a_meter, ImVec2 a_center, float a_from, float a_to)
		{
			const auto& s = a_look.s;
			const auto& n = a_look.n;
			const auto& reading = *a_meter.reading;
			const float px = a_look.px;
			const float radius = n.arcRadius * px;
			// u = 1 (full) at the arc's start unless flipped
			const auto  shape = n.arcFlip ? ArcShape(a_center, radius, a_from, a_to) : ArcShape(a_center, radius, a_to, a_from);
			NeedBand(a_look, a_meter, shape, 1.0f);

			const float  border = s.outline * px;
			const float  half = n.thickness * px * 0.5f;
			// no icons: the arcs are told apart by their colors
			const ImVec2 out = Clock((a_from + a_to) * 0.5f);
			const float  reach = radius + half + border + 3.0f * px;
			if (const auto text = LabelText(reading); n.showLabel && !text.empty()) {
				// like the temperature gauge's label: beside the arc, on its side of the circle
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

		// eases a meter's state towards its reading (fill glides like the temperature marker, the color fades over) and
		// its visibility; the opacity it is drawn with
		float Advance(const Settings& s, const NeedsSettings& n, const Meter& a_meter, float a_dt, bool a_preview, bool a_jump)
		{
			auto&       st = *a_meter.state;
			const auto& reading = *a_meter.reading;
			bool        want = true;
			if (!a_preview && n.visibility == Visibility::kNotComfortable) {
				st.comfortableFor = a_meter.idle ? st.comfortableFor + a_dt : 0.0f;
				want = st.comfortableFor < n.hideDelay;
			}
			st.shown = a_preview ? 1.0f : Approach(st.shown, want ? 1.0f : 0.0f, a_dt / kFadeTime);
			if (std::isnan(st.fill) || s.smoothing <= 0.0f || a_jump) {
				st.fill = reading.fill;
			} else {
				st.fill += (reading.fill - st.fill) * (1.0f - std::exp(-a_dt * 3.0f / s.smoothing));
			}
			const RGBA& goal = StateColor(n, reading);
			st.color = st.colored ? Mix(st.color, goal, std::min(1.0f, a_dt / kColorTime)) : goal;
			st.colored = true;
			return s.opacity * SmoothStep(st.shown);
		}

		// the meters of a group in their places: rings / bars in a row or a column around n.posX / n.posY, or arcs one
		// after another on the circle there
		void DrawGroup(const Settings& s, const NeedsSettings& n, std::span<const Meter> a_meters, float a_dt, float a_time, bool a_preview,
			bool a_jump, float a_hud, bool a_column)
		{
			const auto screen = GetIO()->DisplaySize;
			const auto count = a_meters.size();
			if (screen.x <= 0.0f || screen.y <= 0.0f || count == 0) {
				return;
			}
			const float  px = screen.y / 1080.0f * s.scale;
			const float  size = n.size * px;
			const float  itemW = n.style == NeedStyle::kRing ? size : (n.icons ? size * 1.2f : 0.0f) + n.barLength * px;
			const float  step = (a_column ? size : itemW) + n.spacing * px;
			const float  total = step * count - n.spacing * px;
			const ImVec2 center{ n.posX * screen.x, n.posY * screen.y };
			// top left of the first item
			const ImVec2 first = a_column ? ImVec2{ center.x - itemW * 0.5f, center.y - total * 0.5f } :
			                                ImVec2{ center.x - total * 0.5f, center.y - size * 0.5f };

			const Look look{ s, n, GetBackgroundDrawList(), px, 0.0f, a_time };
			for (std::size_t k = 0; k < count; ++k) {
				const auto& meter = a_meters[k];
				auto        item = look;
				item.alpha = Advance(s, n, meter, a_dt, a_preview, a_jump) * a_hud;
				if (item.alpha <= 0.01f) {
					continue;
				}
				if (n.style == NeedStyle::kArc) {
					// one after another along the circle, a gap between them
					const float slot = static_cast<float>(n.arcReverse ? count - 1 - k : k);
					const float from = n.arcStart + slot * (n.arcSpan + std::copysign(n.arcGap, n.arcSpan));
					Arc(item, meter, center, from, from + n.arcSpan);
					continue;
				}
				const ImVec2 topLeft = first + (a_column ? ImVec2{ 0.0f, step * k } : ImVec2{ step * k, 0.0f });
				if (n.style == NeedStyle::kRing) {
					Ring(item, meter, topLeft + ImVec2{ size * 0.5f, size * 0.5f }, a_column);
				} else {
					Bar(item, meter, topLeft, a_column);
				}
			}
		}

		// The fear indicator: the eye opening wide and the phobia's name while it works; faint, the phobias the player
		// has (if asked for)
		void DrawFear(const Settings& s, const StressReading& a_reading, float a_dt, float a_time, bool a_preview, float a_hud)
		{
			const auto& f = s.fear;
			const auto  screen = GetIO()->DisplaySize;
			if (screen.x <= 0.0f || screen.y <= 0.0f) {
				return;
			}
			const bool afraid = !a_reading.afraid.empty();
			const bool faint = !afraid && f.showPhobias && !a_reading.phobias.empty();
			fearShown = Approach(fearShown, afraid ? 1.0f : faint ? 0.45f : 0.0f, a_dt / kFadeTime);
			if (a_preview && (afraid || faint)) {
				fearShown = afraid ? 1.0f : 0.45f;
			}
			const float alpha = s.opacity * SmoothStep(fearShown) * a_hud;
			if (alpha <= 0.01f) {
				return;
			}
			const float px = screen.y / 1080.0f * s.scale;
			const auto  list = GetBackgroundDrawList();
			ImVec2      at{ f.posX * screen.x, f.posY * screen.y };
			const float beat = afraid && f.pulse ? std::pow(0.5f + 0.5f * std::sin(a_time * 7.5f), 3.0f) : 0.0f;  // a racing heart
			const float half = f.size * px * 0.5f;
			if (f.icon) {
				const RGBA glow = Mix(f.color, RGBA{ 1.0f, 1.0f, 1.0f, 1.0f }, 0.35f * beat);
				Eye(list, at, half * (1.0f + 0.10f * beat), afraid ? 1.0f : 0.4f, ToU32(glow, alpha), ToU32({ 0.05f, 0.02f, 0.07f, 0.9f }, alpha),
					ToU32(s.outlineColor, alpha), s.outline * px);
				at.y += half * 0.62f + 4.0f * px;
			}
			if (!f.showName) {
				return;
			}
			const auto& names = afraid ? a_reading.afraid : a_reading.phobias;
			const float size = f.labelSize * px;
			for (const auto& name : names) {
				const std::string text = Lang::T(name.c_str());
				const auto        ts = TextSize(size, text);
				const RGBA        col = afraid ? Mix(s.labelColor, f.color, 0.25f + 0.35f * beat) : s.labelColor;
				Text(list, size, { at.x - ts.x * 0.5f, at.y }, text, ToU32(col, alpha), ToU32(s.outlineColor, alpha * 0.9f));
				at.y += ts.y + 1.0f * px;
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

		// needs the mod doesn't track (or that are switched off) leave no gap
		static constexpr std::array kIcons{ MeterIcon::kApple, MeterIcon::kDrop, MeterIcon::kMoon, MeterIcon::kBottle, MeterIcon::kDumbbell,
			MeterIcon::kHeart };
		std::array<Meter, kNeedCount> meters{};
		std::size_t                   count = 0;
		for (std::size_t i = 0; i < kNeedCount; ++i) {
			if (readings[i] && n.show[i]) {
				meters[count++] = { kIcons[i], n.needColors[i], &*readings[i], &states[i], readings[i]->comfortable, false };
			} else {
				states[i] = {};
			}
		}
		DrawGroup(s, n, std::span{ meters.data(), count }, a_dt, a_time, a_preview, a_preview && prev.needs && prev.needsSweep, a_hud, n.column);
	}

	void DrawStress(const Settings& s, float a_dt, float a_time, bool a_preview, bool a_gameplay, float a_hud)
	{
		const auto& n = s.stress;
		if (!Stress::Available() || (!n.enabled && !s.fear.enabled)) {
			stressState = {};
			fearShown = 0.0f;
			return;
		}
		auto        reading = Stress::Current();
		const auto& prev = GetPreview();
		if (a_preview && (prev.stress || (!reading.stress && reading.phobias.empty()))) {
			reading = StressDemo(a_time, prev.stress && prev.stressSweep);
		} else if (a_preview && reading.afraid.empty()) {
			// not afraid right now (that takes a fight with what you fear): the menu shows a made up phobia
			const auto demo = StressDemo(a_time, false);
			reading.fears = true;
			reading.afraid = demo.afraid;
		}
		if (!a_preview && !a_gameplay) {
			return;
		}
		if (n.enabled && reading.stress) {
			const Meter meter{ MeterIcon::kBolt, n.needColors[0], &*reading.stress, &stressState, reading.stress->value <= 0.01f, n.arcFlip };
			DrawGroup(s, n, std::span{ &meter, 1 }, a_dt, a_time, a_preview, a_preview && prev.stress && prev.stressSweep, a_hud, false);
		} else {
			stressState = {};
		}
		if (s.fear.enabled && reading.fears) {
			DrawFear(s, reading, a_dt, a_time, a_preview, a_hud);
		} else {
			fearShown = 0.0f;
		}
	}
}
