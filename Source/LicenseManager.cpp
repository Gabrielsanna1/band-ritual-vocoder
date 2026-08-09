#include "LicenseManager.h"

// Your Gumroad product ID (from the License key settings)
const char* LicenseManager::gumroadProductId = "04syWLp10CnllZAEWgKrcQ==";

// Trial server URL - UPDATE THIS after deploying your Cloudflare Worker!
// Format: https://YOUR-WORKER-NAME.YOUR-SUBDOMAIN.workers.dev
static const char* trialServerUrl = "https://bandritual-trial.vocodertechtm.workers.dev";

// Static encryption key component (obfuscated - not plain text)
// This is combined with machine ID to create unique encryption per machine
static const char* getStaticKeyPart()
{
    // Obfuscated key - decoded at runtime to avoid simple string extraction
    // The actual key is "RitualAudio2025X" but stored XOR'd
    static char key[32] = {0};
    static bool decoded = false;

    if (!decoded)
    {
        // Obfuscated bytes (key XOR'd with pattern)
        static const unsigned char obf[] = {
            'R' ^ 0x5A, 'i' ^ 0x3B, 't' ^ 0x7C, 'u' ^ 0x1D,
            'a' ^ 0x4E, 'l' ^ 0x2F, 'A' ^ 0x6A, 'u' ^ 0x0B,
            'd' ^ 0x5C, 'i' ^ 0x3D, 'o' ^ 0x7E, '2' ^ 0x1F,
            '0' ^ 0x4A, '2' ^ 0x2B, '5' ^ 0x6C, 'X' ^ 0x0D
        };
        static const unsigned char mask[] = {
            0x5A, 0x3B, 0x7C, 0x1D, 0x4E, 0x2F, 0x6A, 0x0B,
            0x5C, 0x3D, 0x7E, 0x1F, 0x4A, 0x2B, 0x6C, 0x0D
        };

        for (int i = 0; i < 16; ++i)
            key[i] = static_cast<char>(obf[i] ^ mask[i]);
        key[16] = '\0';
        decoded = true;
    }
    return key;
}

LicenseManager::LicenseManager() {}

//==============================================================================
// File Locations
//==============================================================================

juce::File LicenseManager::getLicenseFile() const
{
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    auto dir = appData.getChildFile ("BandRitual");
    dir.createDirectory();
    return dir.getChildFile ("license.dat");
}

juce::File LicenseManager::getTrialFile() const
{
    // Use same location as license file (Application Support) to avoid DAW sandbox issues
    auto appData = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory);
    auto dir = appData.getChildFile ("BandRitual");
    dir.createDirectory();
    return dir.getChildFile (".trial_cache");
}

//==============================================================================
// Machine ID Generation
//==============================================================================

juce::String LicenseManager::getMachineId() const
{
    juce::String machineData;

    // Use unique device ID (preferred modern API)
    machineData += juce::SystemStats::getUniqueDeviceID();

    // Add extra identifiers for robustness
    machineData += juce::SystemStats::getComputerName();
    machineData += juce::SystemStats::getLogonName();

    // SHA256 hash for consistent 64-char hex string
    return juce::SHA256 (machineData.toUTF8()).toHexString();
}

//==============================================================================
// Key Format Validation
// Gumroad keys are typically: XXXXXXXX-XXXXXXXX-XXXXXXXX-XXXXXXXX
// We also accept keys without dashes (32 hex chars)
//==============================================================================

bool LicenseManager::isValidKeyFormat (const juce::String& key) const
{
    auto trimmed = key.trim();

    // Remove dashes for validation
    auto noDashes = trimmed.removeCharacters ("-");

    // Must be 32 characters (standard Gumroad format)
    if (noDashes.length() != 32)
        return false;

    // Must be alphanumeric (Gumroad uses hex-like format)
    for (int i = 0; i < noDashes.length(); ++i)
    {
        auto c = noDashes[i];
        if (! juce::CharacterFunctions::isLetterOrDigit (c))
            return false;
    }

    return true;
}

