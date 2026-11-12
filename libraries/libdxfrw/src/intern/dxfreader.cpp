/******************************************************************************
**  libDXFrw - Library to read/write DXF files (ascii & binary)              **
**                                                                           **
**  Copyright (C) 2011-2015 José F. Soriano, rallazz@gmail.com               **
**                                                                           **
**  This library is free software, licensed under the terms of the GNU       **
**  General Public License as published by the Free Software Foundation,     **
**  either version 2 of the License, or (at your option) any later version.  **
**  You should have received a copy of the GNU General Public License        **
**  along with this program.  If not, see <http://www.gnu.org/licenses/>.    **
******************************************************************************/

#include <cstdlib>
#include <fstream>
#include <string>
#include <charconv>
#include <cstring>
#include <sstream>
#include "dxfreader.h"
#include "drw_textcodec.h"
#include "drw_dbg.h"

bool dxfReader::readRec(int *codeData) {
//    std::string text;
    int code;

    if (!readCode(&code))
        return false;
    *codeData = code;

    if (code < 10)
        readString();
    else if (code < 60)
        readDouble();
    else if (code < 80)
        readInt16();
    else if (code > 89 && code < 100) //TODO this is an int 32b
        readInt32();
    else if (code == 100 || code == 102 || code == 105)
        readString();
    else if (code > 109 && code < 150) //skip not used at the v2012
        readDouble();
    else if (code > 159 && code < 170) //skip not used at the v2012
        readInt64();
    else if (code < 180)
        readInt16();
    else if (code > 209 && code < 240) //skip not used at the v2012
        readDouble();
    else if (code > 269 && code < 290) //skip not used at the v2012
        readInt16();
    else if (code < 300) //TODO this is a boolean indicator, int in Binary?
        readBool();
    else if (code < 310)
        readString();
    else if (code < 320)
        readBinary();
    else if (code < 370)
        readString();
    else if (code < 390)
        readInt16();
    else if (code < 400)
        readString();
    else if (code < 410)
        readInt16();
    else if (code < 420)
        readString();
    else if (code < 430) //TODO this is an int 32b
        readInt32();
    else if (code < 440)
        readString();
    else if (code < 450) //TODO this is an int 32b
        readInt32();
    else if (code < 460) //TODO this is long??
        readInt32();
    else if (code < 470) //TODO this is a floating point double precision??
        readDouble();
    else if (code < 481)
        readString();
    else if( 999 == code && m_bIgnoreComments) {
        readString();
        return readRec( codeData);
    }
    else if (code == 1004)
        readBinary();
    else if (code > 998 && code < 1009) //skip not used at the v2012
        readString();
    else if (code < 1060) //TODO this is a floating point double precision??
        readDouble();
    else if (code < 1071)
        readInt16();
    else if (code == 1071) //TODO this is an int 32b
        readInt32();
    else if (skip)
        //skip safely this dxf entry ( ok for ascii dxf)
        readString();
    else
        //break in binary files because the conduct is unpredictable
        return false;

    return isGood();
}
bool dxfReader::isGood() const {
    return (filestr->good());
}
int dxfReader::getHandleString(){
    const char* p = strData.data();
    const char* end = p + strData.size();
    while (p < end && (*p == ' ' || *p == '\t')) {
        ++p;
    }
    if (end - p >= 2 && p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        p += 2;
    }
    int res = 0;
    auto [ptr, ec] = std::from_chars(p, end, res, 16);
    if (ec != std::errc{}) {
        return 0;
    }
    return res;
}

bool dxfReaderBinary::readCode(int *code) {
    unsigned short *int16p;
    char buffer[2];
    filestr->read(buffer,2);
    int16p = (unsigned short *) buffer;
//exist a 32bits int (code 90) with 2 bytes???
    if ((*code == 90) && (*int16p>2000)){
        DRW_DBG(*code); DRW_DBG(" de 16bits\n");
        filestr->seekg(-4, std::ios_base::cur);
        filestr->read(buffer,2);
        int16p = (unsigned short *) buffer;
    }
    *code = *int16p;
    DRW_DBG(*code); DRW_DBG("\n");

    return (filestr->good());
}

