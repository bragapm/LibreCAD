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

#ifndef DRW_DBG_H
#define DRW_DBG_H

#include <string>
#include <iostream>
#include <memory>
#include <type_traits>
#include "../drw_base.h"
//#include <iomanip>

#define DRW_DBGSL(a) DRW_dbg::getInstance()->setLevel(a)
#define DRW_DBGGL (DRW_dbg::isEnabled() ? DRW_dbg::Level::Debug : DRW_dbg::Level::None)
#define DRW_DBG(a) DRW_dbg::dbgPrint(a)
#define DRW_DBGH(a) DRW_dbg::dbgPrintH(a)
#define DRW_DBGB(a) DRW_dbg::dbgPrintB(a)
#define DRW_DBGHL(a, b, c) DRW_dbg::dbgPrintHL(a, b, c)
#define DRW_DBGPT(a, b, c) DRW_dbg::dbgPrintPT(a, b, c)

class DRW_dbg {
public:
    enum class Level {
        None,
        Debug
    };
    void setLevel(Level lvl);
    /**
     * Sets a custom debug printer to use when non-silent output
     * is required.
     */
    void setCustomDebugPrinter(std::unique_ptr<DRW::DebugPrinter> printer);
    Level getLevel();
    static DRW_dbg *getInstance();
    static bool isEnabled() noexcept { return s_enabled; }
    static void dbgPrint(const std::string& s) {
        if (s_enabled) getInstance()->print(s);
    }
    static void dbgPrint(const char* s) {
        if (s_enabled && s != nullptr) getInstance()->print(std::string(s));
    }
    template <typename T, typename std::enable_if_t<
        !std::is_convertible_v<const T&, const char*> &&
        !std::is_same_v<std::decay_t<T>, std::string>, int> = 0>
    static void dbgPrint(const T& v) {
        if (s_enabled) getInstance()->print(v);
    }
    static void dbgPrintH(long long int i) { if (s_enabled) getInstance()->printH(i); }
    static void dbgPrintB(int i) { if (s_enabled) getInstance()->printB(i); }
    static void dbgPrintHL(int c, int s, int h) { if (s_enabled) getInstance()->printHL(c, s, h); }
    static void dbgPrintPT(double x, double y, double z) { if (s_enabled) getInstance()->printPT(x, y, z); }
    void print(const std::string &s);
    void print(signed char i);
    void print(unsigned char i);
    void print(int i);
    void print(unsigned int i);
    void print(long long int i);
    void print(long unsigned int i);
    void print(long long unsigned int i);
    void print(double d);
    void printH(long long int i);
    void printB(int i);
    void printHL(int c, int s, int h);
    void printPT(double x, double y, double z);

private:
    DRW_dbg();
    static DRW_dbg *instance;
    static inline bool s_enabled{false};
    Level level{Level::None};
    DRW::DebugPrinter silentDebug;
    std::unique_ptr< DRW::DebugPrinter > debugPrinter;
    DRW::DebugPrinter* currentPrinter{nullptr};
};


#endif // DRW_DBG_H
