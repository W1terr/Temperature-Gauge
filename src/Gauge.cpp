#include "Gauge.h"

#include "Band.h"
#include "Draw.h"
#include "Framework.h"
#include "HudIcons.h"
#include "IHud.h"
#include "Lang.h"
#include "Menus.h"
#include "Needs.h"
#include "NeedsGauge.h"
#include "Settings.h"
#include "SmfHuds.h"
#include "Stress.h"
#include "Temperature.h"
#include "UI.h"

namespace UI
{
	using namespace ImGuiMCP;

	namespace
	{
		constexpr float kSampleInterval = 0.25f;  // seconds between game reads
		constexpr float kFadeTime = 0.35f;

		// parts of the gauge's length, from the cold end
		struct Zones
		{
			float cold;
			float hot;
			float comfort;

			explicit Zones(const Settings& s) :
				cold(s.coldZone), hot(std::clamp(s.hotZone, 0.0f, 0.95f - s.coldZone)), comfort(1.0f - cold - hot)
			{}

			// temperature scale (-1 .. 0 cold, 0 .. 1 comfortable, 1 .. 2 hot) -> place along the gauge (0 .. 1)
			float U(float t) const
			{
				t = std::clamp(t, -1.0f, 2.0f);
				if (t < 0.0f) {
					return (t + 1.0f) * cold;
				}
				if (t <= 1.0f) {
					return cold + t * comfort;
				}
				return cold + comfort + (t - 1.0f) * hot;
			}

			float T(float u) const
			{
				if (u < cold) {
					return u / cold - 1.0f;
				}
				if (u <= cold + comfort) {
					return comfort > 0.0f ? (u - cold) / comfort : 0.5f;
				}
				return hot > 0.0f ? 1.0f + (u - cold - comfort) / hot : 2.0f;
			}
		};

		// temperature scale -> color: blue shades in the cold zone, green, red shades in the hot zone
		RGBA ZoneColor(const Settings& s, float t)
		{
			if (t < 0.0f) {
				return Mix(s.coldEnd, s.coldEdge, std::clamp(t + 1.0f, 0.0f, 1.0f));
			}
			if (t > 1.0f) {
				return Mix(s.hotEdge, s.hotEnd, std::clamp(t - 1.0f, 0.0f, 1.0f));
			}
			if (s.blendZones) {
				constexpr float kBlend = 0.3f;
				if (t < kBlend) {
					return Mix(s.coldEdge, s.comfortable, t / kBlend);
				}
				if (t > 1.0f - kBlend) {
					return Mix(s.comfortable, s.hotEdge, (t - (1.0f - kBlend)) / kBlend);
				}
			}
			return s.comfortable;
		}

		void Chevron(ImDrawList* a_list, ImVec2 a_center, ImVec2 a_dir, ImVec2 a_side, float a_size, ImU32 a_col, ImU32 a_outline, float a_px)
		{
			const std::array<ImVec2, 3> pts{ a_center - a_dir * (a_size * 0.45f) + a_side * (a_size * 0.6f),
				a_center + a_dir * (a_size * 0.45f), a_center - a_dir * (a_size * 0.45f) - a_side * (a_size * 0.6f) };
			ImDrawListManager::AddPolyline(a_list, pts.data(), 3, a_outline, 0, 3.6f * a_px);
			ImDrawListManager::AddPolyline(a_list, pts.data(), 3, a_col, 0, 2.0f * a_px);
		}

		// the HUD is up and the game runs (no menus over it)
		bool InGameplay()
		{
			const auto ui = RE::UI::GetSingleton();
			const auto player = RE::PlayerCharacter::GetSingleton();
			return ui && player && player->Is3DLoaded() && ui->IsShowingMenus() && !ui->GameIsPaused() && !Menus::HudCovered();
		}

		struct State
		{
			std::chrono::steady_clock::time_point last{};
			float                                 sampleTimer{ 0.0f };
			std::atomic<bool>                     samplePending{ false };
			float                                 shown{ 0.0f };
			float                                 comfortableFor{ 0.0f };
			float                                 time{ 0.0f };
			float                                 body{ NAN };
		};
		State state;

