#pragma once

#include <chrono>
#include <unordered_map>
#include <string>
#include <stdint.h>
#include "defer.hpp"

namespace profile{

static std::unordered_map<std::string, uint32_t> data;

using Clock = std::chrono::high_resolution_clock;

struct ProfileHelper{
	Clock::time_point start_time;
	Clock::time_point end_time;
	const std::string name;

	explicit ProfileHelper(const std::string& str) 
		: name(str) {
		start_time = Clock::now();
	}

	~ProfileHelper(){
		end_time = Clock::now();
		uint32_t time = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time).count();
		data[name] = (data[name] + time)/2;
	}
};

}

#define PROFILE_SCOPE(str) \
	::profile::ProfileHelper CONCATENATE(__profile_, __LINE__) (str);
