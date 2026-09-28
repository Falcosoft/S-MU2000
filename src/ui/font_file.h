// license:BSD-3-Clause
//
// The panel's CJK face: one regular and, where the system has one, one bold.
// This is the same shape the editor windows have always used -- a short list of
// family names per platform, first one that resolves wins -- with a bold
// alongside the regular.
//
// Its own header because panel.cpp needs the bytes too (it re-rasterizes at the
// sizes the LCD asks for) and must not drag the renderer backends in with it:
// panel.cpp only ever draws.

#ifndef S_MU2000_UI_FONT_FILE_H
#define S_MU2000_UI_FONT_FILE_H

#pragma once

#include "imgui.h"

#include <cstddef>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>

// One face a platform offers. A file to read, or -- on Windows, where GDI has no
// path to give and hands over the font file itself -- a way to ask for the
// bytes. The walk below stops at the first face it accepts, so nothing is read
// until it is actually being tried: enumerating the Japanese families on
// Windows turns up twenty of them.
struct face_bytes {
	std::vector<unsigned char> data;
	int                        face = 0;   // index inside a TTC; 0 for a lone font
};

struct face_offer {
	std::string                          path;
	std::function<face_bytes()>          fetch;
};

// The families, most wanted first. macOS ships the first five on every release
// since 10.15; the rest are for the ones that do not, and for a face installed
// alongside them. Linux is where the name varies most between distributions and
// the package may not be installed at all.
static const char *const cjk_families[] = {
	"Hiragino Sans",
	"Hiragino Kaku Gothic ProN",
	"Hiragino Kaku Gothic Pro",
	"Hiragino Sans GB",
	"Osaka",
	"Yu Gothic",
	"MS PGothic",
	"Noto Sans CJK JP",
	"Noto Sans CJK",
	"Noto Sans JP",
	"Source Han Sans",
	"IPAGothic",
	"IPAPGothic",
	"VL Gothic",
	"Takao Gothic",
	"MS Gothic",
};

// **The weight goes in the family name, not in a weight attribute.** A macOS
// descriptor asked for kCTFontWeightTrait 600 gets HiraginoSans-W3 back for 400,
// 600 and 700 alike, and the weight cannot even be read back to check: the
// descriptor echoes the weight that was asked for, so a family with no bold at
// all (Osaka) also reports 600. Asking for the family at its bold name is the
// one thing CoreText honours. A family with no such name resolves to its
// regular file, which draws as before, and that is the whole fallback: the
// panel draws its regular face at the two bold slots, so a machine without a
// bold is no worse off than one without Japanese.

#if defined(_WIN32)

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

// The families on this machine that can draw Shift-JIS -- EnumFontFamiliesEx
// with lfCharSet, the parameter that exists for exactly this -- and then the
// file behind a request for one of them, which is GetFontData with the 'ttcf'
// tag (the whole collection, every face in it).
//
// **lfCharSet is SHIFTJIS_CHARSET and not DEFAULT_CHARSET, and that is the
// whole trick.** With DEFAULT_CHARSET, GDI substitutes a fallback face *per
// glyph, at draw time*, so asking for a family with no Japanese in it still
// renders Japanese and DrawText never notices. ImGui rasterizes from memory and
// never goes through that per-glyph linker, so the substitution has to happen
// during mapping instead, which is what naming the charset does -- and it is
// what puts a real Japanese face inside the font object for GetFontData to hand
// back. The rest of the request only has to match between the two weights, so
// that GDI maps both to the same family.

static const char *const cjk_wanted_families[] = {
	// what a Japanese Windows install has, best first
	"Yu Gothic UI", "Yu Gothic", "Meiryo UI", "Meiryo",
	// and the tail, for the installs that have none of the above
	"MS Gothic", "MS UI Gothic", "MS PGothic",
};

static int CALLBACK cjk_collect_family(const LOGFONTA *lf, const TEXTMETRICA *,
                                       DWORD, LPARAM param)
{
	auto *names = reinterpret_cast<std::vector<std::string> *>(param);
	if (names->size() >= 16)
		return 0;                          // more than enough to choose from
	const std::string name = lf->lfFaceName;
	if (name.empty())
		return 1;
	for (const std::string &have : *names)  // one face is reported per family
		if (have == name)
			return 1;
	names->push_back(name);
	return 1;
}

// Font files are big-endian throughout.
static unsigned cjk_tt16(const unsigned char *p)
{
	return (unsigned(p[0]) << 8) | p[1];
}

static unsigned cjk_tt32(const unsigned char *p)
{
	return (unsigned(p[0]) << 24) | (unsigned(p[1]) << 16) |
	       (unsigned(p[2]) << 8) | p[3];
}

