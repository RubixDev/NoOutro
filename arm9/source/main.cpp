#include "download.h"
#include "menu.h"
#include "multibootstrap.h"
#include "unzip.h"
#include "version.h"
#include "wifi.h"

#include "lexbor/dom/dom.h"
#include "lexbor/html/parser.h"

#include <dirent.h>
#include <fat.h>
#include <math.h>
#include <nds.h>
#include <stdio.h>

bool verbose = false;
static const std::string BASE_URL = "https://myrient.erista.me/files/No-Intro/";
static const std::string DATA_DIR = "sd:/no-outro";
static const std::string ZIP_FILE_PATH = DATA_DIR + "/rom.zip";
static const std::string ROMS_DIR = "sd:/roms/";

const char *read_file(const char *filename) {
    char *buffer = 0;
    long length;
    FILE *f = fopen(filename, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell(f);
        fseek(f, 0, SEEK_SET);
        buffer = (char *)malloc(length);
        if (buffer) {
            fread(buffer, 1, length, f);
        }
        fclose(f);
    }

    return buffer;
}

unsigned int count_lines(std::string string) {
    int newlines = 0;
    const char *p = string.c_str();
    for (unsigned int i = 0; i < string.length(); i++) {
        if (p[i] == '\n') {
            newlines++;
        }
    }
    return newlines;
}

