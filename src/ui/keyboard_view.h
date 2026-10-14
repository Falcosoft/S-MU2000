// license:BSD-3-Clause
//
// 鍵盤の窓（gui だけ。issue #212）。実機の前面の MIDI IN A に鍵盤をつないで弾く、という使い方を画面の上でする。
//   ・送り先の口（A-D）とチャンネル（1-16）を選ぶ。既定は口 A のチャンネル 1（前面の MIDI IN A と同じ）
//   ・5 オクターブの鍵盤。押すと鳴り、押したまま横へずらすと隣の鍵へ移る。鍵の下ほど強い（ほかの窓の鍵盤と同じ）。
//     「強さ一定」にすると、どこを押しても決めた強さ
//   ・ピッチベンド（離すと真ん中へ戻る）とモジュレーションの縦の帯、サステイン（CC64）の切り替え
//   ・PC のキーボードでも弾ける（一覧の窓の鍵盤と同じ並び: A 段・Q 段・数字の段が白鍵、Z で半音下・X で半音上、
//     PageUp / PageDown でオクターブ）
// 窓を閉じたり隠したりしたら、押さえていた音を全部離す

#ifndef S_MU2000_UI_KEYBOARD_VIEW_H
#define S_MU2000_UI_KEYBOARD_VIEW_H

#pragma once

#include "bridge.h"
#include "xg_ui.h"
#include "ui/lang.h"
#include "ui/texts.h"

#include "imgui.h"

#include <algorithm>
#include <array>
#include <cstdio>

namespace ui {

class keyboard_view : public imgui_view
{
public:
	const wchar_t *title() const override
	{
		return get_lang() == lang::ja ? L"S-MU2000 鍵盤" : L"S-MU2000 Keyboard";
	}
	int default_width() const override  { return 900; }
	int default_height() const override { return 300; }

	void hidden(bridge &br) override { release_all(br); }

	void draw(xg::model &, const xg_snapshot &, bridge &br) override
	{
		const ImGuiViewport *vp = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(vp->WorkPos);
		ImGui::SetNextWindowSize(vp->WorkSize);
		ImGui::Begin("##keyboard", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
		                                    ImGuiWindowFlags_NoBringToFrontOnFocus);
		const float fs = ImGui::GetFontSize();

		// ---- 送り先・強さ・オクターブ・サステイン
		{
			static const char *const PORTS[4] = { "A", "B", "C", "D" };
			ImGui::AlignTextToFramePadding();
			ImGui::TextUnformatted(UI_TEXT(kb_port, "Port"));
			ImGui::SameLine();
			ImGui::SetNextItemWidth(fs * 3.5f);
			int port = m_port;
			if (ImGui::Combo("##port", &port, PORTS, 4) && port != m_port) {
				release_all(br);
				m_port = port;
			}
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(kb_port_tip, "Port\nWhich MIDI input of the unit the keys play into. A is the front MIDI IN A, where a keyboard was traditionally plugged in. C and D exist on USB only (parts 33-64)."));
			ImGui::SameLine();
			ImGui::TextUnformatted(UI_TEXT(kb_channel, "Channel"));
			ImGui::SameLine();
			ImGui::SetNextItemWidth(fs * 5.0f);
			int ch = m_channel + 1;
			if (ImGui::InputInt("##ch", &ch, 1, 1)) {
				ch = std::clamp(ch, 1, 16);
				if (ch - 1 != m_channel) {
					release_all(br);
					m_channel = ch - 1;
				}
			}
			ImGui::SameLine(0, fs * 1.5f);
			ImGui::Checkbox(UI_TEXT(kb_fixed, "Fixed velocity"), &m_fixed);
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(kb_fixed_tip, "Fixed velocity\nOff: the lower on the key you press, the louder (as on the other keyboards). On: every key plays at the velocity set here."));
			ImGui::SameLine();
			ImGui::BeginDisabled(!m_fixed);
			ImGui::SetNextItemWidth(fs * 7.0f);
			ImGui::SliderInt("##vel", &m_velocity, 1, 127, UI_TEXT(kb_velocity_fmt, "velocity %d"));
			ImGui::EndDisabled();
			ImGui::SameLine(0, fs * 1.5f);
			if (ImGui::ArrowButton("##down", ImGuiDir_Left))
				shift_octave(-1, br);
			ImGui::SameLine();
			char oct[32];
			std::snprintf(oct, sizeof(oct), "C%d", m_base / 12 - 2);
			ImGui::TextUnformatted(oct);
			ImGui::SameLine();
			if (ImGui::ArrowButton("##up", ImGuiDir_Right))
				shift_octave(+1, br);
			if (ImGui::IsItemHovered())
				xgui::hint("%s", UI_TEXT(kb_octave_tip, "Octave\nMoves the keyboard by an octave. The name is its lowest key (Yamaha's naming: middle C = C3). PageUp / PageDown do the same from the PC keyboard."));
			ImGui::SameLine(0, fs * 1.5f);
			if (ImGui::Checkbox(UI_TEXT(kb_sustain, "Sustain"), &m_sustain))
				send_cc(64, m_sustain ? 127 : 0, br);
			ImGui::SameLine(0, fs * 1.5f);
			if (ImGui::SmallButton(UI_TEXT(kb_all_off, "All notes off"))) {
				release_all(br);
				for (int c : { 123, 120 })
					send_cc(c, 0, br);
			}
		}

