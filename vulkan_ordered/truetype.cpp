#include "truetype.h"



/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
/*///////////////////////////////////////// Big Endian to Little Endian /////////////////////////////////////////*/
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

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



/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
/*//////////////////////////////////////////////// glyph_metrics ////////////////////////////////////////////////*/
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

glyph_metrics::glyph_metrics(float x_min, float x_max, float y_min, float y_max, float advance_width, float left_side_bearing) :
	m_x_min{ x_min }, m_x_max{ x_max }, m_y_min{ y_min }, m_y_max{ y_max }, m_advance_width{ advance_width }, m_left_side_bearing{ left_side_bearing } {}

float glyph_metrics::x_min() const noexcept { return m_x_min; }

float glyph_metrics::x_max() const noexcept { return m_x_max; }

float glyph_metrics::y_min() const noexcept { return m_y_min; }

float glyph_metrics::y_max() const noexcept { return m_y_max; }

float glyph_metrics::x_size() const noexcept { return x_max() - x_min(); }

float glyph_metrics::y_size() const noexcept { return y_max() - y_min(); }

float glyph_metrics::advance_width() const noexcept { return m_advance_width; }

float glyph_metrics::left_side_bearing() const noexcept { return m_left_side_bearing; }

float glyph_metrics::right_side_bearing() const noexcept { return advance_width() - left_side_bearing() - x_size(); }



/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
/*///////////////////////////////////////////////// glyph_data ///////////////////////////////////////////////////*/
/*////////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

glyph_data::glyph_data() noexcept :
	m_point_cnt{ 0 },
	m_contour_cnt{ 0 },
	m_points{ nullptr },
	m_metrics{ 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F }
{}

glyph_data::glyph_data(uint32_t contour_cnt, uint32_t point_cnt, glyph_metrics metrics, och::vec2* raw_data_ownership_transferred) noexcept :
	m_point_cnt{ point_cnt },
	m_contour_cnt{ contour_cnt },
	m_points{ raw_data_ownership_transferred },
	m_metrics{ metrics }
{}

glyph_data::~glyph_data() noexcept
{
	free(m_points);
}

const och::vec2& glyph_data::operator[](uint32_t point_idx) const noexcept
{
	return m_points[point_idx];
}

och::vec2& glyph_data::operator[](uint32_t point_idx) noexcept
{
	return m_points[point_idx];
}

const uint32_t* glyph_data::contour_end_indices() const noexcept
{
	return reinterpret_cast<const uint32_t*>(m_points + m_point_cnt);
}

const glyph_metrics& glyph_data::metrics() const noexcept
{
	return m_metrics;
}

uint32_t glyph_data::contour_cnt() const noexcept
{
	return m_contour_cnt;
}

uint32_t glyph_data::point_cnt() const noexcept
{
	return m_point_cnt;
}

uint32_t glyph_data::contour_beg_index(uint32_t contour_idx) const noexcept
{
	return contour_idx ? contour_end_indices()[contour_idx - 1] : 0;
}

uint32_t glyph_data::contour_end_index(uint32_t contour_idx) const noexcept
{
	return contour_end_indices()[contour_idx];
}



/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////*/
/*//////////////////////////////////////////////// truetype_file ////////////////////////////////////////////////*/
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

/*/////////////////////////////////////////// codepoint mappers /////////////////////////////////////////////////*/
/*///////////////////////////////////////////////////////////////////////////////////////////////////////////////*/

