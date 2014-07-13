#define BLINK 0x80
#define LIGHT 0x08
#define RED	0x4
#define GREEN	0x2
#define BLUE	0x1
#define WHITE (RED+GREEN+BLUE)
#define BLACK (0+0+0)
#define YELLOW (RED+GREEN+0) 
#define VGACHAR(blink, light, fg,bg) (blink + (bg<<4) + light + fg)
#define ERRORCHAR VGACHAR(BLINK, LIGHT, YELLOW, BLACK)
