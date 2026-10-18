#ifndef UPRISING_TAGGED_SCALAR_STREAM_H
#define UPRISING_TAGGED_SCALAR_STREAM_H
#include <stdint.h>
#include <stddef.h>

/* Four-byte little-endian scalar records, each prefixed by the retail tag.
 * Ports distinguish transport failure from a short read; short reads retain
 * untouched bytes exactly as the existing save/replay stream does. */
typedef struct TaggedScalarStreamServices {
    void *context;
    int (*read)(void *, uint8_t bytes[4], uint32_t *elements);
    int (*write)(void *, const uint8_t bytes[4]);
    int (*flush)(void *);
} TaggedScalarStreamServices;
int tagged_scalar_stream_write(uint32_t *marker, uint32_t value,
    const TaggedScalarStreamServices *);
int tagged_scalar_stream_read(uint32_t *marker, uint32_t *value,
    uint32_t *marker_elements, const TaggedScalarStreamServices *);
/* Shared raw scalar/float/blob transport. Fixed-width tags and values are
 * serialized explicitly; byte buffers are borrowed only for the call. */
typedef struct TaggedRecordStreamServices {
    void *context;
    int (*read)(void *,uint8_t *,size_t element_bytes,size_t count,uint32_t *elements);
    int (*write)(void *,const uint8_t *,size_t element_bytes,size_t count);
    int (*flush)(void *);
} TaggedRecordStreamServices;
int tagged_record_stream_write(uint32_t *marker,uint32_t tag,const uint8_t *,size_t,
    const TaggedRecordStreamServices *);
int tagged_record_stream_read(uint32_t *marker,uint8_t *,size_t,
    uint32_t *marker_elements,uint32_t *payload_elements,const TaggedRecordStreamServices *);
int tagged_record_stream_write_elements(uint32_t *marker,uint32_t tag,const uint8_t *,
    size_t element_bytes,size_t count,const TaggedRecordStreamServices *);
int tagged_record_stream_read_elements(uint32_t *marker,uint8_t *,size_t element_bytes,
    size_t count,uint32_t *marker_elements,uint32_t *payload_elements,const TaggedRecordStreamServices *);
#endif
