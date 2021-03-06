#pragma once

#include <cstdint>

#include "och_fio.h"

#include "och_matmath.h"

#include "och_err.h"


struct glyph_metrics
{
	float m_x_min;

	float m_x_max;

	float m_y_min;

	float m_y_max;

	float m_advance_width;

	float m_left_side_bearing;

	glyph_metrics(float x_min, float x_max, float y_min, float y_max, float advance_width, float left_side_bearing);

	float x_min() const noexcept;

	float x_max() const noexcept;

	float y_min() const noexcept;

	float y_max() const noexcept;

	float x_size() const noexcept;

	float y_size() const noexcept;

	float advance_width() const noexcept;

	float left_side_bearing() const noexcept;

	float right_side_bearing() const noexcept;
};

struct glyph_data
{
private:

	glyph_metrics m_metrics;

	uint32_t m_point_cnt;

	uint32_t m_contour_cnt;

	och::vec2* m_points;

public:

	glyph_data() noexcept;

	glyph_data(uint32_t contour_cnt, uint32_t point_cnt, glyph_metrics metrics, och::vec2* raw_data_ownership_transferred) noexcept;

	~glyph_data() noexcept;

	const och::vec2& operator[](uint32_t point_idx) const noexcept;

	och::vec2& operator[](uint32_t point_idx) noexcept;

	const uint32_t* contour_end_indices() const noexcept;

	const glyph_metrics& metrics() const noexcept;

	uint32_t contour_cnt() const noexcept;

	uint32_t point_cnt() const noexcept;

	uint32_t contour_beg_index(uint32_t contour_idx) const noexcept;

	uint32_t contour_end_index(uint32_t contour_idx) const noexcept;
};




struct truetype_file
{
private:

	struct ttf_file_header
	{
		uint32_t version;
		uint16_t num_tables;
		uint16_t search_range;
		uint16_t entry_selector;
		uint16_t range_shift;
	};

	struct table_tag
	{
		char cs[4];

		table_tag(const char* name) noexcept : cs{ name[0], name[1], name[2], name[3] } {}

		bool operator==(const table_tag& rhs) const noexcept
		{
			for (uint32_t i = 0; i != 4; ++i)
				if (cs[i] != rhs.cs[i])
					return false;

			return true;
		}

		bool operator<(const table_tag& rhs) const noexcept
		{
			for (uint32_t i = 0; i != 4; ++i)
				if (cs[i] != rhs.cs[i])
					return cs[i] < rhs.cs[i];

			return false;
		}
	};



	struct head_table_data
	{
		uint16_t major_version;
		uint16_t minor_version;
		uint32_t revision;
		uint32_t checksum_adjustment;
		uint32_t magic_number;
		uint16_t flags;
		uint16_t units_per_em;
		uint32_t created_hi;
		uint32_t created_lo;
		uint32_t modified_hi;
		uint32_t modified_lo;
		int16_t x_min;
		int16_t y_min;
		int16_t x_max;
		int16_t y_max;
		uint16_t mac_style;
		uint16_t lowest_rec_ppem;
		int16_t font_direction_hints;
		int16_t index_to_loc_format;
		int16_t glyph_data_format;
	};

	struct maxp_table_data
	{
		uint16_t major_version;
		uint16_t minor_version;
		uint16_t num_glyphs;
		uint16_t max_points;
		uint16_t max_contours;
		uint16_t max_composite_points;
		uint16_t max_composite_contours;
		uint16_t max_zones;
		uint16_t max_twilight_points;
		uint16_t max_storage;
		uint16_t max_function_defs;
		uint16_t max_instruction_defs;
		uint16_t max_stack_elements;
		uint16_t max_size_of_instructions;
		uint16_t max_component_elemenents;
		uint16_t max_component_depth;
	};

	struct hhea_table_data
	{
		uint16_t major_version;
		uint16_t minor_version;
		int16_t ascender;
		int16_t descender;
		int16_t line_gap;
		uint16_t advance_width_max;
		int16_t min_left_side_bearing;
		int16_t min_right_side_bearing;
		int16_t x_max_extent;
		int16_t caret_slope_rise;
		int16_t caret_slope_run;
		int16_t caret_offset;
		int16_t reserved_0;
		int16_t reserved_1;
		int16_t reserved_2;
		int16_t reserved_3;
		int16_t metric_data_format;
		uint16_t number_of_h_metrics;
	};

	struct cmap_table_data
	{
		uint16_t version;
		uint16_t num_tables;
	};

