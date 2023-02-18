#ifndef CASTS_H
#define CASTS_H

/* Useful type casts */
#define cast(t, exp)	((t)(exp))

#define castVoid(i)	cast(void, (i))
#define castVoidPtr(i)	cast(void *, (i))
#define castInt(i)	cast(int32_t, (i))
#define castUInt(i)	cast(uint32_t, (i))
#define castByte(i)	cast(unsigned char, (i))
#define castCharPtr(i)	cast(char *, (i))
#define castSizeT(i)	cast(size_t, (i))

#endif // CASTS_H
