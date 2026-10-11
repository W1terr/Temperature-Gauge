#include "Gauge.h"

#include "Framework.h"
#include "HudIcons.h"
#include "Lang.h"
#include "Menus.h"
#include "Settings.h"
#include "Temperature.h"
#include "UI.h"

namespace UI
{
	using namespace ImGuiMCP;

	namespace
	{
		constexpr float kPi = std::numbers::pi_v<float>;
		constexpr float kSampleInterval = 0.25f;  // seconds between game reads
		constexpr float kFadeTime = 0.35f;
		constexpr int   kSegments = 72;           // along the gauge
		constexpr int   kCapSegments = 10;        // per round end

		ImVec2 operator+(ImVec2 a, ImVec2 b) { return { a.x + b.x, a.y + b.y }; }
		ImVec2 operator-(ImVec2 a, ImVec2 b) { return { a.x - b.x, a.y - b.y }; }
		ImVec2 operator*(ImVec2 a, float f) { return { a.x * f, a.y * f }; }

		ImVec2 Normalized(ImVec2 a)
		{
			const float len = std::sqrt(a.x * a.x + a.y * a.y);
			return len > 1e-6f ? ImVec2{ a.x / len, a.y / len } : ImVec2{ 0.0f, -1.0f };
		}

		float SmoothStep(float x)
		{
			x = std::clamp(x, 0.0f, 1.0f);
			return x * x * (3.0f - 2.0f * x);
		}

		float Approach(float a_value, float a_goal, float a_step)
		{
			return a_value < a_goal ? std::min(a_value + a_step, a_goal) : std::max(a_value - a_step, a_goal);
		}

		ImU32 ToU32(const RGBA& a_color, float a_alpha)
		{
			const auto channel = [](float v) { return static_cast<ImU32>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f); };
			return channel(a_color[0]) | (channel(a_color[1]) << 8) | (channel(a_color[2]) << 16) |
			       (channel(a_color[3] * a_alpha) << 24);
		}

		RGBA Mix(const RGBA& a, const RGBA& b, float t)
		{
			return { std::lerp(a[0], b[0], t), std::lerp(a[1], b[1], t), std::lerp(a[2], b[2], t), std::lerp(a[3], b[3], t) };
		}

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

		// The gauge's center line in screen space, sampled from the cold end (u = 0) to the hot end (u = 1)
		struct Shape
		{
			std::array<ImVec2, kSegments + 1> point;
			std::array<ImVec2, kSegments + 1> normal;  // away from the center of the curve

			template <class T>
			static T Sample(const std::array<T, kSegments + 1>& a_values, float u)
			{
				const float f = std::clamp(u, 0.0f, 1.0f) * kSegments;
				const int   i = std::min(static_cast<int>(f), kSegments - 1);
				const float t = f - i;
				return a_values[i] * (1.0f - t) + a_values[i + 1] * t;
			}

			ImVec2 Point(float u) const { return Sample(point, u); }
			ImVec2 Normal(float u) const { return Normalized(Sample(normal, u)); }
			ImVec2 Tangent(float u) const  // towards the hot end
			{
				const float f = std::clamp(u, 0.0f, 1.0f) * kSegments;
				const int   i = std::min(static_cast<int>(f), kSegments - 1);
				return Normalized(point[i + 1] - point[i]);
			}
		};

		Shape BuildShape(const Settings& s, float a_px, ImVec2 a_screen)
		{
			// Upright and unrotated: the hot end on top, bending so the curve's center lies to the right
			// (like the Crimson Desert gauge), the middle of the gauge at the origin.
			const float length = s.length * a_px;
			const float angle = s.curve * kPi / 180.0f;
			Shape       shape;
			for (int i = 0; i <= kSegments; ++i) {
				float g = static_cast<float>(i) / kSegments;
				if (s.flip) {
					g = 1.0f - g;
				}
				ImVec2 p, n;
				if (angle < 1e-3f) {
					p = { 0.0f, -(g - 0.5f) * length };
					n = { -1.0f, 0.0f };
				} else {
					const float radius = length / angle;
					const float theta = kPi + (g - 0.5f) * angle;
					n = { std::cos(theta), std::sin(theta) };
					p = { radius + radius * n.x, radius * n.y };
				}
				if (s.mirror) {
					p.x = -p.x;
					n.x = -n.x;
				}
				shape.point[i] = p;
				shape.normal[i] = n;
			}
			// center the bounding box on the configured position
			ImVec2 lo{ FLT_MAX, FLT_MAX }, hi{ -FLT_MAX, -FLT_MAX };
			for (const auto& p : shape.point) {
				lo = { std::min(lo.x, p.x), std::min(lo.y, p.y) };
				hi = { std::max(hi.x, p.x), std::max(hi.y, p.y) };
			}
			const ImVec2 mid = (lo + hi) * 0.5f;
			const float  r = s.rotation * kPi / 180.0f;
			const float  cs = std::cos(r), sn = std::sin(r);
			const auto   rotate = [&](ImVec2 v) { return ImVec2{ v.x * cs - v.y * sn, v.x * sn + v.y * cs }; };
			const ImVec2 origin{ s.posX * a_screen.x, s.posY * a_screen.y };
			for (int i = 0; i <= kSegments; ++i) {
				shape.point[i] = origin + rotate(shape.point[i] - mid);
				shape.normal[i] = rotate(shape.normal[i]);
			}
			return shape;
		}

