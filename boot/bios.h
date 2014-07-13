#define READ 2
#define WRITE 3
#define DISKA 0
#define DISKB 1
#define DISKC 0x80

/* dh: head, dl: drive
   ch: track, cl: sector(count from 1)
   es:bx: buffer address
   ah: service, al: nr of sector 
   
   cf=0:succeed
   cf=1:fail	
   al=error state */
#define drive dl
#define cyl ch
#define head dh
#define sect cl		/* count from 1 */
#define nsect al
#define service ah

#if 0
struct word {
	char code;
	struct attr {
		foreground:3;
		light:1;	/* only applied to foreground */
		background:3;
		blink:1;	/* only applied to foreground */
	};
}
#endif

#define VIDEOSEG (0xb800)
#define VIDEOPA (0xb8000)
#define BYTEPERLINE (160)
#define BLINK 0x80
#define LIGHT 0x08
#define RED	0x4
#define GREEN	0x2
#define BLUE	0x1
#define WHITE (RED+GREEN+BLUE)
#define BLACK (0+0+0)
#define YELLOW (RED+GREEN+0) 
#define ATTR(blink, light, fg,bg) (blink + (bg<<4) + light + fg)
#define ERRORCHAR ATTR(BLINK, LIGHT, YELLOW, BLACK)

#define echo \
movw $VIDEOSEG,%ax; \
movw %ax,%es; \
xorw %di,%di; \
movb $'x',%al; \
movb $ERRORCHAR,%ah; \
movw $80,%cx; \
cld; rep; stosw; 1: jmp 1b 
