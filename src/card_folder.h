// license:BSD-3-Clause
//
// PC のフォルダーを、そのまま SmartMedia として差す（issue #218）。
//
//   差すとき   フォルダーの中のファイル（下のフォルダーも）を読み、書式済みのカードの画像をメモリの上に作る
//              （smartmedia::format）。カードの大きさは、中身が半分に収まるいちばん小さいもの（16-128MB）
//   書いたとき 本体がカードに書いたら（SAVE など）、カードの中のファイルを読み出し、フォルダーと中身が違うもの・
//              無いものだけをフォルダーへ書く。**カードから消えたファイルは、フォルダーからは消さない**
//              （PC の側のファイルを消してしまわないように）。次に差したときにはまたカードに出てくる
//
// カードのファイルの名前は 8.3 の大文字（FAT の決まり）。フォルダーの名前がそれに収まらないファイルは
// 入れずに、skipped に並べて返す。大文字と小文字だけが違う名前は、同じファイルとして書き戻す。

#ifndef S_MU2000_CARD_FOLDER_H
#define S_MU2000_CARD_FOLDER_H

#pragma once

#include "smartmedia.h"

#include <string>
#include <vector>

namespace smu2000::cardfolder {

// folder の中身から、書式済みのカードを作って card に入れる。だめなら false（理由は err）。
// 名前が 8.3 に収まらないなどで入れなかったものは skipped に（フォルダーからの道）
bool load(const std::string &folder, smartmedia &card, std::vector<std::string> &skipped, std::string &err);

// カードの中身（smartmedia::raw）をフォルダーへ書き戻す。書いたファイルの数を written に。
// だめなら false（カードが読めない・書けない）
bool sync(const std::vector<u8> &raw, const std::string &folder, int &written, std::string &err);

// その道がフォルダーか
bool is_folder(const std::string &path);

} // namespace smu2000::cardfolder

#endif // S_MU2000_CARD_FOLDER_H
