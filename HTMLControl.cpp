/***************************************************************************/

/*
 * Portions Copyright (c) 1999 GMRS Software GmbH
 * Carl-von-Linde-Str. 38, D-85716 Unterschleissheim, http://www.gmrs.de
 * All rights reserved.
 *
 * Author: Arno Unkrig <arno@unkrig.de>
 */

/* This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License in the file COPYING for more details.
 */

/***************************************************************************/

/*
 * Changes to version 1.2.2 were made by Martin Bayer <mbayer@zedat.fu-berlin.de>
 * Dates and reasons of modifications:
 * Fre Jun  8 17:20:38 CEST 2001: fixed coredump
 * Sun Apr  7 11:31:10 CEST 2002: fixed parser errors
 * Wed Jul  2 22:00:21 CEST 2003: ported to g++ 3.3
 * Sun Nov 16 22:13:50 CET 2003: better rendering of XHTML
 */

/***************************************************************************/

#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "html.h"
#include "HTMLControl.h"
#include "sgml.h"
#include "cmp_nocase.h"

#ifndef nelems
#define nelems(array) (sizeof(array) / sizeof((array)[0]))
#endif

enum {
    NOT_A_TAG,
    START_TAG, BLOCK_START_TAG,
    END_TAG, BLOCK_END_TAG,
    NON_CONTAINER_TAG
};

/* ------------------------------------------------------------------------- */

/*
 * Effectively, this method simply invokes "yylex2()", but it does some
 * postprocessing on PCDATA tokens that would be difficult to do in "yylex2()".
 */
int
HTMLControl::yylex(yy_HTMLParser_stype *value_return)
{
    for (;;) { // Notice the "return" at the end of the body!
        int token, tag_type;

        if (next_token == EOF) {
            token = yylex2(value_return, &tag_type);
        }
        else {
            token = next_token;
            *value_return = next_token_value;
            tag_type = next_token_tag_type;
            next_token = EOF;
        }

        /*
         * Switch on/off "literal mode" on "<PRE>" and "</PRE>".
         */
        if (token == PRE) {
            literal_mode = true;

            /*
             * Swallow '\n' immediately following "<PRE>".
             */
            if (next_token == EOF) {
                next_token = yylex2(&next_token_value, &next_token_tag_type);
            }
            if (next_token == PCDATA) {
                string &s(*next_token_value.strinG);
                if (!s.empty() && s[0] == '\n') s.erase(0, 1);
            }
        }

        if (token == END_PRE) literal_mode = false;

        if (token == PCDATA) {
            /*
             * In order to post-process the PCDATA token, we need to look ahead one
             * token...
             */
            if (next_token == EOF) {
                next_token = yylex2(&next_token_value, &next_token_tag_type);
            }

            /*
             * Erase " '\n' { ' ' } " immediately before "</PRE>".
             */
            if (next_token == END_PRE) {
                string &s(*value_return->strinG);
                string::size_type x = s.length();
                while (x > 0 && s[x - 1] == ' ') --x;
                if (x > 0 && s[x - 1] == '\n') s.erase(x - 1, string::npos);
            }
            else

                /*
                 * Erase whitespace before end tag or block start tag.
                 */
                 if (!literal_mode && (
                     next_token_tag_type == END_TAG ||
                     next_token_tag_type == BLOCK_END_TAG ||
                     next_token_tag_type == BLOCK_START_TAG
                     )) {
                     string &s(*value_return->strinG);
                     string::size_type x = s.length();
                     while (x > 0 && isspace(s[x - 1])) --x;
                     s.erase(x, string::npos);
                 }

            /*
             * Collate sequences of whitespace, if not in "literal mode".
             */
            if (!literal_mode) {
                string &s(*value_return->strinG);
                //      bool   whitespace_only = true;
                for (string::size_type x = 0; x < s.length(); ++x) {
                    if (isspace(s[x])) {
                        string::size_type y;
                        for (y = x + 1; y < s.length() && isspace(s[y]); ++y);
                        s.replace(x, y - x, " ");
                    }
                    else {
                        //          whitespace_only = false;
                    }
                }
                if (s.empty()) { delete value_return->strinG; continue; }
            }
        }

        /*
         * Erase whitespace after start tag or block end tag, if not in "literal
         * mode".
         */
        if (!literal_mode && (
            (
            tag_type == START_TAG ||
            tag_type == BLOCK_START_TAG ||
            tag_type == BLOCK_END_TAG ||
            token == BR ||
            token == HR
            ) &&
            token != SCRIPT && token != STYLE
            )) {
            if (next_token == EOF) {
                next_token = yylex2(&next_token_value, &next_token_tag_type);
            }
            if (next_token == PCDATA) {
                string &s(*next_token_value.strinG);
                string::size_type x;
                for (x = 0; x < s.length() && isspace(s[x]); ++x);
                if (x > 0) s.erase(0, x);
                if (s.empty()) {
                    delete next_token_value.strinG;
                    next_token = EOF;
                }
            }
        }

        return token;
    }
}

