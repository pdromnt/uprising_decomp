#include "tagged_scalar_stream.h"

static void encode(uint8_t output[4], uint32_t value) {
    unsigned i;
    for (i = 0u; i < 4u; ++i) output[i] = (uint8_t)(value >> (i * 8u));
}
static uint32_t decode(const uint8_t input[4]) {
    return (uint32_t)input[0] | (uint32_t)input[1] << 8u |
        (uint32_t)input[2] << 16u | (uint32_t)input[3] << 24u;
}
int tagged_scalar_stream_write(uint32_t *marker, uint32_t value,
        const TaggedScalarStreamServices *services) {
    uint8_t bytes[4];
    if (!marker || !services || !services->write || !services->flush) return 0;
    *marker = 0xfedcba92u;
    encode(bytes, *marker);
    if (!services->write(services->context, bytes)) return 0;
    encode(bytes, value);
    return services->write(services->context, bytes) &&
        services->flush(services->context);
}
int tagged_scalar_stream_read(uint32_t *marker, uint32_t *value,
        uint32_t *marker_elements, const TaggedScalarStreamServices *services) {
    uint8_t bytes[4];
    uint32_t ignored;
    if (!marker || !value || !marker_elements || !services || !services->read) return 0;
    encode(bytes, *marker);
    if (!services->read(services->context, bytes, marker_elements)) return 0;
    *marker = decode(bytes);
    encode(bytes, *value);
    if (!services->read(services->context, bytes, &ignored)) return 0;
    *value = decode(bytes);
    return 1;
}
int tagged_record_stream_write_elements(uint32_t *marker,uint32_t tag,const uint8_t *payload,size_t element_bytes,size_t count,
    const TaggedRecordStreamServices *s){
    uint8_t prefix[4];
    if(!marker || !s || !s->write || !s->flush || !element_bytes ||
        count>UINT32_MAX || count>SIZE_MAX/element_bytes || (!payload && count))return 0;
    *marker=tag;encode(prefix,tag);
    return s->write(s->context,prefix,4u,1u) && s->write(s->context,payload,element_bytes,count) && s->flush(s->context);
}
int tagged_record_stream_read_elements(uint32_t *marker,uint8_t *payload,size_t element_bytes,size_t count,
    uint32_t *marker_elements,uint32_t *payload_elements,const TaggedRecordStreamServices *s){
    uint8_t prefix[4];
    if(!marker || !s || !s->read || !marker_elements || !payload_elements || !element_bytes ||
        count>UINT32_MAX || count>SIZE_MAX/element_bytes || (!payload && count))return 0;
    encode(prefix,*marker);
    if(!s->read(s->context,prefix,4u,1u,marker_elements))return 0;
    *marker=decode(prefix);
    return s->read(s->context,payload,element_bytes,count,payload_elements);
}
int tagged_record_stream_write(uint32_t *marker,uint32_t tag,const uint8_t *payload,size_t bytes,
    const TaggedRecordStreamServices *s){return tagged_record_stream_write_elements(marker,tag,payload,1u,bytes,s);}
int tagged_record_stream_read(uint32_t *marker,uint8_t *payload,size_t bytes,uint32_t *marker_elements,
    uint32_t *payload_elements,const TaggedRecordStreamServices *s){
    return tagged_record_stream_read_elements(marker,payload,1u,bytes,marker_elements,payload_elements,s);
}
