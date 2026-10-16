// license:BSD-3-Clause
//
// PC のフォルダーを SmartMedia として差す（card_folder.h）

#include "card_folder.h"
#include "card_fs.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <system_error>

namespace fs = std::filesystem;

namespace smu2000::cardfolder {

namespace {

fs::path to_path(const std::string &utf8)
{
	return fs::path(std::u8string(reinterpret_cast<const char8_t *>(utf8.data()), utf8.size()));
}

std::string to_utf8(const fs::path &p)
{
	const std::u8string s = p.u8string();
	return std::string(reinterpret_cast<const char *>(s.data()), s.size());
}

// FAT の 8.3 の名前に、大文字にするだけで収まるか。収まれば out に
bool fat_name(const std::string &name, std::string &out)
{
	std::string up;
	for (char c : name) {
		if (c >= 'a' && c <= 'z')
			c = char(c - 'a' + 'A');
		up += c;
	}
	const size_t dot = up.find('.');
	if (dot != up.rfind('.'))
		return false;                           // 点が 2 つ以上
	const std::string stem = up.substr(0, dot);
	const std::string ext = dot == std::string::npos ? std::string() : up.substr(dot + 1);
	if (stem.empty() || stem.size() > 8 || ext.size() > 3 || (dot != std::string::npos && ext.empty()))
		return false;
	for (char c : stem + ext)
		if (!((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || std::string("_-$~!#%&'()@^`{}").find(c) != std::string::npos))
			return false;
	out = up;
	return true;
}

bool read_file(const fs::path &p, std::vector<u8> &out)
{
	std::ifstream f(p, std::ios::binary);
	if (!f)
		return false;
	out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
	return !f.bad();
}

// 書きかけで止まっても元のファイルが壊れないように、横に書いてから置き換える
bool write_file(const fs::path &p, const std::vector<u8> &bytes)
{
	fs::path tmp = p;
	tmp += ".smu-tmp";
	{
		std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
		if (!f)
			return false;
		f.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
		if (!f)
			return false;
	}
	std::error_code ec;
	fs::rename(tmp, p, ec);
	if (ec) {
		fs::remove(p, ec);
		fs::rename(tmp, p, ec);
	}
	return !ec;
}

// dir の中で、name（8.3 の大文字）と大文字小文字を除いて同じ名前のものを探す。無ければ name のまま
fs::path match_in(const fs::path &dir, const std::string &name)
{
	std::error_code ec;
	for (const fs::directory_entry &e : fs::directory_iterator(dir, ec)) {
		std::string fat;
		if (fat_name(to_utf8(e.path().filename()), fat) && fat == name)
			return e.path();
	}
	return dir / to_path(name);
}

} // namespace

bool is_folder(const std::string &path)
{
	std::error_code ec;
	return !path.empty() && fs::is_directory(to_path(path), ec);
}

bool load(const std::string &folder, smartmedia &card, std::vector<std::string> &skipped, std::string &err)
{
	const fs::path root = to_path(folder);
	std::error_code ec;
	if (!fs::is_directory(root, ec)) {
		err = "not a folder";
		return false;
	}
	std::vector<smartmedia::root_file> files;
	size_t total = 0;
	for (fs::recursive_directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end; it != end; it.increment(ec)) {
		if (ec)
			break;
		if (!it->is_regular_file(ec))
			continue;
		const fs::path rel = it->path().lexically_relative(root);
		std::string name;
		bool ok = true;
		for (const fs::path &part : rel) {
			std::string fat;
			if (!fat_name(to_utf8(part), fat)) {
				ok = false;
				break;
			}
			name += (name.empty() ? "" : "/") + fat;
		}
		smartmedia::root_file rf;
		if (!ok || !read_file(it->path(), rf.bytes)) {
			skipped.push_back(to_utf8(rel));
			continue;
		}
		total += rf.bytes.size();
		rf.name = name;
		files.push_back(std::move(rf));
	}
	// 同じ名前が 2 つ（大文字小文字だけ違う）なら、あとのほうを入れない
	std::sort(files.begin(), files.end(), [](const auto &a, const auto &b) { return a.name < b.name; });
	for (size_t i = 1; i < files.size();)
		if (files[i].name == files[i - 1].name) {
			skipped.push_back(files[i].name);
			files.erase(files.begin() + std::ptrdiff_t(i));
		} else {
			i++;
		}
	// 大きさ: 中身が半分に収まるいちばん小さいもの（SAVE で書き足せるように）。無ければ収まるもの
	static const u32 SIZES[4] = { 16, 32, 64, 128 };
	for (int pass = 0; pass < 2; pass++)
		for (u32 mb : SIZES) {
			if (pass == 0 && u64(total) * 2 > u64(mb) * 1024 * 1024)
				continue;
			smartmedia c;
			if (c.create(mb) && c.format(files)) {
				c.clear_dirty();
				card = std::move(c);
				return true;
			}
		}
	err = files.size() > 256 ? "too many files in the top folder (256 at most)" : "the folder does not fit on a 128MB card";
	return false;
}

bool sync(const std::vector<u8> &raw, const std::string &folder, int &written, std::string &err)
{
	written = 0;
	std::vector<cardfs::entry> entries;
	if (!cardfs::list(raw, entries, err))
		return false;
	const fs::path root = to_path(folder);
	for (const cardfs::entry &e : entries) {
		std::vector<u8> bytes;
		if (!cardfs::read(raw, e.path, bytes, err))
			return false;
		// 道をたどり、ディレクトリは無ければ作る
		fs::path at = root;
		size_t from = 0;
		for (;;) {
			const size_t slash = e.path.find('/', from);
			const std::string part = e.path.substr(from, slash == std::string::npos ? std::string::npos : slash - from);
			at = match_in(at, part);
			if (slash == std::string::npos)
				break;
			std::error_code ec;
			fs::create_directories(at, ec);
			from = slash + 1;
		}
		std::vector<u8> now;
		if (read_file(at, now) && now == bytes)
			continue;
		if (!write_file(at, bytes)) {
			err = "cannot write " + to_utf8(at);
			return false;
		}
		written++;
	}
	return true;
}

} // namespace smu2000::cardfolder
