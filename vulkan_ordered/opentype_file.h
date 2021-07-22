#include <cstdint>
#include <cassert>

#include "och_fio.h"

#include "och_fmt.h"

__forceinline uint16_t be_to_le(uint16_t v) noexcept
{
	return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8);
}

__forceinline int16_t be_to_le(int16_t v) noexcept
{
	return ((v & 0x00FF) << 8) | ((v & 0xFF00) >> 8);
}

__forceinline uint32_t be_to_le(uint32_t v) noexcept
{
	return ((v & 0x000000FF) << 24) | ((v & 0x0000FF00) << 8) | ((v & 0x00FF0000) >> 8) | ((v & 0xFF000000) >> 24);
}

__forceinline int32_t be_to_le(int32_t v) noexcept
{
	return ((v & 0x000000FF) << 24) | ((v & 0x0000FF00) << 8) | ((v & 0x00FF0000) >> 8) | ((v & 0xFF000000) >> 24);
}


namespace fnt
{
	struct point
	{
		float m_x, m_y;

		float x() const noexcept
		{
			uint32_t ix;

			memcpy(&ix, &m_x, 4);

			ix &= ~(1 << 31);

			float rx;

			memcpy(&rx, &ix, 4);

			return rx;
		}

		float y() const noexcept { return m_y; }

		bool is_on_line() const noexcept { return m_x < 0.0F; }
	};

	struct contour
	{
		const uint16_t m_point_cnt;
		const point* m_points;
		const uint8_t* m_is_on_line;

		point operator[](uint16_t point_idx) const noexcept { return m_points[point_idx]; }
	};

	struct glyph_data
	{
		float m_x_min;

		float m_x_max;

		float m_y_min;

		float m_y_max;

		float m_advance_width;

		float m_left_side_bearing;

		uint16_t m_point_cnt;

		uint16_t m_contour_cnt;

		point* m_points;

		uint16_t* m_contour_end_indices;

		glyph_data(uint16_t contour_cnt, uint16_t point_cnt, float x_min, float x_max, float y_min, float y_max, float advance_width, float left_side_bearing) noexcept : 
			m_point_cnt{ point_cnt }, 
			m_contour_cnt{ contour_cnt }, 
			m_points{ point_cnt || contour_cnt ? static_cast<point*>(malloc(point_cnt * sizeof(point) + contour_cnt * sizeof(uint16_t))) : nullptr }, 
			m_contour_end_indices{ reinterpret_cast<uint16_t*>(m_points + point_cnt) },
			m_x_min{ x_min },
			m_x_max{ x_max },
			m_y_min{ y_min },
			m_y_max{ y_max },
			m_advance_width{advance_width},
			m_left_side_bearing{left_side_bearing}
		{}

		glyph_data() noexcept :
			m_point_cnt{ 0 },
			m_contour_cnt{ 0 },
			m_points{ nullptr },
			m_contour_end_indices{ nullptr },
			m_x_min{ 0 },
			m_x_max{ 0 },
			m_y_min{ 0 },
			m_y_max{ 0 },
			m_advance_width{0},
			m_left_side_bearing{0}
		{}

		glyph_data(glyph_data&& rvalue) noexcept :
			m_point_cnt{ rvalue.m_point_cnt },
			m_contour_cnt{ rvalue.m_contour_cnt },
			m_points{ rvalue.m_points },
			m_contour_end_indices{ rvalue.m_contour_end_indices },
			m_x_min{ rvalue.m_x_min },
			m_x_max{ rvalue.m_x_max },
			m_y_min{ rvalue.m_y_min },
			m_y_max{ rvalue.m_y_max },
			m_advance_width{ rvalue.m_advance_width },
			m_left_side_bearing{ rvalue.m_left_side_bearing }
		{ rvalue.m_points = nullptr; }

		~glyph_data() noexcept { if(m_points) free(m_points); }

		contour get_contour(uint16_t contour_idx) const noexcept
		{
			uint16_t beg_point = contour_idx ? m_contour_end_indices[contour_idx - 1] : 0;

			return { static_cast<uint16_t>(m_contour_end_indices[contour_idx] - beg_point), m_points + beg_point };
		}

