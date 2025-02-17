#include "LevelZero.hpp"
#include "Value.hpp"
#include "Util.hpp"
#include "Option.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>

LevelZero::LevelZero(const std::string &dir): dir(dir){
    if (!std::__fs::filesystem::exists(std::__fs::filesystem::path(dir))) {
        std::__fs::filesystem::create_directories(std::__fs::filesystem::path(dir));
        size = 0;
        byteCnt = 0;
        save();
    } else {
        std::ifstream ifs(dir + "/index", std::ios::binary);
        ifs.read((char*) &size, sizeof(uint64_t));
        ifs.read((char*) &byteCnt, sizeof(uint64_t));
        for (uint64_t i = 0; i < size; ++i) {
            uint64_t no;
            ifs.read((char*) &no, sizeof(uint64_t));
            ssts.emplace_back(SSTableId(dir, no));
        }
        ifs.close();
    }
}

Value LevelZero::search(uint64_t key) const {
    for (uint64_t i = 1; i <= size; ++i) {
        Value res = ssts[size - i].search(key);
        if (res.visible)
            return res;
    }
    return {false};
}

void LevelZero::add(const std::map<int, Value> &mem, uint64_t &no) {
    if (Option::LEVELING) {
        if (size) {
            std::map<int, Value> entries = ssts[0].load();
            std::vector<std::map<int, Value>> v;
            v.emplace_back(entries);
            v.emplace_back(mem);
            clear();
            ssts.push_back(SSTable(Util::compact(v), SSTableId(dir, no++)));
        }
        else {
            ssts.emplace_back(mem, SSTableId(dir, no++));
        }
        byteCnt = ssts[0].getSpace();
        size = 1;
    }
    else {
        ssts.emplace_back(mem, SSTableId(dir, no++));
        ++size;
        byteCnt += mem.size();
    }
    save();
}

std::map<int, Value> LevelZero::extract() {
    std::map<int, Value> t;
    if (Option::LEVELING) {
        t = ssts[0].load();
    }
    else {
        std::vector<std::map<int, Value>> inputs;
        for (const SSTable &sst: ssts) {
            inputs.emplace_back(sst.load());
            sst.remove();
        }
        t = Util::compact(inputs);
    }
    clear();
    save();
    return t;
}

void LevelZero::clear() {
    while (!ssts.empty()) {
        ssts.back().remove();
        ssts.pop_back();
    }
    size = 0;
    byteCnt = 0;
    save();
}

uint64_t LevelZero::space() const {
    return byteCnt;
}

void LevelZero::save() const {
    std::ofstream ofs(dir + "/index", std::ios::binary);
    ofs.write((char*) &size, sizeof(uint64_t));
    ofs.write((char*) &byteCnt, sizeof(uint64_t));
    for (const SSTable &sst : ssts) {
        uint64_t no = sst.number();
        ofs.write((char*) &no, sizeof(uint64_t));
    }
    ofs.close();
}