bool dxfReaderBinary::readString() {
    type = STRING;
    std::getline(*filestr, strData, '\0');
    DRW_DBG(strData); DRW_DBG("\n");
    return (filestr->good());
}

bool dxfReaderBinary::readString(std::string *text) {
    type = STRING;
    std::getline(*filestr, *text, '\0');
    DRW_DBG(*text); DRW_DBG("\n");
    return (filestr->good());
}

bool dxfReaderBinary::readBinary() {
    unsigned char chunklen {0};

    filestr->read( reinterpret_cast<char *>(&chunklen), 1);
    filestr->seekg( chunklen, std::ios_base::cur);
    DRW_DBG( chunklen); DRW_DBG( " byte(s) binary data bypassed\n");

    return (filestr->good());
}

bool dxfReaderBinary::readInt16() {
    type = INT32;
    char buffer[2];
    filestr->read(buffer,2);
    intData = (int)((buffer[1] << 8) | buffer[0]);
    DRW_DBG(intData); DRW_DBG("\n");
    return (filestr->good());
}

bool dxfReaderBinary::readInt32() {
    type = INT32;
    unsigned *int32p;
    char buffer[4];
    filestr->read(buffer,4);
    int32p = (unsigned *) buffer;
    intData = *int32p;
    DRW_DBG(intData); DRW_DBG("\n");
    return (filestr->good());
}

bool dxfReaderBinary::readInt64() {
    type = INT64;
    unsigned long long int *int64p; //64 bits integer pointer
    char buffer[8];
    filestr->read(buffer,8);
    int64p = (unsigned long long int *) buffer;
    int64 = *int64p;
    DRW_DBG(int64); DRW_DBG(" int64\n");
    return (filestr->good());
}

bool dxfReaderBinary::readDouble() {
    type = DOUBLE;
    double *result;
    char buffer[8];
    filestr->read(buffer,8);
    result = (double *) buffer;
    doubleData = *result;
    DRW_DBG(doubleData); DRW_DBG("\n");
    return (filestr->good());
}

//saved as int or add a bool member??
bool dxfReaderBinary::readBool() {
    char buffer[1];
    filestr->read(buffer,1);
    intData = (int)(buffer[0]);
    DRW_DBG(intData); DRW_DBG("\n");
    return (filestr->good());
}

namespace {
inline const char* skipLeadingSpaces(const char* p, const char* end) {
    while (p < end && (*p == ' ' || *p == '\t' || *p == '\v' || *p == '\f')) {
        ++p;
    }
    return p;
}

inline int parseAsciiInt(const char* p, const char* end) {
    p = skipLeadingSpaces(p, end);
    if (p < end && *p == '+' && (p + 1 < end) && (p[1] >= '0' && p[1] <= '9')) {
        ++p;
    }
    int val = 0;
    auto [ptr, ec] = std::from_chars(p, end, val, 10);
    if (ec == std::errc{}) {
        return val;
    }
    if (ec == std::errc::result_out_of_range) {
        std::string tmp(p, static_cast<std::size_t>(end - p));
        return std::atoi(tmp.c_str());
    }
    return 0;
}

inline double parseAsciiDouble(const char* p, const char* end) {
    p = skipLeadingSpaces(p, end);
    if (p < end && *p == '+' && (p + 1 < end) &&
        ((p[1] >= '0' && p[1] <= '9') || p[1] == '.')) {
        ++p;
    }
    double val = 0.0;
    auto [ptr, ec] = std::from_chars(p, end, val);
    if (ec == std::errc{}) {
        return val;
    }
    std::string text(p, static_cast<std::size_t>(end - p));
    std::istringstream sd(text);
    if (!(sd >> val)) {
        val = 0.0;
    }
    return val;
}
} // namespace

dxfReaderAscii::dxfReaderAscii(std::ifstream *stream)
    : dxfReader(stream)
    , m_buf(kBufferSize)
{
    skip = true;
}

bool dxfReaderAscii::isGood() const {
    return !m_eof && !filestr->bad();
}