// usWeightClass of the face starting at byte `off`, or -1 when it has no OS/2
// table to ask. 100 is thin, 400 regular, 700 bold, 900 black.
static int cjk_face_weight_at(const unsigned char *d, size_t n, size_t off)
{
	if (off + 12 > n)
		return -1;
	const unsigned tables = cjk_tt16(d + off + 4);
	for (unsigned i = 0; i < tables; i++) {
		const size_t rec = off + 12 + size_t(i) * 16;
		if (rec + 16 > n)
			return -1;
		if (cjk_tt32(d + rec) != 0x4F532F32u)   // 'OS/2'
			continue;
		const size_t at = cjk_tt32(d + rec + 8);
		if (at + 6 > n)
			return -1;
		return int(cjk_tt16(d + at + 4));
	}
	return -1;
}

// Byte offsets of every face in d: a collection lists them in its header, a
// lone font is one face at zero. Returns how many, or 0 when d is not a font.
static size_t cjk_face_offsets(const unsigned char *d, size_t n,
                               size_t *offs, size_t cap)
{
	if (n < 12 || cap == 0)
		return 0;
	if (cjk_tt32(d) != 0x74746366u) {           // not 'ttcf': one face
		offs[0] = 0;
		return 1;
	}
	if (n < 16)
		return 0;
	const unsigned count = cjk_tt32(d + 8);
	if (count == 0 || count > cap || 12 + size_t(count) * 4 > n)
		return 0;
	for (unsigned i = 0; i < count; i++) {
		const size_t off = cjk_tt32(d + 12 + size_t(i) * 4);
		if (off + 12 > n)
			return 0;
		offs[i] = off;
	}
	return count;
}

// One GetFontData call, both spellings. The size query with a null buffer is
// spelled the same on both toolchains, which is why only the data call below
// needs the branch.
static bool cjk_get_font_bytes(HDC dc, DWORD tag, std::vector<unsigned char> &data)
{
	const DWORD size = GetFontData(dc, tag, 0, nullptr, 0);
	if (!size || size == DWORD(GDI_ERROR))
		return false;
	data.resize(size);
	// The two toolchains disagree here and getting it wrong is a
	// runtime memory error rather than a compile error: the SDK says
	// LPDWORD, MinGW's wingdi.h says DWORD. Each is therefore called
	// the way its own header declares it, and the two must not be
	// "tidied" into one.
#if defined(__MINGW32__)
	const bool ok = GetFontData(dc, tag, 0, data.data(), DWORD(data.size())) != GDI_ERROR;
#else
	DWORD want = DWORD(data.size());
	const bool ok = GetFontData(dc, tag, 0, data.data(), &want) != GDI_ERROR;
	if (ok)
		data.resize(want);
#endif
	if (!ok)
		data.clear();
	return ok;
}

// The whole collection behind one GDI request, and which face of it GDI mapped
// to. GetFontData with a zero tag hands back a single face of a TTC, which is
// what crashed stb_truetype on Yu Gothic UI: a face's tables point outside
// themselves. The 'ttcf' tag hands over the whole collection, and the face is
// named by ImFontConfig::FontNo on the way into the atlas.
//
// Nothing in GDI reports which face it mapped to, so the weight does: the
// mapped face's own TEXTMETRIC is read back, and the collection face whose
// OS/2 weight sits closest to it wins.
static face_bytes cjk_gdi_bytes(const char *family, int weight, bool bold)
{
	HFONT font = CreateFontA(-13, 0, 0, 0, weight, FALSE, FALSE, FALSE,
	                         SHIFTJIS_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
	                         CLEARTYPE_QUALITY, VARIABLE_PITCH, family);
	if (!font)
		return {};
	face_bytes out;
	if (HDC dc = CreateCompatibleDC(nullptr)) {
		// GetFontData reads out of the font *selected into a DC*, so it wants
		// a DC of its own rather than the font object, and the mapping-only
		// request needs no window to draw into.
		const HGDIOBJ was = SelectObject(dc, font);
		TEXTMETRICA tm{};
		const int target = GetTextMetricsA(dc, &tm) && tm.tmWeight
		                     ? int(tm.tmWeight)
		                     : (bold ? FW_BOLD : FW_NORMAL);
		if (!cjk_get_font_bytes(dc, 0x74746366u /* 'ttcf' */, out.data))
			cjk_get_font_bytes(dc, 0, out.data);   // a lone font, not a collection
		SelectObject(dc, was);
		DeleteDC(dc);
		if (!out.data.empty()) {
			size_t offs[32];
			const size_t count =
			    cjk_face_offsets(out.data.data(), out.data.size(), offs, 32);
			if (!count)
				out.data.clear();
			else {
				size_t best = 0;
				unsigned gap = ~0u;
				for (size_t i = 0; i < count; i++) {
					const int w = cjk_face_weight_at(out.data.data(),
					                                 out.data.size(), offs[i]);
					if (w < 0)
						continue;
					const unsigned d = unsigned(abs(w - target));
					if (d < gap) {
						gap = d;
						best = i;
					}
				}
				out.face = int(best);
			}
		}
	}
	DeleteObject(font);
	return out;
}