uint32_t cmap_f4(const void* raw_tbl, uint32_t cpt) noexcept
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

	int32_t lo = 0, hi = seg_cnt - 1;

	while (lo <= hi)
	{
		int32_t mid = lo + ((hi - lo) >> 1);

		uint16_t beg_code = be_to_le(beg_codes[mid]);

		uint16_t end_code = be_to_le(end_codes[mid]);

		if (beg_code <= short_cpt && end_code >= short_cpt)
		{
			if (id_range_offsets[mid])
			{
				const uint16_t id = be_to_le(*(id_range_offsets + mid + (be_to_le(id_range_offsets[mid]) >> 1) + short_cpt - beg_code));

				return id ? id + be_to_le(id_deltas[mid]) : id;
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

uint32_t cmap_f12(const void* raw_tbl, uint32_t cpt) noexcept
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

	int64_t lo = 0, hi = be_to_le(hdr->num_groups) - 1;

	while (lo <= hi)
	{
		int64_t mid = lo + ((hi - lo) >> 1);

		const uint32_t beg_code = be_to_le(groups[mid].beg_charcode);

		const uint32_t end_code = be_to_le(groups[mid].end_charcode);

		if (beg_code <= cpt && end_code >= cpt)
			return be_to_le(groups[mid].beg_glyph_id) + cpt - beg_code;
		else if (beg_code > cpt)
			hi = mid - 1;
		else if (end_code < cpt)
			lo = mid + 1;
		else
			return 0;
	}

	return 0;
}



truetype_file::truetype_file(const char* filename) noexcept : m_file{ filename, och::fio::access_read, och::fio::open_normal, och::fio::open_fail, 0, 0, och::fio::share_read_write_delete }
{
	const file_type tentative_file_type = query_file_type(m_file);

	if (tentative_file_type == file_type::invalid)
		return;

	m_table_cnt = be_to_le(m_file[0].num_tables);

	// Load tables

	m_glyph_offsets = get_table("loca");

	m_glyph_data = get_table("glyf");

	m_horizontal_layout_data = get_table("hmtx");

	const head_table_data* head_tbl = static_cast<const head_table_data*>(get_table("head"));

	const maxp_table_data* maxp_tbl = static_cast<const maxp_table_data*>(get_table("maxp"));

	const hhea_table_data* hhea_tbl = static_cast<const hhea_table_data*>(get_table("hhea"));

	const cmap_table_data* cmap_tbl = static_cast<const cmap_table_data*>(get_table("cmap"));

	if (!maxp_tbl || !head_tbl || !hhea_tbl || !cmap_tbl || !m_glyph_offsets || !m_glyph_data || !m_horizontal_layout_data)
		return;

	if (!(m_codepoint_mapper = query_codepoint_mapping(cmap_tbl)).data)
		return;

	m_flags.full_glyph_offsets = head_tbl->index_to_loc_format;

	const int32_t max_x_sz = be_to_le(head_tbl->x_max) - be_to_le(head_tbl->x_min);

	const int32_t max_y_sz = be_to_le(head_tbl->y_max) - be_to_le(head_tbl->y_min);

	m_normalization_factor = 1.0F / static_cast<float>(max_x_sz > max_y_sz ? max_x_sz : max_y_sz);

	m_x_min_global = be_to_le(head_tbl->x_min) * m_normalization_factor;

	m_y_min_global = be_to_le(head_tbl->y_min) * m_normalization_factor;

	m_full_horizontal_layout_cnt = be_to_le(hhea_tbl->number_of_h_metrics);

	m_glyph_cnt = be_to_le(maxp_tbl->num_glyphs);

	m_max_composite_glyph_cnt = be_to_le(maxp_tbl->max_component_elemenents);

	m_file_type = tentative_file_type;
}

glyph_data truetype_file::get_glyph(char32_t codepoint) const noexcept
{
	const uint32_t glyph_id = m_codepoint_mapper.get_glyph_id(codepoint);

	uint32_t metrics_glyph_id = glyph_id; // Only overwritten if there is a USE_MY_METRICS flag in a composite glyph component

	internal_glyph_data glf = get_glyph_data_recursive(glyph_id, metrics_glyph_id);

	return glf.to_glyph_data(get_glyph_metrics(metrics_glyph_id));
}

truetype_file::operator bool() const noexcept
{
	return m_file_type != file_type::invalid;
}

const void* truetype_file::get_table(table_tag tag)
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
			return reinterpret_cast<const void*>(reinterpret_cast<const uint8_t*>(m_file.data()) + be_to_le(mid->offset));
		else if (mid->tag < tag)
			lo = mid + 1;
		else
			hi = mid - 1;
	}

	return nullptr;
}

