// =============================================================================
// DRIFT standalone entry point.
//
// Same pattern as SP·L's custom standalone: subclass StandaloneFilterWindow,
// hide JUCE's hardcoded Options button, slot our own with an extended menu
// (About / Bug Report / GitHub / Ko-fi alongside the JUCE defaults).
// =============================================================================

#include "PluginEditor.h"
#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>

namespace
{
    constexpr const char* kDriftVersion = "v1.0.0";

    juce::TextButton* findJuceOptionsButton (juce::StandaloneFilterWindow& w)
    {
        for (int i = 0; i < w.getNumChildComponents(); ++i)
            if (auto* b = dynamic_cast<juce::TextButton*> (w.getChildComponent (i)))
                if (b->getButtonText() == "Options")
                    return b;
        return nullptr;
    }
}

//==============================================================================
class DriftStandaloneWindow  : public juce::StandaloneFilterWindow
{
public:
    DriftStandaloneWindow (const juce::String& title,
                           juce::Colour backgroundColour,
                           std::unique_ptr<juce::StandalonePluginHolder> pluginHolderIn)
        : juce::StandaloneFilterWindow (title, backgroundColour, std::move (pluginHolderIn))
    {
        if (auto* juceBtn = findJuceOptionsButton (*this))
            juceBtn->setVisible (false);

        driftOptionsButton.setButtonText ("Options");
        driftOptionsButton.setTriggeredOnMouseDown (true);
        driftOptionsButton.onClick = [this] { showOptionsMenu(); };
        juce::Component::addAndMakeVisible (driftOptionsButton);
    }

    void resized() override
    {
        juce::StandaloneFilterWindow::resized();
        driftOptionsButton.setBounds (8, 6, 60, getTitleBarHeight() - 8);
    }

private:
    void showOptionsMenu()
    {
        juce::PopupMenu m;
        m.addItem (1, "Audio/MIDI Settings...");
        m.addSeparator();
        m.addItem (2, "Save current state...");
        m.addItem (3, "Load a saved state...");
        m.addSeparator();
        m.addItem (4, "Reset to default state");
        m.addSeparator();
        m.addItem (5, "About / Credits...");
        m.addItem (6, "Report a Bug (email)");
        m.addItem (7, "GitHub (source + releases)");
        m.addItem (8, "Tip on Ko-fi");
        m.addSeparator();
        m.addItem (9, juce::String ("DRIFT ") + kDriftVersion, false);

        m.showMenuAsync (
            juce::PopupMenu::Options().withTargetComponent (&driftOptionsButton),
            [this] (int choice)
            {
                switch (choice)
                {
                    case 1: pluginHolder->showAudioSettingsDialog(); break;
                    case 2: pluginHolder->askUserToSaveState();      break;
                    case 3: pluginHolder->askUserToLoadState();      break;
                    case 4: resetToDefaultState();                   break;
                    case 5: showAboutOverlay();                      break;
                    case 6: juce::URL ("mailto:elliottdevs@gmail.com"
                                       "?subject=DRIFT%20Bug%20Report"
                                       "&body=DRIFT%20version%3A%20" + juce::String (kDriftVersion) +
                                       "%0AOS%3A%20%0A%0A"
                                       "What%20happened%3A%0A%0A"
                                       "Steps%20to%20reproduce%3A%0A").launchInDefaultBrowser();
                            break;
                    case 7: juce::URL ("https://github.com/itselliott/drift")
                                 .launchInDefaultBrowser(); break;
                    case 8: juce::URL ("https://ko-fi.com/itselliott")
                                 .launchInDefaultBrowser(); break;
                    default: break;
                }
            });
    }

    void showAboutOverlay()
    {
        if (auto* proc = getAudioProcessor())
            if (auto* editor = proc->getActiveEditor())
                if (auto* driftEditor = dynamic_cast<DriftAudioProcessorEditor*> (editor))
                    driftEditor->showAboutOverlay();
    }

    juce::TextButton driftOptionsButton;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DriftStandaloneWindow)
};

//==============================================================================
class DriftStandaloneApp  : public juce::JUCEApplication
{
public:
    DriftStandaloneApp()
    {
        juce::PropertiesFile::Options options;
        options.applicationName     = juce::CharPointer_UTF8 (JucePlugin_Name);
        options.filenameSuffix      = ".settings";
        options.osxLibrarySubFolder = "Application Support";
       #if JUCE_LINUX || JUCE_BSD
        options.folderName          = "~/.config";
       #else
        options.folderName          = "";
       #endif
        appProperties.setStorageParameters (options);
    }

    const juce::String getApplicationName() override    { return juce::CharPointer_UTF8 (JucePlugin_Name); }
    const juce::String getApplicationVersion() override { return JucePlugin_VersionString; }
    bool moreThanOneInstanceAllowed() override          { return true; }
    void anotherInstanceStarted (const juce::String&) override {}

    void initialise (const juce::String&) override
    {
        mainWindow = createWindow();
        if (mainWindow != nullptr)
            mainWindow->setVisible (true);
        else
            pluginHolder = createPluginHolder();
    }

    void shutdown() override
    {
        pluginHolder.reset();
        mainWindow.reset();
        appProperties.saveIfNeeded();
    }

    void systemRequestedQuit() override
    {
        if (pluginHolder != nullptr)        pluginHolder->savePluginState();
        if (mainWindow   != nullptr)        mainWindow->pluginHolder->savePluginState();

        if (juce::ModalComponentManager::getInstance()->cancelAllModalComponents())
        {
            juce::Timer::callAfterDelay (100, []
            {
                if (auto* app = juce::JUCEApplicationBase::getInstance())
                    app->systemRequestedQuit();
            });
        }
        else
        {
            quit();
        }
    }

private:
    std::unique_ptr<DriftStandaloneWindow> createWindow()
    {
        if (juce::Desktop::getInstance().getDisplays().displays.isEmpty())
            return nullptr;

        return std::make_unique<DriftStandaloneWindow> (
            getApplicationName(),
            juce::LookAndFeel::getDefaultLookAndFeel()
                .findColour (juce::ResizableWindow::backgroundColourId),
            createPluginHolder());
    }

    std::unique_ptr<juce::StandalonePluginHolder> createPluginHolder()
    {
       #ifdef JucePlugin_PreferredChannelConfigurations
        constexpr juce::StandalonePluginHolder::PluginInOuts channels[] {
            JucePlugin_PreferredChannelConfigurations
        };
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig (
            channels, juce::numElementsInArray (channels));
       #else
        const juce::Array<juce::StandalonePluginHolder::PluginInOuts> channelConfig;
       #endif
        auto holder = std::make_unique<juce::StandalonePluginHolder> (
            appProperties.getUserSettings(), false, juce::String{}, nullptr,
            channelConfig, false);

        // Un-mute the audio input at startup so audio flows through DRIFT
        // immediately. Suppresses JUCE's "Audio input is muted to avoid
        // feedback loop" banner. Trade-off: standalone users with a mic +
        // open speakers near each other can get acoustic feedback. Fix
        // that on their side by using headphones, by sending audio via
        // LINK from SP·L, or by manually muting input in Audio Settings.
        holder->getMuteInputValue().setValue (false);
        return holder;
    }

    juce::ApplicationProperties             appProperties;
    std::unique_ptr<DriftStandaloneWindow>  mainWindow;
    std::unique_ptr<juce::StandalonePluginHolder> pluginHolder;
};

//==============================================================================
#if JucePlugin_Build_Standalone
 START_JUCE_APPLICATION (DriftStandaloneApp)
#endif
