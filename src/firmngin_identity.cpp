#include "firmngin_identity.h"

#if defined(ESP8266) || defined(ESP32)
#include <LittleFS.h>
#endif

namespace
{
    struct IdentityPaths
    {
        const char *meta;
        const char *clientCert;
        const char *privateKey;
        const char *caCert;
        const char *serviceCaCert;
    };

    static const char *IDENTITY_DIR = "/firmnginfs";
    static const IdentityPaths ACTIVE_PATHS = {
        "/firmnginfs/meta.txt",
        "/firmnginfs/client.pem",
        "/firmnginfs/private.pem",
        "/firmnginfs/ca.pem",
        "/firmnginfs/service_ca.pem"};
    static const IdentityPaths STAGED_PATHS = {
        "/firmnginfs/meta.next",
        "/firmnginfs/client.next",
        "/firmnginfs/private.next",
        "/firmnginfs/ca.next",
        "/firmnginfs/service_ca.next"};
    static const IdentityPaths BACKUP_PATHS = {
        "/firmnginfs/meta.bak",
        "/firmnginfs/client.bak",
        "/firmnginfs/private.bak",
        "/firmnginfs/ca.bak",
        "/firmnginfs/service_ca.bak"};
    static const char *COMMIT_MARKER_PATH = "/firmnginfs/commit.pending";

    static const char *LEGACY_META_PATH = "/firmngin/meta.txt";
    static const char *LEGACY_CLIENT_CERT_PATH = "/firmngin/client.pem";
    static const char *LEGACY_PRIVATE_KEY_PATH = "/firmngin/private.pem";
    static const char *LEGACY_CA_CERT_PATH = "/firmngin/ca.pem";
    static const char *LEGACY_SERVICE_CA_CERT_PATH = "/firmngin/service_ca.pem";

    String trimLine(const String &line)
    {
        String out = line;
        out.trim();
        return out;
    }

    bool isCertificatePEM(const String &content)
    {
        return content.indexOf("-----BEGIN CERTIFICATE-----") >= 0 &&
               content.indexOf("-----END CERTIFICATE-----") >= 0;
    }

    bool isPrivateKeyPEM(const String &content)
    {
        return (content.indexOf("-----BEGIN PRIVATE KEY-----") >= 0 &&
                content.indexOf("-----END PRIVATE KEY-----") >= 0) ||
               (content.indexOf("-----BEGIN RSA PRIVATE KEY-----") >= 0 &&
                content.indexOf("-----END RSA PRIVATE KEY-----") >= 0) ||
               (content.indexOf("-----BEGIN EC PRIVATE KEY-----") >= 0 &&
                content.indexOf("-----END EC PRIVATE KEY-----") >= 0);
    }

    bool isHexValue(const String &value, size_t requiredLength)
    {
        if (value.length() != requiredLength)
            return false;
        for (unsigned int i = 0; i < value.length(); i++)
        {
            const char c = value[i];
            if (!((c >= '0' && c <= '9') ||
                  (c >= 'a' && c <= 'f') ||
                  (c >= 'A' && c <= 'F')))
                return false;
        }
        return true;
    }

    bool splitMetaLine(const String &line, String &key, String &value)
    {
        int idx = line.indexOf('=');
        if (idx <= 0)
            return false;
        key = line.substring(0, idx);
        value = line.substring(idx + 1);
        key.trim();
        value.trim();
        return key.length() > 0;
    }

    bool isValidHttpsBaseUrl(const String &value)
    {
        if (!value.startsWith("https://") || value.length() <= 8 || value.endsWith("/"))
            return false;

        int authorityEnd = value.indexOf('/', 8);
        String authority = authorityEnd >= 0 ? value.substring(8, authorityEnd) : value.substring(8);
        if (authority.length() == 0 || authority.indexOf('@') >= 0)
            return false;

        for (unsigned int i = 0; i < value.length(); i++)
        {
            const char c = value[i];
            if (c <= ' ' || c == '?' || c == '#')
                return false;
        }
        return true;
    }

    bool findMetaValue(const String &meta, const char *wantedKey, String &out)
    {
        int start = 0;
        while (start < static_cast<int>(meta.length()))
        {
            int end = meta.indexOf('\n', start);
            if (end < 0)
                end = meta.length();

            String line = trimLine(meta.substring(start, end));
            String key;
            String value;
            if (splitMetaLine(line, key, value) && key == wantedKey)
            {
                out = value;
                return true;
            }
            start = end + 1;
        }
        out = "";
        return false;
    }

