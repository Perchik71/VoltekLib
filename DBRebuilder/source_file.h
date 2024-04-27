#pragma once

#include <stdio.h>
#include <stdint.h>

#include <string>
#include <vector>

class source_file
{
public:
    source_file();
    source_file(const char* fname);
    ~source_file();

    bool open(const char* fname);
    void get_code_range(uint32_t* start, uint32_t* end) const;

    uint32_t off2rva(uint32_t target) const;
    uint32_t rva2off(uint32_t target) const;

    bool is_rva_in_code_range(uint32_t rva) const;
    bool get_raw(uint32_t rva, std::string& raw) const;

    uint32_t find_pattern(std::string mask);
    std::vector<uint32_t> find_patterns(std::string mask);

    uint32_t search(std::string mask, int32_t offset, char check_char);

    bool get_pe_section_range(const char* section, uint32_t* start, uint32_t* end);

    bool can_mask_by_raw64(uint32_t rva) const;
    bool get_mask_by_raw64(uint32_t rva, std::string& mask) const;
    bool get_mask_by_raw64_ex(uint32_t rva, std::string& mask) const;
private:
    uint32_t _begin;
    uint32_t _code_start, _code_end;
    std::vector<char> data;
};