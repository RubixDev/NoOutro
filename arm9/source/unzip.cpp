#include "unzip.h"
#include <nds.h>

UNZIP zip;

void *myOpen(const char *filename, int32_t *size) {
    FILE *f;
    size_t filesize;
    f = fopen(filename, "r+b");
    if (f) {
        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);
        *size = (int32_t)filesize;
    }
    return (void *)f;
}

void myClose(void *p) {
    ZIPFILE *pzf = (ZIPFILE *)p;
    if (pzf)
        fclose((FILE *)pzf->fHandle);
}

int32_t myRead(void *p, uint8_t *buffer, int32_t length) {
    ZIPFILE *pzf = (ZIPFILE *)p;
    if (!pzf)
        return 0;
    return (int32_t)fread(buffer, 1, length, (FILE *)pzf->fHandle);
}

int32_t mySeek(void *p, int32_t position, int type) {
    ZIPFILE *pzf = (ZIPFILE *)p;
    if (!pzf)
        return 0;
    return fseek((FILE *)pzf->fHandle, position, type);
}

int unzip_file(
    std::string zip_path,
    std::string output_dir,
    std::function<bool(std::string path)> for_each_file
) {
    // TODO: mkdir -p output_dir
    int status;
    uLong bytes_read;
    uint8_t buf[1 << 19]; // 0.5 MiB
    unz_file_info fi;
    char fileName[256];

    status = zip.openZIP(zip_path.c_str(), myOpen, myClose, myRead, mySeek);
    if (status == UNZ_OK) {
        zip.gotoFirstFile();
        status = UNZ_OK;
        while (status == UNZ_OK) {
            status = zip.openCurrentFile();
            if (status != UNZ_OK) {
                printf("Failed to extract file: %d\n", status);
                zip.closeZIP();
                return -1;
            }
            status = zip.getFileInfo(&fi, fileName, sizeof(fileName), NULL, 0, NULL, 0);
            if (status != UNZ_OK) {
                printf("Failed to extract file: %d\n", status);
                zip.closeZIP();
                return -1;
            }
            consoleClear();
            printf("Extracting file: %s\n", fileName);
            bytes_read = 0;
            status = 1;
            std::string filePath = output_dir + "/" + std::string(fileName);
            FILE *f = fopen(filePath.c_str(), "wb");
            while (status > 0) {
                status = zip.readCurrentFile(buf, sizeof(buf));
                if (status >= 0) {
                    bytes_read += status;
                    fwrite(buf, status, 1, f);

                    iprintf("\x1B[4;0H%ld/%ld bytes\n", bytes_read, fi.uncompressed_size);
                    if (fi.uncompressed_size > 0) {
                        char bar[31];
                        bar[30] = 0;
                        for (unsigned int i = 0; i < 30; i++)
                            bar[i] = (bytes_read * 30 / (fi.uncompressed_size | 1) > i) ? '=' : ' ';
                        iprintf("[%s]\n", bar);
                    }
                } else {
                    printf("Failed to extract file: %d\n", status);
                    zip.closeZIP();
                    return -1;
                }
            }
            fclose(f);
            printf("Total bytes read: %ld\n", bytes_read);
            status = zip.closeCurrentFile();
            if (for_each_file) {
                if (!for_each_file(filePath)) {
                    return -2;
                }
            }
            status = zip.gotoNextFile();
        }
        zip.closeZIP();
    }
    return 0;
}
