#include <stdarg.h>
#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "elf.h"
#include "linetable.h"

#define ALIGN_PTR(cptr,sptr,b) ((cptr) + ((b) -((cptr)-(sptr))%(b)))
#define MAX_TABLE_ROW_LEN   10000
#define MAX_FILE_ROW_LEN    200
#define MAX_DIR_ROW_LEN     100
#define MAX_FILE_NAME_LEN   64
#define MAX_DIR_NAME_LEN    64
#define ELF_FILE_NAME              "kernel.elf"
#define ELF_DATA_LEN        524288

// line table
struct linetablerow g_linetable[MAX_TABLE_ROW_LEN];
// dir table
struct dirdesc g_dirtable[MAX_DIR_ROW_LEN];
// file table
struct filedesc g_filetable[MAX_FILE_ROW_LEN];

//elf table
uint8 g_elfdata[ELF_DATA_LEN];
uint32 g_elffile_sz;
uint32 g_linetable_sz;
uint32 g_filetable_sz;
uint32 g_dirtable_sz;

void fatal_impl(const char* file, int line, const char* fmt, ...) {
  va_list ap;
  va_start(ap, fmt);
  printf("%s:%d: ", file, line);
  //vprintf(fmt, ap);
  printf("\n");
  exit(1);
}

#define fatal(fmt, args...) fatal_impl(__FILE__, __LINE__, fmt, ##args)
#define check(x) if (!(x)) { fatal("check %s failed", #x); }

//#define trace_on
#ifdef trace_on
#define trace printf
#else
#define trace if (0) printf
#endif

extern int ltloadelf(char * file);
extern secthdr * ltlookupsection(const char * target);
extern void ltparseline();
extern int ltaddr2line(uint64 address, char * file, int* line);
extern void ltdump();

uint32 ltinit(){
	g_linetable_sz = 0;
	g_filetable_sz = 0;
	g_dirtable_sz = 0;
	trace("init lttable!\n\r");
	ltloadelf(ELF_FILE_NAME);
	ltparseline();
	//ltdump();
	return 0;
}

static int strcmp(const char *p, const char *q)
{
  while(*p && *p == *q)
    p++, q++;
  return (uchar)*p - (uchar)*q;
}


static int ltchar2int(int8 x){
	if(x&(1<<7)){
	   return x-256;
	}else{
	   return x;
	}
}


static uint8 * ltread(uint8  * src,uint8* dst, int n) {
	memmove(dst, src, n);
	return src + n;
}

static uint8 * ltreaduint8(uint8  * src,uint8* out) { 
	return ltread(src,out, 1);
}


static uint8 *ltreadint8(int8  * src,int8 * out) {
	memmove(out, src, 1);
	return (uint8 *)src + 1;
}

static uint8 * ltreaduint16(uint8  * src,uint16 * out) { 
	return ltread(src,(uint8 *)out, 2); 
}

static uint8 * ltreaduint32(uint8  * src,uint32 * out) { 
	return ltread(src,(uint8 *)out, 4);
}


static uint8 * ltreaduint64(uint8  * src,uint64 * out) { 
	return ltread(src,(uint8 *)out, 8);
}

static uint8 * ltreadstr(uint8  * src,char * dst) {
	uint8 c;
	int len = 0;
	
	for (;;) {
	  src = ltreaduint8(src,&c);
	  dst[len] = c;
	  len++;
	  if (c == 0)
		break;
	}
	
	return src;
}

static uint8 * ltreaduleb128(uint8  * src,uint64 * out) {
	uint64 value = 0;
	int shift = 0;
	uint8 b;
	
	for (;;) {
	  src = ltreaduint8(src,&b);
	  value |= ((uint64)b & 0x7F) << shift;
	  if ((b & 0x80) == 0)
	    break;
	  shift += 7;
	}
	
	if (out)
	  *out = value;
	return src;
}

static uint8 * ltreadsleb128(uint8 * src,uint64 * out) {
    uint64 value = 0;
    int shift = 0;
    uint8 b;
	
    for (;;) {
	  src = ltreaduint8(src,&b);
      value |= ((uint64)b & 0x7F) << shift;
      shift += 7;
      if ((b & 0x80) == 0)
        break;
    }
    if (shift < 64 && (b & 0x40))
      value |= -(1 << shift);
    if (out)
      *out = value;
	
    return src;
}