inline void cjk_offers(bool bold, std::vector<face_offer> &out)
{
	// A DC of its own, not a null one. The docs say the handle is ignored, but
	// a null DC enumerates nothing here: every Japanese family on the machine
	// comes back as an empty list, and the walk then ends at the embedded font.
	HDC dc = CreateCompatibleDC(nullptr);
	LOGFONTA filter = {};                // not LOGFONT: that is the wide one here
	filter.lfCharSet = SHIFTJIS_CHARSET;
	filter.lfWeight  = FW_DONTCARE;
	std::vector<std::string> families;
	EnumFontFamiliesExA(dc, &filter, cjk_collect_family,
	                    reinterpret_cast<LPARAM>(&families), 0);
	if (dc)
		DeleteDC(dc);
	// GDI hands the families back in name order, so put the ones we would
	// rather have up front, keeping their own order among themselves and
	// leaving the rest behind in the order GDI gave them.
	std::vector<std::string> ordered;
	for (const char *want : cjk_wanted_families)
		for (const std::string &have : families)
			if (_stricmp(have.c_str(), want) == 0)
				ordered.push_back(have);
	for (const std::string &have : families) {
		bool promoted = false;
		for (const std::string &first : ordered)
			promoted = promoted || first == have;
		if (!promoted)
			ordered.push_back(have);
	}
	const int weight = bold ? FW_BOLD : FW_DONTCARE;
	for (const std::string &family : ordered)
		out.push_back({ std::string(),
		                [family, weight, bold] {
			                return cjk_gdi_bytes(family.c_str(), weight, bold);
		                } });
}

#elif defined(__APPLE__)

#include <CoreText/CoreText.h>

// The family-name -> file walk CoreText does for us. An absent family comes
// back with no URL at all, which is a real answer, unlike fontconfig's.
static std::string cjk_family_path(const char *family)
{
	CFStringRef cf = CFStringCreateWithCString(nullptr, family, kCFStringEncodingUTF8);
	if (!cf)
		return {};
	const void *keys[]   = { kCTFontFamilyNameAttribute };
	const void *values[] = { cf };
	CFDictionaryRef attrs = CFDictionaryCreate(nullptr, keys, values, 1,
	                                           &kCFTypeDictionaryKeyCallBacks,
	                                           &kCFTypeDictionaryValueCallBacks);
	CFRelease(cf);
	if (!attrs)
		return {};
	CTFontDescriptorRef desc = CTFontDescriptorCreateWithAttributes(attrs);
	CFRelease(attrs);
	if (!desc)
		return {};
	CFURLRef url = (CFURLRef)CTFontDescriptorCopyAttribute(desc, kCTFontURLAttribute);
	CFRelease(desc);
	if (!url)
		return {};
	char buf[1024] = {};
	std::string path;
	if (CFURLGetFileSystemRepresentation(url, true, (UInt8 *)buf, sizeof(buf)))
		path = buf;
	CFRelease(url);
	return path;
}

inline void cjk_offers(bool bold, std::vector<face_offer> &out)
{
	for (const char *family : cjk_families) {
		const std::string path =
			bold ? cjk_family_path((std::string(family) + " W6").c_str())
			     : cjk_family_path(family);
		if (!path.empty())
			out.push_back({ path, {} });
	}
}

#else

#include <fontconfig/fontconfig.h>

// One match for one family name. Asking fontconfig for all of them in a single
// multi-valued FC_FAMILY would rank the families against each other and hand
// back one winner, which throws away the only thing this needs: the chance to
// *skip* a family that is not installed and try the next. So each is asked for
// on its own and the walk goes down the answers in order.
//
// Noto Sans CJK splits its weights into separate files, so asking for a bold
// weight is all it takes to land on a different one. A family with no bold
// matches its regular file, which is the fallback: the panel then draws its
// regular face at the two bold slots.
static std::string cjk_fontconfig_match(const char *family, bool bold)
{
	FcPattern *pat = FcPatternCreate();
	if (!pat)
		return {};
	FcPatternAddString(pat, FC_FAMILY,
	                   reinterpret_cast<const FcChar8 *>(family));
	FcPatternAddDouble(pat, FC_SIZE, 16.0);
	if (bold)
		FcPatternAddInteger(pat, FC_WEIGHT, FC_WEIGHT_BOLD);
	FcConfigSubstitute(nullptr, pat, FcMatchPattern);
	FcDefaultSubstitute(pat);
	FcResult res = FcResultNoMatch;
	std::string path;
	if (FcPattern *m = FcFontMatch(nullptr, pat, &res)) {
		FcChar8 *file = nullptr;
		if (FcPatternGetString(m, FC_FILE, 0, &file) == FcResultMatch && file)
			path = reinterpret_cast<const char *>(file);
		FcPatternDestroy(m);
	}
	FcPatternDestroy(pat);
	return path;
}