MenuItem createSubMenu(
    std::string title,
    std::string ident,
    std::string dir,
    std::string remoteDir,
    std::string description,
    PrintConsole *topConsole,
    PrintConsole *bottomConsole,
    std::function<bool(std::string roms_path)> customExtract = nullptr
) {
    MenuItem item = {
        title,
        0,
        [topConsole, bottomConsole, title, ident, dir, remoteDir, customExtract](Menu &menu) {
            const std::string html_path = DATA_DIR + "/" + ident + ".html";
            const std::string roms_path = ROMS_DIR + dir;
            const std::string url = BASE_URL + remoteDir;
            Menu subMenu(title);
            subMenu.addItem(MenuItem {
                "Browse Titles                X",
                KEY_X,
                [topConsole, bottomConsole, html_path, roms_path, url, customExtract](Menu &menu) {
                    consoleClear();
                    Menu::print("Reading index file...\n");
                    const char *html_c = read_file(html_path.c_str());
                    if (!html_c) {
                        Menu::prompt(
                            KEY_A | KEY_B,
                            "Could not read index file!\nPlease make sure you have\ndownloaded "
                            "the latest index\nusing the \"Update Index\" menu\noption.\n"
                        );
                        return;
                    }
                    std::string html = html_c;
                    // we only want the relevant lines with the table data
                    size_t start = html.find("\n<tr>");
                    size_t end = html.rfind("</tbody>");
                    html = html.substr(start + 1, end - start + 1);
                    unsigned int total_lines = count_lines(html);

                    // chunk the html in chunks of up to 200 lines each,
                    // parse each chunk and add the items to a menu
                    Menu titleListMenu("Available Titles", topConsole, bottomConsole);
                    start = 0;
                    size_t chunk_len = 0;
                    int chunk_count = 0;
                    unsigned int current_line = 0;
                    while (start < html.length()) {
                        chunk_count++;
                        chunk_len = 0;
                        int newlines = 0;
                        size_t bytes_since_newline = 0;
                        for (unsigned int i = 0; start + i < html.length() && newlines < 200; i++) {
                            bytes_since_newline++;
                            if (html[start + i] == '\n') {
                                chunk_len += bytes_since_newline;
                                bytes_since_newline = 0;
                                newlines++;
                            }
                        }
                        if (chunk_len == 0)
                            break;

                        std::string chunk =
                            "<table><tbody>" + html.substr(start, chunk_len) + "</tbody></table>";

                        lxb_status_t status;
                        lxb_dom_element_t *element;
                        lxb_html_document_t *document = lxb_html_document_create();
                        lxb_dom_collection_t *collection =
                            lxb_dom_collection_make(&document->dom_document, 2000);
                        size_t html_len = strlen(chunk.c_str());

                        // progress bar
                        // Menu::print("Parsing index chunk %d...\n", chunk_count);
                        iprintf("\x1B[2;0HParsing index... \n");
                        char bar[31];
                        bar[30] = 0;
                        for (unsigned int i = 0; i < 30; i++)
                            bar[i] = (current_line * 30 / (total_lines | 1) > i) ? '=' : ' ';
                        Menu::print("[%s]\n", bar);

                        // parse html
                        status = lxb_html_document_parse(
                            document, (lxb_char_t *)chunk.c_str(), html_len
                        );
                        if (status != LXB_STATUS_OK) {
                            Menu::print("Failed to parse index file\n");
                            return;
                        }

                        // get all tr elements
                        status = lxb_dom_elements_by_tag_name(
                            (lxb_dom_element_t *)document->body,
                            collection,
                            (const lxb_char_t *)"tr",
                            2
                        );
                        if (status != LXB_STATUS_OK) {
                            Menu::print("Failed to find tr elements in index\n");
                            return;
                        }

                        // get data from all tr elements
                        for (size_t i = 0; i < lxb_dom_collection_length(collection); i++) {
                            element = lxb_dom_collection_element(collection, i);

                            lxb_dom_collection_t *size_elem_collection =
                                lxb_dom_collection_make(&document->dom_document, 1);
                            lxb_dom_elements_by_class_name(
                                element, size_elem_collection, (const lxb_char_t *)"size", 4
                            );
                            lxb_dom_element_t *size_elem =
                                lxb_dom_collection_element(size_elem_collection, 0);
                            size_t size_len;
                            lxb_char_t *size_c =
                                lxb_dom_node_text_content((lxb_dom_node_t *)size_elem, &size_len);
                            std::string size = std::string((const char *)size_c, size_len);

                            lxb_dom_collection_t *a_elem_collection =
                                lxb_dom_collection_make(&document->dom_document, 1);
                            lxb_dom_elements_by_tag_name(
                                element, a_elem_collection, (const lxb_char_t *)"a", 1
                            );
                            lxb_dom_element_t *a_elem =
                                lxb_dom_collection_element(a_elem_collection, 0);
                            size_t title_len, href_len;
                            const lxb_char_t *title_c = lxb_dom_element_get_attribute(
                                a_elem, (const lxb_char_t *)"title", 5, &title_len
                            );
                            const lxb_char_t *href_c = lxb_dom_element_get_attribute(
                                a_elem, (const lxb_char_t *)"href", 4, &href_len
                            );
                            std::string title = std::string((const char *)title_c, title_len);
                            std::string href = std::string((const char *)href_c, href_len);

                            lxb_dom_collection_destroy(size_elem_collection, true);
                            lxb_dom_collection_destroy(a_elem_collection, true);

                            MenuItem item(
                                title,
                                0,
                                [href, roms_path, url, customExtract](Menu &menu) {
                                    u16 key = Menu::prompt(
                                        KEY_A | KEY_B,
                                        "Download selected title?\n\n(<A> Yes, <B> No)\n"
                                    );
                                    if (key & KEY_A) {
                                        int ret =
                                            download((url + href).c_str(), ZIP_FILE_PATH.c_str());
                                        if (ret >= 0) {
                                            Menu::print("\nDownload Successful!\n");
                                            bool res =
                                                customExtract
                                                    ? customExtract(roms_path)
                                                    : unzip_file(ZIP_FILE_PATH, roms_path) == 0;
                                            if (!res) {
                                                Menu::prompt(
                                                    KEY_A | KEY_B,
                                                    "\x1b[31mExtraction failed.\n\x1b[33mPress A "
                                                    "or B to go back.\x1b[39m\n"
                                                );
                                            } else {
                                                Menu::prompt(
                                                    KEY_A | KEY_B,
                                                    "\x1b[32mExtraction successful!\n\x1b[33mPress "
                                                    "A or B to go back.\x1b[39m\n"
                                                );
                                            }
                                        } else {
                                            Menu::printDelay(
                                                60, "\nDownload Failed\nCode: %d\n", ret
                                            );
                                        }
                                    }
                                }
                            );
                            item.details = "Full Title:\n" + title + "\n\nDownload Size: " + size;
                            titleListMenu.addItem(item);
                        }

                        lxb_dom_collection_destroy(collection, true);
                        lxb_html_document_destroy(document);

                        start += chunk_len;
                        current_line += newlines;
                    }

                    // clang-format off
                    titleListMenu.addItem({"Back", KEY_B, [topConsole, bottomConsole](Menu &menu) {
                        menu.exit();
                        consoleSelect(topConsole);
                        consoleClear();
                        consoleSelect(bottomConsole);
                        consoleClear();
                    }});
                    // clang-format on
                    consoleClear();
                    titleListMenu.run();
                }});
            // clang-format off
            subMenu.addItem({"Update Index                 R", KEY_R, [html_path, url](Menu &menu) {
                int ret = download(url.c_str(), html_path.c_str());
                if (ret >= 0) {
                    Menu::print("\nDownload Successful!\n");
                } else {
                    Menu::print("\nDownload Failed\nCode: %d\n", ret);
                }
            }});
            subMenu.addItem({"Back                         B", KEY_B, [](Menu &menu) {
                menu.exit();
            }});
            // clang-format on

            consoleClear();
            subMenu.run();
        }};
    item.details = title + "\n\nIdentifier: " + ident + "\n\nDownloads to: " + ROMS_DIR + dir +
                   "/\n\nDescription:\n\n" + description;
    return item;
}