    bool mountFS()
    {
#if defined(ESP8266) || defined(ESP32)
#if defined(ESP32)
        return LittleFS.begin(true);
#else
        return LittleFS.begin();
#endif
#else
        return false;
#endif
    }

    void unmountFS()
    {
#if defined(ESP8266) || defined(ESP32)
        LittleFS.end();
#endif
    }

    bool ensureIdentityDirectory()
    {
#if defined(ESP8266) || defined(ESP32)
        if (LittleFS.exists(IDENTITY_DIR))
            return true;
        return LittleFS.mkdir(IDENTITY_DIR);
#else
        return false;
#endif
    }

    bool readTextFile(const char *path, String &out)
    {
        if (!LittleFS.exists(path))
        {
            out = "";
            return false;
        }
        File file = LittleFS.open(path, "r");
        if (!file)
            return false;
        out = file.readString();
        file.close();
        return out.length() > 0;
    }

    bool writeTextFile(const char *path, const String &content)
    {
        if (!ensureIdentityDirectory())
            return false;
        File file = LittleFS.open(path, "w");
        if (!file)
            return false;
        size_t written = file.print(content);
        file.close();
        return written == content.length();
    }

    void removeFile(const char *path)
    {
        if (LittleFS.exists(path))
            LittleFS.remove(path);
    }

    void removePathSet(const IdentityPaths &paths)
    {
        removeFile(paths.meta);
        removeFile(paths.clientCert);
        removeFile(paths.privateKey);
        removeFile(paths.caCert);
        removeFile(paths.serviceCaCert);
    }

    bool renameFile(const char *from, const char *to)
    {
        if (!LittleFS.exists(from))
            return true;
        removeFile(to);
        return LittleFS.rename(from, to);
    }

    bool parseMeta(const String &meta, FirmnginIdentityRecord &out)
    {
        bool invalidServiceEndpoint = false;
        int start = 0;
        while (start < static_cast<int>(meta.length()))
        {
            int end = meta.indexOf('\n', start);
            if (end < 0)
                end = meta.length();

            String line = trimLine(meta.substring(start, end));
            String key;
            String value;
            if (splitMetaLine(line, key, value))
            {
                if (key == "device_id")
                    out.deviceId = value;
                else if (key == "device_key")
                    out.deviceKey = value;
                else if (key == "decryptor")
                    out.decryptor = value;
                else if (key == "tls")
                    out.tlsMode = value;
                else if (key == "fingerprint")
                    out.fingerprintHex = value;
                else if (key == "broker")
                    out.brokerAddr = value;
                else if (key == "broker_port")
                    out.brokerPort = value.toInt();
                else if (key == "api_base_url")
                {
                    if (isValidHttpsBaseUrl(value))
                        out.apiBaseUrl = value;
                    else
                        invalidServiceEndpoint = true;
                }
                else if (key == "ota_base_url")
                {
                    if (isValidHttpsBaseUrl(value))
                        out.otaBaseUrl = value;
                    else
                        invalidServiceEndpoint = true;
                }
            }
            start = end + 1;
        }
        return !invalidServiceEndpoint;
    }

    bool validateIdentityRecord(const FirmnginIdentityRecord &record)
    {
        if (record.deviceId.length() == 0 || record.deviceKey.length() == 0)
            return false;
        if (!isHexValue(record.decryptor, 32) && !isHexValue(record.decryptor, 64))
            return false;
        if (record.tlsMode != "ca" && record.tlsMode != "fingerprint")
            return false;
        if (!isValidHttpsBaseUrl(record.apiBaseUrl) || !isValidHttpsBaseUrl(record.otaBaseUrl))
            return false;
        if (!isCertificatePEM(record.clientCert) || !isPrivateKeyPEM(record.privateKey))
            return false;
        if (!isCertificatePEM(record.serviceCaCert))
            return false;
        if (record.tlsMode == "ca" && !isCertificatePEM(record.caCert))
            return false;
        if (record.tlsMode == "fingerprint" && !isHexValue(record.fingerprintHex, 40))
            return false;
        return true;
    }

    bool loadRecord(const IdentityPaths &paths, FirmnginIdentityRecord &out)
    {
        out = FirmnginIdentityRecord();
        String meta;
        if (!readTextFile(paths.meta, meta) || !parseMeta(meta, out))
            return false;
        readTextFile(paths.clientCert, out.clientCert);
        readTextFile(paths.privateKey, out.privateKey);
        readTextFile(paths.caCert, out.caCert);
        readTextFile(paths.serviceCaCert, out.serviceCaCert);
        return validateIdentityRecord(out);
    }

