// license:BSD-3-Clause
// SWP50 の EG の模型（swp50_eg.h）を、実機の録音から出した数字と比べる。
// 数字は doc/motif-rack-es-capture.md の表のもの（2026-10-13、MOTIF-RACK ES のデジタル出力）。
//
//   g++ -std=c++17 -O1 -o swp50_egtest src/motif/swp50_egtest.cpp && ./swp50_egtest

#include "swp50_eg.h"

#include <cstdio>
#include <initializer_list>

static int g_fail = 0;

static void near(const char *what, double got, double want, double tol_ratio)
{
	const bool ok = std::fabs(got - want) <= std::fabs(want) * tol_ratio;
	std::printf("%s  %-46s 模型 %10.3f  実機 %10.3f\n", ok ? "OK" : "NG", what, got, want);
	if (!ok)
		g_fail++;
}

static double db(double g) { return 20.0 * std::log10(g); }

int main()
{
	// 下がる側の表（8C0DDC08）の値。式 FULL / increment と一致するはず
	near("time_down(31)", swp50_eg::time_down(31), 559206, 1e-6);
	near("time_down(47)", swp50_eg::time_down(47), 139802, 1e-5);
	near("time_down(63)", swp50_eg::time_down(63), 34950, 1e-4);
	near("time_down(1)", swp50_eg::time_down(1), 7456085, 1e-6);

	// 上がる側の表（8C0DDA08）。式は近似なので 3% まで
	near("time_up(0)", swp50_eg::time_up(0), 873373, 0.03);
	near("time_up(15)", swp50_eg::time_up(15), 234696, 0.03);
	near("time_up(31)", swp50_eg::time_up(31), 58361, 0.03);
	near("time_up(47)", swp50_eg::time_up(47), 14289, 0.03);

	// リリース 96（速さ 31）: キーオフから T の 25%・50%・75% の時刻の大きさ（dB）
	{
		swp50_eg eg;
		eg.set(0);
		eg.go(255, 31);
		const double T = 559206;
		double g = 1;
		int n = 0;
		for (const double fr : { 0.25, 0.5, 0.75 }) {
			for (; n < int(fr * T); n++)
				g = eg.step();
			char name[64];
			std::snprintf(name, sizeof name, "release rate 31, dB at %.2f T", fr);
			near(name, db(g), fr == 0.25 ? -24.42 : fr == 0.5 ? -48.49 : -72.56, 0.02);
		}
	}

	// アタック 127（速さ 0）: 0 から全開へ。表の時間 T の 10%・25%・50% での振幅
	{
		swp50_eg eg;
		eg.set(255);
		eg.go(0, 0);
		const double T = 873373;
		double g = 0;
		int n = 0;
		for (const double fr : { 0.10, 0.25, 0.50 }) {
			for (; n < int(fr * T); n++)
				g = eg.step();
			char name[64];
			std::snprintf(name, sizeof name, "attack rate 0, amplitude at %.2f T", fr);
			near(name, g, fr == 0.10 ? 0.105 : fr == 0.25 ? 0.245 : 0.499, 0.06);
		}
	}

	// ディケイの時間 80（表の時間 139,802）で、高さ 0 → 0x20（音量のパラメーター 64）へ。
	// 本体と同じ選び方で速さを決めると、行き先に届くまで約 3.1〜3.2 秒（実機は 3.11 秒）
	{
		const uint32_t r = swp50_eg::rate_for(139802, 0x20, false);
		swp50_eg eg;
		eg.set(0);
		eg.go(0x20, r);
		int n = 0;
		while (eg.atten < 0x20 - 0.5 * 16 / 6.0206 && n < 44100 * 10) {     // 行き先の 0.5dB 手前まで
			eg.step();
			n++;
		}
		near("decay to level 64: seconds to within 0.5 dB", n / 44100.0, 3.112, 0.05);
		near("decay to level 64: final dB", db(swp50_eg::gain_of(0x20)), -12.32, 0.03);
	}

	std::printf(g_fail ? "swp50_egtest: %d 件の食い違い\n" : "swp50_egtest: OK\n", g_fail);
	return g_fail ? 1 : 0;
}