		// Writes vertices / indices straight into the draw list (per vertex colors for the gradient)
		class Mesh
		{
		public:
			Mesh(ImDrawList* a_list, int a_indices, int a_vertices) :
				_list(a_list), _indices(a_indices), _vertices(a_vertices)
			{
				ImDrawListManager::PrimReserve(_list, a_indices, a_vertices);
				_base = _list->_VtxCurrentIdx;  // after PrimReserve: it may start a new vertex offset
				_uv = GetFontTexUvWhitePixel();
			}
			~Mesh()
			{
				// whatever wasn't written goes back (shouldn't happen)
				const int idxLeft = _indices - _idxWritten;
				const int vtxLeft = _vertices - _vtxWritten;
				_list->_VtxCurrentIdx += _vtxWritten;
				if (idxLeft > 0 || vtxLeft > 0) {
					ImDrawListManager::PrimUnreserve(_list, idxLeft, vtxLeft);
				}
			}
			Mesh(const Mesh&) = delete;
			Mesh& operator=(const Mesh&) = delete;

			int Vertex(ImVec2 a_pos, ImU32 a_col)
			{
				auto* v = _list->_VtxWritePtr++;
				v->pos = a_pos;
				v->uv = _uv;
				v->col = a_col;
				return _vtxWritten++;
			}
			void Triangle(int a, int b, int c)
			{
				*_list->_IdxWritePtr++ = static_cast<ImDrawIdx>(_base + a);
				*_list->_IdxWritePtr++ = static_cast<ImDrawIdx>(_base + b);
				*_list->_IdxWritePtr++ = static_cast<ImDrawIdx>(_base + c);
				_idxWritten += 3;
			}
			void Quad(int a, int b, int c, int d)
			{
				Triangle(a, b, c);
				Triangle(a, c, d);
			}

		private:
			ImDrawList*  _list;
			int          _indices;
			int          _vertices;
			int          _idxWritten{ 0 };
			int          _vtxWritten{ 0 };
			unsigned int _base{ 0 };
			ImVec2       _uv{};
		};

		// A band along the shape, a_halfWidth to both sides, anti-aliased edges, optional round ends.
		// a_color(u) gives the color at u (0 cold end .. 1 hot end).
		template <class ColorFn>
		void Band(ImDrawList* a_list, const Shape& a_shape, float a_halfWidth, bool a_round, ColorFn&& a_color)
		{
			const float solid = std::max(a_halfWidth - 0.5f, 0.0f);
			const float fringe = a_halfWidth + 0.5f;
			const int   capVertices = a_round ? 2 * (1 + 2 * (kCapSegments + 1)) : 0;
			const int   capIndices = a_round ? 2 * 9 * kCapSegments : 0;
			Mesh        mesh(a_list, 18 * kSegments + capIndices, 4 * (kSegments + 1) + capVertices);

			for (int i = 0; i <= kSegments; ++i) {
				const auto   col = a_color(static_cast<float>(i) / kSegments);
				const auto   clear = col & 0x00FFFFFF;
				const ImVec2 p = a_shape.point[i];
				const ImVec2 n = a_shape.normal[i];
				const int    v = mesh.Vertex(p + n * fringe, clear);
				mesh.Vertex(p + n * solid, col);
				mesh.Vertex(p - n * solid, col);
				mesh.Vertex(p - n * fringe, clear);
				if (i > 0) {
					const int prev = v - 4;
					mesh.Quad(prev, v, v + 1, prev + 1);
					mesh.Quad(prev + 1, v + 1, v + 2, prev + 2);
					mesh.Quad(prev + 2, v + 2, v + 3, prev + 3);
				}
			}
			if (!a_round) {
				return;
			}
			for (const float u : { 0.0f, 1.0f }) {
				const auto   col = a_color(u);
				const auto   clear = col & 0x00FFFFFF;
				const ImVec2 p = a_shape.Point(u);
				const ImVec2 n = a_shape.Normal(u);
				const ImVec2 out = a_shape.Tangent(u) * (u > 0.5f ? 1.0f : -1.0f);
				const int    center = mesh.Vertex(p, col);
				int          last = -1;
				for (int k = 0; k <= kCapSegments; ++k) {
					const float  phi = kPi * k / kCapSegments;
					const ImVec2 dir = n * std::cos(phi) + out * std::sin(phi);
					const int    v = mesh.Vertex(p + dir * solid, col);
					mesh.Vertex(p + dir * fringe, clear);
					if (last >= 0) {
						mesh.Triangle(center, last, v);
						mesh.Quad(last, last + 1, v + 1, v);
					}
					last = v;
				}
			}
		}