		point get_point(uint16_t point_idx) const noexcept { return m_points[point_idx]; }

		float x_min() const noexcept { return m_x_min; }

		float x_max() const noexcept { return m_x_max; }
		
		float y_min() const noexcept { return m_y_min; }
		
		float y_max() const noexcept { return m_y_max; }

		float advance_width() const noexcept { return m_advance_width; }

		float left_side_bearing() const noexcept { return m_left_side_bearing; }

		uint16_t contour_cnt() const noexcept { return m_contour_cnt; }

		uint16_t point_cnt() const noexcept { return m_point_cnt; }
	};
}

struct file_header
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

enum class file_type
{
	invalid,
	opentype,
	truetype
};



struct opentype_file
{
private:

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
	
	union loca_table_data
	{
		const uint16_t* short_offsets;
		const uint32_t* full_offsets;
	};

	using glyf_fn = fnt::glyph_data(*) (const void*, uint32_t) noexcept;

	struct codepoint_mapper_data
	{
		using cmap_fn = uint32_t(*) (const void*, uint32_t) noexcept;

		cmap_fn mapper;
		const void* data;

		static uint32_t cmap_f4(const void* raw_tbl, uint32_t cpt) noexcept
		{
			if (cpt > 0xFFFF)
				return 0;

			uint16_t short_cpt = static_cast<uint16_t>(cpt);

			struct f4_tbl
			{
				uint16_t format;
				uint16_t length;
				uint16_t language;
				uint16_t seg_cnt_x2;
				uint16_t search_range;
				uint16_t entry_selector;
				uint16_t range_shift;
			};

			const f4_tbl* tbl = static_cast<const f4_tbl*>(raw_tbl);

			uint16_t seg_cnt = be_to_le(tbl->seg_cnt_x2) >> 1;

			const uint16_t* end_codes = reinterpret_cast<const uint16_t*>(tbl + 1);

			const uint16_t* beg_codes = end_codes + seg_cnt + 1;

			const uint16_t* id_deltas = beg_codes + seg_cnt;

			const uint16_t* id_range_offsets = id_deltas + seg_cnt;

			uint32_t lo = 0, hi = static_cast<uint32_t>(seg_cnt - 1);

			while (lo <= hi)
			{
				uint32_t mid = (lo + hi) >> 1;

				uint16_t beg_code = be_to_le(beg_codes[mid]);

				uint16_t end_code = be_to_le(end_codes[mid]);

				if (beg_code <= short_cpt && end_code >= short_cpt)
				{
					if (id_range_offsets[mid]) // Zefix noamoi??
					{
						uint16_t id_offset = be_to_le(id_range_offsets[mid]) >> 1;

						uint16_t cp_offset = short_cpt - beg_code;

						const uint16_t* base = id_range_offsets + mid;

						const uint16_t* id_ptr = base + id_offset + cp_offset;

						uint16_t id = be_to_le(*id_ptr);

						if (id)
							id += be_to_le(id_deltas[mid]);

						return id;
					}
					else
						return short_cpt - be_to_le(id_deltas[mid]);
				}
				else if (beg_code > short_cpt)
					hi = mid - 1;
				else if (end_code < short_cpt)
					lo = mid + 1;
				else
					return 0;
			}

			return 0;
		}