    bool anyPathExists(const IdentityPaths &paths)
    {
        return LittleFS.exists(paths.meta) ||
               LittleFS.exists(paths.clientCert) ||
               LittleFS.exists(paths.privateKey) ||
               LittleFS.exists(paths.caCert) ||
               LittleFS.exists(paths.serviceCaCert);
    }

    bool restoreBackupFile(const char *backupPath, const char *activePath)
    {
        if (!LittleFS.exists(backupPath))
            return true;
        String content;
        return readTextFile(backupPath, content) && writeTextFile(activePath, content);
    }

    bool restoreBackupIdentity(bool removeUnbackedActive)
    {
        if (removeUnbackedActive)
            removePathSet(ACTIVE_PATHS);
        bool restored = true;
        restored = restoreBackupFile(BACKUP_PATHS.clientCert, ACTIVE_PATHS.clientCert) && restored;
        restored = restoreBackupFile(BACKUP_PATHS.privateKey, ACTIVE_PATHS.privateKey) && restored;
        restored = restoreBackupFile(BACKUP_PATHS.caCert, ACTIVE_PATHS.caCert) && restored;
        restored = restoreBackupFile(BACKUP_PATHS.serviceCaCert, ACTIVE_PATHS.serviceCaCert) && restored;
        restored = restoreBackupFile(BACKUP_PATHS.meta, ACTIVE_PATHS.meta) && restored;
        return restored;
    }

    void recoverInterruptedCommit()
    {
        if (!LittleFS.exists(COMMIT_MARKER_PATH))
            return;

        FirmnginIdentityRecord active;
        if (loadRecord(ACTIVE_PATHS, active))
        {
            removePathSet(BACKUP_PATHS);
        }
        else
        {
            const bool hasBackup = anyPathExists(BACKUP_PATHS);
            const bool backupWasComplete = LittleFS.exists(BACKUP_PATHS.meta) || !hasBackup;
            if (!restoreBackupIdentity(backupWasComplete))
                return;
            removePathSet(BACKUP_PATHS);
        }
        removePathSet(STAGED_PATHS);
        removeFile(COMMIT_MARKER_PATH);
    }

    bool commitStagedIdentity()
    {
        FirmnginIdentityRecord staged;
        if (!loadRecord(STAGED_PATHS, staged))
            return false;

        removePathSet(BACKUP_PATHS);
        if (!writeTextFile(COMMIT_MARKER_PATH, "1"))
            return false;

        bool backupOK = true;
        backupOK = renameFile(ACTIVE_PATHS.clientCert, BACKUP_PATHS.clientCert) && backupOK;
        backupOK = renameFile(ACTIVE_PATHS.privateKey, BACKUP_PATHS.privateKey) && backupOK;
        backupOK = renameFile(ACTIVE_PATHS.caCert, BACKUP_PATHS.caCert) && backupOK;
        backupOK = renameFile(ACTIVE_PATHS.serviceCaCert, BACKUP_PATHS.serviceCaCert) && backupOK;
        backupOK = renameFile(ACTIVE_PATHS.meta, BACKUP_PATHS.meta) && backupOK;
        if (!backupOK)
        {
            if (!restoreBackupIdentity(false))
                return false;
            removePathSet(BACKUP_PATHS);
            removeFile(COMMIT_MARKER_PATH);
            return false;
        }

        bool commitOK = true;
        commitOK = renameFile(STAGED_PATHS.clientCert, ACTIVE_PATHS.clientCert) && commitOK;
        commitOK = renameFile(STAGED_PATHS.privateKey, ACTIVE_PATHS.privateKey) && commitOK;
        commitOK = renameFile(STAGED_PATHS.caCert, ACTIVE_PATHS.caCert) && commitOK;
        commitOK = renameFile(STAGED_PATHS.serviceCaCert, ACTIVE_PATHS.serviceCaCert) && commitOK;
        commitOK = renameFile(STAGED_PATHS.meta, ACTIVE_PATHS.meta) && commitOK;

        FirmnginIdentityRecord committed;
        if (!commitOK || !loadRecord(ACTIVE_PATHS, committed))
        {
            if (!restoreBackupIdentity(true))
                return false;
            removePathSet(BACKUP_PATHS);
            removePathSet(STAGED_PATHS);
            removeFile(COMMIT_MARKER_PATH);
            return false;
        }

        removePathSet(BACKUP_PATHS);
        removeFile(COMMIT_MARKER_PATH);
        return true;
    }

