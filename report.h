#ifndef _report_h__
#define _report_h__

#ifdef __cplusplus
extern "C" unsigned char *memsurface_data;
extern "C" unsigned char used2[256];
extern "C" int last_pc;
#else
extern unsigned char *memsurface_data;
extern unsigned char used2[256];
extern int last_pc;
#endif

#define report_memread(addr) do { int idx = (((addr)&0xff00)<<4)+2; idx+= ((addr)%256)<<3; memsurface_data[idx]=0xff; memsurface_data[idx+2048]=0xff; memsurface_data[idx+4]=0xff; memsurface_data[idx+4+2048]=0xff; used2[((addr)&0xff00)>>8]=1; } while(0)

#define report_memread2(addr, opcode) do { int i, idx; if ((addr)==last_pc) { break; } else { for (i=0; i<5; i++) { idx = (((addr)+i)&0xff00)<<4; idx+= (((addr)+i)%256)<<3; memsurface_data[idx]=0xff; memsurface_data[idx+2048]=0xff; memsurface_data[idx+4]=0xff; memsurface_data[idx+4+2048]=0xff; used2[((addr)&0xff00)>>8]=1;  } } } while(0)

#define report_memwrite(addr) do { int idx = (((addr)&0xff00)<<4)+1; idx += ((addr) % 256)<<3; memsurface_data[idx]=0xff; memsurface_data[idx+4] = 0xff; memsurface_data[idx+2048]=0xff; memsurface_data[idx+2048+4] = 0xff; } while(0)

#endif