		static uint32_t cmap_f12(const void* raw_tbl, uint32_t cpt) noexcept
		{
			struct f12_tbl
			{
				uint16_t format;
				uint16_t reserved;
				uint32_t length;
				uint32_t language;
				uint32_t num_groups;
			};

			const f12_tbl* hdr = static_cast<const f12_tbl*>(raw_tbl);

			struct f12_group
			{
				uint32_t beg_charcode;
				uint32_t end_charcode;
				uint32_t beg_glyph_id;
			};

			const f12_group* groups = reinterpret_cast<const f12_group*>(hdr + 1);

			uint32_t lo = 0, hi = be_to_le(hdr->length) - 1;

			while (lo <= hi)
			{
				uint32_t mid = lo + ((hi - lo) >> 1);

				if (be_to_le(groups[mid].beg_charcode) >= cpt && be_to_le(groups[mid].end_charcode) <= cpt)
					return groups[mid].beg_glyph_id + cpt - groups[mid].beg_charcode;
				else if (be_to_le(groups[mid].beg_charcode) > cpt)
					hi = mid - 1;
				else if (be_to_le(groups[mid].end_charcode) < cpt)
					lo = mid + 1;
				else
					return 0;
			}

			return 0;
		}
	};



	och::mapped_file<file_header> m_file;

	file_type m_file_type = file_type::invalid;

	uint32_t m_table_cnt;

	uint32_t m_glyph_cnt;

	float m_normalization_factor;

	float m_x_min_global;

	float m_y_min_global;

	uint32_t m_full_horizontal_layout_cnt;

	codepoint_mapper_data m_codepoint_mapper;

	loca_table_data m_glyph_offsets;

	const void* m_glyph_data;

	const void* m_horizontal_layout_data;

	struct
	{
		bool full_glyph_offsets : 1;
	} m_flags{};

public:

	opentype_file(const char* filename) noexcept : m_file{ filename, och::fio::access_read, och::fio::open_normal, och::fio::open_fail, 0, 0, och::fio::share_read_write_delete }
	{
		const file_type tentative_file_type = query_file_type(m_file);

		if (tentative_file_type == file_type::invalid)
			return;

		m_table_cnt = be_to_le(m_file[0].num_tables);

		// Load tables

		m_glyph_offsets.short_offsets = get_table<uint16_t>("loca");

		m_glyph_data = get_table("glyf");

		m_horizontal_layout_data = get_table("hmtx");

		const head_table_data* head_tbl = get_table<head_table_data>("head");

		const maxp_table_data* maxp_tbl = get_table<maxp_table_data>("maxp");

		const hhea_table_data* hhea_tbl = get_table<hhea_table_data>("hhea");

		const cmap_table_data* cmap_tbl = get_table<cmap_table_data>("cmap");

		if (!maxp_tbl || !head_tbl || !hhea_tbl || !cmap_tbl || !m_glyph_offsets.short_offsets || !m_glyph_data || !m_horizontal_layout_data)
			return;

		if(!(m_codepoint_mapper = query_codepoint_mapping(cmap_tbl)).data)
			return;

		m_flags.full_glyph_offsets = head_tbl->index_to_loc_format;

		const int32_t max_x_sz = be_to_le(head_tbl->x_max) - be_to_le(head_tbl->x_min);

		const int32_t max_y_sz = be_to_le(head_tbl->y_max) - be_to_le(head_tbl->y_min);

		m_normalization_factor = 1.0F / static_cast<float>(max_x_sz > max_y_sz ? max_x_sz : max_y_sz);

		m_x_min_global = be_to_le(head_tbl->x_min) * m_normalization_factor;

		m_y_min_global = be_to_le(head_tbl->y_min) * m_normalization_factor;

		m_full_horizontal_layout_cnt = be_to_le(hhea_tbl->number_of_h_metrics);

		m_glyph_cnt = be_to_le(maxp_tbl->num_glyphs);

		m_file_type = tentative_file_type;
	}

	fnt::glyph_data operator[](char32_t codepoint) const noexcept
	{
		const uint32_t glyph_id = m_codepoint_mapper.mapper(m_codepoint_mapper.data, static_cast<uint32_t>(codepoint));

		return interpret_glyph_data(glyph_id);
	}

	operator bool() const noexcept
	{
		return m_file_type != file_type::invalid;
	}

private:

	template<typename T = void>
	const T* get_table(table_tag tag)
	{
		struct table_record
		{
			table_tag tag;
			uint32_t checksum;
			uint32_t offset;
			uint32_t lenght;
		};

		const table_record* lo = reinterpret_cast<const table_record*>(m_file.data() + 1);

		const table_record* hi = lo + m_table_cnt;

		while (lo <= hi)
		{
			const table_record* mid = lo + ((hi - lo) >> 1);

			if (mid->tag == tag)
				return reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(m_file.data()) + be_to_le(mid->offset));
			else if (mid->tag < tag)
				lo = mid + 1;
			else
				hi = mid - 1;
		}

		return nullptr;
	}

	fnt::glyph_data interpret_glyph_data(uint32_t glyph_id) const noexcept
	{
		struct glyph_header
		{
			int16_t num_contours;
			int16_t x_min;
			int16_t y_min;
			int16_t x_max;
			int16_t y_max;
		};

		// If this is violated, something went wrong in the character mapper. Uh-Oh.
		assert(glyph_id < m_glyph_cnt);

		uint32_t glyph_offset;

		if (m_flags.full_glyph_offsets)
			glyph_offset = be_to_le(m_glyph_offsets.full_offsets[glyph_id]);
		else
			glyph_offset = be_to_le(m_glyph_offsets.short_offsets[glyph_id]) * 2;

		float advance_width;

		float left_side_bearing;

		struct horizontal_metrics
		{
			uint16_t advance_width;
			int16_t left_side_bearing;
		};

		if (glyph_id >= m_full_horizontal_layout_cnt)
		{
			advance_width = static_cast<float>(be_to_le(static_cast<const horizontal_metrics*>(m_horizontal_layout_data)[m_full_horizontal_layout_cnt - 1].advance_width));

			left_side_bearing = static_cast<float>(be_to_le(reinterpret_cast<const int16_t*>(static_cast<const horizontal_metrics*>(m_horizontal_layout_data) + m_full_horizontal_layout_cnt)[glyph_id - m_full_horizontal_layout_cnt]));
		}
		else
		{
			advance_width = static_cast<float>(be_to_le(static_cast<const horizontal_metrics*>(m_horizontal_layout_data)[glyph_id].advance_width));

			left_side_bearing = static_cast<float>(be_to_le(static_cast<const horizontal_metrics*>(m_horizontal_layout_data)[glyph_id].left_side_bearing));
		}

		advance_width *= m_normalization_factor;

		left_side_bearing *= m_normalization_factor;

		const glyph_header* header = reinterpret_cast<const glyph_header*>(static_cast<const uint8_t*>(m_glyph_data) + glyph_offset);

		int16_t contour_cnt = be_to_le(header->num_contours);

		if (contour_cnt >= 0) // Simple Glyph
		{
			constexpr uint8_t                    ON_CURVE_POINT = 0x01;
			constexpr uint8_t                    X_SHORT_VECTOR = 0x02;
			constexpr uint8_t                    Y_SHORT_VECTOR = 0x04;
			constexpr uint8_t                       REPEAT_FLAG = 0x08;
			constexpr uint8_t X_IS_SAME_OR_POSITIVE_X_SHORT_VEC = 0x10;
			constexpr uint8_t Y_IS_SAME_OR_POSITIVE_Y_SHORT_VEC = 0x20;
			constexpr uint8_t                    OVERLAP_SIMPLE = 0x40;
			constexpr uint8_t                      DECODED_BITS = 0x37;

			const uint16_t* endpt_inds = reinterpret_cast<const uint16_t*>(header + 1);

			const uint16_t instruction_bytes = be_to_le(endpt_inds[contour_cnt]);

			const uint8_t* instructions = reinterpret_cast<const uint8_t*>(endpt_inds + contour_cnt + 1);

			const uint8_t* raw_data = instructions + instruction_bytes; // Flags, X-Coords, Y-Coords

			const uint16_t point_cnt = be_to_le(endpt_inds[contour_cnt - 1]) + 1;

			const int16_t x_min = be_to_le(header->x_min);
			const int16_t x_max = be_to_le(header->x_max);
			const int16_t y_min = be_to_le(header->y_min);
			const int16_t y_max = be_to_le(header->y_max);

			fnt::glyph_data ret(contour_cnt, point_cnt, x_min * m_normalization_factor - m_x_min_global, x_max * m_normalization_factor - m_x_min_global, y_min * m_normalization_factor - m_y_min_global, y_max * m_normalization_factor - m_y_min_global, advance_width, left_side_bearing);

			for (uint16_t i = 0; i != contour_cnt; ++i)
				ret.m_contour_end_indices[i] = endpt_inds[i];

			uint8_t* decoded_flags = static_cast<uint8_t*>(malloc(point_cnt));

			uint32_t byte_idx = 0, write_idx = 0;

			for (; write_idx < point_cnt;)
			{
				uint8_t f = raw_data[byte_idx++];

				decoded_flags[write_idx++] = f & DECODED_BITS;

				if (f & REPEAT_FLAG)
				{
					uint8_t repeat_cnt = raw_data[byte_idx++];

					for (uint8_t i = 0; i != repeat_cnt; ++i)
						decoded_flags[write_idx++] = f & DECODED_BITS;
				}
			}

			// If this is not true, there was an error while expanding the raw flags array
			assert(write_idx == point_cnt);
			
			int16_t prev_x = 0;

			for (uint32_t i = 0; i != point_cnt; ++i)
			{
				int16_t delta_x = 0;

				if (decoded_flags[i] & X_SHORT_VECTOR)
				{
					delta_x = static_cast<uint16_t>(raw_data[byte_idx++]);

					if (!(decoded_flags[i] & X_IS_SAME_OR_POSITIVE_X_SHORT_VEC))
						delta_x = -delta_x;
				}
				else if(!(decoded_flags[i] & X_IS_SAME_OR_POSITIVE_X_SHORT_VEC))
				{
					delta_x = static_cast<int16_t>(raw_data[byte_idx++]) << 8;
					delta_x |= raw_data[byte_idx++];
				}

				const int16_t new_x = prev_x + delta_x;

				ret.m_points[i].m_x = new_x * m_normalization_factor - m_x_min_global;

				if (decoded_flags[i] & ON_CURVE_POINT)
					ret.m_points[i].m_x = -ret.m_points[i].m_x;

				prev_x = new_x;
			}

			int32_t prev_y = 0;

			for (uint32_t i = 0; i != point_cnt; ++i)
			{
				int16_t delta_y = 0;

				if (decoded_flags[i] & Y_SHORT_VECTOR)
				{
					delta_y = static_cast<uint16_t>(raw_data[byte_idx++]);

					if (!(decoded_flags[i] & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VEC))
						delta_y = -delta_y;
				}
				else if (!(decoded_flags[i] & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VEC))
				{
					delta_y = static_cast<int16_t>(raw_data[byte_idx++]) << 8;
					delta_y |= raw_data[byte_idx++];
				}

				const int32_t new_y = prev_y + delta_y;

				ret.m_points[i].m_y = new_y * m_normalization_factor - m_y_min_global;

				prev_y = new_y;
			}

			free(decoded_flags);

			return ret;
		}
		else // Composite Glyph
		{
			constexpr uint16_t ARG_1_AND_2_ARE_WORDS = 0x0001;
			constexpr uint16_t ARGS_ARE_XY_VALUES = 0x0002;
			constexpr uint16_t ROUND_XY_TO_GRID = 0x0004;
			constexpr uint16_t WE_HAVE_A_SCALE = 0x0008;
			constexpr uint16_t MORE_COMPONENTS = 0x0020;
			constexpr uint16_t WE_HAVE_A_X_AND_Y_SCALE = 0x0040;
			constexpr uint16_t WE_HAVE_A_TWO_BY_TWO = 0x0080;
			constexpr uint16_t WE_HAVE_INSTRUCTIONS = 0x0100;
			constexpr uint16_t USE_MY_METRICS = 0x0200;
			constexpr uint16_t OVERLAP_COMPOUND = 0x0400;
			constexpr uint16_t SCALED_COMPONENT_OFFSET = 0x0800;
			constexpr uint16_t UNSCALED_COMPONENT_OFFSET = 0x1000;

			uint16_t flags;

			uint16_t component_glyph_idx;

			uint32_t word_idx = 0;

			const uint16_t* raw_data = reinterpret_cast<const uint16_t*>(header + 1);

			do
			{
				flags = be_to_le(raw_data[word_idx++]);

				component_glyph_idx = be_to_le(raw_data[word_idx++]);

				int16_t arg1;
				int16_t arg2;
				float scale_x = 1.0F;
				float scale_y = 1.0F;
				float scale_01 = 0.0F;
				float scale_10 = 0.0F;

				if (flags & ARG_1_AND_2_ARE_WORDS)
				{
					arg1 = static_cast<int16_t>(be_to_le(raw_data[word_idx++]));

					arg2 = static_cast<int16_t>(be_to_le(raw_data[word_idx++]));
				}
				else
				{
					uint16_t both_args = be_to_le(raw_data[word_idx++]);

					arg1 = static_cast<int16_t>(both_args) >> 8;

					arg2 = static_cast<int16_t>(static_cast<int8_t>(both_args)); // Sign extension shenanigans
				}

				if (flags & WE_HAVE_A_SCALE)
				{
					scale_x = scale_y = static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14);
				}
				else if (flags & WE_HAVE_A_X_AND_Y_SCALE)
				{
					scale_x = static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14);

					scale_y = static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14);
				}
				else if (flags & WE_HAVE_A_TWO_BY_TWO)
				{
					scale_x = static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14);

					scale_01 = static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14);
					
					scale_10 = static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14);

					scale_y = static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14);
				}
			} 
			while (flags & MORE_COMPONENTS);

			och::print("Composite Glyphs are not yet implemented!\n");

			return {};
		}
	}

	static file_type query_file_type(const och::mapped_file<file_header>& file) noexcept
	{
		if (!file)
			return file_type::invalid;

		if (file[0].version == 0x00000100 || file[0].version == 0x74727565 || file[0].version == 0x74797031)
			return file_type::truetype;
		else if (file[0].version == 0x4F54544F)
			return file_type::opentype;
		else
			return file_type::invalid;
	}

	static codepoint_mapper_data query_codepoint_mapping(const cmap_table_data* cmap_table) noexcept
	{
		uint16_t encoding_cnt = be_to_le(cmap_table->num_tables);

		struct encoding_record
		{
			uint16_t platform_id;
			uint16_t encoding_id;
			uint32_t table_offset;
		};

		const encoding_record* curr_encoding = reinterpret_cast<const encoding_record*>(cmap_table + 1);

		och::print("Number of available encodings: {}\n\n", encoding_cnt);

		const encoding_record* unicode_full = nullptr;

		const encoding_record* unicode_bmp = nullptr;

		for (uint32_t i = 0; i != encoding_cnt; ++i, ++curr_encoding)
		{
			uint16_t platform = be_to_le(curr_encoding->platform_id);

			uint16_t encoding = be_to_le(curr_encoding->encoding_id);

			uint16_t format = be_to_le(*reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(cmap_table) + be_to_le(curr_encoding->table_offset)));

			och::print("platform: {}\nencoding:{}\nformat:{}\n\n", be_to_le(curr_encoding->platform_id), be_to_le(curr_encoding->encoding_id), format);

			// Select best encoding

			if (((platform == 0 && encoding == 3) || (platform == 3 && encoding == 1)) && format == 4)
				unicode_bmp = curr_encoding;
			else if (((platform == 0 && encoding == 4) || (platform == 3 && encoding == 10)) && format == 12)
				unicode_full = curr_encoding;
		}

		if (unicode_full)
			return { codepoint_mapper_data::cmap_f12, reinterpret_cast<const uint8_t*>(cmap_table) + be_to_le(unicode_full->table_offset) };
		else if (unicode_bmp)
			return { codepoint_mapper_data::cmap_f4, reinterpret_cast<const uint8_t*>(cmap_table) + be_to_le(unicode_bmp->table_offset) };
		else
			return {};
	}

	
};