/*
static uint8 * ltreadinitiallength(uint8 * src,uint32 *out) {
	src = ltreaduint32(src,out);
	if ((*out & 0xFFFFFFF0) == 0xFFFFFFF0)
	  fatal("initial length extension (dwarf64?) unimplemented");
	return src;
}


static uint8 * ltalign(uint8 * curr,uint8 * start,int boundary) {
	return ALIGN_PTR(curr,start,boundary);
}
*/

static int ltinitregister(struct lineregister *reg,uint32 offset){
	reg->address = 0;
	reg->file = offset;
	reg->line = 1;
	reg->column = 0;
	reg->basic_block = 0;
	reg->end_sequence = 0;
	reg->prologue_end = 0;
	reg->epilogue_begin = 0;
	reg->isa = 0;
	reg->discriminator = 0;
	
	return 0;
}

static int ltemit(const struct lineregister *reg){
    if (reg->address != 0) {
	  //int file = reg.file;
	  trace("emit %p %d %d\n",reg->address,reg->file,reg->line);
	  g_linetable[g_linetable_sz].addr = reg->address;
	  g_linetable[g_linetable_sz].file = reg->file;
	  g_linetable[g_linetable_sz].line = reg->line;
	  g_linetable_sz++;
	}

	return 0;
}

int ltloadelf(char * file){
	struct elfhdr *elf;
	struct inode *ip;

	// open elf file
	begin_op();
	if((ip = namei(file)) == 0){
		end_op();
		return -1;
	}
	ilock(ip);

	//read elf 
	g_elffile_sz = readi(ip, 0, (uint64)g_elfdata, 0, sizeof(g_elfdata));
	if(g_elffile_sz < sizeof(struct elfhdr))
		goto bad;
	trace("read elf file,size = %d\n\r",g_elffile_sz);

	// Check ELF header
	elf = (struct elfhdr *)g_elfdata;
	if(elf->magic != ELF_MAGIC)
	   goto bad;
	iunlockput(ip);
    end_op();
    ip = 0;	
	
	trace("read elf file success!\n");
bad:
   if(ip){
	 iunlockput(ip);
	 end_op();
   }
   
   return -1;
}

secthdr * ltlookupsection(const char * target) {
  struct elfhdr * header = (elfhdr *)g_elfdata;
  struct secthdr * sheaders = (struct secthdr*)(g_elfdata + header->shoff);
  int sheader_count = header->shnum;

  struct secthdr * sheader_names = &sheaders[header->shstrndx];
  char* names = (char*)&g_elfdata[sheader_names->sh_offset];

  //get the target section
  for (int i = 0; i < sheader_count; i++) {
    struct secthdr* shdr = &sheaders[i];
    char* name = names + shdr->sh_name;
    if (strcmp(name, target) == 0){
	  trace("lookup section success!\n");
      return shdr;
    }
  }
  
  return NULL;
}