//==============================================================================
// Encryption/Decryption using Blowfish
//==============================================================================

juce::String LicenseManager::getEncryptionKey() const
{
    // Combine static key with machine ID for unique encryption per machine
    juce::String combined = juce::String (getStaticKeyPart()) + getMachineId().substring (0, 16);
    return juce::SHA256 (combined.toUTF8()).toHexString().substring (0, 56); // Blowfish key max 56 bytes
}

juce::String LicenseManager::encryptData (const juce::String& plainText) const
{
    juce::String key = getEncryptionKey();
    juce::BlowFish blowfish (key.toRawUTF8(), (int) key.getNumBytesAsUTF8());

    // Convert to memory block
    juce::MemoryBlock data;
    data.append (plainText.toRawUTF8(), plainText.getNumBytesAsUTF8());

    // Pad to 8-byte boundary (Blowfish block size)
    size_t padding = 8 - (data.getSize() % 8);
    if (padding < 8)
    {
        for (size_t i = 0; i < padding; ++i)
            data.append ("\0", 1);
    }

    // Encrypt in place
    blowfish.encrypt (data);

    // Return as Base64
    return juce::Base64::toBase64 (data.getData(), data.getSize());
}

juce::String LicenseManager::decryptData (const juce::String& encryptedBase64) const
{
    juce::String key = getEncryptionKey();
    juce::BlowFish blowfish (key.toRawUTF8(), (int) key.getNumBytesAsUTF8());

    // Decode Base64
    juce::MemoryBlock data;
    juce::MemoryOutputStream stream (data, false);
    if (! juce::Base64::convertFromBase64 (stream, encryptedBase64))
        return {};

    // Decrypt in place
    blowfish.decrypt (data);

    // Convert back to string (remove null padding)
    return juce::String::fromUTF8 (static_cast<const char*> (data.getData()), (int) data.getSize()).trimEnd();
}

//==============================================================================
// Online Time Verification (prevents clock manipulation)
//==============================================================================

juce::int64 LicenseManager::getOnlineTime() const
{
    // Try to get time from a reliable source
    // Using worldtimeapi.org which returns JSON with unix timestamp
    juce::URL url ("http://worldtimeapi.org/api/ip");

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
    std::unique_ptr<juce::InputStream> stream (url.createInputStream (false, nullptr, nullptr, {}, 5000));
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    if (stream == nullptr)
        return 0; // Offline - will use local time

    auto response = stream->readEntireStreamAsString();
    auto json = juce::JSON::parse (response);

    if (auto* obj = json.getDynamicObject())
    {
        // worldtimeapi returns "unixtime" as seconds
        auto unixTime = obj->getProperty ("unixtime");
        if (unixTime.isInt() || unixTime.isInt64())
            return static_cast<juce::int64> (unixTime) * 1000LL; // Convert to milliseconds
    }

    return 0; // Failed to get online time
}

//==============================================================================
// Server-Based Trial Tracking
// This prevents infinite trials by storing trial data on a server
//==============================================================================