truetype_file::internal_glyph_data truetype_file::get_glyph_data_recursive(uint32_t glyph_id, uint32_t& out_glyph_id_for_metrics_to_use) const noexcept
{
	// Bail out if there was some sort of error in the glyph-id mapper. This is e.g. the case for '\0' or '\n' in calibri.
	if (glyph_id >= m_glyph_cnt)
		return {};

	const glyph_header* header = find_glyph(glyph_id);

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

		const uint16_t point_cnt = be_to_le(endpt_inds[contour_cnt - 1]) + 1;

		const uint16_t instruction_bytes = be_to_le(endpt_inds[contour_cnt]);

		const uint8_t* instructions = reinterpret_cast<const uint8_t*>(endpt_inds + contour_cnt + 1);

		const uint8_t* raw_data = instructions + instruction_bytes; // Flags, X-Coords, Y-Coords

		internal_glyph_data ret;

		ret.create(point_cnt, contour_cnt);

		for (uint16_t i = 0; i != contour_cnt; ++i)
			ret.contour_end_indices()[i] = be_to_le(endpt_inds[i]);

		uint8_t* decoded_flags = static_cast<uint8_t*>(malloc(point_cnt));

		uint32_t byte_idx = 0, write_idx = 0;

		while (write_idx < point_cnt)
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

		// If this does not hold, there was an error while expanding the raw flags array
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
			else if (!(decoded_flags[i] & X_IS_SAME_OR_POSITIVE_X_SHORT_VEC))
			{
				delta_x = static_cast<int16_t>(raw_data[byte_idx++]) << 8;
				delta_x |= raw_data[byte_idx++];
			}

			const int16_t new_x = prev_x + delta_x;

			ret.points()[i].x = new_x * m_normalization_factor;

			if (decoded_flags[i] & ON_CURVE_POINT)
				ret.set_on_curve(i);
			else
				ret.set_off_curve(i);

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

			ret.points()[i].y = new_y * m_normalization_factor;

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

		uint32_t word_idx = 0;

		const uint16_t* raw_data = reinterpret_cast<const uint16_t*>(header + 1);

		internal_glyph_data* components = static_cast<internal_glyph_data*>(malloc(m_max_composite_glyph_cnt * sizeof(internal_glyph_data)));

		uint32_t component_cnt = 0;

		uint16_t previous_total_point_cnt = 0;

		uint16_t previous_total_contour_cnt = 0;

		do
		{
			flags = be_to_le(raw_data[word_idx++]);

			const uint16_t component_glyph_id = be_to_le(raw_data[word_idx++]);

			components[component_cnt++] = get_glyph_data_recursive(component_glyph_id, out_glyph_id_for_metrics_to_use);

			internal_glyph_data& curr_component = components[component_cnt - 1];

			int16_t arg1;
			int16_t arg2;
			float scale_x;
			float scale_y;
			float scale_01;
			float scale_10;

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

			if (flags & UNSCALED_COMPONENT_OFFSET) // Translate before scaling and rotation
			{
				if (flags & ARGS_ARE_XY_VALUES)
				{
					float dx = static_cast<float>(arg1) * m_normalization_factor;

					float dy = static_cast<float>(arg2) * m_normalization_factor;

					curr_component.translate(dx, dy);
				}
				else
				{
					const och::vec2& matched_old = find_glyph_point_in_internal_composite(components, static_cast<uint16_t>(arg1));

					const och::vec2& matched_new = find_glyph_point_in_internal_composite(components, static_cast<uint16_t>(arg2));

					float dx = matched_old.x - matched_new.x;

					float dy = matched_old.y - matched_new.y;

					curr_component.translate(dx, dy);
				}
			}

			if (flags & WE_HAVE_A_SCALE)
			{
				scale_x = scale_y = (static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14)) * m_normalization_factor;

				curr_component.scale(scale_x, scale_y);
			}
			else if (flags & WE_HAVE_A_X_AND_Y_SCALE)
			{
				scale_x = (static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14)) * m_normalization_factor;

				scale_y = (static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14)) * m_normalization_factor;

				curr_component.scale(scale_x, scale_y);
			}
			else if (flags & WE_HAVE_A_TWO_BY_TWO)
			{
				scale_x = (static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14)) * m_normalization_factor;

				scale_01 = (static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14)) * m_normalization_factor;

				scale_10 = (static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14)) * m_normalization_factor;

				scale_y = (static_cast<float>(be_to_le(raw_data[word_idx++])) / (1 << 14)) * m_normalization_factor;

				curr_component.transform(scale_x, scale_01, scale_10, scale_y);
			}

			if (!(flags & UNSCALED_COMPONENT_OFFSET)) // Translate after scaling and rotation
			{
				if (flags & ARGS_ARE_XY_VALUES)
				{
					float dx = static_cast<float>(arg1) * m_normalization_factor;

					float dy = static_cast<float>(arg2) * m_normalization_factor;

					curr_component.translate(dx, dy);
				}
				else
				{
					const och::vec2& matched_old = find_glyph_point_in_internal_composite(components, static_cast<uint16_t>(arg1));

					const och::vec2& matched_new = find_glyph_point_in_internal_composite(components, static_cast<uint16_t>(arg2));

					float dx = matched_old.x - matched_new.x;

					float dy = matched_old.y - matched_new.y;

					curr_component.translate(dx, dy);
				}
			}

			previous_total_point_cnt += static_cast<uint16_t>(curr_component.point_cnt);

			previous_total_contour_cnt += static_cast<uint16_t>(curr_component.contour_cnt);
		} while (flags & MORE_COMPONENTS);

		internal_glyph_data ret;

		ret.create(previous_total_point_cnt, previous_total_contour_cnt);

		uint16_t point_idx = 0;

		uint16_t contour_idx = 0;

		for (uint32_t i = 0; i != component_cnt; ++i)
		{
			for (uint16_t j = 0; j != components[i].point_cnt; ++j)
				ret.points()[point_idx + j] = components[i].points()[j];

			for (uint16_t j = 0; j != components[i].contour_cnt; ++j)
				ret.contour_end_indices()[contour_idx + j] = components[i].contour_end_indices()[j] + point_idx;

			for (uint16_t j = 0; j != components[i].point_cnt; ++j)
				components[i].is_on_line(j) ? ret.set_on_curve(point_idx + j) : ret.set_off_curve(point_idx + j);

			contour_idx += static_cast<uint16_t>(components[i].contour_cnt);

			point_idx += static_cast<uint16_t>(components[i].point_cnt);
		}

		for (uint32_t i = 0; i != component_cnt; ++i)
			components[i].destroy();

		free(components);

		return ret;
	}
}