		// ---- 左: ベンドとモジュレーションの帯。右: 鍵盤
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float wheel_w = fs * 3.0f, h = std::max(fs * 5.0f, avail.y - fs * 1.6f);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		const auto wheel = [&](const char *id, const char *label, int &value, int lo, int hi, bool spring, auto send) {
			ImGui::BeginGroup();
			const ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::InvisibleButton(id, ImVec2(wheel_w, h));
			const bool act = ImGui::IsItemActive();
			int v = value;
			if (act) {
				const float t = std::clamp(1.0f - (ImGui::GetIO().MousePos.y - p.y) / h, 0.0f, 1.0f);
				v = lo + int(std::lround(t * float(hi - lo)));
			} else if (spring) {
				v = (lo + hi + 1) / 2;
			}
			if (v != value) {
				value = v;
				send(v);
			}
			dl->AddRectFilled(p, ImVec2(p.x + wheel_w, p.y + h), IM_COL32(28, 30, 36, 255), 4.0f);
			const float y = p.y + h * (1.0f - float(value - lo) / float(hi - lo));
			dl->AddRectFilled(ImVec2(p.x + 2, y - fs * 0.35f), ImVec2(p.x + wheel_w - 2, y + fs * 0.35f), IM_COL32(110, 200, 255, 255), 3.0f);
			const ImVec2 ts = ImGui::CalcTextSize(label);
			ImGui::SetCursorScreenPos(ImVec2(p.x + (wheel_w - ts.x) * 0.5f, p.y + h + 2));
			ImGui::TextUnformatted(label);
			ImGui::EndGroup();
		};
		wheel("##bend", UI_TEXT(kb_bend, "Bend"), m_bend, 0, 16383, true, [&](int v) {
			const u8 m[3] = { u8(0xe0 | m_channel), u8(v & 0x7f), u8(v >> 7) };
			br.send_port(m_port, m, 3);
		});
		if (ImGui::IsItemHovered())
			xgui::hint("%s", UI_TEXT(kb_bend_tip, "Pitch bend\nDrag up or down; it springs back to the middle when released."));
		ImGui::SameLine();
		wheel("##mod", UI_TEXT(kb_mod, "Mod"), m_mod, 0, 127, false, [&](int v) { send_cc(1, v, br); });
		if (ImGui::IsItemHovered())
			xgui::hint("%s", UI_TEXT(kb_mod_tip, "Modulation wheel (controller 1)\nStays where you leave it."));
		ImGui::SameLine();
		keys(h, br);
		pc_keys(br);
		ImGui::End();
	}