		// a_needs / a_stress: those too (only while their gauges are on or the settings show them)
		void ScheduleSample(bool a_needs, bool a_stress)
		{
			if (state.samplePending.exchange(true)) {
				return;
			}
			SKSE::GetTaskInterface()->AddTask([a_needs, a_stress] {
				IHud::KeepOpen();
				Temperature::Sample();
				if (a_needs) {
					Needs::Sample();
				}
				if (a_stress) {
					Stress::Sample();
				}
				state.samplePending = false;
			});
		}

		Reading PreviewReading(const Preview& a_preview, float a_time)
		{
			Reading r;
			r.body = a_preview.sweep ? 0.5f + 1.5f * std::sin(a_time * 0.7f) : a_preview.body;
			r.target = a_preview.target;
			r.trend = a_preview.trend;
			r.comfortable = r.body >= 0.0f && r.body <= 1.0f;
			r.danger = r.body < -0.55f || r.body > 1.6f;
			r.stage = Lang::T(r.body < -0.55f ? "Freezing" : r.body < 0.0f ? "Cold" : r.body <= 1.0f ? "Comfortable" : "Hot");
			return r;
		}

		void Draw(const Settings& s, const Reading& a_reading, float a_alpha)
		{
			const auto  io = GetIO();
			const auto  screen = io->DisplaySize;
			if (screen.x <= 0.0f || screen.y <= 0.0f) {
				return;
			}
			const auto  list = GetBackgroundDrawList();
			const float px = screen.y / 1080.0f * s.scale;
			const auto  shape = BuildShape({ s.length, s.curve, s.rotation, s.mirror, s.flip, { s.posX * screen.x, s.posY * screen.y } }, px);
			const float half = s.thickness * px * 0.5f;
			const float border = s.outline * px;
			const ImU32 outline = ToU32(s.outlineColor, a_alpha);
			const Zones zones(s);
			// the ends fade out over the last tenth of the gauge
			const auto fade = [&](float u) { return s.fadeEnds ? SmoothStep(std::min(u, 1.0f - u) / 0.1f) : 1.0f; };

			// gauge
			if (border > 0.0f) {
				Band(list, shape, half + border, s.roundEnds, [&](float u) { return ToU32(s.outlineColor, a_alpha * fade(u)); });
			}
			Band(list, shape, half, s.roundEnds, [&](float u) { return ToU32(ZoneColor(s, zones.T(u)), a_alpha * fade(u)); });
			if (s.zoneLines) {
				const float line = half + border * 0.5f;
				for (const float u : { zones.cold, zones.cold + zones.comfort }) {
					if (u <= 0.0f || u >= 1.0f) {
						continue;
					}
					const ImVec2 p = shape.Point(u);
					const ImVec2 n = shape.Normal(u);
					ImDrawListManager::AddLine(list, p - n * line, p + n * line, ToU32(s.outlineColor, std::min(1.0f, a_alpha * 1.5f)),
						std::max(1.0f, 1.3f * px));
				}
			}

			const float sideSign = s.markerInside ? -1.0f : 1.0f;
			const float edge = half + border + 1.0f * px;
			const float pulse = s.pulseDanger && a_reading.danger ? 1.0f + 0.14f * (0.5f + 0.5f * std::sin(state.time * 7.0f)) : 1.0f;
			const float arrow = 10.0f * px * s.markerSize;
			const float outlineWidth = std::max(1.0f, border * 0.8f);

			// where the body temperature is heading: a small hollow pointer on the other side
			if (s.showTarget && a_reading.target) {
				const float  u = zones.U(*a_reading.target);
				const ImVec2 c = shape.Point(u);
				const ImVec2 m = shape.Normal(u) * -sideSign;
				const ImVec2 t = shape.Tangent(u);
				const ImVec2 tip = c + m * (edge + 1.0f * px);
				const ImVec2 base = tip + m * (arrow * 0.6f);
				const float           w = arrow * 0.35f;
				std::array<ImVec2, 3> tri{ tip, base + t * w, base - t * w };
				MakeClockwise(tri);
				ImDrawListManager::AddTriangleFilled(list, tri[0], tri[1], tri[2], ToU32(s.targetColor, a_alpha * 0.3f));
				ImDrawListManager::AddTriangle(list, tri[0], tri[1], tri[2], outline, 3.0f * px);
				ImDrawListManager::AddTriangle(list, tri[0], tri[1], tri[2], ToU32(s.targetColor, a_alpha), 1.4f * px);
			}

			const float  u = zones.U(state.body);
			const ImVec2 c = shape.Point(u);
			const ImVec2 m = shape.Normal(u) * sideSign;  // towards the arrow
			const ImVec2 t = shape.Tangent(u);
			const float  a = arrow * pulse;
			const ImU32  markerCol = ToU32(s.markerColor, a_alpha);
			const ImVec2 labelDir = m;  // the label goes past the arrow
			float        reach = edge;  // how far out the markers go

			switch (s.marker) {
			case MarkerStyle::kLine:
				{
					const float over = half + border + 3.0f * px * s.markerSize * pulse;
					ImDrawListManager::AddLine(list, c - m * over, c + m * over, outline, 4.5f * px * s.markerSize);
					ImDrawListManager::AddLine(list, c - m * over, c + m * over, markerCol, 2.5f * px * s.markerSize);
					reach = over + 2.0f * px;
					break;
				}
			case MarkerStyle::kArrow:
				{
					Dart(list, c + m * edge, m, t, a * 1.2f, markerCol, outline, outlineWidth * 0.6f);
					reach = edge + a * 1.2f;
					break;
				}
			}

			// warming up / cooling down: chevrons beside the marker pointing along the gauge
			if (s.showTrend && a_reading.trend != 0) {
				const float  dir = a_reading.trend > 0 ? 1.0f : -1.0f;
				const ImVec2 along = t * dir;
				const ImVec2 side{ -along.y, along.x };
				const int    count = std::abs(a_reading.trend);
				const RGBA&  color = a_reading.trend >= 2 ? s.hotEnd : a_reading.trend == 1 ? s.hotEdge : a_reading.trend == -1 ? s.coldEdge : s.coldEnd;
				const float  size = arrow * 0.6f;
				for (int k = 0; k < count; ++k) {
					const float  glow = 0.35f + 0.65f * (0.5f + 0.5f * std::sin(state.time * 5.0f - k * 1.3f));
					const ImVec2 p = c + m * (edge + arrow * 0.5f) + along * (arrow * 1.4f + k * size * 0.9f);
					Chevron(list, p, along, side, size, ToU32(color, a_alpha * glow), ToU32(s.outlineColor, a_alpha * glow), px);
				}
			}

			if (s.showLabel && !a_reading.stage.empty()) {
				// names from a definition file (Frostfall) have translations, spell names are in the game's language
				const std::string stage = Lang::T(a_reading.stage.c_str());
				const float       size = s.labelSize * px;
				const auto        ts = TextSize(size, stage);
				const ImVec2 p = c + labelDir * (reach + 4.0f * px);
				ImVec2       pos;
				if (labelDir.x > 0.35f) {
					pos = { p.x, p.y - ts.y * 0.5f };
				} else if (labelDir.x < -0.35f) {
					pos = { p.x - ts.x, p.y - ts.y * 0.5f };
				} else {
					pos = { p.x - ts.x * 0.5f, labelDir.y > 0.0f ? p.y : p.y - ts.y };
				}
				Text(list, size, pos, stage, ToU32(s.labelColor, a_alpha), ToU32(s.outlineColor, a_alpha * 0.9f));
			}
		}

