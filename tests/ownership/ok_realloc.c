#include <stdlib.h>
void ok1(void); void ok2(void); void ok3(void); void ok4(void);
void ok1(void){ [[owner]] char *p = malloc(4); if(!p) return; p = realloc(p, 8); free(p); }
void ok2(void){ [[owner]] char *p = NULL; p = realloc(p, 8); free(p); }
void ok3(void){ [[owner]] char *p = malloc(4); if(!p) return; for(int i=0;i<3;i++) p = realloc(p, 8*i+8); free(p); }
void ok4(void){ [[owner]] char *p = malloc(4); if(!p) return; p = realloc(p, 8); free(p); }
int main(void) { ok1(); ok2(); ok3(); ok4(); return 0; }