static int ltparseone(uint8 * data, int len)
{	
  uint8 * p = data;
  uint8 * end = NULL;
  dlhdr header;  
  char  c = 0;
  uint32 fileoffset = g_filetable_sz;
  uint32 diroffset = g_dirtable_sz;
  struct lineregister regs;
  
  //read header Length
  p = ltreaduint32(p, &header.dl_length);
  p = ltreaduint16(p, &header.dl_dfversion);
  p = ltreaduint32(p, &header.dl_prolength);
  p = ltreaduint8(p, &header.dl_milength);
  p = ltreaduint8(p, &header.dl_isstmt);
  p = ltreadint8((int8 *)p,(int8 *)&c);
  header.dl_linebase = ltchar2int(c);
  p = ltreaduint8(p, &header.dl_linerange);
  p = ltreaduint8(p, &header.dl_opcodebase);
  end = data + header.dl_length + 4;
  
  trace("Offset:%d\n",header.dl_length);  
  trace("DWARF Version:%d\n",header.dl_dfversion);
  trace("Prologue Length:%d\n",header.dl_prolength);
  trace("Minimum Instruction Length:%d\n",header.dl_milength);
  trace("Initial value of 'is_stmt':%d\n",header.dl_isstmt);
  trace("Line Base:%d\n",header.dl_linebase);
  trace("Line Range:%d\n",header.dl_linerange);
  trace("Opcode Base:%d\n",header.dl_opcodebase);

  //Opcodes
  for (int i = 1; i < header.dl_opcodebase; i++) {
	// In theory, we could record the opcode lengths here.
	// But we don't need them.
	uint8 c;
	p = ltreaduint8(p,&c);
    trace("%d op = %d\n",i,c); 
  }

  //Directory table
  for (;;) {
	char dir[MAX_FILE_NAME_LEN];
	p = ltreadstr(p, dir);
  	if(strlen(dir) == 0)
		break;	
	//record path.
	memset(g_dirtable[g_dirtable_sz].name,0,MAX_FILE_NAME_LEN);
	memmove((void *)g_dirtable[g_dirtable_sz].name,(void *)dir,strlen(dir));
	trace("read dirname  = %s,index = %d\n",g_dirtable[g_dirtable_sz].name,g_dirtable_sz);
	g_dirtable_sz++;
  }

  //File Name Table
  for (;;) {
	char file[MAX_FILE_NAME_LEN];
	p = ltreadstr(p, file);
	if(strlen(file) == 0)
		break;
	uint64 dir_index = 0, mtime = 0, file_length = 0;
	p = ltreaduleb128(p,&dir_index);
	p = ltreaduleb128(p,&mtime);
	p = ltreaduleb128(p,&file_length);
	memmove(g_filetable[g_filetable_sz].name,file,strlen(file));
	g_filetable[g_filetable_sz].dir = dir_index + diroffset - 1;
	g_filetable[g_filetable_sz].idx = g_filetable_sz;
	trace("read filename  = %s,dir = %d ,index = %d\n",file,dir_index,g_filetable_sz);
	g_filetable_sz++;
	trace("%s %d %d %d\n", file, (int)dir_index, (int)mtime, (int)file_length);
  }

  //Registers regs(default_is_stmt);
  regs.is_stmt = header.dl_isstmt;
  ltinitregister(&regs,fileoffset);
  
  while (p < end) {
	uint8 op;
	uint16 pc;
	uint64 tmp;
	uint64 len;
    uint64 addr;
	uint64 delta;
	uint64 file;
	int adjusted_opcode;
	int address_increment;
	char str[MAX_FILE_NAME_LEN];
	//int line_increment;
	
	// read op 
	p = ltreaduint8(p,&op);
	trace("op = %x \n", op);
	switch (op) {
		case 0x0: { // extended
		  p = ltreaduleb128(p,&len);  // length
		  p = ltreaduint8(p,&op);
		  trace("exten op code = %d\n",op);
		  switch (op) {
			  case 0x01:  // DW_LNE_end_sequence
				trace("end sequence\n");
				regs.end_sequence = 1;
				ltemit(&regs);
				ltinitregister(&regs,fileoffset);
				//regs = Registers(default_is_stmt);
				break;
			  case 0x02:  // DW_LNE_set_address
				p = ltreaduint64(p,&addr);
				trace("set addr %p\n", addr);
				regs.address = addr;
				break;
			  case 0x04:  // DW_LNE_set_discriminator
				ltreaduleb128(p,&addr);
				trace("set discriminator %d\n", (int)addr);
				regs.discriminator = (int)addr;
				break;
			  case 0x03:  // DW_LNE_define_file
				p = ltreadstr(p, str);
			    p = ltreaduleb128(p,&tmp);
				p = ltreaduleb128(p,&tmp);
				p = ltreaduleb128(p,&tmp);
			  case 0x80:  // DW_LNE_lo_user
			  case 0xff:  // DW_LNE_hi_user
			  default:
				trace("unhandled extended op 0x%x\n", op);
		  }
		  break;
		}

		case 0x1:  // DW_LNS_copy
		  trace("copy\n");
		  ltemit(&regs);
		  regs.basic_block = 0;
		  regs.prologue_end = 0;
		  regs.epilogue_begin = 0;
		  //regs.discriminator = 0;
		  break;

		case 0x2: {  // DW_LNS_advance_pc
		  p = ltreaduleb128(p,&delta);
		  delta *= header.dl_milength;
		  regs.address += delta;
		  trace("advance pc %d => %p\n", (int)delta, (long long)regs.address);
		  break;
		}

		case 0x3: {  // DW_LNS_advance_line
		  p = ltreadsleb128(p,&delta);
		  regs.line += delta;
		  trace("advance line %d => %d\n", (int)delta, (int)regs.line);
		  break;
		}

		case 0x4: { // DW_LNS_set_file
		  p = ltreaduleb128(p,&file);
		  regs.file = file + fileoffset - 1;
		  trace("set file = %d\n",regs.file);
		  break;
		}

		case 0x5: { // DW_LNS_set_column
		  p = ltreaduleb128(p,&tmp);
		  trace("unimpl DW_LNS_set_column\n");
		  break;
		}

		case 0x6: { // DW_LNS_negate_stmt
		  trace("negate stmt\n");
		  regs.is_stmt = 1 - regs.is_stmt;
		  break;
		}

		case 0x7: { // DW_LNS_set_basic_block
		  regs.basic_block = 1;
		  trace("unimpl\n");
		  break;
		}

		case 0x8: { // DW_LNS_const_add_pc
		  adjusted_opcode = 255 - header.dl_opcodebase;
		  address_increment = (adjusted_opcode/header.dl_linerange) * header.dl_milength;
		  regs.address += address_increment;
		  trace("add pc %d => %p\n", address_increment, regs.address);
		  break;
		}

		case 0x9: { // DW_LNS_fixed_advance_pc
		  p = ltreaduint16(p,&pc);
		  regs.address += pc;
		  trace("DW_LNS_fixed_advance_pc address = %p\n",regs.address);
		  break;
		}

		case 0xa: { // DW_LNS_set_prologue_end
		  regs.prologue_end = 1;
		  break;
		}

		case 0xb: { // DW_LNS_set_epilogue_begin
		  regs.epilogue_begin = 1;
		  break;
		}

		case 0xc: { // DW_LNS_set_isa
		  regs.isa = 1;
		  break;
		}
		
		default: {
		   break;
		}
	}
  }
  trace("detech len = %d\n",end - data);
  
  return end - data;
}

