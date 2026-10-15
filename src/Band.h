#pragma once

#include "Draw.h"

// The gauge's geometry and pieces, shared by the temperature gauge and the needs bars (render thread): a center line
// (straight or bent), a band along it with per-vertex colors, Crimson Desert's notched arrowhead.
namespace UI
{
	inline constexpr int kSegments = 72;     // along the gauge
	inline constexpr int kCapSegments = 10;  // per round end

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

	// Where and how a gauge lies: length in pixels at 1080p, degrees it bends through, degrees clockwise (0 = upright,
	// the far end on top), which way it bends, which end is which, its middle on screen
	struct ShapeSpec
	{
		float  length;
		float  curve;
		float  rotation;
		bool   mirror;
		bool   flip;
		ImVec2 origin;
	};

	inline Shape BuildShape(const ShapeSpec& s, float a_px)
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
		const ImVec2 origin = s.origin;
		for (int i = 0; i <= kSegments; ++i) {
			shape.point[i] = origin + rotate(shape.point[i] - mid);
			shape.normal[i] = rotate(shape.normal[i]);
		}
		return shape;
	}

	// Clockwise degrees from 12 o'clock -> direction on screen (y down)
	inline ImVec2 Clock(float a_degrees)
	{
		const float a = a_degrees * kPi / 180.0f;
		return { std::sin(a), -std::cos(a) };
	}

	inline float ClockAngle(ImVec2 a_dir)
	{
		return std::atan2(a_dir.x, -a_dir.y) * 180.0f / kPi;
	}

	// A piece of a circle: u = 0 at a_from, u = 1 at a_to (clockwise degrees from 12 o'clock), normals outwards
	inline Shape ArcShape(ImVec2 a_center, float a_radius, float a_from, float a_to)
	{
		Shape shape;
		for (int i = 0; i <= kSegments; ++i) {
			const ImVec2 dir = Clock(std::lerp(a_from, a_to, static_cast<float>(i) / kSegments));
			shape.point[i] = a_center + dir * a_radius;
			shape.normal[i] = dir;
		}
		return shape;
	}

	// The circle a bent gauge lies on (nullopt while it's straight) and the clockwise angles of its middle and ends
	struct Circle
	{
		ImVec2 center;
		float  radius;
		float  middle;  // degrees from 12 o'clock
	};

	inline std::optional<Circle> ShapeCircle(const ShapeSpec& a_spec, float a_px)
	{
		if (a_spec.curve < 1.0f) {
			return std::nullopt;
		}
		const auto  shape = BuildShape(a_spec, a_px);
		const float radius = a_spec.length * a_px / (a_spec.curve * kPi / 180.0f);
		const auto  mid = shape.Point(0.5f);
		const auto  out = shape.Normal(0.5f);  // away from the center
		const auto  center = mid - out * radius;
		return Circle{ center, radius, ClockAngle(out) };
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
	inline void Band(ImDrawList* a_list, const Shape& a_shape, float a_halfWidth, bool a_round, ColorFn&& a_color)
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

	// Crimson Desert's arrowhead: a notched dart with its tip at a_tip, its body along a_out (pointing back at the gauge)
	inline void Dart(ImDrawList* a_list, ImVec2 a_tip, ImVec2 a_out, ImVec2 a_side, float a_length, ImU32 a_fill, ImU32 a_outline, float a_border)
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
}