/* ------------------------------------------------------------------------- */

/*
 * Keep this array sorted alphabetically!
 */
static const struct TextToIntP {
    char      name[11];
    char      block_tag;
    const int *start_tag_code;
    const int *end_tag_code;
} tag_names[] = {
#define pack1(tag) { #tag, 0, &HTMLParser::tag, 0 }
#define pack2(tag) { #tag, 0, &HTMLParser::tag, &HTMLParser::END_##tag }
#define pack3(tag) { #tag, 1, &HTMLParser::tag, &HTMLParser::END_##tag }
    pack2(A),
    pack3(ADDRESS),
    pack2(APPLET),
    pack1(AREA),
    pack2(B),
    pack1(BASE),
    pack1(BASEFONT),
    pack2(BIG),
    pack3(BLOCKQUOTE),
    pack3(BODY),
    pack1(BR),
    pack3(CAPTION),
    pack3(CENTER),
    pack3(CITE),
    pack2(CODE),
    pack3(DD),
    pack2(DFN),
    pack3(DIR),
    pack3(DIV),
    pack3(DL),
    pack3(DT),
    pack2(EM),
    pack2(FONT),
    pack3(FORM),
    pack3(H1),
    pack3(H2),
    pack3(H3),
    pack3(H4),
    pack3(H5),
    pack3(H6),
    pack3(HEAD),
    pack1(HR),
    pack3(HTML),
    pack2(I),
    pack1(IMG),
    pack1(INPUT),
    pack1(ISINDEX),
    pack2(KBD),
    pack3(LI),
    pack1(LINK),
    pack2(MAP),
    pack3(MENU),
    pack1(META),
    pack2(NOBR),
    pack3(OL),
    pack3(OPTION),
    pack3(P),
    pack1(PARAM),
    pack3(PRE),
    pack2(SAMP),
    pack3(SCRIPT),
    pack2(SELECT),
    pack2(SMALL),
    pack2(STRIKE),
    pack2(STRONG),
    pack3(STYLE),
    pack2(SUB),
    pack2(SUP),
    pack3(TABLE),
    pack3(TD),
    pack2(TEXTAREA),
    pack3(TH),
    pack3(TITLE),
    pack3(TR),
    pack2(TT),
    pack2(U),
    pack3(UL),
    pack2(VAR),
#undef pack
};

/* ------------------------------------------------------------------------- */