private:
	static constexpr int OCTAVES = 5, KEYS = OCTAVES * 12 + 1;
	static constexpr int PC_KEYS = 37;

	// 鍵 n（窓の左端からの 0 始まり）の左右の端と、黒鍵か
	struct key_box { float x0, x1; bool black; };
	key_box box_of(int n, float left, float kw) const
	{
		static const bool BLACK[12] = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
		static const float WHITE_POS[12] = { 0, 0.6f, 1, 1.6f, 2, 3, 3.6f, 4, 4.6f, 5, 5.6f, 6 };
		const int note = m_base + n;
		const float x = left + (float((note - m_base) / 12 * 7) + WHITE_POS[note % 12]) * kw;
		return { x, BLACK[note % 12] ? x + kw * 0.62f : x + kw - 1.0f, BLACK[note % 12] };
	}

	void keys(float h, bridge &br)
	{
		const float w = ImGui::GetContentRegionAvail().x;
		const ImVec2 p = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##keys", ImVec2(w, h), ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		ImDrawList *dl = ImGui::GetWindowDrawList();
		const float kw = w / float(OCTAVES * 7 + 1);
		const float black_h = h * 0.6f;
		// マウスの下の鍵（黒鍵を先に見る）と強さ
		int under = -1, vel = m_velocity;
		const ImVec2 mp = ImGui::GetIO().MousePos;
		if (mp.x >= p.x && mp.x < p.x + w && mp.y >= p.y && mp.y < p.y + h) {
			for (int pass = 0; pass < 2 && under < 0; pass++)
				for (int n = 0; n < KEYS; n++) {
					const key_box b = box_of(n, p.x, kw);
					if (b.black != (pass == 0))
						continue;
					if (b.black && mp.y > p.y + black_h)
						continue;
					if (mp.x >= b.x0 && mp.x < b.x1) {
						under = n;
						break;
					}
				}
			if (!m_fixed)
				vel = std::clamp(int(30 + 97 * (mp.y - p.y) / h), 1, 127);
		}
		const bool down = ImGui::IsItemActive() && (ImGui::IsMouseDown(ImGuiMouseButton_Left) || ImGui::IsMouseDown(ImGuiMouseButton_Right));
		const int want = down && under >= 0 ? m_base + under : -1;
		if (want != m_mouse_note) {
			if (m_mouse_note >= 0)
				note(m_mouse_note, false, 0, br);
			m_mouse_note = want;
			if (want >= 0)
				note(want, true, vel, br);
		}
		if (ImGui::IsItemHovered() && !down)
			xgui::hint("%s", UI_TEXT(kb_keys_tip, "Press a key to play it; slide to move to the next key. Lower on the key is louder (unless Fixed velocity is on).\nPC keyboard plays too, white keys only: the A row from the lowest C, the Q row an octave up, the number row another octave up. Hold Z to flatten or X to sharpen what you play. PageUp / PageDown move the octave."));
		// 白鍵、黒鍵の順に描く。押さえている鍵は橙
		for (int pass = 0; pass < 2; pass++)
			for (int n = 0; n < KEYS; n++) {
				const key_box b = box_of(n, p.x, kw);
				if (b.black != (pass == 1))
					continue;
				const bool on = m_held[size_t(m_base + n)] > 0;
				const ImU32 c = on ? IM_COL32(236, 116, 70, 255) : b.black ? IM_COL32(30, 30, 32, 255) : IM_COL32(222, 222, 216, 255);
				dl->AddRectFilled(ImVec2(b.x0, p.y), ImVec2(b.x1, p.y + (b.black ? black_h : h)), c, b.black ? 2.0f : 3.0f);
				if (!b.black && (m_base + n) % 12 == 0) {
					char name[8];
					std::snprintf(name, sizeof(name), "C%d", (m_base + n) / 12 - 2);
					const ImVec2 ts = ImGui::CalcTextSize(name);
					dl->AddText(ImVec2((b.x0 + b.x1 - ts.x) * 0.5f, p.y + h - ts.y - 3), IM_COL32(90, 90, 90, 255), name);
				}
			}
	}

	// PC のキーボード（一覧の窓の鍵盤と同じ並び）
	void pc_keys(bridge &br)
	{
		static constexpr ImGuiKey KEYS_PC[PC_KEYS] = {
			ImGuiKey_A, ImGuiKey_S, ImGuiKey_D, ImGuiKey_F, ImGuiKey_G, ImGuiKey_H, ImGuiKey_J, ImGuiKey_K,
			ImGuiKey_L, ImGuiKey_Semicolon, ImGuiKey_Apostrophe, ImGuiKey_Backslash,
			ImGuiKey_Q, ImGuiKey_W, ImGuiKey_E, ImGuiKey_R, ImGuiKey_T, ImGuiKey_Y, ImGuiKey_U, ImGuiKey_I,
			ImGuiKey_O, ImGuiKey_P, ImGuiKey_LeftBracket, ImGuiKey_RightBracket,
			ImGuiKey_1, ImGuiKey_2, ImGuiKey_3, ImGuiKey_4, ImGuiKey_5, ImGuiKey_6, ImGuiKey_7, ImGuiKey_8,
			ImGuiKey_9, ImGuiKey_0, ImGuiKey_Minus, ImGuiKey_Equal, ImGuiKey_Oem102,
		};
		static constexpr int WHITE[13] = { 0, 2, 4, 5, 7, 9, 11, 12, 14, 16, 17, 19, 21 };
		const auto offset = [](int i) { return i < 12 ? WHITE[i] : i < 24 ? 12 + WHITE[i - 12] : 24 + WHITE[i - 24]; };
		for (int i = 0; i < PC_KEYS; i++)
			if (m_pc_note[i] >= 0 && !ImGui::IsKeyDown(KEYS_PC[i])) {
				note(m_pc_note[i], false, 0, br);
				m_pc_note[i] = -1;
			}
		const ImGuiIO &io = ImGui::GetIO();
		if (io.WantTextInput || io.KeyCtrl || io.KeyAlt || !ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
			return;
		if (ImGui::IsKeyPressed(ImGuiKey_PageDown, false))
			shift_octave(-1, br);
		if (ImGui::IsKeyPressed(ImGuiKey_PageUp, false))
			shift_octave(+1, br);
		const int semi = (ImGui::IsKeyDown(ImGuiKey_X) ? 1 : 0) - (ImGui::IsKeyDown(ImGuiKey_Z) ? 1 : 0);
		for (int i = 0; i < PC_KEYS; i++) {
			if (!ImGui::IsKeyPressed(KEYS_PC[i], false) || m_pc_note[i] >= 0)
				continue;
			const int n = m_base + offset(i) + semi;
			if (n < 0 || n > 127)
				continue;
			note(n, true, m_velocity, br);
			m_pc_note[i] = n;
		}
	}

	// 同じ鍵をマウスと PC のキーで重ねて押せるので、押さえている数を数え、最後の 1 つで離す
	void note(int n, bool on, int vel, bridge &br)
	{
		n = std::clamp(n, 0, 127);
		int &held = m_held[size_t(n)];
		if (on) {
			if (held++ == 0) {
				const u8 m[3] = { u8(0x90 | m_channel), u8(n), u8(std::clamp(vel, 1, 127)) };
				br.send_port(m_port, m, 3);
			}
		} else if (held > 0 && --held == 0) {
			const u8 m[3] = { u8(0x80 | m_channel), u8(n), 64 };
			br.send_port(m_port, m, 3);
		}
	}

	void send_cc(int cc, int value, bridge &br)
	{
		const u8 m[3] = { u8(0xb0 | m_channel), u8(cc), u8(value) };
		br.send_port(m_port, m, 3);
	}

	void shift_octave(int dir, bridge &br)
	{
		const int next = std::clamp(m_base + dir * 12, 0, 127 - (KEYS - 1));
		if (next == m_base)
			return;
		if (m_mouse_note >= 0) {
			note(m_mouse_note, false, 0, br);
			m_mouse_note = -1;
		}
		m_base = next;
	}

	// 押さえている音を全部離し、ベンドとモジュレーションとサステインを戻す（窓を隠した・口やチャンネルを変えた）
	void release_all(bridge &br)
	{
		for (int n = 0; n < 128; n++)
			if (m_held[size_t(n)] > 0) {
				m_held[size_t(n)] = 1;
				note(n, false, 0, br);
			}
		m_mouse_note = -1;
		m_pc_note.fill(-1);
		if (m_bend != 8192) {
			m_bend = 8192;
			const u8 m[3] = { u8(0xe0 | m_channel), 0, 0x40 };
			br.send_port(m_port, m, 3);
		}
		if (m_mod) {
			m_mod = 0;
			send_cc(1, 0, br);
		}
		if (m_sustain) {
			m_sustain = false;
			send_cc(64, 0, br);
		}
	}

	int m_port = 0, m_channel = 0;
	int m_base = 36;                 // いちばん左の鍵（C1。真ん中の C は C3 = 60）
	int m_velocity = 100;
	bool m_fixed = false, m_sustain = false;
	int m_bend = 8192, m_mod = 0;
	int m_mouse_note = -1;
	std::array<int, 128> m_held{};
	std::array<int, PC_KEYS> m_pc_note = [] { std::array<int, PC_KEYS> a; a.fill(-1); return a; }();
};

} // namespace ui

#endif // S_MU2000_UI_KEYBOARD_VIEW_H
