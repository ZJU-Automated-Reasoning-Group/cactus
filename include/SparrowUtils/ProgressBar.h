#pragma once
#include <string>


namespace Sparrow {

    class ProgressBar {
    public:
        enum ProgressBarStyle {
            PBS_NumberStyle,            // Show a percentage number only, e.g. 5%
            PBS_CharacterStyle,         // Show an ascii progress bar a percentage number, e.g. [###           ] 5%
            PBS_NONE                    // An indicator
        };

    private:
        /// Title of the progress bar
        std::string Title;

        bool HideProgressBar;

        /// Style of the progress bar
        ProgressBarStyle Style;

        /// Windows width
        int WindowWidth;

        /// The buffer of characters, which
        /// will be shown in console as a progress bar.
        char* ProgressBuffer;

        /// How frequently to update the progress bar.
        /// For example, it Delta is 0.01, then the bar
        /// will be updated every 1 percent. This can
        /// avoid too many outputs.
        /// @{
        float UpdateFrequency;
        float LastUpdatePercent;
        /// @}

    public:
        ProgressBar(std::string Title, ProgressBarStyle Style = PBS_CharacterStyle, float UpdateFrequency = 0.01);

        virtual ~ProgressBar();

        /// The input must be in [0, 1]
        /// 0 means it just starts.
        /// 1 means it completes.
        void showProgress(float Percent);

        float getPercent();

        /// Clear the progress bar status
        void resetProgress(ProgressBarStyle NewStyle = PBS_NONE);

        void resetTitle(const std::string& NewTitle);

        /// Change a new line
        void endProgress();

    private:
        /// In case the console width is changed
        /// at runtime.
        void resize();

        void clearBuffer();
    };

}