    void removeIdentityFiles(const char *metaPath, const char *clientPath, const char *privatePath, const char *caPath, const char *serviceCaPath)
    {
        if (LittleFS.exists(metaPath))
            LittleFS.remove(metaPath);
        if (LittleFS.exists(clientPath))
            LittleFS.remove(clientPath);
        if (LittleFS.exists(privatePath))
            LittleFS.remove(privatePath);
        if (LittleFS.exists(caPath))
            LittleFS.remove(caPath);
        if (LittleFS.exists(serviceCaPath))
            LittleFS.remove(serviceCaPath);
    }

#if defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0)
    String gMetaBuffer;
    String gPendingCertKind;
    String gPendingCertBody;
#endif
}

namespace FirmnginIdentity
{
    bool isProvisioned()
    {
        FirmnginIdentityRecord record;
        return load(record);
    }

    bool load(FirmnginIdentityRecord &out)
    {
#if !defined(ESP8266) && !defined(ESP32)
        (void)out;
        return false;
#else
        if (!mountFS())
            return false;
        recoverInterruptedCommit();
        const bool loaded = loadRecord(ACTIVE_PATHS, out);
        unmountFS();
        return loaded;
#endif
    }

    bool save(const FirmnginIdentityRecord &record)
    {
#if !defined(ESP8266) && !defined(ESP32)
        (void)record;
        return false;
#else
        if (!validateIdentityRecord(record))
            return false;
        if (!mountFS())
            return false;
        recoverInterruptedCommit();
        removePathSet(STAGED_PATHS);

        String meta;
        meta += "device_id=" + record.deviceId + "\n";
        meta += "device_key=" + record.deviceKey + "\n";
        if (record.decryptor.length() > 0)
            meta += "decryptor=" + record.decryptor + "\n";
        meta += "tls=" + record.tlsMode + "\n";
        if (record.fingerprintHex.length() > 0)
            meta += "fingerprint=" + record.fingerprintHex + "\n";
        if (record.brokerAddr.length() > 0)
            meta += "broker=" + record.brokerAddr + "\n";
        meta += "broker_port=" + String(record.brokerPort > 0 ? record.brokerPort : 58884) + "\n";
        meta += "api_base_url=" + record.apiBaseUrl + "\n";
        meta += "ota_base_url=" + record.otaBaseUrl + "\n";

        const bool staged =
            writeTextFile(STAGED_PATHS.clientCert, record.clientCert) &&
            writeTextFile(STAGED_PATHS.privateKey, record.privateKey) &&
            (record.caCert.length() == 0 || writeTextFile(STAGED_PATHS.caCert, record.caCert)) &&
            writeTextFile(STAGED_PATHS.serviceCaCert, record.serviceCaCert) &&
            writeTextFile(STAGED_PATHS.meta, meta);
        const bool saved = staged && commitStagedIdentity();
        if (!saved)
            removePathSet(STAGED_PATHS);
        unmountFS();
        return saved;
#endif
    }

    bool clear()
    {
#if !defined(ESP8266) && !defined(ESP32)
        return false;
#else
        if (!mountFS())
            return false;
        removePathSet(ACTIVE_PATHS);
        removePathSet(STAGED_PATHS);
        removePathSet(BACKUP_PATHS);
        removeFile(COMMIT_MARKER_PATH);
        removeIdentityFiles(LEGACY_META_PATH, LEGACY_CLIENT_CERT_PATH, LEGACY_PRIVATE_KEY_PATH, LEGACY_CA_CERT_PATH, LEGACY_SERVICE_CA_CERT_PATH);
        const bool cleared =
            !anyPathExists(ACTIVE_PATHS) &&
            !anyPathExists(STAGED_PATHS) &&
            !anyPathExists(BACKUP_PATHS) &&
            !LittleFS.exists(COMMIT_MARKER_PATH) &&
            !LittleFS.exists(LEGACY_META_PATH) &&
            !LittleFS.exists(LEGACY_CLIENT_CERT_PATH) &&
            !LittleFS.exists(LEGACY_PRIVATE_KEY_PATH) &&
            !LittleFS.exists(LEGACY_CA_CERT_PATH) &&
            !LittleFS.exists(LEGACY_SERVICE_CA_CERT_PATH);
        unmountFS();
        return cleared;
#endif
    }

    bool applyRuntimeCredentials(FirmnginIdentityRecord &record)
    {
        return load(record);
    }

