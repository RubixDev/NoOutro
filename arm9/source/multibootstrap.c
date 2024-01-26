// Code in this file is copied and modified from
// https://gbatemp.net/threads/multibootstrap-patch-multiboot-gba-roms-to-run-from-a-cartridge.613958/

#include "multibootstrap.h"

unsigned const char expected_entrypoint[4] = {0x2E, 0x00, 0x00, 0xEA};
unsigned const char bootstrap_code[] = {
    0x02, 0x03, 0x5F, 0xE3, 0x06, 0x00, 0x00, 0xBA, 0x02, 0x04, 0xA0, 0xE3, 0x01, 0x17, 0x80,
    0xE2, 0x02, 0x23, 0xA0, 0xE3, 0x04, 0x30, 0x92, 0xE4, 0x04, 0x30, 0x80, 0xE4, 0x01, 0x00,
    0x50, 0xE1, 0xFB, 0xFF, 0xFF, 0xDA, 0x04, 0xF0, 0x1F, 0xE5, 0xC0, 0x00, 0x00, 0x02};

bool multibootstrap_patchFile(const char *path) {
    FILE *file = fopen(path, "rb+");
    unsigned char entrypoint[4];
    fread(&entrypoint, 4, 1, file);
    if (memcmp(expected_entrypoint, entrypoint, 4)) {
        puts("Expected entrypoint not found");
        return false;
    }

    fseek(file, 0, SEEK_END);
    int filesize = ftell(file);
    unsigned char byte = 0;
    if (filesize > 256 * 1024) {
        printf("Too big to be a multiboot ROM!\npath: %s\nsize: %d\n", path, filesize);
        return false;
    } else if (filesize == 256 * 1024) {
        puts("Finding end of trimmed multiboot ROM");
        fseek(file, -1, SEEK_END);
        do {
            fread(&byte, 1, 1, file);
            fseek(file, -2, SEEK_CUR);
        } while (byte == 0x00 || byte == 0xff);
        fseek(file, 2, SEEK_CUR);
    }
    while ((filesize = ftell(file)) % 4) {
        fwrite(&byte, 1, 1, file);
    }
    puts("Injecting bootstrap code");
    fwrite(bootstrap_code, sizeof(bootstrap_code), 1, file);
    puts("Patching entrypoint");
    unsigned char new_entrypoint[4];
    new_entrypoint[0] = (filesize - 8) >> 2;
    new_entrypoint[1] = (filesize - 8) >> 10;
    new_entrypoint[2] = (filesize - 8) >> 18;
    new_entrypoint[3] = 0xea;
    fseek(file, 0, SEEK_SET);
    fwrite(new_entrypoint, 4, 1, file);

    puts("Successfuly patched");
    fclose(file);
    return true;
}
