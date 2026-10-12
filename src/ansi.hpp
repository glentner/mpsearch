// SPDX-FileCopyrightText: 2024 Geoffrey Lentner
// SPDX-License-Identifier: GPL-3.0-only WITH GPL-3.0-linking-exception
//
// String methods
//

#pragma once


// Standard libs
#include <map>
#include <string>
#include <regex>


void init_tty();


enum Ansi {
    RESET     = 0,
    BOLD      = 1,
    FAINT     = 2,
    ITALIC    = 3,
    UNDERLINE = 4,
    BLACK     = 30,
    RED       = 31,
    GREEN     = 32,
    YELLOW    = 33,
    BLUE      = 34,
    MAGENTA   = 35,
    CYAN      = 36,
    WHITE     = 37
};


static std::map<Ansi, std::string>
ansi_by_id {
	{RESET,     "\033[0m"},
	{BOLD,      "\033[1m"},
	{FAINT,     "\033[2m"},
	{ITALIC,    "\033[3m"},
	{UNDERLINE, "\033[4m"},
	{BLACK,     "\033[30m"},
	{RED,       "\033[31m"},
	{GREEN,     "\033[32m"},
	{YELLOW,    "\033[33m"},
	{BLUE,      "\033[34m"},
	{MAGENTA,   "\033[35m"},
	{CYAN,      "\033[36m"},
	{WHITE,     "\033[37m"}
};


std::string format_ansi(Ansi ansi, const std::string& text);
std::string bold(const std::string& text);
std::string faint(const std::string& text);
std::string italic(const std::string& text);
std::string underline(const std::string& text);
std::string black(const std::string& text);
std::string red(const std::string& text);
std::string green(const std::string& text);
std::string yellow(const std::string& text);
std::string blue(const std::string& text);
std::string magenta(const std::string& text);
std::string cyan(const std::string& text);
std::string white(const std::string& text);


std::string colorize_usage(const std::string& text);