		// ImGui fills (and anti-aliases) polygons whose points go clockwise on screen; the other way round its concave
		// fill closes the dart's notch (a blunt triangle when the arrow sat on the other side of the gauge)
		template <std::size_t N>
		void MakeClockwise(std::array<ImVec2, N>& a_points)
		{
			float area = 0.0f;
			for (std::size_t i = 0; i < N; ++i) {
				const auto& a = a_points[i];
				const auto& b = a_points[(i + 1) % N];
				area += a.x * b.y - b.x * a.y;
			}
			if (area < 0.0f) {
				std::ranges::reverse(a_points);
			}
		}

		// Crimson Desert's arrowhead: a notched dart with its tip at a_tip, its body along a_out (pointing back at the gauge)
		void Dart(ImDrawList* a_list, ImVec2 a_tip, ImVec2 a_out, ImVec2 a_side, float a_length, ImU32 a_fill, ImU32 a_outline, float a_border)
		{
			const float           width = a_length * 0.8f;
			std::array<ImVec2, 4> pts{ a_tip, a_tip + a_out * a_length + a_side * width, a_tip + a_out * (a_length * 0.55f),
				a_tip + a_out * a_length - a_side * width };
			MakeClockwise(pts);
			if (a_border > 0.0f) {
				ImDrawListManager::AddPolyline(a_list, pts.data(), 4, a_outline, ImDrawFlags_Closed, a_border * 2.0f);
			}
			ImDrawListManager::AddConcavePolyFilled(a_list, pts.data(), 4, a_fill);
		}

		void Chevron(ImDrawList* a_list, ImVec2 a_center, ImVec2 a_dir, ImVec2 a_side, float a_size, ImU32 a_col, ImU32 a_outline, float a_px)
		{
			const std::array<ImVec2, 3> pts{ a_center - a_dir * (a_size * 0.45f) + a_side * (a_size * 0.6f),
				a_center + a_dir * (a_size * 0.45f), a_center - a_dir * (a_size * 0.45f) - a_side * (a_size * 0.6f) };
			ImDrawListManager::AddPolyline(a_list, pts.data(), 3, a_outline, 0, 3.6f * a_px);
			ImDrawListManager::AddPolyline(a_list, pts.data(), 3, a_col, 0, 2.0f * a_px);
		}

		void Text(ImDrawList* a_list, float a_size, ImVec2 a_pos, const std::string& a_text, ImU32 a_col, ImU32 a_shadow)
		{
			const auto  font = GetFont();
			const float o = std::max(1.0f, a_size / 14.0f);
			for (const auto& d : { ImVec2{ -o, 0 }, ImVec2{ o, 0 }, ImVec2{ 0, -o }, ImVec2{ 0, o } }) {
				ImDrawListManager::AddText(a_list, font, a_size, a_pos + d, a_shadow, a_text.c_str());
			}
			ImDrawListManager::AddText(a_list, font, a_size, a_pos, a_col, a_text.c_str());
		}

		ImVec2 TextSize(float a_size, const std::string& a_text)
		{
			const auto  base = CalcTextSize(a_text.c_str());
			const float scale = a_size / std::max(1.0f, GetFontSize());
			return { base.x * scale, base.y * scale };
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

		void ScheduleSample()
		{
			if (state.samplePending.exchange(true)) {
				return;
			}
			SKSE::GetTaskInterface()->AddTask([] {
				Temperature::Sample();
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
			const auto  shape = BuildShape(s, px, screen);
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

		state.sampleTimer -= dt;
		if (state.sampleTimer <= 0.0f) {
			state.sampleTimer = kSampleInterval;
			ScheduleSample();
			HudIcons::Update(s.hideVanillaIcon);
		}

		const bool preview = SettingsOpen();
		auto       reading = Temperature::Current();
		const auto& prev = GetPreview();
		if (preview && (prev.enabled || !reading)) {
			reading = PreviewReading(prev, state.time);
		}
		if (!reading || (!preview && !s.enabled)) {
			state.shown = 0.0f;
			return;
		}
		if (!preview && !InGameplay()) {
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

		const float hud = s.followHud && !preview ? HudIcons::Alpha() : 1.0f;
		const float alpha = s.opacity * SmoothStep(state.shown) * hud;
		if (alpha <= 0.01f) {
			return;
		}
		Draw(s, *reading, alpha);
	}
}
