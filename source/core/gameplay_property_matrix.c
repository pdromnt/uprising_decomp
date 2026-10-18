#include "gameplay_property_matrix.h"
#include <stdlib.h>
#include <string.h>

static int numeric(unsigned char c) {return (c>='0' && c<='9') || c=='.';}
/* Windows text fgets(...,510): bounded chunks, CRLF normalization, final
 * unterminated lines. Input bytes are borrowed only for this parse. */
static int next_line(const uint8_t *data,size_t size,size_t *cursor,char line[510]) {
    size_t used=0;
    if(*cursor>=size)return 0;
    while(*cursor<size && used<509u) {
        unsigned char c=data[(*cursor)++];
        if(c=='\r' && *cursor<size && data[*cursor]=='\n')c=data[(*cursor)++];
        line[used++]=(char)c;if(c=='\n')break;
    }
    line[used]=0;return 1;
}
/* 0054b130: preserve the actual digit/dot tokenizer, including sign characters
 * treated as separators. Extra numeric fields in a row fail. The original's
 * reversed '1'..'0' comparison on its ONE trailing line rejects a dot but
 * ignores integer digits; retain that quirk rather than inventing a grammar. */
int gameplay_property_matrix_parse(const uint8_t *data,size_t size,float output[234]) {
    unsigned row,column;size_t cursor=0;char line[510];
    if(!data || !output)return 0;
    for(column=0;column<13u;++column)output[column]=1.0f;
    for(row=1u;row<18u;++row) {
        size_t position=0,length;output[row*13u]=1.0f;
        if(!next_line(data,size,&cursor,line))return 0;
        length=strlen(line);
        for(column=1u;column<13u;++column) {
            size_t start,end;char token[510];
            while(position<length && !numeric((unsigned char)line[position]))++position;
            if(position==length)return 0;
            start=position;
            while(position<length && numeric((unsigned char)line[position]))++position;
            end=position;memcpy(token,line+start,end-start);token[end-start]=0;
            output[row*13u+column]=(float)strtod(token,0);
        }
        while(position<length)if(numeric((unsigned char)line[position++]))return 0;
    }
    if(next_line(data,size,&cursor,line) && strchr(line,'.'))return 0;
    return 1;
}