int
HTMLControl::yylex2(yy_HTMLParser_stype *value_return, int *tag_type_return)
{
    int c;

    *tag_type_return = NOT_A_TAG;
    for (;;) {   // Notice the "return" at the end of this loop.
        /*
         * Get the first character of the token.
         */
        c = get_char();
        if (c == EOF) return EOF;

        if (c == '<') {
            /*
             * Examine the first character of the tag.
             */
            c = get_char();
            if (c == '!') {
                c = get_char();
                if (c == '-') {
                    c = get_char();
                    if (c != '-') return SCAN_ERROR;

                    /*
                 * This is a comment... skip it!
                 *
                 *   <!-- Single-line comment -->
                 *
                 *   <!-- Multi-
                 *        line
                 *        comment //-->
                 *
                 * EXTENSION: Allow "-->" as the terminator of a multi-line comment.
                 */
                    int state = 0;
                    do {
                        c = get_char();
                        if (c == EOF) return SCAN_ERROR;
                        switch (state) {
                        case 0: if (c == '-') state = 1; break;
                        case 1: state = c == '-' ? 2 : 0; break;
                        case 2: state = c == '>' ? 3 : c == '-' ? 2 : 0; break;
                        }
                    } while (state != 3);
                    continue;   // Start over
                }

                /*
                 * Scan "<!DOCTYPE ...>" tag.
                 */
                if (!isalpha(c)) return SCAN_ERROR;
                string tag_name(1, '!');
                tag_name += c;
                for (;;) {
                    c = get_char();
                    if (!isalnum(c) && c != '-') break;
                    tag_name += c;
                }
                if (cmp_nocase(tag_name, "!DOCTYPE") != 0) return SCAN_ERROR;
                while (c != '>') {
                    c = get_char();
                    if (c == EOF) return SCAN_ERROR;
                    // Let newline not close the DOCTYPE tag - Arno
                }
                return DOCTYPE;
            }

            if (c == '/' || isalpha(c) || c == '_') {
                string tag_name;
                bool   is_end_tag = false;

                if (c == '/') { is_end_tag = true; c = get_char(); }
                if (!isalpha(c) && c != '_') return SCAN_ERROR;
                tag_name += c;
                for (;;) {
                    c = get_char();
                    if (!isalnum(c) && c != '-' && c != '_' && c != ':') break;
                    // Tolerate colons in tags for MS-Word's sake - Arno
                    tag_name += c;
                }

                while (isspace(c)) c = get_char();

                /*
                 * Scan tag attributes (only for opening tags). Create the
                 * "tag_attributes" only on demand; this saves a lot of overhead.
                 */
                auto_ptr<list<TagAttribute> > tag_attributes;
                if (!is_end_tag) {
                    while (isalpha(c) || c == '_') {
                        TagAttribute attribute;

                        /*
                         * Scan attribute name.
                         */
                        attribute.first = c;
                        for (;;) {
                            c = get_char();
                            if (!isalpha(c) && c != '-' && c != '_' && c != ':') break;
                            // Same as in line 352 - Arno
                            attribute.first += c;
                        }

                        while (isspace(c)) c = get_char(); // Skip WS after attribute name

                        /*
                         * Scan (optional) attribute value.
                         */
                        if (c == '=') {
                            c = get_char();
                            while (isspace(c)) c = get_char();
                            if (c == '"' || c == '\'') {
                                int closing_quote = c;   // Same as opening quote!
                                for (;;) {
                                    c = get_char();
                                    if (c == EOF) return SCAN_ERROR;
                                    // Accept multiple-line elements - Arno
                                    if (c == closing_quote) break;

                                    /*
                                     * Do *not* interpret "&auml;" and consorts here! This
                                     * would ruin tag attributes like "HREF=hhh?a=1&b=2".
                                     */
                                    attribute.second += c;
                                }
                                c = get_char();    // Get next char after closing quote.
                            }
                            else
                                while (c != '>' && (unsigned char)c > (unsigned char) ' ') {
                                    // This is for non-ACSII chars - Arno
                                    if (c == EOF) return SCAN_ERROR;
                                    // Same as in line 390
                                    attribute.second += c;
                                    c = get_char();
                                }

                            while (isspace(c)) c = get_char();   // Skip WS after attr value
                        }

                        /*
                         * Store the attribute.
                         */
                        if (!tag_attributes.get()) {
                            tag_attributes.reset(new list<TagAttribute>);
                        }
                        tag_attributes->push_back(attribute);
                    }
                }

                // accept XHTML tags like <hr /> - Alexander Solovey

                if (c != '>') {
                    if (c == '/') {
                        c = get_char();
                        if (c != '>') {
                            return SCAN_ERROR;
                        }
                    }
                    else {
                        return SCAN_ERROR;
                    }
                }

                if (debug_scanner) {
                    std::cerr << "Scanned tag \"<" << (is_end_tag ? "/" : "") << tag_name;
                    if (!is_end_tag && tag_attributes.get()) {
                        const list<TagAttribute>           &ta(*tag_attributes);
                        list<TagAttribute>::const_iterator j;
                        for (j = ta.begin(); j != ta.end(); ++j) {
                            std::cerr << " " << (*j).first << "=\"" << (*j).second << "\"";
                        }
                    }
                    std::cerr << ">\"" << std::endl;
                }

                /*
                 * Look up the tag in the table of recognized tags.
                 */
                static int(*const f)(const char *, const char *) = cmp_nocase;
                const TextToIntP *tag = (const TextToIntP *)bsearch(
                    tag_name.c_str(),
                    tag_names, nelems(tag_names), sizeof(TextToIntP),
                    (int(*)(const void *, const void *)) f
                    );
                if (tag == NULL) { /* EXTENSION: Swallow unknown tags. */
                    if (debug_scanner) {
                        std::cerr << "Tag unknown -- swallowed." << std::endl;
                    }
                    continue;
                }

                /*
                 * Return the BISON token code for the tag.
                 */
                if (is_end_tag) {
                    if (!tag->end_tag_code) {
                        if (debug_scanner) {
                            std::cerr << "Non-container end tag scanned." << std::endl;
                        }
                        continue;
                    }
                    *tag_type_return = tag->block_tag ? BLOCK_END_TAG : END_TAG;
                    return *tag->end_tag_code;
                }
                else {
                    *tag_type_return = (
                        !tag->end_tag_code ? NON_CONTAINER_TAG :
                        tag->block_tag ? BLOCK_START_TAG : START_TAG
                        );
                    value_return->tag_attributes = tag_attributes.release();
                    return *tag->start_tag_code;
                }
            }

            /*
             * EXTENSION: This tag did not match "<!", and not "</", and not
             * "<[A-Za-z-]", so take it as literal text.
             */
            unget_char(c);
            c = '<';
        }

        if (c == '\n' || (unsigned char)c >= (unsigned char) ' ') {
            // Same as in line 402
            string *s = value_return->strinG = new string;

            while (c != EOF) {
                /*
                 * Accept literal '<' in some cases.
                 */
                if (c == '<') {
                    int c2;
                    unget_char(c2 = get_char());
                    if (c2 == '!' || c2 == '/' || isalpha(c2)) { unget_char(c); break; }
                }

                *s += c;
                c = get_char();
            }

            replace_sgml_entities(s);  // Replace "&auml;" and consorts.

            /*
             * Swallow empty PCDATAs.
             */
            if (s->empty()) { delete s; continue; }

            if (debug_scanner) std::cerr << "Scanned PCDATA \"" << *s << "\"" << std::endl;

            return PCDATA;
        }

        return SCAN_ERROR;
    }
}

