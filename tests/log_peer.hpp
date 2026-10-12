// SPDX-FileCopyrightText: 2026 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// Checks: a second unit for the logger
//
// The logger's checks set its state in one translation unit and read or log from this one,
// so a state that is not shared shows up as a difference between the two. Everything here
// is defined in `log_peer.cpp`: defined in this header, it would run in the calling file.
//
// The guards keep one check from leaking into another. `SavedLogger` puts the logger's
// state back as it found it, and `CaptureCerr` takes what is logged off the run's output
// and gives `std::cerr` back, even through a throw.
//

#pragma once


// Standard libs
#include <sstream>
#include <streambuf>
#include <string>

// Internal libs
#include "log.hpp"


namespace log_peer {


	// Where the logger's state lies, as this unit sees it
	struct Addresses {
		const void* style;
		const void* level;
		const void* hostname;
		const void* task_id;
		const void* name;
	};


	Addresses addresses();


	void set_level(logging::Level level);
	void set_style(logging::Style style);


	std::string debug(const std::string& text);
	std::string info(const std::string& text);


	std::string plain(const std::string& text);
	bool stamped(const std::string& line, const std::string& rest);


	class SavedLogger {
	public:
		SavedLogger();
		~SavedLogger();

		SavedLogger(const SavedLogger&) = delete;
		SavedLogger& operator=(const SavedLogger&) = delete;

	private:
		logging::Style style;
		logging::Level level;
		std::string hostname;
		std::string task_id;
		std::string name;

	};


	class CaptureCerr {
	public:
		CaptureCerr();
		~CaptureCerr();

		CaptureCerr(const CaptureCerr&) = delete;
		CaptureCerr& operator=(const CaptureCerr&) = delete;

		std::string text() const;

	private:
		std::ostringstream buffer;
		std::streambuf* saved;

	};

}