bool LicenseManager::checkTrialServer (bool& hasTrialed, bool& canStart, juce::int64& serverStartTime,
                                        int& daysRemaining, bool& expired)
{
    DBG ("checkTrialServer() called");

    juce::String machineId = getMachineId();
    juce::String urlStr = juce::String (trialServerUrl) + "/trial/check?machineId=" + juce::URL::addEscapeChars (machineId, true);

    DBG ("  URL: " << urlStr);

    juce::URL url (urlStr);

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
    std::unique_ptr<juce::InputStream> stream (url.createInputStream (false, nullptr, nullptr, {}, 8000));
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    if (stream == nullptr)
    {
        DBG ("  Failed to connect to trial server");
        return false; // Network error - will fall back to local
    }

    auto response = stream->readEntireStreamAsString();
    DBG ("  Server response: " << response);

    auto json = juce::JSON::parse (response);
    if (auto* obj = json.getDynamicObject())
    {
        hasTrialed = obj->getProperty ("hasTrialed");
        canStart = obj->getProperty ("canStartTrial");

        if (hasTrialed)
        {
            serverStartTime = static_cast<juce::int64> (obj->getProperty ("startTime"));
            daysRemaining = static_cast<int> (obj->getProperty ("daysRemaining"));
            expired = obj->getProperty ("expired");
        }

        DBG ("  hasTrialed: " << (hasTrialed ? "YES" : "NO"));
        DBG ("  canStart: " << (canStart ? "YES" : "NO"));
        DBG ("  daysRemaining: " << daysRemaining);
        DBG ("  expired: " << (expired ? "YES" : "NO"));

        return true;
    }

    DBG ("  Failed to parse server response");
    return false;
}

bool LicenseManager::registerTrialOnServer (juce::int64& serverStartTime)
{
    DBG ("registerTrialOnServer() called");

    juce::String machineId = getMachineId();
    juce::String urlStr = juce::String (trialServerUrl) + "/trial/start";

    DBG ("  URL: " << urlStr);

    juce::URL url (urlStr);

    // Create JSON body
    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty ("machineId", machineId);
    juce::String jsonBody = juce::JSON::toString (juce::var (body.get()));

    DBG ("  Request body: " << jsonBody);

    // POST request
    url = url.withPOSTData (jsonBody);

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
    std::unique_ptr<juce::InputStream> stream (url.createInputStream (true, nullptr, nullptr,
        "Content-Type: application/json", 8000));
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    if (stream == nullptr)
    {
        DBG ("  Failed to connect to trial server");
        return false;
    }

    auto response = stream->readEntireStreamAsString();
    DBG ("  Server response: " << response);

    auto json = juce::JSON::parse (response);
    if (auto* obj = json.getDynamicObject())
    {
        bool success = obj->getProperty ("success");
        if (success)
        {
            serverStartTime = static_cast<juce::int64> (obj->getProperty ("startTime"));
            bool alreadyTrialed = obj->getProperty ("alreadyTrialed");

            DBG ("  Trial registered. startTime: " << serverStartTime);
            DBG ("  alreadyTrialed: " << (alreadyTrialed ? "YES" : "NO"));

            return true;
        }
    }

    DBG ("  Failed to register trial on server");
    return false;
}

//==============================================================================
// License Data Storage (encrypted JSON)
//==============================================================================

bool LicenseManager::saveLicenseData (const juce::String& key, juce::int64 lastVerified)
{
    auto machineId = getMachineId();

    // Create JSON object
    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("key", key);
    obj->setProperty ("machineId", machineId);
    obj->setProperty ("lastVerified", lastVerified);
    obj->setProperty ("checksum", juce::SHA256 ((key + machineId + juce::String (lastVerified)).toUTF8()).toHexString().substring (0, 16));

    juce::String jsonStr = juce::JSON::toString (juce::var (obj.get()));
    juce::String encrypted = encryptData (jsonStr);

    return getLicenseFile().replaceWithText (encrypted);
}

bool LicenseManager::loadLicenseData (juce::String& key, juce::String& machineId, juce::int64& lastVerified)
{
    auto file = getLicenseFile();
    if (! file.existsAsFile())
        return false;

    juce::String encrypted = file.loadFileAsString().trim();
    if (encrypted.isEmpty())
        return false;

    juce::String decrypted = decryptData (encrypted);
    if (decrypted.isEmpty())
        return false;

    auto json = juce::JSON::parse (decrypted);
    if (! json.isObject())
        return false;

    if (auto* obj = json.getDynamicObject())
    {
        key = obj->getProperty ("key").toString();
        machineId = obj->getProperty ("machineId").toString();
        lastVerified = static_cast<juce::int64> (obj->getProperty ("lastVerified"));

        // Verify checksum (detects tampering)
        juce::String storedChecksum = obj->getProperty ("checksum").toString();
        juce::String computedChecksum = juce::SHA256 ((key + machineId + juce::String (lastVerified)).toUTF8()).toHexString().substring (0, 16);

        if (storedChecksum != computedChecksum)
            return false; // Tampered!

        return key.isNotEmpty() && machineId.isNotEmpty();
    }

    return false;
}