const och::vec2& truetype_file::find_glyph_point_in_internal_composite(internal_glyph_data* components, uint16_t point_idx) const noexcept
{
	uint16_t component_idx = 0;

	uint32_t running_point_cnt = components[0].point_cnt;

	while (running_point_cnt < point_idx)
		running_point_cnt += components[++component_idx].point_cnt;

	return components[component_idx].points()[point_idx - running_point_cnt];
}

const truetype_file::glyph_header* truetype_file::find_glyph(uint32_t glyph_id) const noexcept
{
	uint32_t glyph_offset;

	if (m_flags.full_glyph_offsets)
		glyph_offset = be_to_le(static_cast<const uint32_t*>(m_glyph_offsets)[glyph_id]);
	else
		glyph_offset = be_to_le(static_cast<const uint16_t*>(m_glyph_offsets)[glyph_id]) * 2;

	return reinterpret_cast<const glyph_header*>(static_cast<const uint8_t*>(m_glyph_data) + glyph_offset);
}

glyph_metrics truetype_file::get_glyph_metrics(uint32_t glyph_id) const noexcept
{
	if (glyph_id >= m_glyph_cnt)
		return { 0.0F, 0.0F, 0.0F, 0.0F, 0.0F, 0.0F };

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

	const glyph_header* header = find_glyph(glyph_id);

	const float x_min = be_to_le(header->x_min) * m_normalization_factor;
	const float x_max = be_to_le(header->x_max) * m_normalization_factor;
	const float y_min = be_to_le(header->y_min) * m_normalization_factor;
	const float y_max = be_to_le(header->y_max) * m_normalization_factor;

	return glyph_metrics(x_min, x_max, y_min, y_max, advance_width, left_side_bearing);
}

