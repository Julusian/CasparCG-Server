#pragma once

#include <string>
#include <vector>
#include <common/memory.h>

#include "string_metrics.h"
#include "text_info.h"
#include "../../../frame/geometry.h"

namespace caspar { namespace core { namespace text {

class texture_atlas;
enum class unicode_block;

class texture_font
{
	texture_font();
	texture_font(const texture_font&);
	const texture_font& operator=(const texture_font&);

public:
	texture_font(texture_atlas&, const text_info&, bool normalize_coordinates);
	void load_glyphs(unicode_block block, const color<double>& col);
	void set_tracking(int tracking);
	std::vector<frame_geometry::coord> create_vertex_stream(const std::wstring& str, int x, int y, int parent_width, int parent_height, string_metrics* metrics);
	string_metrics measure_string(const std::wstring& str);

private:
	struct impl;
	spl::shared_ptr<impl> impl_;
};

enum class unicode_block
{
	Basic_Latin,
	Latin_1_Supplement,
	Latin_Extended_A,
	Latin_Extended_B,
	IPA_Extensions,
	Spacing_Modifier_Letters,
	Combining_Diacritical_Marks,
	Greek_and_Coptic,
	Cyrillic,
	Cyrillic_Supplement,
	Armenian,
	Hebrew,
	Arabic,
	Syriac,
	Arabic_Supplement,
	Thaana,
	NKo,
	Samaritan,
	Mandaic,
	Arabic_Extended_A,
	Devanagari,
	Bengali,
	Gurmukhi,
	Gujarati,
	Oriya,
	Tamil,
	Telugu,
	Kannada,
	Malayalam,
	Sinhala,
	Thai,
	Lao,
	Tibetan,
	Myanmar,
	Georgian,
	Hangul_Jamo,
	Ethiopic,
	Ethiopic_Supplement,
	Cherokee,
	Unified_Canadian_Aboriginal_Syllabics,
	Ogham,
	Runic,
	Tagalog,
	Hanunoo,
	Buhid,
	Tagbanwa,
	Khmer,
	Mongolian,
	Unified_Canadian_Aboriginal_Syllabics_Extended,
	Limbu,
	Tai_Le,
	New_Tai_Lue,
	Khmer_Symbols,
	Buginese,
	Tai_Tham,
	Balinese,
	Sundanese,
	Batak,
	Lepcha,
	Ol_Chiki,
	Sundanese_Supplement,
	Vedic_Extensions,
	Phonetic_Extensions,
	Phonetic_Extensions_Supplement,
	Combining_Diacritical_Marks_Supplement,
	Latin_Extended_Additional,
	Greek_Extended,
	General_Punctuation,
	Superscripts_and_Subscripts,
	Currency_Symbols,
	Combining_Diacritical_Marks_for_Symbols,
	Letterlike_Symbols,
	Number_Forms,
	Arrows,
	Mathematical_Operators,
	Miscellaneous_Technical,
	Control_Pictures,
	Optical_Character_Recognition,
	Enclosed_Alphanumerics,
	Box_Drawing,
	Block_Elements,
	Geometric_Shapes,
	Miscellaneous_Symbols,
	Dingbats,
	Miscellaneous_Mathematical_Symbols_A,
	Supplemental_Arrows_A,
	Braille_Patterns,
	Supplemental_Arrows_B,
	Miscellaneous_Mathematical_Symbols_B,
	Supplemental_Mathematical_Operators,
	Miscellaneous_Symbols_and_Arrows,
	Glagolitic,
	Latin_Extended_C,
	Coptic,
	Georgian_Supplement,
	Tifinagh,
	Ethiopic_Extended,
	Cyrillic_Extended_A,
	Supplemental_Punctuation,
	CJK_Radicals_Supplement,
	Kangxi_Radicals,
	Ideographic_Description_Characters,
	CJK_Symbols_and_Punctuation,
	Hiragana,
	Katakana,
	Bopomofo,
	Hangul_Compatibility_Jamo,
	Kanbun,
	Bopomofo_Extended,
	CJK_Strokes,
	Katakana_Phonetic_Extensions,
	Enclosed_CJK_Letters_and_Months,
	CJK_Compatibility,
	CJK_Unified_Ideographs_Extension_A,
	Yijing_Hexagram_Symbols,
	CJK_Unified_Ideographs,
	Yi_Syllables,
	Yi_Radicals,
	Lisu,
	Vai,
	Cyrillic_Extended_B,
	Bamum,
	Modifier_Tone_Letters,
	Latin_Extended_D,
	Syloti_Nagri,
	Common_Indic_Number_Forms,
	Phags_pa,
	Saurashtra,
	Devanagari_Extended,
	Kayah_Li,
	Rejang,
	Hangul_Jamo_Extended_A,
	Javanese,
	Cham,
	Myanmar_Extended_A,
	Tai_Viet,
	Meetei_Mayek_Extensions,
	Ethiopic_Extended_A,
	Meetei_Mayek,
	Hangul_Syllables,
	Hangul_Jamo_Extended_B,
	High_Surrogates,
	High_Private_Use_Surrogates,
	Low_Surrogates,
	Private_Use_Area,
	CJK_Compatibility_Ideographs,
	Alphabetic_Presentation_Forms,
	Arabic_Presentation_Forms_A,
	Variation_Selectors,
	Vertical_Forms,
	Combining_Half_Marks,
	CJK_Compatibility_Forms,
	Small_Form_Variants,
	Arabic_Presentation_Forms_B,
	Halfwidth_and_Fullwidth_Forms,
	Specials,
	Linear_B_Syllabary,
	Linear_B_Ideograms,
	Aegean_Numbers,
	Ancient_Greek_Numbers,
	Ancient_Symbols,
	Phaistos_Disc,
	Lycian,
	Carian,
	Old_Italic,
	Gothic,
	Ugaritic,
	Old_Persian,
	Deseret,
	Shavian,
	Osmanya,
	Cypriot_Syllabary,
	Imperial_Aramaic,
	Phoenician,
	Lydian,
	Meroitic_Hieroglyphs,
	Meroitic_Cursive,
	Kharoshthi,
	Old_South_Arabian,
	Avestan,
	Inscriptional_Parthian,
	Inscriptional_Pahlavi,
	Old_Turkic,
	Rumi_Numeral_Symbols,
	Brahmi,
	Kaithi,
	Sora_Sompeng,
	Chakma,
	Sharada,
	Takri,
	Cuneiform,
	Cuneiform_Numbers_and_Punctuation,
	Egyptian_Hieroglyphs,
	Bamum_Supplement,
	Miao,
	Kana_Supplement,
	Byzantine_Musical_Symbols,
	Musical_Symbols,
	Ancient_Greek_Musical_Notation,
	Tai_Xuan_Jing_Symbols,
	Counting_Rod_Numerals,
	Mathematical_Alphanumeric_Symbols,
	Arabic_Mathematical_Alphabetic_Symbols,
	Mahjong_Tiles,
	Domino_Tiles,
	Playing_Cards,
	Enclosed_Alphanumeric_Supplement,
	Enclosed_Ideographic_Supplement,
	Miscellaneous_Symbols_And_Pictographs,
	Emoticons,
	Transport_And_Map_Symbols,
	Alchemical_Symbols,
	CJK_Unified_Ideographs_Extension_B,
	CJK_Unified_Ideographs_Extension_C,
	CJK_Unified_Ideographs_Extension_D,
	CJK_Compatibility_Ideographs_Supplement,
	Tags,
	Variation_Selectors_Supplement,
	Supplementary_Private_Use_Area_A,
	Supplementary_Private_Use_Area_B
};

}}}