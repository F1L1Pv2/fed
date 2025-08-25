#ifndef GAP_BUFFER
#define GAP_BUFFER

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <stdlib.h>

typedef struct{
   char* items;
   size_t count;
   size_t cap;

   size_t gap_begin;
   size_t gap_end;
   size_t gap_default_size;
} GapBuffer;

bool GapBuffer_init(GapBuffer* buf, char* text, size_t n, size_t gap_size);
void GapBuffer_get_strs(GapBuffer* buf, char** out1,  size_t* n1, char** out2, size_t* n2);
void GapBuffer_insert_char(GapBuffer* buf, char c);
char GapBuffer_backspace(GapBuffer* buf);
char GapBuffer_delete(GapBuffer* buf);
void GapBuffer_goto(GapBuffer* buf, size_t index);
void GapBuffer_left(GapBuffer* buf);
void GapBuffer_right(GapBuffer* buf);
size_t GapBuffer_cursor_pos(GapBuffer* buf);
char GapBuffer_char_at(GapBuffer* buf, size_t index);
void GapBuffer_reset(GapBuffer* buf);
void GapBuffer_bake(GapBuffer* buf, char* str);

#ifdef GAP_BUFFER_IMPLEMENTATION

bool GapBuffer_init(GapBuffer* buf, char* text, size_t n, size_t gap_size){
    if(!gap_size) return false;
    if(text && n == 0) n = strlen(text);
    buf->gap_default_size = gap_size;
    buf->cap = buf->gap_default_size + n;
    buf->items = realloc(buf->items, buf->cap);
    if(!buf->items) return false;
    buf->count = n;
    buf->gap_begin = 0;
    buf->gap_end = buf->gap_default_size;
    if(text) memcpy(buf->items+buf->gap_end, text, n);
    return true;
}

void GapBuffer_get_strs(GapBuffer* buf, char** out1,  size_t* n1, char** out2, size_t* n2){
    *out1 = buf->items;
    *n1 = buf->gap_begin;
    *out2 = buf->items + buf->gap_end;
    *n2 = buf->cap - buf->gap_end;
}

void GapBuffer_insert_char(GapBuffer* buf, char c){
    if(buf->gap_end - buf->gap_begin == 0){
        size_t old_end_size = buf->cap - buf->gap_end;
        buf->cap+= buf->gap_default_size;
        buf->items = realloc(buf->items, buf->cap);
        buf->gap_end += buf->gap_default_size;
        memmove(buf->items + buf->gap_end, buf->items + buf->gap_begin, old_end_size);
    }
    buf->items[buf->gap_begin++] = c;
    buf->count++;
}

char GapBuffer_backspace(GapBuffer* buf){
    if(buf->gap_begin) {
        buf->gap_begin--;
        buf->count--;
        return buf->items[buf->gap_begin];
    }
    return 0;
}

char GapBuffer_delete(GapBuffer* buf){
    if(buf->gap_end < buf->cap) {
        buf->gap_end++;
        buf->count--;
        return buf->items[buf->gap_end - 1];
    }
    return 0;
}

void GapBuffer_goto(GapBuffer* buf, size_t index){
    size_t gap_len = buf->gap_end - buf->gap_begin;

    if(index < buf->gap_begin){
        memmove(buf->items + index + gap_len, 
                buf->items + index, 
                buf->gap_begin - index
               );
    }else{
        memmove(buf->items + buf->gap_begin, 
                buf->items + buf->gap_end, 
                index - buf->gap_begin
               );
    }
    buf->gap_begin = index;
    buf->gap_end = buf->gap_begin + gap_len;
}

void GapBuffer_left(GapBuffer* buf){
    if(buf->gap_begin == 0) return;
    buf->items[--buf->gap_end] = buf->items[--buf->gap_begin];
}

void GapBuffer_right(GapBuffer* buf){
    if(buf->gap_end >= buf->cap) return;
    buf->items[buf->gap_begin++] = buf->items[buf->gap_end++];
}

size_t GapBuffer_cursor_pos(GapBuffer* buf){
    return buf->gap_begin;
}

char GapBuffer_char_at(GapBuffer* buf, size_t index) {
    if (index < buf->gap_begin) {
        return buf->items[index];
    } else {
        return buf->items[index + (buf->gap_end - buf->gap_begin)];
    }
}

void GapBuffer_reset(GapBuffer* buf){
    buf->count = 0;
    buf->gap_begin = 0;
    buf->gap_end = buf->cap;
}

//It assumes string is already allocated or has enough space
void GapBuffer_bake(GapBuffer* buf, char* str){
    memcpy(str,buf->items,buf->gap_begin);
    memcpy(str+buf->gap_begin, buf->items+buf->gap_end, buf->cap - buf->gap_end);
}

#endif

#endif
