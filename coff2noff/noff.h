/* noff.h 
 *     Data structures defining the Nachos Object Code Format
 *
 *     Basically, we only know about three types of segments:
 *	code (read-only), initialized data, and unitialized data
 */

#define NOFFMAGIC	0xbadfad 	/* magic number denoting Nachos 
					 * object code file 
					 */

// 23-0126[j]: 記錄一個 segment 的資訊(虛擬位址、在檔案中的位址、SegmentSize)
typedef struct segment {
  int virtualAddr;		/* location of segment in virt addr space */
  int inFileAddr;		/* location of segment in this file */
  int size;			/* size of segment */
} Segment;

// 23-0126[j]: 記錄一個 coff檔案 的「所有segment資訊」
//             也就3個：code(text)、initData(data)、uninitData(bss)
typedef struct noffHeader {
   int noffMagic;		/* should be NOFFMAGIC */
   Segment code;		/* executable code segment */ 
   Segment initData;		/* initialized data segment */
#ifdef RDATA
   Segment readonlyData;	/* read only data */
#endif
   Segment uninitData;		/* uninitialized data segment --
				               * should be zero'ed before use 
				               */
} NoffHeader;