inline void cjk_offers(bool bold, std::vector<face_offer> &out)
{
	// No dedupe: three family names can resolve to one file, and the walk stops
	// at the first face it gets, so a repeat on the list costs nothing.
	for (const char *family : cjk_families)
		if (std::string path = cjk_fontconfig_match(family, bold); !path.empty())
			out.push_back({ std::move(path), {} });
}

#endif

// ---- taking the first face that comes back ---------------------------------

static bool cjk_read_file(const std::string &path, std::vector<unsigned char> &data)
{
	FILE *f = std::fopen(path.c_str(), "rb");
	if (!f)
		return false;
	unsigned char buf[65536];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0)
		data.insert(data.end(), buf, buf + n);
	std::fclose(f);
	return !data.empty();
}

static bool cjk_offer_bytes(const face_offer &offer, face_bytes &out)
{
	if (offer.fetch)
		out = offer.fetch();
	else if (!cjk_read_file(offer.path, out.data))
		return false;
	return !out.data.empty();
}

// The first face the walk turns up, read into memory once and kept to process
// exit.
//
// Reading once matters: ImFontAtlas::AddFontFromFileTTF reads from disk on every
// call and has no cache, and the panel re-rasterizes its six sizes whenever the
// window changes size -- a CJK face is megabytes, so that would be megabytes per
// size per resize. Glyph rasterization itself is lazy in 1.92, so only the glyphs
// actually drawn cost anything. The buffer outlives every atlas in the process
// and the atlas never frees it (FontDataOwnedByAtlas = false at the call
// sites), which is what the lazy bakes need.
//
// The walk runs once per weight, and a walk that came back empty is remembered,
// or a machine with no Japanese font installed would re-enumerate its fonts on
// every call.
inline const void *cjk_face_data(bool bold, size_t &bytes, int &face)
{
	static bool walked[2] = { false, false };
	static face_bytes kept[2];
	const int slot = bold ? 1 : 0;
	if (!walked[slot]) {
		walked[slot] = true;
		std::vector<face_offer> offers;
		cjk_offers(bold, offers);
		for (face_offer &offer : offers) {
			face_bytes got;
			if (cjk_offer_bytes(offer, got)) {
				kept[slot] = std::move(got);
				break;
			}
		}
	}
	bytes = kept[slot].data.size();
	face = kept[slot].face;
	return kept[slot].data.empty() ? nullptr : kept[slot].data.data();
}

inline const void *cjk_font_data(size_t &bytes, int &face)
{
	return cjk_face_data(false, bytes, face);
}

// The bold face, the same way. Null when this machine has none to be had, and
// the caller then draws the regular face -- so a machine without a bold is no
// worse off than one without Japanese.
inline const void *cjk_bold_font_data(size_t &bytes, int &face)
{
	return cjk_face_data(true, bytes, face);
}

// Put one CJK face into an atlas at a given size, and return the font ImGui
// will draw with -- or ImGui's built-in, if this machine has no Japanese font
// to be found. The buffer stays ours (FontDataOwnedByAtlas = false) because it
// outlives every atlas in the process, which the lazy bakes need.
//
// `bold` asks for the second face, and a machine with none to be had gets the
// regular one -- which is the same face it would have drawn anyway.
//
// **One font setup for the whole program.** The panel's six sizes
// (build_fonts), the window's own pieces (imshell::panel_fonts) and the five PC
// editor windows all come through here. One list of names, in one place: the
// editor windows are three separate hosts and a copy each is a copy to drift.
inline ImFont *add_cjk_font(ImFontAtlas *atlas, float px = 16.0f, bool bold = false)
{
	size_t bytes = 0;
	int face = 0;
	const void *data = bold ? cjk_bold_font_data(bytes, face) : cjk_font_data(bytes, face);
	if (!data && bold) {
		size_t regular = 0;
		data = cjk_font_data(regular, face);
		bytes = regular;
	}
	if (data) {
		ImFontConfig cfg;
		cfg.FontDataOwnedByAtlas = false;
		cfg.FontNo = face;             // which face of a TTC; 0 for a lone font
		if (ImFont *font = atlas->AddFontFromMemoryTTF(
		        const_cast<void *>(data), int(bytes), px, &cfg))
			return font;
	}
	return atlas->AddFontDefault();
}

#endif // S_MU2000_UI_FONT_FILE_H