//==============================================================================
// Trial Data Storage (encrypted)
//==============================================================================

bool LicenseManager::saveTrialData (juce::int64 startTime, juce::int64 serverTime)
{
    DBG ("saveTrialData() called");

    auto machineId = getMachineId();
    DBG ("  Machine ID length: " << machineId.length());

    juce::DynamicObject::Ptr obj = new juce::DynamicObject();
    obj->setProperty ("startTime", startTime);
    obj->setProperty ("serverTime", serverTime);
    obj->setProperty ("machineId", machineId);
    obj->setProperty ("checksum", juce::SHA256 ((juce::String (startTime) + machineId + juce::String (serverTime)).toUTF8()).toHexString().substring (0, 16));

    juce::String jsonStr = juce::JSON::toString (juce::var (obj.get()));
    DBG ("  JSON length: " << jsonStr.length());

    juce::String encrypted = encryptData (jsonStr);
    DBG ("  Encrypted length: " << encrypted.length());

    auto trialFile = getTrialFile();
    DBG ("  Trial file path: " << trialFile.getFullPathName());
    DBG ("  Parent dir exists: " << (trialFile.getParentDirectory().exists() ? "YES" : "NO"));

    bool result = trialFile.replaceWithText (encrypted);
    DBG ("  replaceWithText result: " << (result ? "SUCCESS" : "FAILED"));

    if (result)
    {
        DBG ("  File exists after save: " << (trialFile.existsAsFile() ? "YES" : "NO"));
        DBG ("  File size: " << trialFile.getSize());
    }

    return result;
}

bool LicenseManager::loadTrialData (juce::int64& startTime, juce::String& machineId, juce::int64& serverTime)
{
    DBG ("loadTrialData() called");

    auto file = getTrialFile();
    DBG ("  File path: " << file.getFullPathName());

    if (! file.existsAsFile())
    {
        DBG ("  File does not exist!");
        return false;
    }

    juce::String encrypted = file.loadFileAsString().trim();
    if (encrypted.isEmpty())
    {
        DBG ("  File is empty!");
        return false;
    }
    DBG ("  Encrypted data length: " << encrypted.length());

    juce::String decrypted = decryptData (encrypted);
    if (decrypted.isEmpty())
    {
        DBG ("  Decryption failed!");
        return false;
    }
    DBG ("  Decrypted data length: " << decrypted.length());

    auto json = juce::JSON::parse (decrypted);
    if (! json.isObject())
    {
        DBG ("  JSON parse failed!");
        return false;
    }

    if (auto* obj = json.getDynamicObject())
    {
        startTime = static_cast<juce::int64> (obj->getProperty ("startTime"));
        serverTime = static_cast<juce::int64> (obj->getProperty ("serverTime"));
        machineId = obj->getProperty ("machineId").toString();

        // Verify checksum
        juce::String storedChecksum = obj->getProperty ("checksum").toString();
        juce::String computedChecksum = juce::SHA256 ((juce::String (startTime) + machineId + juce::String (serverTime)).toUTF8()).toHexString().substring (0, 16);

        if (storedChecksum != computedChecksum)
            return false; // Tampered!

        return startTime > 0 && machineId.isNotEmpty();
    }

    return false;
}

//==============================================================================
// Trial Management
// SERVER-BASED: Trial data stored on Cloudflare Worker to prevent infinite trials
// Local file is just a cache - server is the source of truth
//==============================================================================

