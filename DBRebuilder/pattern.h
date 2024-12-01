#pragma once

#include <share.h>
#include <stdio.h>
#include <stdint.h>

#include <vector>
#include <string>

class pattern
{
public:
	pattern();
	~pattern();
	
	bool failed_beg(std::string raw) const;
	std::string transform(std::string raw, uint32_t rva) const;
private:
	bool init();
	void release();

	std::string hex2str(const uint8_t* raw, size_t len) const;

	void* _instruction;
};