	#pragma pack(push, 1)
	struct os_2_table_data
	{
		uint16_t version;
		int16_t x_avg_char_width;
		uint16_t us_weight_class;
		uint16_t us_width_class;
		uint16_t fs_type;
		int16_t y_subscript_x_size;
		int16_t y_subscript_y_size;
		int16_t y_subscript_x_offset;
		int16_t y_subscript_y_offset;
		int16_t y_superscript_x_size;
		int16_t y_superscript_y_size;
		int16_t y_superscript_x_offset;
		int16_t y_superscript_y_offset;
		int16_t y_strikeout_size;
		int16_t y_strikeout_position;
		int16_t family_class;
		uint8_t panose[10];
		uint32_t unicode_range_1;
		uint32_t unicode_range_2;
		uint32_t unicode_range_3;
		uint32_t unicode_range_4;
		uint8_t vendor_id[4];
		uint16_t fs_selection;
		uint16_t first_char_index;
		uint16_t last_char_index;
		int16_t typo_ascender;
		int16_t typo_descender;
		int16_t typo_line_gap;
		uint16_t win_ascent;
		uint16_t win_descent;
		uint32_t code_page_range_1;
		uint32_t code_page_range_2;
		int16_t x_height;
		int16_t cap_height;
		uint16_t default_char;
		uint16_t break_char;
		uint16_t max_context;
		uint16_t lower_optical_point_size;
		uint16_t upper_optical_point_size;
	};
#pragma pack(pop)



	struct glyph_header
	{
		int16_t num_contours;
		int16_t x_min;
		int16_t y_min;
		int16_t x_max;
		int16_t y_max;
	};

	struct internal_glyph_data
	{
		uint32_t point_cnt = 0;
		uint32_t contour_cnt = 0;

	private:

		och::vec2* m_points = nullptr;

	public:

		och::vec2* points() noexcept;

		uint32_t* contour_end_indices() noexcept;

		void set_on_curve(uint32_t point_idx) noexcept;

		void set_off_curve(uint32_t point_idx) noexcept;

		bool is_on_line(uint32_t point_idx) noexcept;

		void create(uint32_t point_cnt_, uint32_t contour_cnt_) noexcept;

		void destroy() noexcept;

		glyph_data to_glyph_data(glyph_metrics metrics, float global_x_min, float global_y_min) noexcept;

		void translate(float dx, float dy) noexcept;

		void scale(float sx, float sy) noexcept;

		void transform(float xx, float xy, float yx, float yy) noexcept;
	};

	struct codepoint_mapper_data
	{
		using cmap_fn = uint32_t(*) (const void*, uint32_t) noexcept;

		cmap_fn mapper;

		const void* data;

		uint32_t map_codept_to_glyph_id(char32_t cpt) const noexcept;
	};



	och::mapped_file<ttf_file_header> m_file;

	uint32_t m_table_cnt;

	uint32_t m_glyph_cnt;

	uint32_t m_full_horizontal_layout_cnt;

	uint32_t m_max_composite_glyph_cnt;

	float m_normalization_factor;

	float m_x_min_global;

	float m_y_min_global;

	float m_line_height;

	codepoint_mapper_data m_codepoint_mapper;

	const void* m_loca_tbl;

	const void* m_glyf_tbl;

	const void* m_hmtx_tbl;

	struct
	{
		bool full_glyph_offsets : 1;
		bool is_valid_file : 1;
	} m_flags{};

public:

	och::status create(const char* filename) noexcept;

	void close() noexcept;

	float baseline_offset() const noexcept;

	float line_height() const noexcept;

	glyph_data get_glyph_data_from_codepoint(char32_t codepoint) const noexcept;

	glyph_metrics get_glyph_metrics_from_codept(char32_t codepoint) const noexcept;

	uint32_t get_glyph_id_from_codept(char32_t codepoint) const noexcept;

	glyph_data get_glyph_data_from_id(uint32_t glyph_id) const noexcept;

	glyph_metrics get_glyph_metrics_from_id(uint32_t glyph_id) const noexcept;

	operator bool() const noexcept;

private:

	const void* get_table(table_tag tag);

	internal_glyph_data get_glyph_data_recursive(uint32_t glyph_id, uint32_t& out_glyph_id_for_metrics_to_use) const noexcept;

	const och::vec2& find_glyph_point_in_internal_composite(internal_glyph_data* components, uint16_t point_idx) const noexcept;

	const glyph_header* find_glyph(uint32_t glyph_id) const noexcept;

	glyph_metrics internal_get_glyph_metrics(uint32_t glyph_id) const noexcept;

	static bool is_truetype_file(const och::mapped_file<ttf_file_header>& file) noexcept;

	static codepoint_mapper_data query_codepoint_mapping(const void* cmap_table) noexcept;
};
