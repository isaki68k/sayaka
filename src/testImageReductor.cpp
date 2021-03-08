/*
 * Copyright (C) 2021 Tetsuya Isaki
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "test.h"
#include "ImageReductor.h"

static void
test_enum()
{
	std::vector<std::pair<const std::string, ReductorReduceMode>> table_RRM = {
		{ "Fast",			ReductorReduceMode::Fast },
		{ "Simple",			ReductorReduceMode::Simple },
		{ "HighQuality",	ReductorReduceMode::HighQuality },
	};
	for (const auto& a : table_RRM) {
		const auto& exp = a.first;
		const auto n = a.second;
		std::string act(ImageReductor::RRM2str(n));
		xp_eq(exp, act, exp);
	}

	std::vector<std::pair<const std::string, ReductorColorMode>> table_RCM = {
		{ "Mono",			ReductorColorMode::Mono },
		{ "Gray",			ReductorColorMode::Gray },
		{ "GrayMean",		ReductorColorMode::GrayMean },
		{ "Fixed8",			ReductorColorMode::Fixed8 },
		{ "FixedX68k",		ReductorColorMode::FixedX68k },
		{ "FixedANSI16",	ReductorColorMode::FixedANSI16 },
		{ "Fixed256",		ReductorColorMode::Fixed256 },
		{ "Fixed256RGBI",	ReductorColorMode::Fixed256RGBI },
		{ "Custom",			ReductorColorMode::Custom },

		{ "Mono",			ReductorColorMode::RCM_Mono },
		{ "Gray",			ReductorColorMode::RCM_Gray },
		{ "GrayMean",		ReductorColorMode::RCM_GrayMean },
		{ "Fixed8",			ReductorColorMode::RCM_Fixed8 },
		{ "FixedX68k",		ReductorColorMode::RCM_FixedX68k },
		{ "FixedANSI16",	ReductorColorMode::RCM_FixedANSI16 },
		{ "Fixed256",		ReductorColorMode::RCM_Fixed256 },
		{ "Fixed256RGBI",	ReductorColorMode::RCM_Fixed256RGBI },
		{ "Custom",			ReductorColorMode::RCM_Custom },
	};
	for (const auto& a : table_RCM) {
		const auto& exp = a.first;
		const auto n = a.second;
		std::string act(ImageReductor::RCM2str(n));
		xp_eq(exp, act, exp);
	}

	std::vector<std::pair<const std::string, ReductorFinderMode>> table_RFM = {
		{ "Default",		ReductorFinderMode::RFM_Default },
		{ "HSV",			ReductorFinderMode::RFM_HSV },
	};
	for (const auto& a : table_RFM) {
		const auto& exp = a.first;
		const auto n = a.second;
		std::string act(ImageReductor::RFM2str(n));
		xp_eq(exp, act, exp);
	}

	std::vector<std::pair<const std::string, ReductorDiffuseMethod>> table_RDM={
		{ "FS",				ReductorDiffuseMethod::RDM_FS },
		{ "Atkinson",		ReductorDiffuseMethod::RDM_ATKINSON },
		{ "Jajuni",			ReductorDiffuseMethod::RDM_JAJUNI },
		{ "Stucki",			ReductorDiffuseMethod::RDM_STUCKI },
		{ "Burkes",			ReductorDiffuseMethod::RDM_BURKES },
		{ "2",				ReductorDiffuseMethod::RDM_2 },
		{ "3",				ReductorDiffuseMethod::RDM_3 },
		{ "RGB",			ReductorDiffuseMethod::RDM_RGB },
	};
	for (const auto& a : table_RDM) {
		const auto& exp = a.first;
		const auto n = a.second;
		std::string act(ImageReductor::RDM2str(n));
		xp_eq(exp, act, exp);
	}

	std::vector<std::pair<const std::string, ResizeAxisMode>> table_RAX = {
		{ "Both",				ResizeAxisMode::Both },
		{ "Width",				ResizeAxisMode::Width },
		{ "Height",				ResizeAxisMode::Height },
		{ "Long",				ResizeAxisMode::Long },
		{ "Short",				ResizeAxisMode::Short },
		{ "ScaleDownBoth",		ResizeAxisMode::ScaleDownBoth },
		{ "ScaleDownWidth",		ResizeAxisMode::ScaleDownWidth },
		{ "ScaleDownHeight",	ResizeAxisMode::ScaleDownHeight },
		{ "ScaleDownLong",		ResizeAxisMode::ScaleDownLong },
		{ "ScaleDownShort",		ResizeAxisMode::ScaleDownShort },
	};
	for (const auto& a : table_RAX) {
		const auto& exp = a.first;
		const auto n = a.second;
		std::string act(ImageReductor::RAX2str(n));
		xp_eq(exp, act, exp);
	}
}

void
test_ImageReductor()
{
	test_enum();
}