/* ------------------------------------------------------------------------- */

bool
HTMLControl::read_cdata(const char *terminal, string *value_return)
{
    string &s(*value_return);
    int    c;
    int    state = 0;

    for (;;) {
        c = get_char();
        if (c == EOF) return false;
        if (toupper(c) == terminal[state]) {
            state++;
            if (terminal[state] == '\0') {
                // s.erase(s.length() - state);
                // caused core dump on empty STYLE and SCRIPT elements - Johannes Geiger
                s.erase(s.length() - state + 1);
                return true;
            }
        }
        else {
            state = 0;
        }
        s += c;
    }
}

/* ------------------------------------------------------------------------- */

int
HTMLControl::get_char()
{
    if (number_of_ungotten_chars > 0) {
        return ungotten_chars[--number_of_ungotten_chars];
    }

    int c = is.get();
    while (c == '\r') c = is.get();

    if (c == EOF) {
        ;
    }
    else
        if (c == '\n') {
            current_line++;
            current_column = 0;
        }
        else {
            current_column++;
        }

        return c;
}

/* ------------------------------------------------------------------------- */

void
HTMLControl::unget_char(int c)
{
    if (number_of_ungotten_chars == nelems(ungotten_chars)) {
        yyerror("Too many chars ungotten");
        return;
    }
    ungotten_chars[number_of_ungotten_chars++] = c;
}

/* ------------------------------------------------------------------------- */