void LicenseManager::initializeOrLoadTrial()
{
    DBG ("initializeOrLoadTrial() called - SERVER-BASED");

    // STEP 1: Try to check the server first (this is the authoritative source)
    bool hasTrialed = false;
    bool canStart = false;
    juce::int64 serverStartTime = 0;
    int serverDaysRemaining = 0;
    bool serverExpired = false;

    if (checkTrialServer (hasTrialed, canStart, serverStartTime, serverDaysRemaining, serverExpired))
    {
        DBG ("  Server check successful");

        if (hasTrialed)
        {
            // Server knows about this machine's trial
            DBG ("  Server says: trial exists, daysRemaining=" << serverDaysRemaining << ", expired=" << (serverExpired ? "YES" : "NO"));

            inTrial = true;
            trialExpired = serverExpired;
            trialDaysRemaining = serverDaysRemaining;
            firstLaunch = false;
            lastError.clear();

            // Update local cache with server data
            // serverStartTime = when trial started, current time = when we synced
            saveTrialData (serverStartTime, juce::Time::currentTimeMillis());
            return;
        }
        else
        {
            // Server says this machine never trialed - first launch!
            DBG ("  Server says: no trial for this machine - first launch");
            firstLaunch = true;
            inTrial = false;
            trialExpired = false;
            trialDaysRemaining = 0;
            return;
        }
    }

    // STEP 2: Server unreachable - fall back to local file
    DBG ("  Server unreachable, checking local cache...");
    DBG ("  Trial file path: " << getTrialFile().getFullPathName());
    DBG ("  Trial file exists: " << (getTrialFile().existsAsFile() ? "YES" : "NO"));

    juce::String currentMachineId = getMachineId();
    juce::int64 startTime = 0;
    juce::String storedMachineId;
    juce::int64 serverTime = 0;

    if (loadTrialData (startTime, storedMachineId, serverTime))
    {
        // Verify machine ID matches
        if (storedMachineId != currentMachineId)
        {
            DBG ("  Machine ID mismatch - different machine");
            firstLaunch = true;
            inTrial = false;
            trialExpired = false;
            trialDaysRemaining = 0;
            return;
        }

        // We have local cache but couldn't reach server
        // Allow trial to continue but enforce limits
        juce::int64 now = juce::Time::currentTimeMillis();
        juce::int64 msPerDay = 24LL * 60LL * 60LL * 1000LL;

        // Check 1: How long since trial started (for trial expiration)
        juce::int64 elapsedSinceStart = now - startTime;
        int daysElapsed = static_cast<int> (elapsedSinceStart / msPerDay);

        // Check 2: How long since last server sync (for offline limit)
        juce::int64 elapsedSinceSync = now - serverTime;
        int daysSinceSync = static_cast<int> (elapsedSinceSync / msPerDay);

        // Check 3: Clock rollback detection (current time before last sync time)
        bool clockRolledBack = now < serverTime || now < startTime;

        if (clockRolledBack)
        {
            // System clock was set backward - suspicious
            DBG ("  Clock rollback detected! now=" << now << ", serverTime=" << serverTime << ", startTime=" << startTime);
            inTrial = true;
            trialExpired = true;
            trialDaysRemaining = 0;
            firstLaunch = false;
            lastError = "Trial expired: system clock was changed. Please connect to internet to verify.";
        }
        else if (daysSinceSync > OFFLINE_GRACE_DAYS)
        {
            // Too long without server verification - require internet
            DBG ("  Offline too long: " << daysSinceSync << " days since last sync (max " << OFFLINE_GRACE_DAYS << ")");
            inTrial = true;
            trialExpired = true;
            trialDaysRemaining = 0;
            firstLaunch = false;
            lastError = "Trial requires internet verification. Please connect to continue.";
        }
        else if (daysElapsed < 0 || daysElapsed >= TRIAL_DAYS)
        {
            // Trial period expired
            DBG ("  Trial expired: " << daysElapsed << " days elapsed");
            inTrial = true;
            trialExpired = true;
            trialDaysRemaining = 0;
            firstLaunch = false;
            lastError.clear();
        }
        else
        {
            // Still valid according to local cache
            inTrial = true;
            trialExpired = false;
            trialDaysRemaining = TRIAL_DAYS - daysElapsed;
            firstLaunch = false;
            lastError.clear();
            DBG ("  Using local cache: " << trialDaysRemaining << " days remaining (last sync " << daysSinceSync << " days ago)");
        }
        return;
    }

    // STEP 3: No server, no local file - but we're offline
    // Treat as first launch but they'll need internet to actually start trial
    DBG ("  No trial data anywhere - first launch (will need internet to start)");
    firstLaunch = true;
    inTrial = false;
    trialExpired = false;
    trialDaysRemaining = 0;
}

