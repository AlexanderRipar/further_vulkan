#include <cstdint>

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
		float x, y;

		point deflag() const noexcept
		{
			uint32_t ix, iy;

			memcpy(&ix, &x, 4);

			memcpy(&iy, &y, 4);

			ix &= ~(1 << 31);

			iy &= ~(1 << 31);

			point ret;

			memcpy(&ret.x, &ix, 4);

			memcpy(&ret.y, &iy, 4);

			return ret;
		}
	};

	struct contour
	{
		const uint16_t m_point_cnt;
		const point* m_points;
		const uint8_t* m_is_on_line;

		point operator[](uint16_t point_idx) const noexcept { return m_points[point_idx].deflag(); }

		bool is_on_line(uint16_t point_idx) const noexcept { return m_points[point_idx].x < 0.0F; }
	};

	struct glyph_data
	{
		uint16_t m_point_cnt;

		uint16_t m_contour_cnt;

		point* m_points;

		uint16_t* m_contour_end_indices;

		glyph_data(uint16_t contour_cnt, uint16_t point_cnt) noexcept : m_point_cnt{ point_cnt }, m_contour_cnt{ contour_cnt }, m_points{ static_cast<point*>(malloc(point_cnt * sizeof(point) + contour_cnt * sizeof(uint16_t))) }, m_contour_end_indices{ reinterpret_cast<uint16_t*>(m_points + point_cnt) } {}

		glyph_data() noexcept : m_point_cnt{ 0 }, m_contour_cnt{ 0 }, m_points{ nullptr }, m_contour_end_indices{ nullptr } {}

		~glyph_data() noexcept { free(m_points); }

		contour get_contour(uint16_t contour_idx) const noexcept
		{
			uint16_t beg_point = contour_idx ? m_contour_end_indices[contour_idx - 1] : 0;

			return { static_cast<uint16_t>(m_contour_end_indices[contour_idx] - beg_point), m_points + beg_point };
		}

		point get_point(uint16_t point_idx) const noexcept { return m_points[point_idx].deflag(); }

		bool is_on_line(uint16_t point_idx) const noexcept { return m_points[point_idx].x < 0.0F; }
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

	using cmap_fn = uint32_t(*) (const void*, uint32_t) noexcept;

	using glyf_fn = fnt::glyph_data(*) (const void*, uint32_t) noexcept;

	och::mapped_file<file_header> m_file;

	file_type m_file_type;

	uint32_t m_table_cnt;

	uint32_t m_glyph_cnt;

	cmap_fn m_character_mapper;

	const void* m_character_map_data;

	const void* m_glyph_offsets;

	const void* m_glyph_data;

	struct
	{
		bool full_glyph_offsets : 1;
	} m_flags{};

public:

	opentype_file(const char* filename) noexcept : m_file{ filename, och::fio::access_read, och::fio::open_normal, och::fio::open_fail, 0, 0, och::fio::share_read_write_delete }
	{
		if (!m_file)
		{
			m_file_type = file_type::invalid;

			return;
		}

		if (m_file[0].version == 0x00000100 || m_file[0].version == 0x74727565 || m_file[0].version == 0x74797031)
			m_file_type = file_type::truetype;
		else if (m_file[0].version == 0x4F54544F)
			m_file_type = file_type::opentype;
		else
		{
			m_file_type = file_type::invalid;

			return;
		}

		m_table_cnt = be_to_le(m_file[0].num_tables);

		const void* maxp_tbl = get_table("maxp");

		const void* head_tbl = get_table("head");

		m_glyph_offsets = get_table("loca");

		m_glyph_data = get_table("glyf");

		if (!maxp_tbl || !head_tbl || !m_glyph_offsets || !m_glyph_data)
		{
			m_file_type = file_type::invalid;

			return;
		}

		struct head_data
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

		const head_data* head_ptr = static_cast<const head_data*>(head_tbl);

		constexpr uint32_t off = offsetof(head_data, index_to_loc_format);

		m_flags.full_glyph_offsets = head_ptr->index_to_loc_format;

		m_glyph_cnt = be_to_le(static_cast<const uint16_t*>(maxp_tbl)[2]);

		if (!load_unicode_mapping())
		{
			m_file_type = file_type::invalid;

			return;
		}
	}

	fnt::glyph_data operator[](char32_t codepoint) const noexcept
	{
		uint32_t glyph_id = m_character_mapper(m_character_map_data, static_cast<uint32_t>(codepoint));

		if (glyph_id > m_glyph_cnt)
		{
			och::print("Invalid Glyph-ID computed; {} is bigger than {}.\n", glyph_id, m_glyph_cnt);

			return {};
		}

		uint32_t glyph_offset;

		if (m_flags.full_glyph_offsets)
			glyph_offset = be_to_le(static_cast<const uint32_t*>(m_glyph_offsets)[glyph_id]);
		else
			glyph_offset = be_to_le(static_cast<const uint16_t*>(m_glyph_offsets)[glyph_id]) * 2;

		och::print("Glyph ID: {}\nOffset: {}\n", glyph_id, glyph_offset);

		return interpret_glyph_data(glyph_offset);
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

	fnt::glyph_data interpret_glyph_data(uint32_t offset) const noexcept
	{
		struct glyph_header
		{
			int16_t num_contours;
			int16_t x_min;
			int16_t y_min;
			int16_t x_max;
			int16_t y_max;
		};

		const glyph_header* hdr = reinterpret_cast<const glyph_header*>(static_cast<const uint8_t*>(m_glyph_data) + offset);

		int16_t contour_cnt = be_to_le(hdr->num_contours);

		och::print("Contour count: {}\n", contour_cnt);

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

			const uint16_t* endpt_inds = reinterpret_cast<const uint16_t*>(hdr + 1);

			const uint16_t instruction_bytes = be_to_le(endpt_inds[contour_cnt]);

			const uint8_t* instructions = reinterpret_cast<const uint8_t*>(endpt_inds + contour_cnt + 1);

			const uint8_t* raw_data = instructions + instruction_bytes; // Flags, X-Coords, Y-Coords

			const uint16_t point_cnt = be_to_le(endpt_inds[contour_cnt - 1]) + 1;

			fnt::glyph_data ret(contour_cnt, point_cnt);

			uint8_t* decoded_flags = static_cast<uint8_t*>(malloc(point_cnt));

			int16_t* decoded_x = static_cast<int16_t*>(malloc(point_cnt * 2));

			int16_t* decoded_y = static_cast<int16_t*>(malloc(point_cnt * 2));

			int16_t x_min = be_to_le(hdr->x_min);
			int16_t x_max = be_to_le(hdr->x_max);
			int16_t y_min = be_to_le(hdr->y_min);
			int16_t y_max = be_to_le(hdr->y_max);

			int32_t x_sz = static_cast<int32_t>(x_max) - x_min;
			int32_t y_sz = static_cast<int32_t>(y_max) - y_min;


			och::print("Instruction bytes: {}\nPoint count: {}\n", instruction_bytes, point_cnt);

			

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

			och::print("Write Index: {} of {}\n", write_idx, point_cnt);

			if (write_idx != point_cnt)
				och::print("!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!\n");

			

			for (uint32_t i = 0; i != point_cnt; ++i)
				if (decoded_flags[i] & X_SHORT_VECTOR)
				{
					decoded_x[i] = static_cast<uint16_t>(raw_data[byte_idx++]);

					if (!(decoded_flags[i] & X_IS_SAME_OR_POSITIVE_X_SHORT_VEC))
						decoded_x[i] = -decoded_x[i];
				}
				else
				{
					if (decoded_flags[i] & X_IS_SAME_OR_POSITIVE_X_SHORT_VEC)
						decoded_x[i] = 0;
					else
					{
						decoded_x[i] = static_cast<int16_t>(raw_data[byte_idx++]) << 8;
						decoded_x[i] |= raw_data[byte_idx++];
					}
				}

			for (uint32_t i = 0; i != point_cnt; ++i)
				if (decoded_flags[i] & Y_SHORT_VECTOR)
				{
					decoded_y[i] = static_cast<uint16_t>(raw_data[byte_idx++]);

					if (!(decoded_flags[i] & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VEC))
						decoded_y[i] = -decoded_y[i];
				}
				else
				{
					if (decoded_flags[i] & Y_IS_SAME_OR_POSITIVE_Y_SHORT_VEC)
							decoded_y[i] = 0;
					else
					{
						decoded_y[i] = static_cast<int16_t>(raw_data[byte_idx++]) << 8;
						decoded_y[i] |= raw_data[byte_idx++];
					}
				}

			int16_t cx = 0, cy = 0;

			for (uint32_t i = 0; i != point_cnt; ++i)
			{
				cx += decoded_x[i];
				cy += decoded_y[i];

				och::print("{:3>}:   d({:6>+},{:6>+}) -> ({:6>},{:6>})\n", i, decoded_x[i], decoded_y[i], cx, cy);
			}


		}
		else // Composite Glyph
		{
			
		}

		return {};
	}

	bool load_unicode_mapping() noexcept
	{
		struct cmap_header
		{
			uint16_t version;
			uint16_t num_tables;
		};

		const cmap_header* hdr = get_table<cmap_header>("cmap");

		if (!hdr)
			return false;

		uint16_t encoding_cnt = be_to_le(hdr->num_tables);

		struct encoding_record
		{
			uint16_t platform_id;
			uint16_t encoding_id;
			uint32_t table_offset;
		};

		const encoding_record* curr_encoding = reinterpret_cast<const encoding_record*>(hdr + 1);

		och::print("Number of available encodings: {}\n\n", encoding_cnt);

		const encoding_record* unicode_full = nullptr;

		const encoding_record* unicode_bmp = nullptr;

		for (uint32_t i = 0; i != encoding_cnt; ++i, ++curr_encoding)
		{
			uint16_t platform = be_to_le(curr_encoding->platform_id);

			uint16_t encoding = be_to_le(curr_encoding->encoding_id);

			uint16_t format = be_to_le(*reinterpret_cast<const uint16_t*>(reinterpret_cast<const uint8_t*>(hdr) + be_to_le(curr_encoding->table_offset)));

			och::print("platform: {}\nencoding:{}\nformat:{}\n\n", be_to_le(curr_encoding->platform_id), be_to_le(curr_encoding->encoding_id), format);

			// Select best encoding

			if (((platform == 0 && encoding == 3) || (platform == 3 && encoding == 1)) && format == 4)
				unicode_bmp = curr_encoding;
			else if (((platform == 0 && encoding == 4) || (platform == 3 && encoding == 10)) && format == 12)
				unicode_full = curr_encoding;
		}

		if (unicode_full)
		{
			m_character_map_data = reinterpret_cast<const uint8_t*>(hdr) + be_to_le(unicode_full->table_offset);

			m_character_mapper = cmap_f12;

			return true;
		}
		else if (unicode_bmp)
		{
			m_character_map_data = reinterpret_cast<const uint8_t*>(hdr) + be_to_le(unicode_bmp->table_offset);

			m_character_mapper = cmap_f4;

			return true;
		}
		else
			return false;
	}

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