    void pollSerialProvision()
    {
#if defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0)
        while (Serial.available() > 0)
        {
            String line = Serial.readStringUntil('\n');
            line.trim();
            if (line.length() == 0)
                continue;

            if (line == "FNGIN_HELLO")
            {
                Serial.println("FNGIN:hello protocol=3 service_ca service_endpoint");
                continue;
            }

            if (line.startsWith("FNGIN_META "))
            {
                String metaLine = line.substring(String("FNGIN_META ").length());
                if (metaLine.startsWith("device_id="))
                {
                    if (!mountFS())
                    {
                        Serial.println("FNGIN:provision_failed storage");
                        continue;
                    }
                    recoverInterruptedCommit();
                    removePathSet(STAGED_PATHS);
                    unmountFS();
                    gMetaBuffer = "";
                    gPendingCertKind = "";
                    gPendingCertBody = "";
                }
                gMetaBuffer += metaLine;
                gMetaBuffer += "\n";
                Serial.println("FNGIN:meta_ok");
                continue;
            }

            if (line.startsWith("FNGIN_CERT "))
            {
                int kindStart = String("FNGIN_CERT ").length();
                int space = line.indexOf(' ', kindStart);
                if (space < 0)
                    continue;
                gPendingCertKind = line.substring(kindStart, space);
                gPendingCertBody = line.substring(space + 1);
                Serial.println("FNGIN:cert_part");
                continue;
            }

            if (line == "FNGIN_CERT_END")
            {
                gPendingCertBody.replace("\\n", "\n");
                if (!mountFS())
                {
                    Serial.println("FNGIN:provision_failed storage");
                    continue;
                }
                bool stored = false;
                bool supportedKind = true;
                if (gPendingCertKind == "client")
                    stored = isCertificatePEM(gPendingCertBody) &&
                             writeTextFile(STAGED_PATHS.clientCert, gPendingCertBody);
                else if (gPendingCertKind == "private")
                    stored = isPrivateKeyPEM(gPendingCertBody) &&
                             writeTextFile(STAGED_PATHS.privateKey, gPendingCertBody);
                else if (gPendingCertKind == "ca")
                    stored = isCertificatePEM(gPendingCertBody) &&
                             writeTextFile(STAGED_PATHS.caCert, gPendingCertBody);
                else if (gPendingCertKind == "service_ca")
                    stored = isCertificatePEM(gPendingCertBody) &&
                             writeTextFile(STAGED_PATHS.serviceCaCert, gPendingCertBody);
                else
                    supportedKind = false;
                unmountFS();
                gPendingCertKind = "";
                gPendingCertBody = "";
                if (!supportedKind)
                {
                    Serial.println("FNGIN:provision_failed certificate");
                    continue;
                }
                if (!stored)
                {
                    Serial.println("FNGIN:provision_failed certificate");
                    continue;
                }
                Serial.println("FNGIN:cert_ok");
                continue;
            }

            if (line == "FNGIN_DONE")
            {
                String apiBaseUrl;
                String otaBaseUrl;
                if (!findMetaValue(gMetaBuffer, "api_base_url", apiBaseUrl) || !isValidHttpsBaseUrl(apiBaseUrl))
                {
                    Serial.println("FNGIN:provision_failed endpoint");
                    continue;
                }
                if (!findMetaValue(gMetaBuffer, "ota_base_url", otaBaseUrl) || !isValidHttpsBaseUrl(otaBaseUrl))
                {
                    Serial.println("FNGIN:provision_failed endpoint");
                    continue;
                }
                if (!mountFS())
                {
                    Serial.println("FNGIN:provision_failed storage");
                    continue;
                }
                if (!writeTextFile(STAGED_PATHS.meta, gMetaBuffer))
                {
                    unmountFS();
                    Serial.println("FNGIN:provision_failed storage");
                    continue;
                }

                FirmnginIdentityRecord staged;
                if (!loadRecord(STAGED_PATHS, staged))
                {
                    removeFile(STAGED_PATHS.meta);
                    unmountFS();
                    Serial.println("FNGIN:provision_failed identity");
                    continue;
                }

                if (!commitStagedIdentity())
                {
                    unmountFS();
                    Serial.println("FNGIN:provision_failed storage");
                    continue;
                }
                unmountFS();
                gMetaBuffer = "";
                Serial.println("FNGIN:identity_saved");
                delay(250);
                ESP.restart();
            }
        }
#endif
    }
}
