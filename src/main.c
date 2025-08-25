#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stui.h"
#include "gapbuffer.h"

#define NOB_STRIP_PREFIX
#include "../nob.h"

#define CURSOR_COLOR (STUI_RGB(0xFFE7E7E7))
#define CURSOR_TEXT_COLOR (STUI_RGB(0xFF181818))

void draw_text_len(
        int64_t x, 
        int64_t return_x, 
        int64_t y, 
        const char* text, 
        int n, 
        int64_t width, 
        int64_t height, 
        int64_t* curOut_x, 
        int64_t* curOut_y, 
        size_t cur, 
        size_t fg, 
        size_t bg,
        int64_t scroll
){
    int64_t cur_x = x;
    int64_t cur_y = y;
    int64_t visual_line = 0;

    for(int i = 0; i < n; i++){
        // wrap if needed
        if(cur_x >= x + width){
            cur_x = return_x;
            visual_line++;
        }

        if(text[i] == '\n'){
            if(visual_line >= scroll && visual_line - scroll < height){
                if(i == cur){
                    stui_putchar_color(cur_x, cur_y + (visual_line - scroll), ' ', fg, CURSOR_COLOR);
                }
            }
            cur_x = return_x;
            visual_line++;
            continue;
        }

        // Only draw if inside viewport
        if(visual_line >= scroll && visual_line - scroll < height){
            stui_putchar_color(
                cur_x,
                cur_y + (visual_line - scroll),
                text[i],
                i == cur ? CURSOR_TEXT_COLOR : fg,
                i == cur ? CURSOR_COLOR : bg
            );
        }
        cur_x++;
    }

    // Cursor at end of buffer
    if(cur == n){
        if(visual_line >= scroll && visual_line - scroll < height){
            if(cur_x >= x + width){
                cur_x = return_x;
                visual_line++;
            }
            if(visual_line - scroll < height){
                stui_putchar_color(cur_x, cur_y + (visual_line - scroll), ' ', fg, CURSOR_COLOR);
            }
        }
    }

    if(curOut_x) *curOut_x = cur_x;
    if(curOut_y) *curOut_y = cur_y + (visual_line - scroll);
}