bool dxfReaderAscii::readLine(const char*& outBeg, const char*& outEnd) {
    if (m_eof) {
        outBeg = nullptr;
        outEnd = nullptr;
        return false;
    }

    if (m_pos < m_end) {
        const char* start = m_buf.data() + m_pos;
        const std::size_t avail = m_end - m_pos;
        if (const void* nlPtr = std::memchr(start, '\n', avail)) {
            const char* nl = static_cast<const char*>(nlPtr);
            m_pos = static_cast<std::size_t>((nl - m_buf.data()) + 1);
            const char* lineEnd = nl;
            if (lineEnd > start && lineEnd[-1] == '\r') {
                --lineEnd;
            }
            outBeg = start;
            outEnd = lineEnd;
            return true;
        }
        m_lineScratch.assign(start, avail);
        m_pos = 0;
        m_end = 0;
    } else {
        m_lineScratch.clear();
    }

    while (true) {
        filestr->read(m_buf.data(), static_cast<std::streamsize>(m_buf.size()));
        m_end = static_cast<std::size_t>(filestr->gcount());
        m_pos = 0;
        if (m_end == 0) {
            m_eof = true;
            if (!m_lineScratch.empty() && m_lineScratch.back() == '\r') {
                m_lineScratch.pop_back();
            }
            outBeg = m_lineScratch.data();
            outEnd = outBeg + m_lineScratch.size();
            return false;
        }

        if (const void* nlPtr = std::memchr(m_buf.data(), '\n', m_end)) {
            const char* nl = static_cast<const char*>(nlPtr);
            const std::size_t prefixLen = static_cast<std::size_t>(nl - m_buf.data());
            m_lineScratch.append(m_buf.data(), prefixLen);
            m_pos = prefixLen + 1;
            if (!m_lineScratch.empty() && m_lineScratch.back() == '\r') {
                m_lineScratch.pop_back();
            }
            outBeg = m_lineScratch.data();
            outEnd = outBeg + m_lineScratch.size();
            return true;
        }

        m_lineScratch.append(m_buf.data(), m_end);
        m_end = 0;
    }
}

bool dxfReaderAscii::readCode(int *code) {
    const char* p = nullptr;
    const char* end = nullptr;
    const bool ok = readLine(p, end);
    *code = (p != nullptr) ? parseAsciiInt(p, end) : 0;
    DRW_DBG(*code); DRW_DBG("\n");
    return ok && !filestr->bad();
}

bool dxfReaderAscii::readString(std::string *text) {
    type = STRING;
    const char* p = nullptr;
    const char* end = nullptr;
    const bool ok = readLine(p, end);
    if (p != nullptr && end >= p) {
        text->assign(p, static_cast<std::size_t>(end - p));
    } else {
        text->clear();
    }
    return ok && !filestr->bad();
}

bool dxfReaderAscii::readString() {
    type = STRING;
    const char* p = nullptr;
    const char* end = nullptr;
    const bool ok = readLine(p, end);
    if (p != nullptr && end >= p) {
        strData.assign(p, static_cast<std::size_t>(end - p));
    } else {
        strData.clear();
    }
    DRW_DBG(strData); DRW_DBG("\n");
    return ok && !filestr->bad();
}

bool dxfReaderAscii::readBinary() {
    return readString();
}

bool dxfReaderAscii::readInt16() {
    type = INT32;
    const char* p = nullptr;
    const char* end = nullptr;
    if (readLine(p, end) && !filestr->bad()) {
        intData = parseAsciiInt(p, end);
        DRW_DBG(intData); DRW_DBG("\n");
        return true;
    }
    return false;
}

bool dxfReaderAscii::readInt32() {
    type = INT32;
    return readInt16();
}

bool dxfReaderAscii::readInt64() {
    type = INT64;
    return readInt16();
}

bool dxfReaderAscii::readDouble() {
    type = DOUBLE;
    const char* p = nullptr;
    const char* end = nullptr;
    if (readLine(p, end) && !filestr->bad()) {
        doubleData = parseAsciiDouble(p, end);
        DRW_DBG(doubleData); DRW_DBG('\n');
        return true;
    }
    return false;
}

//saved as int or add a bool member??
bool dxfReaderAscii::readBool() {
    type = BOOL;
    const char* p = nullptr;
    const char* end = nullptr;
    if (readLine(p, end) && !filestr->bad()) {
        intData = parseAsciiInt(p, end);
        DRW_DBG(intData); DRW_DBG("\n");
        return true;
    }
    return false;
}