void LicenseManager::startTrial()
{
    DBG ("LicenseManager::startTrial() called - LEGACY (no code)");
    // Trial now requires a code - this legacy method will fail
    lastError = "Trial code required. Please get a code from our website.";
    DBG ("  ERROR: Trial code required!");
}

bool LicenseManager::startTrialWithCode(const juce::String& trialCode)
{
    DBG ("LicenseManager::startTrialWithCode() called");
    DBG ("  Trial code: " << trialCode);

    if (trialCode.isEmpty())
    {
        lastError = "Please enter a trial code.";
        return false;
    }

    // MUST register on server first with the code
    juce::int64 serverStartTime = 0;

    if (!registerTrialOnServerWithCode (trialCode, serverStartTime))
    {
        DBG ("  ERROR: Failed to register trial on server!");
        // lastError is set by registerTrialOnServerWithCode
        return false;
    }

    DBG ("  Trial registered on server, startTime: " << serverStartTime);

    // Also save locally as cache
    // serverStartTime = when trial started, current time = when we synced
    DBG ("  Saving local cache to: " << getTrialFile().getFullPathName());
    bool saved = saveTrialData (serverStartTime, juce::Time::currentTimeMillis());
    DBG ("  Local save result: " << (saved ? "SUCCESS" : "FAILED"));

    if (!saved)
    {
        DBG ("  ERROR: Failed to save trial file!");
        lastError = "Failed to save trial data.";
        return false;
    }

    inTrial = true;
    trialExpired = false;
    trialDaysRemaining = TRIAL_DAYS;
    firstLaunch = false;

    DBG ("  Trial started successfully. Days remaining: " << trialDaysRemaining);
    return true;
}

bool LicenseManager::registerTrialOnServerWithCode (const juce::String& trialCode, juce::int64& serverStartTime)
{
    DBG ("registerTrialOnServerWithCode() called");
    DBG ("  Trial code: " << trialCode);

    juce::String machineId = getMachineId();
    juce::String urlStr = juce::String (trialServerUrl) + "/trial/start";

    DBG ("  URL: " << urlStr);

    juce::URL url (urlStr);

    // Create JSON body with machineId AND trialCode
    juce::DynamicObject::Ptr body = new juce::DynamicObject();
    body->setProperty ("machineId", machineId);
    body->setProperty ("trialCode", trialCode.trim().toUpperCase());
    juce::String jsonBody = juce::JSON::toString (juce::var (body.get()));

    DBG ("  Request body: " << jsonBody);

    // POST request
    url = url.withPOSTData (jsonBody);

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
    std::unique_ptr<juce::InputStream> stream (url.createInputStream (true, nullptr, nullptr,
        "Content-Type: application/json", 8000));
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    if (stream == nullptr)
    {
        DBG ("  Failed to connect to trial server");
        lastError = "Could not connect to server. Please check your internet connection.";
        return false;
    }

    auto response = stream->readEntireStreamAsString();
    DBG ("  Server response: " << response);

    auto json = juce::JSON::parse (response);
    if (auto* obj = json.getDynamicObject())
    {
        // Check for error responses
        if (obj->hasProperty ("error"))
        {
            juce::String errorMsg = obj->getProperty ("message").toString();
            if (errorMsg.isEmpty())
                errorMsg = obj->getProperty ("error").toString();
            lastError = errorMsg;
            DBG ("  Server error: " << lastError);
            return false;
        }

        bool success = obj->getProperty ("success");
        if (success)
        {
            serverStartTime = static_cast<juce::int64> (obj->getProperty ("startTime"));
            bool alreadyTrialed = obj->getProperty ("alreadyTrialed");

            DBG ("  Trial registered. startTime: " << serverStartTime);
            DBG ("  alreadyTrialed: " << (alreadyTrialed ? "YES" : "NO"));

            return true;
        }
    }

    DBG ("  Failed to register trial on server");
    lastError = "Invalid server response. Please try again.";
    return false;
}