inline void draw_text(
        int64_t x,
        int64_t return_x,
        int64_t y,
        const char* text,
        size_t width,
        size_t height,
        int64_t* curOut_x,
        int64_t* curOut_y,
        size_t cur,
        size_t fg,
        size_t bg,
        int64_t scroll
){
    draw_text_len(x,return_x,y,text,strlen(text),width,height, curOut_x, curOut_y, cur, fg, bg, scroll);
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

void draw_buffer(
        GapBuffer* buf, 
        int64_t x, 
        int64_t y, 
        size_t width, 
        size_t height, 
        size_t fg, 
        size_t bg,
        int64_t scroll
){
    int64_t anchor_x = x;
    int64_t anchor_y = y;
    
    char* text1 = NULL;
    char* text2 = NULL;
    size_t n1 = 0;
    size_t n2 = 0;
    GapBuffer_get_strs(buf, &text1, &n1, &text2,&n2);

    int64_t cur_x = anchor_x;
    int64_t cur_y = anchor_y;
    if(n1) draw_text_len(cur_x, anchor_x, cur_y, text1, n1, width, height, &cur_x, &cur_y, n2 ? -1 : n1, fg, bg, scroll);
    if(n2) draw_text_len(cur_x, anchor_x, cur_y, text2, n2, width - cur_x, height - cur_y, NULL, NULL, 0, fg, bg, n1 ? 0 : scroll);
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

bool GapBuffer_find_backward(GapBuffer* buf, size_t start_index, char searching, size_t* found_index){
    if(start_index > buf->count) return false;
    for(size_t i = start_index; i > 0; i--){
        if(GapBuffer_char_at(buf, i) == searching){
            *found_index = i;
            return true;
        }
    }
    return false;
}

bool GapBuffer_find_forward(GapBuffer* buf, size_t start_index, char searching, size_t* found_index){
    if(start_index > buf->count) return false;
    for(size_t i = start_index; i < buf->count; i++){
        if(GapBuffer_char_at(buf, i) == searching){
            *found_index = i;
            return true;
        }
    }
    return false;
}

// static to keep across calls
static size_t preferred_column = 0;

void GapBuffer_move_up(GapBuffer* buf) {
    size_t cur = GapBuffer_cursor_pos(buf);
    size_t line_start = 0;
    if (GapBuffer_find_backward(buf, cur ? cur - 1 : 0, '\n', &line_start)) {
        line_start++; // move after newline
    }
    size_t prev_line_end = 0;
    if (!GapBuffer_find_backward(buf, line_start ? line_start - 1 : 0, '\n', &prev_line_end)) {
        GapBuffer_goto(buf, 0);
        return;
    }
    size_t prev_line_start = 0;
    if (GapBuffer_find_backward(buf, prev_line_end ? prev_line_end - 1 : 0, '\n', &prev_line_start)) {
        prev_line_start++;
    }
    size_t prev_line_len = prev_line_end - prev_line_start;
    size_t target = prev_line_start + (preferred_column < prev_line_len ? preferred_column : prev_line_len);
    GapBuffer_goto(buf, target);
}

void GapBuffer_move_down(GapBuffer* buf) {
    size_t cur = GapBuffer_cursor_pos(buf);
    size_t line_end = buf->count;
    GapBuffer_find_forward(buf, cur, '\n', &line_end);
    if (line_end == buf->count) return; // no next line
    size_t next_line_start = line_end + 1;
    size_t next_line_end = buf->count;
    GapBuffer_find_forward(buf, next_line_start, '\n', &next_line_end);
    size_t next_line_len = next_line_end - next_line_start;
    size_t target = next_line_start + (preferred_column < next_line_len ? preferred_column : next_line_len);
    GapBuffer_goto(buf, target);
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
    int64_t scroll_offset = 0;
    while(running){
        for(size_t y = 0; y < term_height; ++y) {
            for(size_t x = 0; x < term_width; ++x) {
                stui_putchar(x, y, ' ');
            }
        }

        draw_buffer(&gap_buffer, 0,0, term_width, term_height - 1, TEXT_FG, TEXT_BG, scroll_offset);
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
            draw_buffer(&input_box, 1,term_height - 1, term_width - 1, 1, INPUT_BOX_FG, INPUT_BOX_BG, 0);
        }else{
            int64_t offset = 0;
            draw_text(0,0,term_height - 1, filename, term_width, 1, &offset, NULL, -1, INPUT_BOX_FG, INPUT_BOX_BG, 0);
            stui_putchar_color(offset++, term_height-1, ':', INPUT_BOX_FG, INPUT_BOX_BG);
            stui_putchar_color(offset++, term_height-1, ' ', INPUT_BOX_FG, INPUT_BOX_BG);
            draw_text_len(offset,0,term_height - 1, input_box_msg.items, input_box_msg.count, term_width - offset, 1, NULL, NULL, -1, INPUT_BOX_FG, INPUT_BOX_BG, 0);
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
            if(ch == STUI_KEY_LEFT){
                GapBuffer_left(&gap_buffer);
                // update preferred column
                size_t cur = GapBuffer_cursor_pos(&gap_buffer);
                size_t line_start = 0;
                if (GapBuffer_find_backward(&gap_buffer, cur ? cur-1 : 0, '\n', &line_start)) {
                    line_start++;
                }
                preferred_column = cur - line_start;
                continue;
            }
            if(ch == STUI_KEY_RIGHT){
                GapBuffer_right(&gap_buffer);
                // update preferred column
                size_t cur = GapBuffer_cursor_pos(&gap_buffer);
                size_t line_start = 0;
                if (GapBuffer_find_backward(&gap_buffer, cur ? cur-1 : 0, '\n', &line_start)) {
                    line_start++;
                }
                preferred_column = cur - line_start;
                continue;
            }
            if(ch == STUI_KEY_UP){
                GapBuffer_move_up(&gap_buffer);
                continue;
            }
            if(ch == STUI_KEY_DOWN){
                GapBuffer_move_down(&gap_buffer);
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
            if(ch == 9){
                GapBuffer_insert_char(&gap_buffer, ' ');
                GapBuffer_insert_char(&gap_buffer, ' ');
                GapBuffer_insert_char(&gap_buffer, ' ');
                GapBuffer_insert_char(&gap_buffer, ' ');
                continue;
            }
            GapBuffer_insert_char(&gap_buffer, ch);
        }
    }

    stui_term_set_flags(flags);
    return 0;
}
