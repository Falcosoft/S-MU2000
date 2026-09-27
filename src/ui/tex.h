// license:BSD-3-Clause
//
// One RGBA image in a Dear ImGui texture. The panel art needs this (ui/svg.h
// draws the photo panel background and the antialiased key-top symbols as
// textures); nothing else in the panel does -- every other shape goes
// straight into an ImDrawList.
//
// The ownership dance is ImGui 1.92's, not ours: an ImTextureData is created
// here, filled, and registered with the current context, and the renderer
// backend uploads it during the next Render(). So nothing in this header
// knows about DX11, Metal or SDL_Renderer, which is what lets svg.cpp stay
// platform free.
//
// **α はかけていない値**（straight alpha）が要る。DX11 / Metal / SDLRenderer
// の三つとも SRC_ALPHA 合成で、パネルの絵は半透明の縁を持つので、α を
// かけたまま渡すと縁が濃く出る。
//
// One wrinkle worth knowing: an ImTextureData may only be unregistered while
// the ImGuiContext that owns it is still alive, but a panel outlives its
// window's context (gui.cpp keeps the app in a function-local static, so it
// is destroyed at exit, long after dx11_stop). The registry below remembers
// which context owns which texture; imshell::*_stop() calls
// drop_user_textures() before DestroyContext, and drop_user_textures() also
// empties each owner, so a tex that outlives its window is simply empty.

#ifndef S_MU2000_UI_TEX_H
#define S_MU2000_UI_TEX_H

#pragma once

// RegisterUserTexture / UnregisterUserTexture are still marked EXPERIMENTAL
// and live in the internal header (1.92 exposes no public way to register an
// app-owned texture). The rest of the project already includes it for ImFont
// work, so this is nothing new -- but keep it in this one header rather than
// spreading it further.
#include "imgui_internal.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace ui {
namespace im {

class tex;

// Every texture this process has made, with the context that owns it
struct user_tex {
	ImGuiContext *ctx;
	ImTextureData *data;
	tex           *owner;   // emptied when the context takes the texture away
};

inline std::vector<user_tex> &user_textures()
{
	static std::vector<user_tex> all;
	return all;
}

// Called by the window teardown (imgui_shell.h / imgui_shell_sdl.h) just
// before DestroyContext: everything still registered goes away with the
// context, and every owner is emptied so its destructor has nothing to do
// (defined below, after tex is complete)
inline void drop_user_textures();

class tex
{
public:
	tex() = default;
	tex(const tex &) = delete;
	tex &operator=(const tex &) = delete;
	~tex() { release(); }

	// w × h の 0xAARRGGBB（α はかけていない）を転送する。同じ大きさのものが
	// あるなら転送し直さない（毎フレーム作り直さない）
	bool upload(int w, int h, const std::vector<uint32_t> &argb)
	{
		if (w <= 0 || h <= 0 || argb.size() < size_t(w) * size_t(h))
			return false;
		if (m_data && m_w == w && m_h == h)
			return true;
		release();
		ImGuiContext *ctx = ImGui::GetCurrentContext();
		if (!ctx)
			return false;                 // no context: the picture just does not show
		m_data = IM_NEW(ImTextureData)();
		m_data->Create(ImTextureFormat_RGBA32, w, h);
		m_data->UseColors = true;
		m_w = w;                       // convert() reads these
		m_h = h;
		convert(argb);
		ImGui::RegisterUserTexture(m_data);
		user_textures().push_back({ ctx, m_data, this });
		return true;
	}

	// The context took the texture away with it (drop_user_textures)
	void forget() { m_data = nullptr; m_w = m_h = 0; }

	// Otherwise: unregister from whichever context owns it, then free
	void release()
	{
		ImTextureData *t = m_data;
		forget();
		if (!t)
			return;
		std::vector<user_tex> &all = user_textures();
		for (size_t i = 0; i < all.size(); i++) {
			if (all[i].data != t)
				continue;
			ImGuiContext *prev = ImGui::GetCurrentContext();
			ImGui::SetCurrentContext(all[i].ctx);
			t->SetStatus(ImTextureStatus_WantDestroy);
			ImGui::UnregisterUserTexture(t);
			ImGui::SetCurrentContext(prev);
			all.erase(all.begin() + long(i));
			break;
		}
		IM_DELETE(t);
	}

	bool valid() const { return m_data != nullptr; }
	int width() const { return m_w; }
	int height() const { return m_h; }

	// For ImGui::Image / ImDrawList::AddImage
	ImTextureRef ref() const { return m_data ? m_data->GetTexRef() : ImTextureRef(); }

private:
	// ImTextureFormat_RGBA32 is four bytes in the order r, g, b, a. Our
	// 0xAARRGGBB is a Windows COLORREF order, which has r and b the other way
	// around -- so this copies through a swap. Without it the panel art comes
	// out with its reds and blues exchanged (the beige face reads blue-grey).
	void convert(const std::vector<uint32_t> &argb)
	{
		unsigned char *out = static_cast<unsigned char *>(m_data->GetPixels());
		const size_t n = size_t(m_w) * size_t(m_h);
		for (size_t i = 0; i < n; i++) {
			const uint32_t v = argb[i];
			out[i * 4 + 0] = (unsigned char)(v >> 16);     // r
			out[i * 4 + 1] = (unsigned char)(v >> 8);      // g
			out[i * 4 + 2] = (unsigned char)v;             // b
			out[i * 4 + 3] = (unsigned char)(v >> 24);     // a
		}
	}

	ImTextureData *m_data = nullptr;
	int m_w = 0, m_h = 0;
};

inline void drop_user_textures()
{
	std::vector<user_tex> &all = user_textures();
	for (const user_tex &u : all) {
		ImGuiContext *prev = ImGui::GetCurrentContext();
		ImGui::SetCurrentContext(u.ctx);
		u.data->SetStatus(ImTextureStatus_WantDestroy);
		ImGui::UnregisterUserTexture(u.data);
		ImGui::SetCurrentContext(prev);
		IM_DELETE(u.data);
		if (u.owner)
			u.owner->forget();
	}
	all.clear();
}

} // namespace im
} // namespace ui

#endif // S_MU2000_UI_TEX_H