truetype_file::file_type truetype_file::query_file_type(const och::mapped_file<file_header>& file) noexcept
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

truetype_file::codepoint_mapper_data truetype_file::query_codepoint_mapping(const cmap_table_data* cmap_table) noexcept
{
	uint16_t encoding_cnt = be_to_le(cmap_table->num_tables);

	struct encoding_record
	{
		uint16_t platform_id;
		uint16_t encoding_id;
		uint32_t table_offset;
	};

	const encoding_record* curr_encoding = reinterpret_cast<const encoding_record*>(cmap_table + 1);

	const encoding_record* unicode_full = nullptr;

	const encoding_record* unicode_bmp = nullptr;

	for (uint32_t i = 0; i != encoding_cnt; ++i, ++curr_encoding)
	{
		uint16_t platform = be_to_le(curr_encoding->platform_id);

		uint16_t encoding = be_to_le(curr_encoding->encoding_id);

		uint16_t format = be_to_le(*reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(cmap_table) + be_to_le(curr_encoding->table_offset)));

		// Select best encoding

		if (((platform == 0 && encoding == 3) || (platform == 3 && encoding == 1) || (platform == 3 && encoding == 0)) && format == 4)
			unicode_bmp = curr_encoding;
		else if (((platform == 0 && encoding == 4) || (platform == 3 && encoding == 10)) && format == 12)
			unicode_full = curr_encoding;
	}

	if (unicode_full)
		return { cmap_f12, reinterpret_cast<const uint8_t*>(cmap_table) + be_to_le(unicode_full->table_offset) };
	else if (unicode_bmp)
		return { cmap_f4, reinterpret_cast<const uint8_t*>(cmap_table) + be_to_le(unicode_bmp->table_offset) };
	else
		return {};
}



/*////////////////////////////////////// truetype_file::internal_glyph_data /////////////////////////////////////*/

och::vec2* truetype_file::internal_glyph_data::points() noexcept
{
	return m_points;
}

uint32_t* truetype_file::internal_glyph_data::contour_end_indices() noexcept
{
	return reinterpret_cast<uint32_t*>(m_points + point_cnt);
}

void truetype_file::internal_glyph_data::set_on_curve(uint32_t point_idx) noexcept
{
	reinterpret_cast<uint8_t*>(contour_end_indices() + contour_cnt)[point_idx >> 3] |= 1 << (point_idx & 7);
}

void truetype_file::internal_glyph_data::set_off_curve(uint32_t point_idx) noexcept
{
	reinterpret_cast<uint8_t*>(contour_end_indices() + contour_cnt)[point_idx >> 3] &= ~(1 << (point_idx & 7));
}

bool truetype_file::internal_glyph_data::is_on_line(uint32_t point_idx) noexcept
{
	return reinterpret_cast<const uint8_t*>(contour_end_indices() + contour_cnt)[point_idx >> 3] & (1 << (point_idx & 7));
}

void truetype_file::internal_glyph_data::create(uint32_t point_cnt_, uint32_t contour_cnt_) noexcept
{
	point_cnt = point_cnt_;

	contour_cnt = contour_cnt_;

	m_points = static_cast<och::vec2*>(malloc(point_cnt_ * sizeof(och::vec2) + contour_cnt_ * sizeof(uint32_t) + (point_cnt_ + 7) / 8));
}

void truetype_file::internal_glyph_data::destroy() noexcept
{
	free(m_points);
}

