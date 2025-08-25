#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stui.h"
#include "gapbuffer.h"

#define NOB_STRIP_PREFIX
#include "../nob.h"

#define CURSOR_COLOR (STUI_RGB(0xFFE7E7E7))
#define CURSOR_TEXT_COLOR (STUI_RGB(0xFF181818))

void draw_text_len(size_t x, size_t return_x, size_t y, const char* text, int n, size_t width, size_t height, size_t* curOut_x, size_t* curOut_y, size_t cur, size_t fg, size_t bg){
    size_t cur_x = x;
    size_t cur_y = y;
    for(int i = 0; i < n; i++){
        if(cur_y >= y + height) break;
        if(cur_x >= x + width){
            cur_x = return_x;
            cur_y++;
        }
        if(text[i] != '\n'){
            stui_putchar_color(cur_x, cur_y, text[i], i == cur ? CURSOR_TEXT_COLOR: fg, i == cur ? CURSOR_COLOR: bg);
            cur_x++;
        }else{
            if(i == cur) stui_putchar_color(cur_x, cur_y, ' ', fg, CURSOR_COLOR);

            cur_x = return_x;
            cur_y++;
        }
    }
    if(cur_y < y + height && cur_x < x + width && cur == n) stui_putchar_color(cur_x, cur_y, ' ', fg, CURSOR_COLOR);
    if(curOut_x) *curOut_x = cur_x;
    if(curOut_y) *curOut_y = cur_y;
}

void draw_text(size_t x, size_t return_x, size_t y, const char* text, size_t width, size_t height, size_t* curOut_x, size_t* curOut_y, size_t cur, size_t fg, size_t bg){
    draw_text_len(x,return_x,y,text,strlen(text),width,height, curOut_x, curOut_y, cur, fg, bg);
}

stui_term_flag_t flags;
void restore(void) {
    stui_term_set_flags(flags);
#ifndef DISABLE_ALT_BUFFER
    // Alternate buffer.
    // The escape sequence below shouldn't do anything on terminals that don't support it
    printf("\033[?1049l");
#endif
    printf("\e[?25h"); // Show cursor
    fflush(stdout);
}

#define TEXT_FG (0)
#define TEXT_BG (0)

void draw_buffer(GapBuffer* buf, size_t x, size_t y, size_t width, size_t height, size_t fg, size_t bg){
    size_t anchor_x = x;
    size_t anchor_y = y;
    
    char* text1 = NULL;
    char* text2 = NULL;
    size_t n1 = 0;
    size_t n2 = 0;
    GapBuffer_get_strs(buf, &text1, &n1, &text2,&n2);

    size_t cur_x = anchor_x;
    size_t cur_y = anchor_y;
    if(n1) draw_text_len(cur_x, anchor_x, cur_y, text1, n1, width, height, &cur_x, &cur_y, n2 ? -1 : n1, fg, bg);
    if(n2) draw_text_len(cur_x, anchor_x, cur_y, text2, n2, width, height, NULL, NULL, 0, fg, bg);
    if(n1 == 0 && n2 == 0) stui_putchar_color(cur_x, cur_y, ' ', CURSOR_TEXT_COLOR, CURSOR_COLOR);
}

bool GapBuffer_write_entire_file(GapBuffer* gap_buf, const char* path){
    bool result = true;
    char* text1 = NULL;
    char* text2 = NULL;
    size_t n1 = 0;
    size_t n2 = 0;
    GapBuffer_get_strs(gap_buf, &text1, &n1, &text2,&n2);

    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        nob_log(NOB_ERROR, "Could not open file %s for writing: %s\n", path, strerror(errno));
        nob_return_defer(false);
    }


    char *buf = text1;
    while (n1 > 0) {
        size_t n = fwrite(buf, 1, n1, f);
        if (ferror(f)) {
            nob_log(NOB_ERROR, "Could not write into file %s: %s\n", path, strerror(errno));
            nob_return_defer(false);
        }
        n1 -= n;
        buf  += n;
    }

    buf = text2;
    while (n2 > 0) {
        size_t n = fwrite(buf, 1, n2, f);
        if (ferror(f)) {
            nob_log(NOB_ERROR, "Could not write into file %s: %s\n", path, strerror(errno));
            nob_return_defer(false);
        }
        n2 -= n;
        buf  += n;
    }
