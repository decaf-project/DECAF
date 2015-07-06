/* -*- linux-c -*- */
/*
    This file is part of llconf2

    Copyright (C) 2007  Darius Davis <darius.davis@avocent.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

/**
 * Display a warning indicating that something wasn't where it was expected
 * in a FE.  Prints the first "index" chars of "buffer", with a marker
 * underneath position index+1.
 *
 * @param [in] buffer The text of the FE.  Tabs, newlines etc. are OK.
 * @param [in] index The number of characters into the FE where the problem
 * 		occurred.
 * @param [in] warning_text The text to display on the line "introducing" the
 * 		error.
 * @param [in] text_at_position The text to display immediately after the
 * 		marker indicating the location of the error.
 */
void parse_error_at(const char const *buffer, int index, const char *warning_text, const char *text_at_position);

/**
 * Display a warning indicating that something wasn't where it was expected
 * in some input text.  Prints at least the first "index" chars of "buffer",
 * and continues up to the following newline, then displays a marker and an
 * error message.
 *
 * @param [in] buffer The text of the FE.  Tabs, newlines etc. are OK.
 * @param [in] index The number of characters into the FE where the problem
 * 		occurred.
 * @param [in] expected A description of what was expected at that location.
 * @param [in] assumed What we assumed was there.
 */
void parse_error_at_expected(const char const *buffer, int index, const char *expected, const char *assumed);

