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
struct face_offer {
	std::string                                  path;
	std::vector<unsigned char>                   bytes;
	std::function<std::vector<unsigned char>()> fetch;
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
// file behind a request for one of them, which is GetFontData with a zero table
// tag (the whole file, TTC included).
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
//
// Not yet run on Windows.

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

// The whole file behind one GDI request. GetFontData reads out of the font
// *selected into a DC*, so it wants a DC of its own rather than the font
// object, and the mapping-only request needs no window to draw into.
static std::vector<unsigned char> cjk_gdi_bytes(const char *family, int weight)
{
	HFONT font = CreateFontA(-13, 0, 0, 0, weight, FALSE, FALSE, FALSE,
	                         SHIFTJIS_CHARSET, OUT_TT_PRECIS, CLIP_DEFAULT_PRECIS,
	                         CLEARTYPE_QUALITY, VARIABLE_PITCH, family);
	if (!font)
		return {};
	std::vector<unsigned char> data;
	if (HDC dc = CreateCompatibleDC(nullptr)) {
		const HGDIOBJ was = SelectObject(dc, font);
		DWORD bytes = GetFontData(dc, 0, 0, nullptr, 0);
		if (bytes && bytes != GDI_ERROR) {
			data.resize(bytes);
			// The two toolchains disagree here and getting it wrong is a
			// runtime memory error rather than a compile error: the SDK says
			// LPDWORD, MinGW's wingdi.h says DWORD. Each is therefore called
			// the way its own header declares it, and the two must not be
			// "tidied" into one. A null buffer with a zero size asks for the
			// size on both, which is why the query above needs no branch.
#if defined(__MINGW32__)
			if (GetFontData(dc, 0, 0, data.data(), bytes) == GDI_ERROR)
#else
			if (GetFontData(dc, 0, 0, data.data(), &bytes) == GDI_ERROR)
#endif
				data.clear();
			else
				data.resize(bytes);
		}
		SelectObject(dc, was);
		DeleteDC(dc);
	}
	DeleteObject(font);
	return data;
}

inline void cjk_offers(bool bold, std::vector<face_offer> &out)
{
	LOGFONTA filter = {};                // not LOGFONT: that is the wide one here
	filter.lfCharSet = SHIFTJIS_CHARSET;
	filter.lfWeight  = FW_DONTCARE;
	std::vector<std::string> families;
	EnumFontFamiliesExA(nullptr, &filter, cjk_collect_family,
	                    reinterpret_cast<LPARAM>(&families), 0);
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
		out.push_back({ std::string(), {},
		                [family, weight] { return cjk_gdi_bytes(family.c_str(), weight); } });
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
			out.push_back({ path, {}, {} });
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
			out.push_back({ std::move(path), {}, {} });
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

static bool cjk_offer_bytes(const face_offer &offer, std::vector<unsigned char> &data)
{
	if (!offer.bytes.empty())
		data = offer.bytes;
	else if (offer.fetch)
		data = offer.fetch();
	else if (!cjk_read_file(offer.path, data))
		return false;
	return !data.empty();
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
inline const void *cjk_face_data(bool bold, size_t &bytes)
{
	static bool walked[2] = { false, false };
	static std::vector<unsigned char> kept[2];
	const int slot = bold ? 1 : 0;
	if (!walked[slot]) {
		walked[slot] = true;
		std::vector<face_offer> offers;
		cjk_offers(bold, offers);
		for (face_offer &offer : offers) {
			std::vector<unsigned char> data;
			if (cjk_offer_bytes(offer, data)) {
				kept[slot] = std::move(data);
				break;
			}
		}
	}
	bytes = kept[slot].size();
	return kept[slot].empty() ? nullptr : kept[slot].data();
}

inline const void *cjk_font_data(size_t &bytes)
{
	return cjk_face_data(false, bytes);
}

// The bold face, the same way. Null when this machine has none to be had, and
// the caller then draws the regular face -- so a machine without a bold is no
// worse off than one without Japanese.
inline const void *cjk_bold_font_data(size_t &bytes)
{
	return cjk_face_data(true, bytes);
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
	const void *data = bold ? cjk_bold_font_data(bytes) : cjk_font_data(bytes);
	if (!data && bold) {
		size_t regular = 0;
		data = cjk_font_data(regular);
		bytes = regular;
	}
	if (data) {
		ImFontConfig cfg;
		cfg.FontDataOwnedByAtlas = false;
		if (ImFont *font = atlas->AddFontFromMemoryTTF(
		        const_cast<void *>(data), int(bytes), px, &cfg))
			return font;
	}
	return atlas->AddFontDefault();
}

#endif // S_MU2000_UI_FONT_FILE_H
