#include "firmngin_identity.h"
#include "fng_fs.h"
#include "identity_meta.h"

#if defined(ESP_PLATFORM)
#include <sys/stat.h>
#include <sys/types.h>
#include <cstdio>
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

    bool isDirectory(const char *path)
    {
#if defined(ESP_PLATFORM)
        struct stat st = {};
        return stat(fng_fs_path(path).c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#else
        (void)path;
        return false;
#endif
    }









    bool mountFS()
    {
#if defined(ESP_PLATFORM)
        return fng_fs_mount();
#else
        return false;
#endif
    }

    void unmountFS()
    {
#if defined(ESP_PLATFORM)
        fng_fs_unmount();
#endif
    }

    bool fileExists(const char *path)
    {
#if defined(ESP_PLATFORM)
        struct stat st = {};
        return stat(fng_fs_path(path).c_str(), &st) == 0;
#else
        (void)path;
        return false;
#endif
    }

    bool ensureIdentityDirectory()
    {
#if defined(ESP_PLATFORM)
        if (isDirectory(IDENTITY_DIR))
            return true;
        return mkdir(fng_fs_path(IDENTITY_DIR).c_str(), 0775) == 0 || isDirectory(IDENTITY_DIR);
#else
        return false;
#endif
    }

    bool readTextFile(const char *path, String &out)
    {
#if defined(ESP_PLATFORM)
        if (!fileExists(path))
        {
            out = "";
            return false;
        }
        FILE *file = fopen(fng_fs_path(path).c_str(), "r");
        if (file == nullptr)
            return false;
        out = "";
        char buf[512];
        size_t read = 0;
        while ((read = fread(buf, 1, sizeof(buf), file)) > 0)
        {
            // Cap identity files: PEM bundles are a few KB; a flash-corrupted
            // giant file must not churn the heap on a 320 KB device.
            if (out.length() > 16384)
            {
                fclose(file);
                return false;
            }
            fng_append(out, buf, read);
        }
        fclose(file);
        return out.length() > 0;
#else
        (void)path;
        (void)out;
        return false;
#endif
    }

    bool writeTextFile(const char *path, const String &content)
    {
#if defined(ESP_PLATFORM)
        if (!ensureIdentityDirectory())
            return false;
        FILE *file = fopen(fng_fs_path(path).c_str(), "w");
        if (file == nullptr)
            return false;
        const size_t written = content.length() == 0 ? 0u : fwrite(content.c_str(), 1, content.length(), file);
        fclose(file);
        return written == content.length();
#else
        (void)path;
        (void)content;
        return false;
#endif
    }

    void removeFile(const char *path)
    {
#if defined(ESP_PLATFORM)
        if (fileExists(path))
            remove(fng_fs_path(path).c_str());
#endif
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
#if defined(ESP_PLATFORM)
        if (!fileExists(from))
            return true;
        removeFile(to);
        return rename(fng_fs_path(from).c_str(), fng_fs_path(to).c_str()) == 0;
#else
        (void)from;
        (void)to;
        return false;
#endif
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
        return fileExists(paths.meta) ||
               fileExists(paths.clientCert) ||
               fileExists(paths.privateKey) ||
               fileExists(paths.caCert) ||
               fileExists(paths.serviceCaCert);
    }

    bool restoreBackupFile(const char *backupPath, const char *activePath)
    {
        if (!fileExists(backupPath))
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
        if (!fileExists(COMMIT_MARKER_PATH))
            return;

        FirmnginIdentityRecord active;
        if (loadRecord(ACTIVE_PATHS, active))
        {
            removePathSet(BACKUP_PATHS);
        }
        else
        {
            const bool hasBackup = anyPathExists(BACKUP_PATHS);
            const bool backupWasComplete = fileExists(BACKUP_PATHS.meta) || !hasBackup;
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
        if (fileExists(metaPath))
            removeFile(metaPath);
        if (fileExists(clientPath))
            removeFile(clientPath);
        if (fileExists(privatePath))
            removeFile(privatePath);
        if (fileExists(caPath))
            removeFile(caPath);
        if (fileExists(serviceCaPath))
            removeFile(serviceCaPath);
    }

#if defined(ARDUINO) && defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0)
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
#if !defined(ESP_PLATFORM)
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
#if !defined(ESP_PLATFORM)
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
        if (record.audioStreamUrl.length() > 0)
            meta += "audio_stream_url=" + record.audioStreamUrl + "\n";
        if (record.audioStreamToken.length() > 0)
            meta += "audio_stream_token=" + record.audioStreamToken + "\n";

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
#if !defined(ESP_PLATFORM)
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
            !fileExists(COMMIT_MARKER_PATH) &&
            !fileExists(LEGACY_META_PATH) &&
            !fileExists(LEGACY_CLIENT_CERT_PATH) &&
            !fileExists(LEGACY_PRIVATE_KEY_PATH) &&
            !fileExists(LEGACY_CA_CERT_PATH) &&
            !fileExists(LEGACY_SERVICE_CA_CERT_PATH);
        unmountFS();
        return cleared;
#endif
    }

    bool applyRuntimeCredentials(FirmnginIdentityRecord &record)
    {
        return load(record);
    }

#if defined(ARDUINO) && defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0)
    void pollSerialProvision()
    {
        while (Serial.available() > 0)
        {
            String line = Serial.readStringUntil('\n');
            line = fng_trim(line);
            if (line.length() == 0)
                continue;

            if (line == "FNGIN_HELLO")
            {
                Serial.println("FNGIN:hello protocol=3 service_ca service_endpoint");
                continue;
            }

            if (fng_startsWith(line, "FNGIN_META "))
            {
                String metaLine = fng_substring(line, static_cast<int>(String("FNGIN_META ").length()));
                if (fng_startsWith(metaLine, "device_id="))
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

            if (fng_startsWith(line, "FNGIN_CERT "))
            {
                int kindStart = static_cast<int>(String("FNGIN_CERT ").length());
                int space = fng_indexOf(line, ' ', kindStart);
                if (space < 0)
                    continue;
                gPendingCertKind = fng_substring(line, kindStart, space);
                gPendingCertBody = fng_substring(line, space + 1);
                Serial.println("FNGIN:cert_part");
                continue;
            }

            if (line == "FNGIN_CERT_END")
            {
                gPendingCertBody = fng_replace_all(gPendingCertBody, "\\n", "\n");
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
                fng_delay_ms(250);
                fng_restart();
            }
        }
    }
#else
    void pollSerialProvision()
    {
#if defined(FIRMNGIN_FACTORY_SERIAL) && (FIRMNGIN_FACTORY_SERIAL != 0)
        // Factory serial provisioning is an Arduino-side flow (Serial-based).
        // Pure IDF firmware provisions out-of-band; nothing to poll here.
#endif
    }
#endif
}
