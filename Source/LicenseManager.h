#pragma once

#include <JuceHeader.h>

//==============================================================================
// SECURE License Manager with multiple protection layers:
// 1. Gumroad API verification
// 2. Machine ID binding (prevents simple key sharing)
// 3. Periodic re-verification (every 7 days)
// 4. Blowfish encryption for stored data (not weak XOR)
// 5. Key format validation (rejects random text)
// 6. Online time verification (prevents clock manipulation)
//==============================================================================

class LicenseManager
{
public:
    LicenseManager();

    // Call once in the AudioProcessor constructor
    void loadLicenseFromDisk();

    // Call when user enters a key and clicks "Activate"
    bool tryActivate (const juce::String& keyString);

    // License status
    bool isLicensed() const noexcept           { return licensed; }
    juce::String getLastError() const noexcept { return lastError; }
    juce::String getLicenseKey() const noexcept { return storedKey; }

    // Trial status
    bool isInTrialMode() const noexcept        { return inTrial && !trialExpired; }
    bool isTrialExpired() const noexcept       { return trialExpired; }
    int  getTrialDaysRemaining() const noexcept { return trialDaysRemaining; }

    // First launch detection (no license AND no trial file)
    bool isFirstLaunch() const noexcept        { return firstLaunch; }

    // Start trial - called when user clicks "Start Free Trial"
    // Now requires a valid trial code from Gleam/website
    bool startTrialWithCode(const juce::String& trialCode);

    // Legacy method - will fail without code
    void startTrial();

    // Returns true if plugin should produce audio (licensed OR valid trial with internet)
    bool canProduceAudio() const noexcept      { return true; /* DEV OVERRIDE */ }

    // Returns true if we should apply trial nag (noise interruption)
    bool shouldApplyTrialNag() const noexcept  { return false; /* DEV OVERRIDE */ }

    // Returns true if we need to show welcome/activation popup
    bool needsActivationPrompt() const noexcept { return false; /* DEV OVERRIDE — no activation popup */ }

    // Returns true if trial needs internet connection to work
    bool trialNeedsInternet() const noexcept   { return inTrial && trialDaysRemaining == -1; }

    // Periodic verification check (call from timer in processor)
    void checkPeriodicVerification();

private:
    bool         licensed        = false;
    bool         inTrial         = false;
    bool         trialExpired    = false;
    bool         firstLaunch     = false;
    int          trialDaysRemaining = 0;
    juce::String storedKey;
    juce::String lastError;

    static constexpr int TRIAL_DAYS = 7;
    static constexpr int REVERIFY_DAYS = 7;      // Re-verify license every 7 days
    static constexpr int OFFLINE_GRACE_DAYS = 30; // Allow offline for 30 days max

    juce::File getLicenseFile() const;
    juce::File getTrialFile() const;

    // Machine ID for hardware binding
    juce::String getMachineId() const;

    // Key format validation
    bool isValidKeyFormat (const juce::String& key) const;

    // Secure data encoding/decoding using Blowfish
    juce::String encryptData (const juce::String& plainText) const;
    juce::String decryptData (const juce::String& encryptedBase64) const;

    // License file format: encrypted JSON with key, machineId, lastVerified
    bool saveLicenseData (const juce::String& key, juce::int64 lastVerified);
    bool loadLicenseData (juce::String& key, juce::String& machineId, juce::int64& lastVerified);

    // Trial management
    void initializeOrLoadTrial();
    bool saveTrialData (juce::int64 startTime, juce::int64 serverTime);
    bool loadTrialData (juce::int64& startTime, juce::String& machineId, juce::int64& serverTime);

    // Server-based trial tracking (prevents infinite trials by deleting local file)
    bool checkTrialServer (bool& hasTrialed, bool& canStart, juce::int64& serverStartTime,
                           int& daysRemaining, bool& expired);
    bool registerTrialOnServer (juce::int64& serverStartTime);
    bool registerTrialOnServerWithCode (const juce::String& trialCode, juce::int64& serverStartTime);

    // Online time verification (prevents clock rollback)
    juce::int64 getOnlineTime() const;

    // Contacts Gumroad /v2/licenses/verify
    // incrementUses = true only on first activation (not re-verification)
    bool verifyWithGumroad (const juce::String& key, juce::String& errorOut, bool incrementUses = true);

    // Encryption key derivation (combines static key + machine ID)
    juce::String getEncryptionKey() const;

    // Your Gumroad product ID (from License Key settings)
    static const char* gumroadProductId;
};
