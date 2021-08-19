// Format of an ELF executable file

#define ELF_MAGIC 0x464C457FU  // "\x7FELF" in little endian

// File header
typedef struct elfhdr {
  uint magic;  // must equal ELF_MAGIC
  uchar elf[12];
  ushort type;
  ushort machine;
  uint version;
  uint64 entry;
  uint64 phoff;
  uint64 shoff;
  uint flags;
  ushort ehsize;
  ushort phentsize;
  ushort phnum;
  ushort shentsize;
  ushort shnum;
  ushort shstrndx;
}elfhdr;

// Program section header
typedef struct proghdr {
  uint32 type;
  uint32 flags;
  uint64 off;
  uint64 vaddr;
  uint64 paddr;
  uint64 filesz;
  uint64 memsz;
  uint64 align;
}proghdr;

//section header
typedef struct secthdr{
    uint32   sh_name;  // 节区名称相对于字符串表的位置偏移
    uint32   sh_type;  // 节区类型
    uint64   sh_flags;  // 节区标志位集合
    uint64   sh_addr;  // 节区装入内存的地址
    uint64   sh_offset;  // 节区相对于文件的位置偏移
    uint64   sh_size;  // 节区内容大小
    uint32   sh_link;  // 指定链接的节索引，与具体的节有关
    uint32   sh_info;  // 指定附加信息
    uint64   sh_addralign;  // 节装入内存的地址对齐要求
    uint64   sh_entsize;  // 指定某些节的固定表大小，与具体的节有关
}secthdr ;

//debug line information
typedef struct dlhdr{
	uint32  dl_length;
	uint16  dl_dfversion;
	uint32  dl_prolength;
	uint8   dl_milength;
	uint8   dl_isstmt;
 	int     dl_linebase; //modfiy with signed interger
 	uint8   dl_linerange;
	uint8   dl_opcodebase;
}dlhdr;


//ELF file section header type
#define SHT_NULL      0 //无对应节区，该节其他字段取值无意义
#define SHT_PROGBITS  1 //程序数据
#define SHT_SYMTAB    2 // 符号表
#define SHT_STRTAB   3 //字符串表
#define SHT_RELA    4 //带附加的重定位项
#define SHT_HASH    5 //符号哈希表
#define SHT_DYNAMIC   6 //动态链接信息
#define SHT_NOTE    7 //提示性信息
#define SHT_NOBITS    8//无数据程序空间（bss）
#define SHT_REL       9 //无附加的重定位项
#define SHT_SHLIB    10 //保留
#define SHT_DYNSYM     11//动态链接符号表
#define SHT_INIT_ARRAY    14 //构造函数数组
#define SHT_FINI_ARRAY    15 //析构函数数组
#define SHT_PREINIT_ARRAY   16 //
#define SHT_GROUP            17//
#define SHT_SYMTAB_SHNDX    18 //
#define SHT_NUM            19//
#define SHT_LOPROC    0x70000000
#define SHT_HIPROC     0x7fffffff
#define SHT_LOUSER  0x80000000
#define SHT_HIUSER  0x8fffffff

//section header flag
#define SHF_WRITE  1
#define SHF_ALLOC  2
#define SHF_EXECINSTR 4
#define SHF_MASKPROC 8


// Values for Proghdr type
#define ELF_PROG_LOAD           1

// Flag bits for Proghdr flags
#define ELF_PROG_FLAG_EXEC      1
#define ELF_PROG_FLAG_WRITE     2
#define ELF_PROG_FLAG_READ      4