defer:
    if(f) fclose(f);
    return result;
}


#define STUI_CTRL(chr) ((chr) & 0x1F)
int main(int argc, char** argv){
    const char* program = shift_args(&argc, &argv);
    (void)program;

    if(!argc) {
        fprintf(stderr, "Provide filename!\n");
        return 1;
    }

    const char* filename = shift_args(&argc,&argv);

    String_Builder sb = {0};
    read_entire_file(filename, &sb);

    GapBuffer gap_buffer = {0};
    if(!GapBuffer_init(&gap_buffer, sb.items, sb.count, 128)) {
        printf("Couldn't initialize Gap Buffer!\n");
        return 1;
    }

    GapBuffer input_box = {0};
    if(!GapBuffer_init(&input_box, NULL, 0, 16)) {
        printf("Couldn't initialize Gap Buffer!\n");
        return 1;
    }

    flags = stui_term_get_flags();
    
    stui_term_disable_echo();
    stui_term_enable_instant();
    stui_term_disable_signals_and_sfc();

    size_t term_width, term_height;
    stui_term_get_size(&term_width, &term_height);
    stui_setsize(term_width, term_height);

#ifndef DISABLE_ALT_BUFFER
    printf("\033[?1049h");
#endif
    printf("\e[?25l"); // Hide cursor
    fflush(stdout);
    atexit(restore);
    stui_clear();
    bool command = false;
    char* input_box_msg_text = "Press Ctrl+b to open commands";
    String_Builder input_box_msg = {0};
    sb_append_cstr(&input_box_msg, input_box_msg_text);
    bool running = true;
    while(running){
        for(size_t y = 0; y < term_height; ++y) {
            for(size_t x = 0; x < term_width; ++x) {
                stui_putchar(x, y, ' ');
            }
        }

        draw_buffer(&gap_buffer, 0,0, term_width, term_height - 1, TEXT_FG, TEXT_BG);
        /*
        else{
            static char buf[256];
            char* text1 = NULL;
            char* text2 = NULL;
            size_t n1 = 0;
            size_t n2 = 0;
            GapBuffer_get_strs(&input_box, &text1, &n1, &text2,&n2);
            snprintf(buf,sizeof(buf),
                    "----------\n"
                    "items: 0x%p\n"
                    "count: %zu\n"
                    "cap: %zu\n"
                    "gap_begin: %zu\n"
                    "gap_end: %zu\n"
                    "gap_default_size: %zu\n"
                    "\n"
                    "text1: 0x%p %zu\n"
                    "text2: 0x%p %zu\n"
                    "----------\n",
                    input_box.items,
                    input_box.count,
                    input_box.cap,
                    input_box.gap_begin,
                    input_box.gap_end,
                    input_box.gap_default_size,
                    text1, n1,
                    text2, n2
            );
            draw_text(0,0,0,buf,term_width,term_height, NULL, NULL, -1,0,0);
        }
        */


#define INPUT_BOX_BG (STUI_RGB(0xFF555555))
#define INPUT_BOX_FG (0)
        for(size_t x = 0; x < term_width; x++){
            stui_putchar_color(x, term_height-1, ' ', INPUT_BOX_FG, INPUT_BOX_BG);
        }
       
        if(command) {
            stui_putchar_color(0, term_height-1, '>', INPUT_BOX_FG, INPUT_BOX_BG);
            draw_buffer(&input_box, 1,term_height - 1, term_width - 1, 1, INPUT_BOX_FG, INPUT_BOX_BG);
        }else{
            size_t offset = 0;
            draw_text(0,0,term_height - 1, filename, term_width, 1, &offset, NULL, -1, INPUT_BOX_FG, INPUT_BOX_BG);
            stui_putchar_color(offset++, term_height-1, ':', INPUT_BOX_FG, INPUT_BOX_BG);
            stui_putchar_color(offset++, term_height-1, ' ', INPUT_BOX_FG, INPUT_BOX_BG);
            draw_text_len(offset,0,term_height - 1, input_box_msg.items, input_box_msg.count, term_width - offset, 1, NULL, NULL, -1, INPUT_BOX_FG, INPUT_BOX_BG);
        }

        stui_refresh();

        int ch = stui_get_key();
        if(command){
            if(ch == STUI_KEY_ESC) {
                command = false;
                continue;
            }
            if(ch == STUI_KEY_UP) continue;
            if(ch == STUI_KEY_DOWN) continue;
            if(ch == STUI_KEY_LEFT){
                GapBuffer_left(&input_box);
                continue;
            }
            if(ch == STUI_KEY_RIGHT){
                GapBuffer_right(&input_box);
                continue;
            }
            if(ch == 127){
                GapBuffer_backspace(&input_box);
                continue;
            }
            if(ch == '\n') {
                sb.count = 0;
                da_resize(&sb, input_box.count);
                GapBuffer_bake(&input_box, sb.items);
                String_View sv = sb_to_sv(input_box);
                if(sv_eq(sv, sv_from_cstr("quit")) || sv_eq(sv, sv_from_cstr("q"))){
                    running = false;
                    continue;
                }
                if(sv_eq(sv, sv_from_cstr("w"))){
                    bool result = GapBuffer_write_entire_file(&gap_buffer, filename);
                    input_box_msg.count = 0;
                    if(result){
                        sb_append_cstr(&input_box_msg, "Wrote ");
                        char size[256];
                        snprintf(size, sizeof(size), "%ld", gap_buffer.count);
                        sb_append_cstr(&input_box_msg, size);
                        sb_append_cstr(&input_box_msg, " bytes to file");
                    }else{
                        sb_append_cstr(&input_box_msg, "There was an error during writing file");
                    }
                    command = false;
                    continue;
                }
                if(sv_eq(sv, sv_from_cstr("wq"))){
                    bool result = GapBuffer_write_entire_file(&gap_buffer, filename);
                    if(result){
                        running = false;
                        continue;
                    }
                    input_box_msg.count = 0;
                    sb_append_cstr(&input_box_msg, "There was an error during writing file");
                    command = false;
                    continue;
                }
                input_box_msg.count = 0;
                sb_append_cstr(&input_box_msg, "Unknown Command ");
                sb_append_buf(&input_box_msg, sb.items, sb.count);
                command = false;
                continue;
            }
            GapBuffer_insert_char(&input_box, ch);
        }else{
            if(ch == STUI_CTRL('c')) {
                input_box_msg.count = 0;
                sb_append_cstr(&input_box_msg, "To exit please write `q` or `quit` in input box (Ctrl+b)");
                continue;
            }
            if(ch == STUI_KEY_ESC) continue;
            if(ch == STUI_KEY_UP) continue;
            if(ch == STUI_KEY_DOWN) continue;
            if(ch == STUI_KEY_LEFT){
                GapBuffer_left(&gap_buffer);
                continue;
            }
            if(ch == STUI_KEY_RIGHT){
                GapBuffer_right(&gap_buffer);
                continue;
            }
            if(ch == 127){
                GapBuffer_backspace(&gap_buffer);
                continue;
            }
            if(ch == STUI_CTRL('b')){
                command = true;
                GapBuffer_reset(&input_box);
                continue;
            }
            GapBuffer_insert_char(&gap_buffer, ch);
        }
    }

    stui_term_set_flags(flags);
    return 0;
}
