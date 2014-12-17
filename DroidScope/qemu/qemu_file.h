#ifndef _QEMU_FILE_H
#define _QEMU_FILE_H

#include "hw/hw.h"

typedef enum {
    Q_FIELD_END,          /* mark end of field list */
    Q_FIELD_BYTE,         /* for 1-byte fields */
    Q_FIELD_INT16,        /* for 2-byte fields */
    Q_FIELD_INT32,        /* for 4-byte fields */
    Q_FIELD_INT64,        /* for 8-byte fields */
    Q_FIELD_BUFFER,       /* for buffer fields */
    Q_FIELD_BUFFER_SIZE,  /* to specify the size of buffers */

#if TARGET_LONG_BITS == 64
    Q_FIELD_TL = Q_FIELD_INT64,           /* target long, either 4 or 8 bytes */
#else
    Q_FIELD_TL = Q_FIELD_INT32
#endif

} QFieldType;

typedef struct {
    QFieldType  type : 16; /* field type */
    uint16_t    offset;    /* offset of field in structure */
} QField;

#define  QFIELD_BEGIN(name)  \
    static const QField    name[] = {

#define  _QFIELD_(t, f)    { t, offsetof(QFIELD_STRUCT,f) }
#define  QFIELD_BYTE(f)   _QFIELD_(Q_FIELD_BYTE, f)
#define  QFIELD_INT16(f)  _QFIELD_(Q_FIELD_INT16, f)
#define  QFIELD_INT32(f)  _QFIELD_(Q_FIELD_INT32, f)
#define  QFIELD_INT64(f)  _QFIELD_(Q_FIELD_INT64, f)
#define  QFIELD_TL(f)     _QFIELD_(Q_FIELD_TL, f)

#define  _QFIELD_SIZEOF(f)   sizeof(((QFIELD_STRUCT*)0)->f)

#define  QFIELD_BUFFER(f)  \
    _QFIELD_(Q_FIELD_BUFFER, f), \
    { Q_FIELD_BUFFER_SIZE, (uint16_t)(_QFIELD_SIZEOF(f) >> 16) }, \
    { Q_FIELD_BUFFER_SIZE, (uint16_t) _QFIELD_SIZEOF(f) }

#define  QFIELD_END           \
        { Q_FIELD_END, 0 },   \
    };

extern void  qemu_put_struct(QEMUFile*  f, const QField*  fields, const void*  s);
extern int   qemu_get_struct(QEMUFile*  f, const QField*  fields, void*  s);

#endif /* _QEMU_FILE_H */