//==============================================================================
// License Loading with Security Checks
//==============================================================================

void LicenseManager::loadLicenseFromDisk()
{
    DBG ("LicenseManager::loadLicenseFromDisk() called");

    juce::String key;
    juce::String storedMachineId;
    juce::int64 lastVerified = 0;

    if (loadLicenseData (key, storedMachineId, lastVerified))
    {
        DBG ("  Found license data");
        // SECURITY CHECK 1: Verify machine ID matches
        juce::String currentMachineId = getMachineId();
        if (storedMachineId != currentMachineId)
        {
            // License from different machine - invalid
            licensed = false;
            storedKey.clear();
            lastError = "License not valid for this computer.";
            initializeOrLoadTrial();
            return;
        }

        // SECURITY CHECK 2: Verify key format
        if (! isValidKeyFormat (key))
        {
            // Invalid key format - corrupted or fake
            licensed = false;
            storedKey.clear();
            lastError = "Invalid license key format.";
            initializeOrLoadTrial();
            return;
        }

        // License is valid locally - trust it!
        // Re-verification happens opportunistically in checkPeriodicVerification()
        // We NEVER revoke a license just because user is offline
        storedKey = key;
        licensed = true;
        lastError.clear();
        return;
    }

    // No valid license file - check trial
    licensed = false;
    storedKey.clear();
    lastError.clear();
    initializeOrLoadTrial();
}

//==============================================================================
// Periodic Verification (call from processor timer)
// This is OPPORTUNISTIC - only verifies when internet is available
// NEVER revokes license due to network errors - only if Gumroad explicitly rejects
//==============================================================================

void LicenseManager::checkPeriodicVerification()
{
    if (! licensed)
        return;

    juce::String key;
    juce::String storedMachineId;
    juce::int64 lastVerified = 0;

    if (! loadLicenseData (key, storedMachineId, lastVerified))
        return;

    juce::int64 now = juce::Time::currentTimeMillis();
    juce::int64 daysSinceVerification = (now - lastVerified) / (24LL * 60LL * 60LL * 1000LL);

    if (daysSinceVerification >= REVERIFY_DAYS)
    {
        // Time to try re-verification (silently, in background)
        // Don't increment uses count — this is a re-check, not a new activation
        juce::String error;
        bool verified = verifyWithGumroad (key, error, false);

        if (verified)
        {
            // Success - update timestamp
            saveLicenseData (key, now);
        }
        else if (error.containsIgnoreCase ("network") ||
                 error.containsIgnoreCase ("timeout") ||
                 error.containsIgnoreCase ("connection") ||
                 error.isEmpty())
        {
            // Network error - user is offline, DON'T revoke!
            // Just try again next time they're online
            // License remains valid
        }
        else
        {
            // Gumroad EXPLICITLY rejected the license (refunded, revoked, invalid)
            // This is the ONLY case where we revoke
            licensed = false;
            storedKey.clear();
            lastError = "License was revoked: " + error;

            // Delete the invalid license file
            getLicenseFile().deleteFile();
        }
    }
}

