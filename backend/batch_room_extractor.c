
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>

#define MAX_WORDS 30000
#define MAX_TEXT 512
#define MAX_PATH_LEN 512

typedef struct {
    int block, par, line;
    int x, y, width;
    char text[MAX_TEXT];
} Word;

const char *rooms[] = {
    "CB101","CB102","CB202","CB207","CB212","CB213","CB214","CB215","CB216","CB218",
    "CB301","CB302","CB306","CB307","CB308","CB309","CB310","CB311","CB314","CB315",
    "CB316","CB317","CB319","CB321","CB401","CB402","CB406","CB407","CB408","CB412",
    "CB413","CB415","CB417","CB418","CB419","CB501","CB502","CB512","CB513","CB514",
    "CB601","CB602","CB611","CB613","CB614","CB616",

    "CME007","CME012","CME104","CME105","CME109","CME114","CME116","CME117","CME118",
    "CME201","CME204","CME205","CME206","CME207","CME214","CME215","CME307","CME308",
    "CME309","CME310","CME316","CME317","CME318","CME405","CME406","CME408","CME409",
    "CME410","CME414","CME415","CME416","CME417","CME504","CME505","CME506","CME507",
    "CME508","CME512","CME513","CME604","CME605","CME606","CME607","CME608","CME609",
    "CME615","CME616","CME617",

    "ICT002","ICT004","ICT008","ICT102","ICT103","ICT105","ICT108","ICT109","ICT201",
    "ICT204","ICT205","ICT206","ICT210","ICT211","ICT304","ICT305","ICT306","ICT307",
    "ICT311","ICT312","ICT341","ICT401","ICT402","ICT403","ICT404","ICT405","ICT406",
    "ICT409","ICT410","ICT501","ICT502","ICT503","ICT504","ICT505","ICT506","ICT509",
    "ICT511","ICT603","ICT604","ICT605","ICT614"
};

#define ROOM_COUNT (sizeof(rooms) / sizeof(rooms[0]))

Word words[MAX_WORDS];
int word_count = 0;

FILE *out;
int total_rows = 0;
int total_candidates = 0;

void normalize(const char *src, char *dst) {
    int j = 0;
    for (int i = 0; src[i] && j < MAX_TEXT - 1; i++) {
        if (isalnum((unsigned char)src[i]))
            dst[j++] = (char)toupper((unsigned char)src[i]);
    }
    dst[j] = '\0';
}

int is_same_line(Word *a, Word *b) {
    return a->block == b->block &&
           a->par == b->par &&
           a->line == b->line;
}

int room_exists(const char *candidate) {
    for (int i = 0; i < ROOM_COUNT; i++) {
        if (strcmp(candidate, rooms[i]) == 0)
            return 1;
    }
    return 0;
}

void write_candidate(const char *pdf, const char *file,
                     int page, int x, int y,
                     const char *room, const char *ocr,
                     const char *method) {
    fprintf(out, "%s,%s,%d,%d,%d,%s,\"%s\",%s\n",
            pdf, file, page, x, y, room, ocr, method);
    total_candidates++;
}

void check_single(const char *pdf, const char *file, int page,
                  Word *w) {
    char clean[MAX_TEXT];

    normalize(w->text, clean);

    for (int r = 0; r < ROOM_COUNT; r++) {
        if (strstr(clean, rooms[r]) != NULL) {
            write_candidate(pdf, file, page, w->x, w->y,
                            rooms[r], w->text, "SINGLE_TOKEN");
        }
    }
}