bool gbaMultibootExtract(std::string roms_path) {
    return unzip_file(ZIP_FILE_PATH, roms_path, [](std::string file_path) {
               return multibootstrap_patchFile(file_path.c_str());
           }) == 0;
}

int main(int argc, char **argv) {
    // init top and bottom screen consoles
    PrintConsole topConsoleConfig = *consoleGetDefault();
    PrintConsole *bottomConsole = consoleDemoInit();
    videoSetMode(MODE_0_2D);
    PrintConsole *topConsole =
        consoleInit(&topConsoleConfig, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleSelect(bottomConsole);

    bool fatInited = fatInitDefault();
    if (!fatInited) {
        Menu::prompt(KEY_START, "FAT init failed.\n\nPress START to quit.");
        return 1;
    };

    keysSetRepeat(25, 5);
    scanKeys();
    verbose = keysHeld() & KEY_SELECT;

    // If not using no$gba, initialize Wi-Fi
    if (strncmp((const char *)0x4FFFA00, "no$gba", 6) != 0) {
        Menu::print("Connecting to Wi-Fi...\n");
        Menu::print("If this doesn't connect, ensure\nyour Wi-Fi is working in "
                    "System\nSettings.\n\nIf it still doesn't work your\nrouter isn't compatible "
                    "yet.\n\nHold SELECT while loading for\nverbose logging.\n\n");

        wifiInit();
        uint i = 0;
        while (!wifiConnected()) {
            swiWaitForVBlank();
            const char spinner[] = "-\\|/";
            Menu::print("\x1B[23;31H%c", spinner[(i++ / 6) % (sizeof(spinner) - 1)]);
        }
    }

    // make sure NoOutro folder exists
    mkdir(DATA_DIR.c_str(), 0777);

    consoleClear();

    Menu mainMenu("NoOutro " VER_NUMBER, topConsole, bottomConsole);

    // clang-format off
    mainMenu.addItem(createSubMenu("Nintendo DS",                  "nds",     "nds",     "Nintendo%20-%20Nintendo%20Entertainment%20System%20%28Headered%29/", "Licensed software for the Nintendo DS", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Nintendo DS (Download Play)",  "nds_dp",  "nds",     "Nintendo%20-%20Nintendo%20DS%20%28Download%20Play%29/", "Licensed software sent over the DS Download Play wireless protocol", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Nintendo DS (Private)",        "nds_p",   "nds",     "Nintendo%20-%20Nintendo%20DS%20%28Decrypted%29%20%28Private%29/", "Unlicensed paid software for the Nintendo DS", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Nintendo DSi",                 "dsi",     "dsiware", "Nintendo%20-%20Nintendo%20DSi%20%28Decrypted%29/", "Licensed software for the Nintendo DSi", topConsole, bottomConsole));
    // TODO: the two digital collections need extra work, mainly renaming the extracted files
    // mainMenu.addItem(createSubMenu("Nintendo DSi (Digital)",       "dsi_d",   "dsiware", "Nintendo%20-%20Nintendo%20DSi%20%28Digital%29/", "", topConsole, bottomConsole));
    // mainMenu.addItem(createSubMenu("Nintendo DSi (Digital) (CDN)", "dsi_cdn", "dsiware", "Nintendo%20-%20Nintendo%20DSi%20%28Digital%29%20%28CDN%29%20%28Decrypted%29/", "Licensed downloadable software for the Nintendo DSi (DSiWare)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Atari 2600",                   "a26",     "a26",     "Atari%20-%202600/", "Licensed software for the Atari 2600", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Atari 5200",                   "a52",     "a52",     "Atari%20-%205200/", "Licensed software for the Atari 5200", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Atari 7800",                   "a78",     "a78",     "Atari%20-%207800/", "Licensed software for the Atari 7800", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("ColecoVision",                 "col",     "col",     "Coleco%20-%20ColecoVision/", "Licensed software for the ColecoVision", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Game Boy",                     "gb",      "gb",      "Nintendo%20-%20Game%20Boy/", "Licensed software for the Nintendo Game Boy", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Game Boy (Private)",           "gb_p",    "gb",      "Nintendo%20-%20Game%20Boy%20%28Private%29/", "Unlicensed paid software for the Nintendo Game Boy", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Game Boy Color",               "gbc",     "gb",      "Nintendo%20-%20Game%20Boy%20Color/", "Licensed software for the Nintendo Game Boy Color", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Game Boy Color (Private)",     "gbc_p",   "gb",      "Nintendo%20-%20Game%20Boy%20Color%20%28Private%29/", "Unlicensed paid software for the Nintendo Game Boy Color", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Game Boy Advance",             "gba",     "gba",     "Nintendo%20-%20Game%20Boy%20Advance/", "Licensed software for the Nintendo Game Boy Advance", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Game Boy Advance (Multiboot)", "gba_m",   "gba",     "Nintendo%20-%20Game%20Boy%20Advance%20%28Multiboot%29/", "Licensed software sent over the GBA Multiboot protocol", topConsole, bottomConsole, gbaMultibootExtract));
    mainMenu.addItem(createSubMenu("Game Boy Advance (Private)",   "gba_p",   "gba",     "Nintendo%20-%20Game%20Boy%20Advance%20%28Private%29/", "Unlicensed paid software for the Nintendo Game Boy Advance", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Game Boy Advance (Video)",     "gba_v",   "gba",     "Nintendo%20-%20Game%20Boy%20Advance%20%28Video%29/", "Licensed videos for the Nintendo Game Boy Advance", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("NeoGeo Pocket",                "ngp",     "ngp",     "SNK%20-%20NeoGeo%20Pocket/", "Licensed software for the NeoGeo Pocket", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("NeoGeo Pocket Color",          "ngpc",    "ngp",     "SNK%20-%20NeoGeo%20Pocket%20Color/", "Licensed software for the NeoGeo Pocket Color", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("NES",                          "nes",     "nes",     "Nintendo%20-%20Nintendo%20Entertainment%20System%20%28Headered%29/", "Licensed software for the Nintendo Entertainment System (a.k.a. Family Computer)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("NES (Private)",                "nes_p",   "nes",     "Nintendo%20-%20Nintendo%20Entertainment%20System%20%28Headered%29%20%28Private%29/", "Unlicensed paid software for the Nintendo Entertainment System (a.k.a. Family Computer)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Sega Game Gear",               "gg",      "gg",      "Sega%20-%20Game%20Gear/", "Licensed software for the Sega Game Gear", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Sega Genesis",                 "gen",     "gen",     "Sega%20-%20Mega%20Drive%20-%20Genesis/", "Licensed software for the Sega Genesis (a.k.a. Mega Drive)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Sega Genesis (Private)",       "gen_p",   "gen",     "Sega%20-%20Mega%20Drive%20-%20Genesis%20%28Private%29/", "Unlicensed software for the Sega Genesis (a.k.a. Mega Drive)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Sega Master System",           "sms",     "sms",     "Sega%20-%20Master%20System%20-%20Mark%20III/", "Licensed software for the Sega Master System", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("Sega SG-1000",                 "sg",      "sg",      "Sega%20-%20SG-1000/", "Licensed software for the Sega SG-100", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("SNES",                         "snes",    "snes",    "Nintendo%20-%20Super%20Nintendo%20Entertainment%20System/", "Licensed software for the Super Nintendo Entertainment System (a.k.a. Super Family Computer)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("SNES (Private)",               "snes_p",  "snes",    "Nintendo%20-%20Super%20Nintendo%20Entertainment%20System%20%28Private%29/", "Unlicensed paid software for the Super Nintendo Entertainment System (a.k.a. Super Family Computer)", topConsole, bottomConsole));
    // TODO: Sord M5 doesn't have roms in No-Intro
    // mainMenu.addItem(createSubMenu("Sord M5",                      "m5",      "m5",      "", "Licensed software for the Sord M5", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("TurboGrafx-16",                "tg16",    "tg16",    "NEC%20-%20PC%20Engine%20-%20TurboGrafx-16/", "Licensed software for the TurboGrafx-16 (a.k.a. PC Engine)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("TurboGrafx-16 (Private)",      "tg16_p",  "tg16",    "NEC%20-%20PC%20Engine%20-%20TurboGrafx-16%20%28Private%29/", "Unlicensed paid software for the TurboGrafx-16 (a.k.a. PC Engine)", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("WonderSwan",                   "ws",      "ws",      "Bandai%20-%20WonderSwan/", "Licensed software for the WonderSwan", topConsole, bottomConsole));
    mainMenu.addItem(createSubMenu("WonderSwan Color",             "wsc",     "ws",      "Bandai%20-%20WonderSwan%20Color/", "Licensed software for the WonderSwan Color", topConsole, bottomConsole));

    mainMenu.addItem({"Exit                     START", KEY_START, [](Menu &menu) {
        menu.exit();
    }});
    // clang-format on

    consoleClear();
    mainMenu.run();
}
