#pragma once

#include "och_err.h"
#include "och_matmath.h"
#include "truetype.h"
#include "image_view.h"
#include "heap_buffer.h"

struct glyph_atlas
{
public:

	struct codept_range
	{
		uint32_t beg;

		uint32_t cnt;
	};

	struct glyph_index
	{
		och::vec2 atlas_position;

		och::vec2 atlas_extent;

		och::vec2 real_extent;

		och::vec2 real_bearing;

		float real_advance;
	};

private:

	struct mapper_range
	{
		uint32_t beg;
		uint32_t end;
		int32_t offset;
	};

	uint32_t m_width = 0;

	uint32_t m_height = 0;

	uint32_t m_glyph_scale = 0;

	float m_line_height = 0.0F;

	heap_buffer<uint8_t> m_image;

	heap_buffer<mapper_range> m_map_ranges;

	heap_buffer<glyph_index> m_map_indices;

public:

	// TODO: 
	// Calculate advance to save in m_map_indices
	// Implement mapping equivalent glyphs to a single spot in the image.
	och::status create(const char* truetype_filename, uint32_t glyph_size, uint32_t glyph_padding_pixels, float sdf_clamp, uint32_t map_width, const och::range<codept_range> codept_ranges) noexcept;

	void destroy() noexcept;

	och::status save_glfatl(const char* filename, bool overwrite_existing_file = false) const noexcept;

	och::status load_glfatl(const char* filename) noexcept;

	och::status save_bmp(const char* filename, bool overwrite_existing_file = false) const noexcept;

	float line_height() const noexcept;

	uint32_t width() const noexcept;

	uint32_t height() const noexcept;

	uint32_t glyph_scale() const noexcept;

	uint8_t* data() noexcept;

	const uint8_t* raw_data() const noexcept;

	image_view<uint8_t> view() noexcept;

	image_view<const uint8_t> view() const noexcept;

	glyph_index operator()(uint32_t codepoint) const noexcept;

	och::range<const uint8_t> get_mapper_ranges() const noexcept;

	och::range<const uint8_t> get_mapper_indices() const noexcept;

	~glyph_atlas() noexcept;
};