void ltparseline() {
  uint8 * data = NULL;
  int len = 0;  

  //read elf debug_line section
  struct secthdr * shdr_lines = ltlookupsection(".debug_line");
  if (!shdr_lines)
    fatal("couldn't find .debug_line\n");

  //parse debug line section
  data = g_elfdata + shdr_lines->sh_offset;
  len = shdr_lines->sh_size;
  trace("section len = %d\n",len);
   
  //parese one line
  while (len > 0) {
    uint32 one_len = ltparseone(data, len);	
    data += one_len;
    len -= one_len;
	trace("parse len = %d\n",len);
  }
  //ltdump();
  //sort line number
}

int ltaddr2line(uint64 address, char * file, int* line) {
   int left = 0;
   int right = g_linetable_sz - 1;
   int target = -1;
   char path[128];

   //upper_bound 
   while(left <= right){
	   int mid = (left + right)>>1;
	   if(address < g_linetable[mid].addr){
		 right = mid - 1;
	   }else{
	     target = mid;
		 left = mid + 1;
	   }
   }

   if(target < 0) return -1;
   int fid = g_linetable[target].file;
   int did = g_filetable[fid].dir;
   memset(path,0,sizeof(path));
   memmove(file,g_dirtable[did].name,strlen(g_dirtable[did].name));
   memmove(file + strlen(file),"/",1);
   memmove(file + strlen(file),g_filetable[fid].name,strlen(g_filetable[fid].name));
   *line = g_linetable[target].line;
   
   return 0;
}

void ltdump(){
	//dump dir table
	printf("dir table:\n");
	printf("index    dirname\n");
	printf("------------------------------------------------------\n");
	for(int i = 0; i < g_dirtable_sz; ++i){
		printf(" %d     %s\n",i,g_dirtable[i].name);
	}
	
	//dump file table
	printf("file table:\n");
	printf("index    dindex     filename\n");
	printf("------------------------------------------------------\n");
	for(int i = 0; i < g_filetable_sz; ++i){
		printf(" %d       %s/%s\n",i,g_dirtable[g_filetable[i].dir].name,g_filetable[i].name);
	}
	
	//dump line table
	printf("address table:\n");
	printf("index    address      filename         line\n");
	printf("------------------------------------------------------\n");
	for(int i = 0; i < g_linetable_sz; ++i){
		printf(" %d       %s     %d      %p\n",i,g_filetable[g_linetable[i].file].name,g_linetable[i].line,g_linetable[i].addr);
	}
}