void check_pair(const char *pdf, const char *file, int page,
                Word *a, Word *b) {
    char first[MAX_TEXT], second[MAX_TEXT], combined[MAX_TEXT];
    int ax, bx, dx, dy;

    normalize(a->text, first);
    normalize(b->text, second);

    if (!first[0] || !second[0])
        return;

    ax = a->x + a->width / 2;
    bx = b->x + b->width / 2;

    dx = abs(ax - bx);
    dy = abs(a->y - b->y);

    /* Reject distant tokens; allow OCR vertical displacement. */
    if (dx > 180 || dy > 120)
        return;

    for (int r = 0; r < ROOM_COUNT; r++) {
        snprintf(combined, sizeof(combined), "%s%s", first, second);

        if (strcmp(combined, rooms[r]) == 0) {
            char original[MAX_TEXT * 2];

            snprintf(original, sizeof(original), "%s + %s",
                     a->text, b->text);

            write_candidate(pdf, file, page,
                            a->x < b->x ? a->x : b->x,
                            a->y < b->y ? a->y : b->y,
                            rooms[r], original, "NEARBY_TOKENS");
            return;
        }

        snprintf(combined, sizeof(combined), "%s%s", second, first);

        if (strcmp(combined, rooms[r]) == 0) {
            char original[MAX_TEXT * 2];

            snprintf(original, sizeof(original), "%s + %s",
                     a->text, b->text);

            write_candidate(pdf, file, page,
                            a->x < b->x ? a->x : b->x,
                            a->y < b->y ? a->y : b->y,
                            rooms[r], original, "NEARBY_TOKENS_REVERSED");
            return;
        }
    }
}


int main(void) {
    int i;
    DIR *dir;
    struct dirent *entry;
    char path[MAX_PATH_LEN];

    out = fopen("room_candidates_all.csv", "w");
    if (!out) {
        printf("Could not create output CSV.\n");
        return 1;
    }

    fprintf(out, "PDF_ID,TSV_FILE,PAGE,X,Y,ROOM,OCR_TEXT,METHOD\n");

    dir = opendir("timetable_extraction");
    if (!dir) {
        printf("Could not open timetable_extraction folder.\n");
        fclose(out);
        return 1;
    }

    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;

        if (strncmp(name, "pdf", 3) != 0)
            continue;
        if (strlen(name) < 9 || strstr(name, ".tsv") == NULL)
            continue;

        char pdf[16] = {0};
        strncpy(pdf, name, 5);
        int page = 1;
char *page_marker = strrchr(name, '_');

if (page_marker != NULL) {
    page = atoi(page_marker + 1);
}

        snprintf(path, sizeof(path), "timetable_extraction/%s", name);
        FILE *fp = fopen(path, "r");
        if (!fp)
            continue;

        word_count = 0;
        char line[4096];

        /* Skip TSV header. */
        if (!fgets(line, sizeof(line), fp)) {
            fclose(fp);
            continue;
        }

        while (fgets(line, sizeof(line), fp) && word_count < MAX_WORDS) {
            char copy[4096];
            strncpy(copy, line, sizeof(copy) - 1);
            copy[sizeof(copy) - 1] = '\0';

            char *fields[12];
            int n = 0;
            char *p = strtok(copy, "\t\r\n");

            while (p && n < 12) {
                fields[n++] = p;
                p = strtok(NULL, "\t\r\n");
            }

            if (n < 12 || strcmp(fields[0], "5") != 0)
                continue;

            Word *w = &words[word_count];

            w->block = atoi(fields[2]);
            w->par   = atoi(fields[3]);
            w->line  = atoi(fields[4]);
            w->x     = atoi(fields[6]);
            w->y     = atoi(fields[7]);
            w->width = atoi(fields[8]);

            strncpy(w->text, fields[11], MAX_TEXT - 1);
            w->text[MAX_TEXT - 1] = '\0';

            word_count++;
            total_rows++;
        }

        fclose(fp);

        
        /* Check complete room IDs and nearby split tokens. */
        for (int i = 0; i < word_count; i++) {

            check_single(pdf, name, page, &words[i]);

            for (int j = i + 1; j < word_count; j++) {
                check_pair(pdf, name, page, &words[i], &words[j]);
            }
        }
    }


    closedir(dir);
    fclose(out);

    printf("\nCorrected room extraction finished.\n");
    printf("OCR words examined: %d\n", total_rows);
    printf("Room candidates found: %d\n", total_candidates);
    printf("Output: room_candidates_all.csv\n");
    printf("These are candidates only; timetable cells still need validation.\n");

    return 0;
    }

