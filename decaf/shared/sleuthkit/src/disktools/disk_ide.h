

#define DISK_48	0x01
#define DISK_HPA 0x02

typedef struct {
	uint64_t capacity;
	uint8_t	flags;
} DISK_INFO;

DISK_INFO * identify_device (int fd);
uint64_t get_native_max (int fd);
void set_max (int fd, uint64_t addr);