//==============================================================================
// Gumroad API Verification
//==============================================================================

bool LicenseManager::verifyWithGumroad (const juce::String& key, juce::String& errorOut, bool incrementUses)
{
    if (key.isEmpty())
    {
        errorOut = "Please paste your license key.";
        return false;
    }

    // SECURITY: Validate key format before even contacting Gumroad
    if (! isValidKeyFormat (key))
    {
        errorOut = "Invalid license key format.";
        return false;
    }

    if (gumroadProductId == nullptr || juce::String (gumroadProductId).isEmpty())
    {
        errorOut = "Gumroad product ID is not set.";
        return false;
    }

    // Build POST body
    juce::StringPairArray params;
    params.set ("product_id", juce::String (gumroadProductId));
    params.set ("license_key", key.trim());
    params.set ("increment_uses_count", incrementUses ? "true" : "false");

    juce::String postData;
    auto keys = params.getAllKeys();
    auto vals = params.getAllValues();

    for (int i = 0; i < params.size(); ++i)
    {
        if (i > 0)
            postData << "&";
        postData << juce::URL::addEscapeChars (keys[i], true)
                 << "="
                 << juce::URL::addEscapeChars (vals[i], true);
    }

    juce::URL url ("https://api.gumroad.com/v2/licenses/verify");
    url = url.withPOSTData (postData);

    juce::String extraHeaders = "Content-Type: application/x-www-form-urlencoded\r\n";

    JUCE_BEGIN_IGNORE_WARNINGS_GCC_LIKE ("-Wdeprecated-declarations")
    std::unique_ptr<juce::InputStream> in (url.createInputStream (true, nullptr, nullptr, extraHeaders, 10000, nullptr, nullptr));
    JUCE_END_IGNORE_WARNINGS_GCC_LIKE

    if (in == nullptr)
    {
        errorOut = "Network error contacting Gumroad.";
        return false;
    }

    auto response = in->readEntireStreamAsString();

    if (response.isEmpty())
    {
        errorOut = "Empty response from Gumroad.";
        return false;
    }

    auto json = juce::JSON::parse (response);

    if (! json.isObject())
    {
        errorOut = "Could not parse Gumroad response.";
        return false;
    }

    if (auto* obj = json.getDynamicObject())
    {
        bool success = static_cast<bool> (obj->getProperty ("success"));

        if (! success)
        {
            auto msgVar = obj->getProperty ("message");
            errorOut = msgVar.isString() ? msgVar.toString() : "License not valid.";
            return false;
        }

        // Check uses count — limit to 2 machines
        if (incrementUses)
        {
            auto purchaseVar = obj->getProperty ("purchase");
            if (auto* purchase = purchaseVar.getDynamicObject())
            {
                int usesCount = (int) purchase->getProperty ("uses");
                if (usesCount > 2)
                {
                    errorOut = "This license is already activated on 2 machines. Contact ritualaudiolabs@gmail.com if you need help.";
                    return false;
                }
            }
        }

        return true;
    }

    errorOut = "Unexpected response from Gumroad.";
    return false;
}

//==============================================================================
// Activation
//==============================================================================

bool LicenseManager::tryActivate (const juce::String& keyString)
{
    auto key = keyString.trim();
    juce::String error;

    // Validate and verify with Gumroad
    if (! verifyWithGumroad (key, error))
    {
        licensed = false;
        storedKey.clear();
        lastError = error;
        return false;
    }

    // Store license with machine binding
    juce::int64 now = juce::Time::currentTimeMillis();
    if (! saveLicenseData (key, now))
    {
        licensed = false;
        storedKey.clear();
        lastError = "Failed to save license file.";
        return false;
    }

    storedKey = key;
    licensed = true;
    inTrial = false;
    trialExpired = false;
    lastError.clear();
    return true;
}
