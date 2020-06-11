#ifndef _CRC32_H_
#define _CRC32_H_

uint32_t crc32(uint32_t crc, const void *buf, size_t size);
uint32_t file_crc32(char* file);

#endif /* _CRC32_H_ */
