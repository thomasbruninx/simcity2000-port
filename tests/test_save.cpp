#include "test.h"
#include "save/city.h"
#include "save/error.h"
#include "save/rle.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <filesystem>
#include <random>

using namespace sc2k;

static bool throwsSc2(auto&& f) { try { f(); } catch (const Sc2Error&) { return true; } return false; }

TEST(rle_known_vectors) {
    const std::vector<uint8_t> enc{0x03, 1, 2, 3, 0x85, 9, 0x02, 7, 7};
    const std::vector<uint8_t> want{1, 2, 3, 9, 9, 9, 9, 9, 9, 7, 7};
    CHECK(rleDecode(enc) == want);
    CHECK(rleDecode({}).empty());
}
TEST(rle_rejects_malformed) {
    CHECK(throwsSc2([] { rleDecode(std::vector<uint8_t>{0x80, 1}); }));
    CHECK(throwsSc2([] { rleDecode(std::vector<uint8_t>{0x00}); }));
    CHECK(throwsSc2([] { rleDecode(std::vector<uint8_t>{0x05, 1, 2}); }));
    CHECK(throwsSc2([] { rleDecode(std::vector<uint8_t>{0x90}); }));
}
TEST(rle_roundtrip_random) {
    std::mt19937 rng(1234);
    for (int iter = 0; iter < 300; ++iter) {
        const size_t n = rng() % 2000;
        std::vector<uint8_t> v(n);
        const int alphabet = 1 + rng() % 6;  // small alphabets give long runs
        for (auto& b : v) b = uint8_t(rng() % alphabet);
        CHECK(rleDecode(rleEncode(v)) == v);
    }
}
TEST(rle_long_runs_split_at_128) {
    std::vector<uint8_t> v(1000, 0xAB);
    auto e = rleEncode(v);
    CHECK(e.size() == 2 * 8);  // 7 * 128 + 104
    CHECK(rleDecode(e) == v);
}
TEST(city_rejects_bad_files) {
    CHECK(throwsSc2([] { CityFile::parse(std::vector<uint8_t>{}); }));
    CHECK(throwsSc2([] { CityFile::parse(std::vector<uint8_t>(32, 0)); }));
    std::vector<uint8_t> hdr{'F','O','R','M',0,0,0,4,'S','C','D','H'};
    CHECK(throwsSc2([&] { CityFile::parse(hdr); }));  // valid header but no layers
    hdr[7] = 9;
    CHECK(throwsSc2([&] { CityFile::parse(hdr); }));  // length mismatch
}

// ---- tests against the user's original data (skipped when SC2K_DATA_PATH is unset) ----
static std::vector<std::string> dataFiles() {
    std::vector<std::string> out;
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) return out;
    namespace fs = std::filesystem;
    for (const char* d : {"CITIES", "SCENARIO"})
        if (fs::exists(fs::path(root) / d))
            for (auto& e : fs::directory_iterator(fs::path(root) / d)) {
                auto ext = e.path().extension().string();
                for (auto& ch : ext) ch = char(std::tolower(ch));
                if (ext == ".sc2" || ext == ".scn") out.push_back(e.path().string());
            }
    if (fs::exists(fs::path(root) / "DEFAULT.SC2")) out.push_back((fs::path(root) / "DEFAULT.SC2").string());
    return out;
}

TEST(original_files_parse_and_reserialize_exactly) {
    auto files = dataFiles();
    if (files.empty()) { std::printf("  (skipped: SC2K_DATA_PATH not set)\n"); return; }
    CHECK(files.size() == 61);
    for (auto& f : files) {
        CityFile c = CityFile::load(f);
        std::ifstream in(f, std::ios::binary);
        std::vector<uint8_t> orig((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        CHECK(c.serializeOriginal() == orig);
    }
}
TEST(original_files_canonical_roundtrip_preserves_content) {
    auto files = dataFiles();
    if (files.empty()) return;
    for (auto& f : files) {
        CityFile a = CityFile::load(f);
        CityFile b = CityFile::parse(a.serialize());
        CHECK(a.misc == b.misc); CHECK(a.altm == b.altm);
        CHECK(a.xter == b.xter); CHECK(a.xbld == b.xbld); CHECK(a.xzon == b.xzon); CHECK(a.xund == b.xund);
        CHECK(a.xtxt == b.xtxt); CHECK(a.xbit == b.xbit); CHECK(a.xlab == b.xlab); CHECK(a.xmic == b.xmic);
        CHECK(a.xthg == b.xthg); CHECK(a.xtrf == b.xtrf); CHECK(a.xplt == b.xplt); CHECK(a.xval == b.xval);
        CHECK(a.xcrm == b.xcrm); CHECK(a.xplc == b.xplc); CHECK(a.xfir == b.xfir); CHECK(a.xpop == b.xpop);
        CHECK(a.xrog == b.xrog); CHECK(a.xgrp == b.xgrp); CHECK(a.cnam == b.cnam);
        CHECK(a.extras.size() == b.extras.size());
    }
}
TEST(original_nyc_facts) {
    const char* root = std::getenv("SC2K_DATA_PATH");
    if (!root) return;
    CityFile c = CityFile::load(std::string(root) + "/CITIES/NYC.SC2");
    CHECK(c.xgrp[0] == 81380);                 // city size (population x10), VERIFIED in recon
    CHECK((c.altm.at(0, 0) & 31) <= 31);
    CHECK(!c.isScenario());
    CityFile s = CityFile::load(std::string(root) + "/SCENARIO/ATLANTA.SCN");
    CHECK(s.isScenario()); CHECK(s.name() == "Atlanta");
}
