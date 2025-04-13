#ifdef __linux__
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <termios.h>
#endif

#ifdef _WIN32
// windows.h contains MACRO min and max
// Cause error C2589
// Disable it
#define NOMINMAX  
#include <windows.h>
#endif

#include <cstring>

#include <llvm/Support/CommandLine.h>
#include <llvm/Support/Debug.h>

#include "Util/SparrowUtils/ProgressBar.h"

using namespace Sparrow;
using namespace llvm;

#define BUF_GAP 10




ProgressBar::ProgressBar(std::string Title, ProgressBarStyle Style, float UpdateFrequency) :
            Title(Title), Style(Style), UpdateFrequency(UpdateFrequency) {
#ifdef __linux__
    struct winsize WinSize;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &WinSize);
    WindowWidth = WinSize.ws_col > 80 ? 80 : WinSize.ws_col - Title.length() - 15;
#elif _WIN32
    HANDLE StdOutHandler = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;

    GetConsoleScreenBufferInfo(StdOutHandler, &ConsoleInfo);
    WindowWidth = ConsoleInfo.dwSize.X > 80 ? 80 : ConsoleInfo.dwSize.X - Title.length() - 15;
#endif

    bool HideProgressBar=false;

    if (WindowWidth < 15) {
        ProgressBuffer = nullptr;
        return;
    }

    // To avoid buffer overflow
    ProgressBuffer = (char*) malloc(sizeof(char) * (WindowWidth + BUF_GAP));
    resetProgress();
}

void ProgressBar::showProgress(float Percent) {
    if (Percent < 0)
        Percent = 0;
    else if (Percent > 1)
        Percent = 1;

    if (Percent > 0 && Percent < 1) {
        if (Percent - LastUpdatePercent < UpdateFrequency) {
            return;
        }
    }

    LastUpdatePercent = Percent;

    // In case the window width is changed at runtime.
    resize();

    int Progress = (Percent * 100);
    int ProgressLength = Percent * WindowWidth;

    if (!HideProgressBar) {

        outs() << "\033[?25l\033[37m\033[1m" << Title;

        switch (this->Style) {
        case PBS_NumberStyle:
            outs() << " " << Progress << "%";
            break;
        case PBS_CharacterStyle:
            if (ProgressBuffer) {
                std::memset(ProgressBuffer, '#', ProgressLength);
                outs() << " [" << ProgressBuffer << "] " << Progress << "%";
            } else {
                outs() << " " << Progress << "%";
            }
            break;
        default:
            assert(false && "Unknown style!");
        }

        outs() << "\033[?25h\033[0m\r";
    }
    else {
        outs() << Title << ": " << Progress << "%\r";
    }

    outs().flush();
}

void ProgressBar::endProgress()
{
    outs() << "\n";
}

void ProgressBar::resize() {
    // clear the line.
    if (!HideProgressBar)
        outs() << "\r\033[K";

    int CurrentWindowWidth = 0;

#ifdef __linux__
    struct winsize WinSize;
    ioctl(STDIN_FILENO, TIOCGWINSZ, &WinSize);
    CurrentWindowWidth = WinSize.ws_col > 80 ? 80 : WinSize.ws_col  - Title.length() - 15;
#elif _WIN32
    HANDLE StdOutHandler = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO ConsoleInfo;

    GetConsoleScreenBufferInfo(StdOutHandler, &ConsoleInfo);
    CurrentWindowWidth = ConsoleInfo.dwSize.X > 80 ? 80 : ConsoleInfo.dwSize.X - Title.length() - 15;

#endif

    if (CurrentWindowWidth < 15) {
        free(ProgressBuffer);
        ProgressBuffer = nullptr;
        return;
    }

    if (WindowWidth == CurrentWindowWidth) {
        return;
    }

    WindowWidth = CurrentWindowWidth;
    ProgressBuffer = (char*) realloc(ProgressBuffer, sizeof(char) * (WindowWidth + BUF_GAP));
    clearBuffer();
}

float ProgressBar::getPercent()
{
	return LastUpdatePercent;
}

void ProgressBar::resetProgress(ProgressBarStyle newStyle) {
    LastUpdatePercent = 0;

    if (ProgressBuffer) {
        if (newStyle != PBS_NONE)
            Style = newStyle;
    	clearBuffer();
    }
}

void ProgressBar::clearBuffer()
{
    std::memset(ProgressBuffer, ' ', sizeof(char) * WindowWidth);
    std::memset(ProgressBuffer + WindowWidth, 0x0, sizeof(char) * BUF_GAP);
}

void ProgressBar::resetTitle(const std::string& newTitle)
{
	Title = newTitle;
}

ProgressBar::~ProgressBar() {
    if (ProgressBuffer) {
        free(ProgressBuffer);
        ProgressBuffer = nullptr;
    }
}