glyph_data truetype_file::internal_glyph_data::to_glyph_data(glyph_metrics metrics) noexcept
{
	// Count necessary points

	uint32_t final_point_cnt = 0;

	uint32_t final_contour_cnt = 0;

	{
		uint32_t beg = 0;

		for (uint32_t i = 0; i != contour_cnt; ++i)
		{
			const uint32_t end = contour_end_indices()[i] + 1;

			if (end - beg > 2)		// Don't count degenerate contours
			{
				bool prev_on_line = is_on_line(end - 1);

				for (uint32_t j = beg; j != end; ++j)
				{
					const bool curr_on_line = is_on_line(j);

					final_point_cnt += 1 + (curr_on_line == prev_on_line);

					prev_on_line = curr_on_line;
				}

				++final_contour_cnt;
			}

			beg = end;
		}
	}


	// Allocate necessary storage

	och::vec2* final_points = static_cast<och::vec2*>(malloc(final_point_cnt * sizeof(och::vec2) + final_contour_cnt * sizeof(uint32_t)));

	uint32_t* final_contour_ends = reinterpret_cast<uint32_t*>(final_points + final_point_cnt);


	// Write out decompressed points

	{
		const och::vec2 offset(-metrics.x_min(), -metrics.y_min());

		uint32_t curr_idx = 0;

		uint32_t beg = 0;

		for (uint32_t i = 0; i != contour_cnt; ++i)
		{
			const uint32_t end = contour_end_indices()[i] + 1;

			if (end - beg > 2)
			{
				if (!is_on_line(beg))
				{
					if (!is_on_line(end - 1))
						final_points[curr_idx++] = (points()[end - 1] + points()[beg]) * 0.5F + offset;
					else
						final_points[curr_idx++] = points()[end - 1] + offset;
				}

				bool prev_on_line = !is_on_line(beg);

				for (uint32_t j = beg; j != end - 1; ++j)
				{
					bool curr_on_line = is_on_line(j);

					if (prev_on_line == curr_on_line)
						final_points[curr_idx++] = (points()[j - 1] + points()[j]) * 0.5F + offset;

					final_points[curr_idx++] = points()[j] + offset;

					prev_on_line = curr_on_line;
				}

				if (is_on_line(end - 2) == is_on_line(end - 1))
					final_points[curr_idx++] = (points()[end - 2] + points()[end - 1]) * 0.5F + offset;

				if (!(!is_on_line(beg) && is_on_line(end - 1)))
				{
					final_points[curr_idx++] = points()[end - 1];

					if (is_on_line(end - 1))
						final_points[curr_idx++] = (points()[end - 1] + points()[beg]) * 0.5F + offset;
				}

				final_contour_ends[i] = curr_idx;
			}

			beg = end;
		}
	}

	return glyph_data(final_contour_cnt, final_point_cnt, glyph_metrics(0.0F, metrics.x_size(), 0.0F, metrics.y_size(), metrics.advance_width(), metrics.left_side_bearing() - metrics.x_min()), final_points);
}

void truetype_file::internal_glyph_data::translate(float dx, float dy) noexcept
{
	const och::vec2 d(dx, dy);

	for (uint32_t i = 0; i != point_cnt; ++i)
		m_points[i] += d;
}

void truetype_file::internal_glyph_data::scale(float sx, float sy) noexcept
{
	for (uint32_t i = 0; i != point_cnt; ++i)
	{
		m_points[i].x *= sx;
		m_points[i].y *= sy;
	}
}

void truetype_file::internal_glyph_data::transform(float xx, float xy, float yx, float yy) noexcept
{
	och::mat2 t(xx, xy, yx, yy);

	for (uint32_t i = 0; i != point_cnt; ++i)
		m_points[i] = t * m_points[i];
}




/*///////////////////////////////////// truetype_file::codepoint_mapper_data ////////////////////////////////////*/

uint32_t truetype_file::codepoint_mapper_data::get_glyph_id(char32_t cpt) const noexcept
{
	return mapper(data, static_cast<uint32_t>(cpt));
}