		// the survival mods' own icons this mod stands in for
		std::vector<std::string> IconsToHide(const Settings& s)
		{
			std::vector<std::string> paths;
			if (s.hideVanillaIcon) {
				paths = Temperature::IconPaths();
			}
			if (s.needs.enabled) {
				const auto needs = Needs::IconPaths(s.needs.show);
				paths.insert(paths.end(), needs.begin(), needs.end());
			}
			if (s.stress.enabled) {
				const auto stress = Stress::IconPaths();
				paths.insert(paths.end(), stress.begin(), stress.end());
			}
			return paths;
		}

		// other mods' SKSE Menu Framework HUDs (Frostfall 2026's, Last Seed 2026's bars) the gauges stand in for
		std::vector<std::string> HudsToHide(const Settings& s)
		{
			std::vector<std::string> huds;
			if (s.enabled && s.hideVanillaIcon && s.visibility != Visibility::kNever) {
				huds = Temperature::SmfHuds();
			}
			if (s.needs.enabled) {
				const auto needs = Needs::SmfHuds(s.needs.show);
				huds.insert(huds.end(), needs.begin(), needs.end());
			}
			return huds;
		}

		void DrawTemperature(const Settings& s, float dt, bool preview, bool gameplay, float hud)
		{
			auto        reading = Temperature::Current();
			const auto& prev = GetPreview();
			if (preview && (prev.enabled || !reading)) {
				reading = PreviewReading(prev, state.time);
			}
			if (!reading || (!preview && !s.enabled)) {
				state.shown = 0.0f;
				return;
			}
			if (!preview && !gameplay) {
				return;  // menus: gone right away, back right away
			}

			bool want = true;
			if (!preview) {
				switch (s.visibility) {
				case Visibility::kNever:
					want = false;
					break;
				case Visibility::kNotComfortable:
					state.comfortableFor = reading->comfortable && reading->trend >= 0 ? state.comfortableFor + dt : 0.0f;
					want = state.comfortableFor < s.hideDelay;
					break;
				default:
					break;
				}
			}
			state.shown = preview ? 1.0f : Approach(state.shown, want ? 1.0f : 0.0f, dt / kFadeTime);

			// the marker eases towards the reading (Survival Mode updates the cold in steps)
			const float target = std::clamp(reading->body, -1.0f, 2.0f);
			if (std::isnan(state.body) || (preview && prev.sweep)) {
				state.body = target;
			} else if (s.smoothing <= 0.0f) {
				state.body = target;
			} else {
				state.body += (target - state.body) * (1.0f - std::exp(-dt * 3.0f / s.smoothing));
			}

			const float alpha = s.opacity * SmoothStep(state.shown) * hud;
			if (alpha <= 0.01f) {
				return;
			}
			Draw(s, *reading, alpha);
		}
	}

