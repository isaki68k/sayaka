#pragma once

#include "sayaka.h"
#include <string>

extern FILE *fetch_image(const std::string& cache_filename,
	const std::string& img_url, int resize_width);
