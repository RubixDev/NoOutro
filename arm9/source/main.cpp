#include "download.h"
#include "unzip.h"
#include "version.h"
#include "menu.h"
#include "wifi.h"

#include "lexbor/dom/dom.h"
#include "lexbor/html/parser.h"

#include <dirent.h>
#include <fat.h>
#include <math.h>
#include <nds.h>
#include <stdio.h>

bool verbose = false;
static const std::string URL = "https://myrient.erista.me/files/No-Intro/Nintendo%20-%20Nintendo%20DS%20(Decrypted)/";
static const std::string DEV_URL = "http://192.168.0.173:8000/index.html";
static const std::string DATA_DIR = "sd:/no-outro";
static const std::string INDEX_FILE_PATH = DATA_DIR + "/index.html";
static const std::string ZIP_FILE_PATH = DATA_DIR + "/rom.zip";
static const std::string ROM_DIR = "sd:/roms/nds";

const char *read_file(const char *filename) {
    char *buffer = 0;
    long length;
    FILE *f = fopen(filename, "rb");

    if (f) {
        fseek(f, 0, SEEK_END);
        length = ftell (f);
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

int main(int argc, char **argv) {
    // init top and bottom screen consoles
    PrintConsole topConsoleConfig = *consoleGetDefault();
	PrintConsole *bottomConsole = consoleDemoInit();
    videoSetMode(MODE_0_2D);
	PrintConsole *topConsole = consoleInit(&topConsoleConfig, 0, BgType_Text4bpp, BgSize_T_256x256, 31, 0, true, true);
    consoleSelect(bottomConsole);

	bool fatInited = fatInitDefault();
	if(!fatInited) {
		Menu::prompt(KEY_START, "FAT init failed.\n\nPress START to quit.");
		return 1;
	};

	keysSetRepeat(25, 5);
	scanKeys();
	verbose = keysHeld() & KEY_SELECT;

	// If not using no$gba, initialize Wi-Fi
	if(strncmp((const char *)0x4FFFA00, "no$gba", 6) != 0) {
		Menu::print("Connecting to Wi-Fi...\n");
		Menu::print("If this doesn't connect, ensure\nyour Wi-Fi is working in System\nSettings.\n\nIf it still doesn't work your\nrouter isn't compatible yet.\n\nHold SELECT while loading for\nverbose logging.\n\n");

		wifiInit();
		uint i = 0;
		while(!wifiConnected()) {
			swiWaitForVBlank();
			const char spinner[] = "-\\|/";
			Menu::print("\x1B[23;31H%c", spinner[(i++ / 6) % (sizeof(spinner) - 1)]);
		}
	}

    // make sure NoOutro folder exists
    mkdir(DATA_DIR.c_str(), 0777);

	// if(argc > 1) {
	// 	printf("Running scripts from argv...\n");
	// 	for(int i = 1; i < argc; i++) {
	// 		runScript(argv[i]);
	// 	}
	// }

	consoleClear();

	Menu mainMenu("NoOutro " VER_NUMBER);

	mainMenu.addItem({"Browse Titles                X", KEY_X, [topConsole, bottomConsole](Menu &menu) {
        Menu::print("Reading index file...\n");
        const char *html_c = read_file(INDEX_FILE_PATH.c_str());
        if (!html_c) {
            Menu::print("Could not read index file.\nPlease make sure you downloaded the latest index with the \"Update Index\" menu option.\n");
            return;
        }
        std::string html = html_c;
        // we only want the relevant lines with the table data
        size_t start = html.find("\n<tr>");
        size_t end = html.rfind("</tbody>");
        html = html.substr(start + 1, end - start + 1);

        // chunk the html in chunks of up to 2000 lines each,
        // parse each chunk and add the items to a menu
        Menu titleListMenu("Available Titles", topConsole, bottomConsole);
        start = 0;
        size_t chunk_len = 0;
        int chunk_count = 0;
        while (start < html.length()) {
            chunk_count++;
            chunk_len = 0;
            int newlines = 0;
            size_t bytes_since_newline = 0;
            for (unsigned int i = 0; start + i < html.length() && newlines < 2000; i++) {
                bytes_since_newline++;
                if (html[start + i] == '\n') {
                    chunk_len += bytes_since_newline;
                    bytes_since_newline = 0;
                    newlines++;
                }
            }
            if (chunk_len == 0) break;

            std::string chunk = "<table><tbody>" + html.substr(start, chunk_len) + "</tbody></table>";

            lxb_status_t status;
            lxb_dom_element_t *element;
            lxb_html_document_t *document = lxb_html_document_create();
            lxb_dom_collection_t *collection = lxb_dom_collection_make(&document->dom_document, 2000);
            size_t html_len = strlen(chunk.c_str());

            // parse html
            Menu::print("Parsing index chunk %d...\n", chunk_count);
            status = lxb_html_document_parse(document, (lxb_char_t *)chunk.c_str(), html_len);
            if (status != LXB_STATUS_OK) {
                Menu::print("Failed to parse index file\n");
                return;
            }

            // get all tr elements
            status = lxb_dom_elements_by_tag_name((lxb_dom_element_t *)document->body, collection, (const lxb_char_t *)"tr", 2);
            if (status != LXB_STATUS_OK) {
                Menu::print("Failed to find tr elements in index\n");
                return;
            }

            // get data from all tr elements
            for (size_t i = 0; i < lxb_dom_collection_length(collection); i++) {
                element = lxb_dom_collection_element(collection, i);

                lxb_dom_collection_t *size_elem_collection = lxb_dom_collection_make(&document->dom_document, 1);
                lxb_dom_elements_by_class_name(element, size_elem_collection, (const lxb_char_t *)"size", 4);
                lxb_dom_element_t *size_elem = lxb_dom_collection_element(size_elem_collection, 0);
                size_t size_len;
                lxb_char_t *size_c = lxb_dom_node_text_content((lxb_dom_node_t *)size_elem, &size_len);
                std::string size = std::string((const char *)size_c, size_len);

                lxb_dom_collection_t *a_elem_collection = lxb_dom_collection_make(&document->dom_document, 1);
                lxb_dom_elements_by_tag_name(element, a_elem_collection, (const lxb_char_t *)"a", 1);
                lxb_dom_element_t *a_elem = lxb_dom_collection_element(a_elem_collection, 0);
                size_t title_len, href_len;
                const lxb_char_t *title_c = lxb_dom_element_get_attribute(a_elem, (const lxb_char_t *)"title", 5, &title_len);
                const lxb_char_t *href_c = lxb_dom_element_get_attribute(a_elem, (const lxb_char_t *)"href", 4, &href_len);
                std::string title = std::string((const char *)title_c, title_len);
                std::string href = std::string((const char *)href_c, href_len);

                lxb_dom_collection_destroy(size_elem_collection, true);
                lxb_dom_collection_destroy(a_elem_collection, true);

                MenuItem item(title, 0, [href](Menu &menu) {
                    u16 key = Menu::prompt(KEY_A | KEY_B, "Download selected title?\n\n(<A> Yes, <B> No)\n");
                    if (key & KEY_A) {
                        int ret = download((URL + href).c_str(), ZIP_FILE_PATH.c_str());
                        if (ret >= 0) {
                            Menu::printDelay(60, "\nDownload Successful!\n");
                            unzip_file(ZIP_FILE_PATH, ROM_DIR);
                        } else {
                            Menu::printDelay(60, "\nDownload Failed\nCode: %d\n", ret);
                        }
                    }
                });
                item.details = "Full Title:\n" + title + "\n\nDownload Size: " + size;
                titleListMenu.addItem(item);
            }

            lxb_dom_collection_destroy(collection, true);
            lxb_html_document_destroy(document);

            start += chunk_len;
        }

        titleListMenu.addItem({"Return to main menu", KEY_B, [topConsole, bottomConsole](Menu &menu) {
            menu.exit();
            consoleSelect(topConsole);
            consoleClear();
            consoleSelect(bottomConsole);
            consoleClear();
        }});
        consoleClear();
        titleListMenu.run();
	}});
	mainMenu.addItem({"Update Index                 R", KEY_R, [](Menu &menu) {
        int ret = download(URL.c_str(), INDEX_FILE_PATH.c_str());
        if (ret >= 0) {
            Menu::print("\nDownload Successful!\n");
        } else {
            Menu::print("\nDownload Failed\nCode: %d\n", ret);
        }
	}});
	mainMenu.addItem({"Update Index (Dev)           L", KEY_L, [](Menu &menu) {
        int ret = download(DEV_URL.c_str(), INDEX_FILE_PATH.c_str());
        if (ret >= 0) {
            Menu::print("\nDownload Successful!\n");
        } else {
            Menu::print("\nDownload Failed\nCode: %d\n", ret);
        }
	}});
	mainMenu.addItem({"Exit                     START", KEY_START, [](Menu &menu) {
		menu.exit();
	}});

	consoleClear();
	mainMenu.run();
}