	void __stdcall RenderHud()
	{
		UpdatePageNames();

		Settings s;
		{
			std::scoped_lock lock(Config::Lock());
			s = Config::Get();
		}

		const auto  now = std::chrono::steady_clock::now();
		const float dt = state.last == std::chrono::steady_clock::time_point{} ?
		                     0.0f :
		                     std::min(std::chrono::duration<float>(now - state.last).count(), 0.1f);
		state.last = now;
		state.time += dt;

		const bool preview = SettingsOpen();
		state.sampleTimer -= dt;
		if (state.sampleTimer <= 0.0f) {
			state.sampleTimer = kSampleInterval;
			ScheduleSample(s.needs.enabled || preview, Stress::Available() && (s.stress.enabled || s.fear.enabled || preview));
			HudIcons::Update(IconsToHide(s));
			SmfHuds::Hide(HudsToHide(s));
		}

		const bool  gameplay = preview || InGameplay();
		// the HUD's own opacity (old Immersive HUD fades the whole HUD) and what Immersive HUD SKSE gives each group
		const float hud = s.followHud && !preview ? HudIcons::Alpha() : 1.0f;
		const auto  group = [&](IHud::Group a_group) { return s.followHud && !preview ? hud * IHud::Alpha(a_group) : 1.0f; };
		DrawTemperature(s, dt, preview, gameplay, group(IHud::Group::kTemperature));
		DrawNeeds(s, dt, state.time, preview, gameplay, group(IHud::Group::kNeeds));
		DrawStress(s, dt, state.time, preview, gameplay, group(IHud::Group::kStress));
	}
}
