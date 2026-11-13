#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 4096
#define MAX_TEXT 512

static const char *rooms[] = {
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
#define ROOM_COUNT (sizeof(rooms)/sizeof(rooms[0]))

static int boundary(int value, const int *bounds, int n) {
    int i;
    for (i = 0; i < n - 1; ++i)
        if (value >= bounds[i] && value < bounds[i + 1]) return i;
    return -1;
}

static void csv_text(FILE *f, const char *s) {
    const char *p;
    fputc('"', f);
    for (p = s; *p; ++p) {
        if (*p == '"') fputc('"', f);
        fputc(*p, f);
    }
    fputc('"', f);
}

int main(void) {
    /* Approximate boundaries measured from BT 3rd Year PDF page 1,
       converted to the 3513 x 2550 OCR coordinate system. */
    const int xbounds[] = {539, 805, 1147, 1593, 1829, 2242, 2509, 2890, 3157, 3393};
    const int ybounds[] = {507, 736, 1019, 1296, 1568, 1852};
    const char *days[] = {"MON","TUE","WED","THU","FRI"};
    const char *times[] = {"09:00","10:00","11:00","12:00","13:00","14:00","15:00","16:00","17:00"};
    const char *ends[]  = {"10:00","11:00","12:00","13:00","14:00","15:00","16:00","17:00","18:00"};
    const char *input = "timetable_extraction/pdf05_1.tsv";
    const char *output = "bt05_room_candidates.csv";
    FILE *in = fopen(input, "r"), *out;
    char line[MAX_LINE];
    int rows = 0, found = 0, uncertain = 0;

    if (!in) { printf("Cannot open %s\nRun from the RoomSync project folder.\n", input); return 1; }
    out = fopen(output, "w");
    if (!out) { fclose(in); printf("Cannot create %s\n", output); return 1; }
    fprintf(out, "PDF_ID,Department,Year,Day,Start,End,Room,X,Y,OCR_Text,Review\n");
    fgets(line, sizeof(line), in); /* header */

    while (fgets(line, sizeof(line), in)) {
        char copy[MAX_LINE], *col[12] = {0}, *tok;
        int n = 0, x, y, di, si, i;
        char text[MAX_TEXT];
        strcpy(copy, line);
        tok = strtok(copy, "\t\r\n");
        while (tok && n < 12) { col[n++] = tok; tok = strtok(NULL, "\t\r\n"); }
        if (n < 12 || atoi(col[0]) != 5) continue;
        x = atoi(col[6]); y = atoi(col[7]);
        snprintf(text, sizeof(text), "%s", col[11]);
        for (i = 0; text[i]; ++i) text[i] = (char)toupper((unsigned char)text[i]);
        di = boundary(y, ybounds, 6);
        si = boundary(x, xbounds, 10);
        if (di < 0 || di >= 5 || si < 0 || si >= 9) continue;

        for (i = 0; i < (int)ROOM_COUNT; ++i) {
            if (strstr(text, rooms[i])) {
                fprintf(out, "pdf05,BT,3,%s,%s,%s,%s,%d,%d,", days[di], times[si], ends[si], rooms[i], x, y);
                csv_text(out, text);
                fprintf(out, ",REVIEW_CELL_SPAN\n");
                found++;
            }
        }
        rows++;
        if (strstr(text, "CB") || strstr(text, "CME") || strstr(text, "ICT")) uncertain++;
    }
    fclose(in); fclose(out);
    printf("BT candidate extraction finished.\n");
    printf("OCR rows examined: %d\nRoom occurrences found: %d\nRoom-like OCR rows: %d\n", rows, found, uncertain);
    printf("Output: %s\n", output);
    printf("IMPORTANT: These are candidates only. REVIEW_CELL_SPAN means verify merged-cell duration against the PDF before import.\n");
    return 0;
}
